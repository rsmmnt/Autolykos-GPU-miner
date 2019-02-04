#include "autolykos.h"
#include <cuda.h>
#include <curand.h>

// Little-endian byte access
#ifndef B2B_GET64
#define B2B_GET64(p)                            \
    (((uint64_t) ((uint8_t *) (p))[0]) ^        \
    (((uint64_t) ((uint8_t *) (p))[1]) << 8) ^  \
    (((uint64_t) ((uint8_t *) (p))[2]) << 16) ^ \
    (((uint64_t) ((uint8_t *) (p))[3]) << 24) ^ \
    (((uint64_t) ((uint8_t *) (p))[4]) << 32) ^ \
    (((uint64_t) ((uint8_t *) (p))[5]) << 40) ^ \
    (((uint64_t) ((uint8_t *) (p))[6]) << 48) ^ \
    (((uint64_t) ((uint8_t *) (p))[7]) << 56))
#endif

// Cyclic right rotation
#ifndef ROTR64
#define ROTR64(x, y)  (((x) >> (y)) ^ ((x) << (64 - (y))))
#endif

// G mixing function
#ifndef B2B_G
#define B2B_G(a, b, c, d, x, y)     \
{                                   \
    v[a] = v[a] + v[b] + x;         \
    v[d] = ROTR64(v[d] ^ v[a], 32); \
    v[c] = v[c] + v[d];             \
    v[b] = ROTR64(v[b] ^ v[c], 24); \
    v[a] = v[a] + v[b] + y;         \
    v[d] = ROTR64(v[d] ^ v[a], 16); \
    v[c] = v[c] + v[d];             \
    v[b] = ROTR64(v[b] ^ v[c], 63); \
}
#endif

////////////////////////////////////////////////////////////////////////////////
//  First iteration of hashes precalculation
////////////////////////////////////////////////////////////////////////////////
__global__ void initPrehash(
    const void * data,
    // hashes
    uint32_t * hash,
    uint32_t * next
) {
    uint32_t j;
    uint32_t tid = threadIdx.x;

    // shared memory
    __shared__ uint32_t shared[2 * B_DIM];

    shared[2 * tid] = data[2 * tid];
    shared[2 * tid + 1] = data[2 * tid + 1];
    __syncthreads();

    // 8 * 64 bits = 64 bytes 
    uint64_t * blake2b_iv = (uint64_t *)shared;
    // 192 * 8 bits = 192 bytes 
    uint8_t * sigma = (uint8_t *)(shared + 16);
    //uint32_t * sk = shared + 64;
    // pk || mes || w
    uint32_t * rem = shared + 72;

    // local memory
    // 64 * 32 bits
    uint32_t local[64];

    // 16 * 64 bits = 128 bytes 
    uint64_t * v = (uint64_t *)local;
    // 16 * 64 bits = 128 bytes 
    uint64_t * m = v + 16;
    blake2b_ctx * ctx = (blake2b_ctx *)(local + 8);

    tid = threadIdx.x + blockDim.x * blockIdx.x;

    //====================================================================//
    //  Initialize context
    //====================================================================//
#pragma unroll
    for (j = 0; j < 8; ++j)
    {
        ctx->h[j] = blake2b_iv[j];
    }

    ctx->h[0] ^= 0x01010000 ^ (0 << 8) ^ NUM_BYTE_SIZE;

    ctx->t[0] = 0;
    ctx->t[1] = 0;
    ctx->c = 0;

#pragma unroll
    for (j = 0; j < 128; ++j)
    {
        ctx->b[j] = 0;
    }

    //====================================================================//
    //  Hash tid
    //====================================================================//
#pragma unroll
    for (j = 0; ctx->c < 128 && j < 4; ++j)
    {
        ctx->b[ctx->c++] = ((const uint8_t *)&tid)[j];
    }

#pragma unroll
    while (j < 4)
    {
        ctx->t[0] += ctx->c;
        ctx->t[1] += 1 - !(ctx->t[0] < ctx->c);

#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            v[i] = ctx->h[i];
            v[i + 8] = blake2b_iv[i];
        }

        v[12] ^= ctx->t[0];
        v[13] ^= ctx->t[1];

#pragma unroll
        for (int i = 0; i < 16; i++)
        {
            m[i] = B2B_GET64(&ctx->b[8 * i]);
        }

#pragma unroll
        for (int i = 0; i < 192; i += 16)
        {
            B2B_G(0, 4,  8, 12, m[sigma[i +  0]], m[sigma[i +  1]]);
            B2B_G(1, 5,  9, 13, m[sigma[i +  2]], m[sigma[i +  3]]);
            B2B_G(2, 6, 10, 14, m[sigma[i +  4]], m[sigma[i +  5]]);
            B2B_G(3, 7, 11, 15, m[sigma[i +  6]], m[sigma[i +  7]]);
            B2B_G(0, 5, 10, 15, m[sigma[i +  8]], m[sigma[i +  9]]);
            B2B_G(1, 6, 11, 12, m[sigma[i + 10]], m[sigma[i + 11]]);
            B2B_G(2, 7,  8, 13, m[sigma[i + 12]], m[sigma[i + 13]]);
            B2B_G(3, 4,  9, 14, m[sigma[i + 14]], m[sigma[i + 15]]);
        }

#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            ctx->h[i] ^= v[i] ^ v[i + 8];
        }

        ctx->c = 0;
       
#pragma unroll
        while (ctx->c < 128 && j < 4)
        {
            ctx->b[ctx->c++] = ((const uint8_t *)tid)[j++];
        }
    }

    //====================================================================//
    //  Hash constant message
    //====================================================================//
    for (j = 0; ctx->c < 128 && j < 0x1000; ++j)
    {
        ctx->b[ctx->c++] = !(j & 3) * (j >> 2);
    }

    while (j < 0x1000)
    {
        ctx->t[0] += ctx->c;
        ctx->t[1] += 1 - !(ctx->t[0] < ctx->c);

#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            v[i] = ctx->h[i];
            v[i + 8] = blake2b_iv[i];
        }

        v[12] ^= ctx->t[0];
        v[13] ^= ctx->t[1];

#pragma unroll
        for (int i = 0; i < 16; i++)
        {
            m[i] = B2B_GET64(&ctx->b[8 * i]);
        }

#pragma unroll
        for (int i = 0; i < 192; i += 16)
        {
            B2B_G(0, 4,  8, 12, m[sigma[i +  0]], m[sigma[i +  1]]);
            B2B_G(1, 5,  9, 13, m[sigma[i +  2]], m[sigma[i +  3]]);
            B2B_G(2, 6, 10, 14, m[sigma[i +  4]], m[sigma[i +  5]]);
            B2B_G(3, 7, 11, 15, m[sigma[i +  6]], m[sigma[i +  7]]);
            B2B_G(0, 5, 10, 15, m[sigma[i +  8]], m[sigma[i +  9]]);
            B2B_G(1, 6, 11, 12, m[sigma[i + 10]], m[sigma[i + 11]]);
            B2B_G(2, 7,  8, 13, m[sigma[i + 12]], m[sigma[i + 13]]);
            B2B_G(3, 4,  9, 14, m[sigma[i + 14]], m[sigma[i + 15]]);
        }

#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            ctx->h[i] ^= v[i] ^ v[i + 8];
        }

        ctx->c = 0;
       
        for ( ; ctx->c < 128 && j < 0x1000; ++j)
        {
            ctx->b[ctx->c++] = !(j & 3) * (j >> 2);
        }
    }

    //====================================================================//
    //  Hash public key, message & one-time public key
    //====================================================================//
    for (j = 0; ctx->c < 128 && j < 3 * NUM_BYTE_SIZE; ++j)
    {
        ctx->b[ctx->c++] = ((const uint8_t *)rem)[j];
    }

    while (j < 3 * NUM_BYTE_SIZE)
    {
        ctx->t[0] += ctx->c;
        ctx->t[1] += 1 - !(ctx->t[0] < ctx->c);

#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            v[i] = ctx->h[i];
            v[i + 8] = blake2b_iv[i];
        }

        v[12] ^= ctx->t[0];
        v[13] ^= ctx->t[1];

#pragma unroll
        for (int i = 0; i < 16; i++)
        {
            m[i] = B2B_GET64(&ctx->b[8 * i]);
        }

#pragma unroll
        for (int i = 0; i < 192; i += 16)
        {
            B2B_G(0, 4,  8, 12, m[sigma[i +  0]], m[sigma[i +  1]]);
            B2B_G(1, 5,  9, 13, m[sigma[i +  2]], m[sigma[i +  3]]);
            B2B_G(2, 6, 10, 14, m[sigma[i +  4]], m[sigma[i +  5]]);
            B2B_G(3, 7, 11, 15, m[sigma[i +  6]], m[sigma[i +  7]]);
            B2B_G(0, 5, 10, 15, m[sigma[i +  8]], m[sigma[i +  9]]);
            B2B_G(1, 6, 11, 12, m[sigma[i + 10]], m[sigma[i + 11]]);
            B2B_G(2, 7,  8, 13, m[sigma[i + 12]], m[sigma[i + 13]]);
            B2B_G(3, 4,  9, 14, m[sigma[i + 14]], m[sigma[i + 15]]);
        }

#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            ctx->h[i] ^= v[i] ^ v[i + 8];
        }

        ctx->c = 0;
       
        while (ctx->c < 128 && j < 3 * NUM_BYTE_SIZE)
        {
            ctx->b[ctx->c++] = ((const uint8_t *)rem)[j++];
        }
    }

    //====================================================================//
    //  Finalize hash
    //====================================================================//
    ctx->t[0] += ctx->c;
    ctx->t[1] += 1 - !(ctx->t[0] < ctx->c);

    while (ctx->c < 128)
    {
        ctx->b[ctx->c++] = 0;
    }

#pragma unroll
    for (int i = 0; i < 8; ++i)
    {
        v[i] = ctx->h[i];
        v[i + 8] = blake2b_iv[i];
    }

    v[12] ^= ctx->t[0];
    v[13] ^= ctx->t[1];
    v[14] = ~v[14];

#pragma unroll
    for (int i = 0; i < 16; i++)
    {
        m[i] = B2B_GET64(&ctx->b[8 * i]);
    }

#pragma unroll
    for (int i = 0; i < 192; i += 16)
    {
        B2B_G(0, 4,  8, 12, m[sigma[i +  0]], m[sigma[i +  1]]);
        B2B_G(1, 5,  9, 13, m[sigma[i +  2]], m[sigma[i +  3]]);
        B2B_G(2, 6, 10, 14, m[sigma[i +  4]], m[sigma[i +  5]]);
        B2B_G(3, 7, 11, 15, m[sigma[i +  6]], m[sigma[i +  7]]);
        B2B_G(0, 5, 10, 15, m[sigma[i +  8]], m[sigma[i +  9]]);
        B2B_G(1, 6, 11, 12, m[sigma[i + 10]], m[sigma[i + 11]]);
        B2B_G(2, 7,  8, 13, m[sigma[i + 12]], m[sigma[i + 13]]);
        B2B_G(3, 4,  9, 14, m[sigma[i + 14]], m[sigma[i + 15]]);
    }

#pragma unroll
    for (int i = 0; i < 8; ++i)
    {
        ctx->h[i] ^= v[i] ^ v[i + 8];
    }

#pragma unroll
    for (j = 0; j < NUM_BYTE_SIZE; ++j)
    {
        ((uint8_t *)local)[j] = (ctx->h[j >> 3] >> ((j & 7) << 3)) & 0xFF;
    }

    //===================================================================//
    //  Dump hashult to global memory
    //===================================================================//
    j = ((uint64_t *)local)[3] <= FQ3 && ((uint64_t *)local)[2] <= FQ2
        && ((uint64_t *)local)[1] <= FQ1 && ((uint64_t *)local)[0] <= FQ0;

    next[tid] = (1 - !j) * (tid + 1);

#pragma unroll
    for (int i = 0; i < 8; ++i)
    {
        hash[(tid << 3) + i] = local[i];
    }
}

////////////////////////////////////////////////////////////////////////////////
//  Next iteration of hashes precalculation
////////////////////////////////////////////////////////////////////////////////
__global__ void sortNext(
    const void * data,
    // hashes
    const void * hash,
    void * which
) {
    // numer of index in which
    uint32_t tid = threadIdx.x + blockDim.x * blockIdx.x;

    (uint32_t *)which + tid
        p[3] <= FQ3 && p[2] <= FQ2 && p[1] <= FQ1 & p[0] <= FQ0
}

////////////////////////////////////////////////////////////////////////////////
//  Next iteration of hashes precalculation
////////////////////////////////////////////////////////////////////////////////
__global__ void updatePrehash(
    const uint32_t * data,
    // hashes
    uint32_t * hash,
    uint32_t * next
) {
    uint32_t j;
    uint32_t tid = threadIdx.x;

    // shared memory
    __shared__ uint32_t shared[2 * B_DIM];

    shared[2 * tid] = data[2 * tid];
    shared[2 * tid + 1] = data[2 * tid + 1];
    __syncthreads();

    // 8 * 64 bits = 64 bytes 
    uint64_t * blake2b_iv = (uint64_t *)shared;
    // 192 * 8 bits = 192 bytes 
    uint8_t * sigma = (uint8_t *)(shared + 16);

    // local memory
    // 64 * 32 bits
    uint32_t local[64];

    // 16 * 64 bits = 128 bytes 
    uint64_t * v = (uint64_t *)local;
    // 16 * 64 bits = 128 bytes 
    uint64_t * m = v + 16;
    blake2b_ctx * ctx = (blake2b_ctx *)(local + 8);

    tid = threadIdx.x + blockDim.x * blockIdx.x;

    //====================================================================//
    //  Initialize context
    //====================================================================//
#pragma unroll
    for (j = 0; j < 8; ++j)
    {
        ctx->h[j] = blake2b_iv[j];
    }

    ctx->h[0] ^= 0x01010000 ^ (0 << 8) ^ NUM_BYTE_SIZE;

    ctx->t[0] = 0;
    ctx->t[1] = 0;
    ctx->c = 0;

#pragma unroll
    for (j = 0; j < 128; ++j)
    {
        ctx->b[j] = 0;
    }

    //====================================================================//
    //  Hash previous hash
    //====================================================================//
    for (j = 0; ctx->c < 128 && j < NUM_BYTE_SIZE; ++j)
    {
        ctx->b[ctx->c++]
            = ((const uint8_t *)(hash + ((next[tid] - 1) << 3)))[j];
    }

    while (j < NUM_BYTE_SIZE)
    {
        ctx->t[0] += ctx->c;
        ctx->t[1] += 1 - !(ctx->t[0] < ctx->c);

#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            v[i] = ctx->h[i];
            v[i + 8] = blake2b_iv[i];
        }

        v[12] ^= ctx->t[0];
        v[13] ^= ctx->t[1];

#pragma unroll
        for (int i = 0; i < 16; i++)
        {
            m[i] = B2B_GET64(&ctx->b[8 * i]);
        }

#pragma unroll
        for (int i = 0; i < 192; i += 16)
        {
            B2B_G(0, 4,  8, 12, m[sigma[i +  0]], m[sigma[i +  1]]);
            B2B_G(1, 5,  9, 13, m[sigma[i +  2]], m[sigma[i +  3]]);
            B2B_G(2, 6, 10, 14, m[sigma[i +  4]], m[sigma[i +  5]]);
            B2B_G(3, 7, 11, 15, m[sigma[i +  6]], m[sigma[i +  7]]);
            B2B_G(0, 5, 10, 15, m[sigma[i +  8]], m[sigma[i +  9]]);
            B2B_G(1, 6, 11, 12, m[sigma[i + 10]], m[sigma[i + 11]]);
            B2B_G(2, 7,  8, 13, m[sigma[i + 12]], m[sigma[i + 13]]);
            B2B_G(3, 4,  9, 14, m[sigma[i + 14]], m[sigma[i + 15]]);
        }

#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            ctx->h[i] ^= v[i] ^ v[i + 8];
        }

        ctx->c = 0;
       
        while (ctx->c < 128 && j < NUM_BYTE_SIZE)
        {
            ctx->b[ctx->c++]
                = ((const uint8_t *)(hash + ((next[tid] - 1) << 3)))[j++];
        }
    }

    //====================================================================//
    //  Finalize hash
    //====================================================================//
    ctx->t[0] += ctx->c;
    ctx->t[1] += 1 - !(ctx->t[0] < ctx->c);

    while (ctx->c < 128)
    {
        ctx->b[ctx->c++] = 0;
    }

#pragma unroll
    for (int i = 0; i < 8; ++i)
    {
        v[i] = ctx->h[i];
        v[i + 8] = blake2b_iv[i];
    }

    v[12] ^= ctx->t[0];
    v[13] ^= ctx->t[1];
    v[14] = ~v[14];

#pragma unroll
    for (int i = 0; i < 16; i++)
    {
        m[i] = B2B_GET64(&ctx->b[8 * i]);
    }

#pragma unroll
    for (int i = 0; i < 192; i += 16)
    {
        B2B_G(0, 4,  8, 12, m[sigma[i +  0]], m[sigma[i +  1]]);
        B2B_G(1, 5,  9, 13, m[sigma[i +  2]], m[sigma[i +  3]]);
        B2B_G(2, 6, 10, 14, m[sigma[i +  4]], m[sigma[i +  5]]);
        B2B_G(3, 7, 11, 15, m[sigma[i +  6]], m[sigma[i +  7]]);
        B2B_G(0, 5, 10, 15, m[sigma[i +  8]], m[sigma[i +  9]]);
        B2B_G(1, 6, 11, 12, m[sigma[i + 10]], m[sigma[i + 11]]);
        B2B_G(2, 7,  8, 13, m[sigma[i + 12]], m[sigma[i + 13]]);
        B2B_G(3, 4,  9, 14, m[sigma[i + 14]], m[sigma[i + 15]]);
    }

#pragma unroll
    for (int i = 0; i < 8; ++i)
    {
        ctx->h[i] ^= v[i] ^ v[i + 8];
    }

    for (j = 0; j < NUM_BYTE_SIZE; ++j)
    {
        ((uint8_t *)local)[j] = (ctx->h[j >> 3] >> ((j & 7) << 3)) & 0xFF;
    }
    //===================================================================//
    //  Dump hashult to global memory
    //===================================================================//
    j = ((uint64_t *)local)[3] <= FQ3 && ((uint64_t *)local)[2] <= FQ2
        && ((uint64_t *)local)[1] <= FQ1 && ((uint64_t *)local)[0] <= FQ0;

    next[tid] *= 1 - !j;

#pragma unroll
    for (int i = 0; i < 8; ++i)
    {
        hash[((next[tid] - 1) << 3) + i] = local[i];
    }
}


////////////////////////////////////////////////////////////////////////////////
//  Hash * secret key mod q
////////////////////////////////////////////////////////////////////////////////
__global__ void finalizePrehash(
    const uint32_t * data,
    // hashes
    uint32_t * hash
) {
    uint32_t tid = threadIdx.x;

    // shared memory
    __shared__ uint32_t shared[B_DIM];
    shared[tid] = data[tid + 64];
    __syncthreads();
    // 8 * 32 bits = 32 bytes
    uint32_t * sk = shared;

    // local memory
    uint32_t r[18];
    r[16] = r[17] = 0;

    tid = threadIdx.x + blockDim.x * blockIdx.x;
    uint32_t * x = hash + (tid << 3); 

    //====================================================================//
    //  x[0] * y -> r[0, ..., 7, 8]
    //====================================================================//
    // initialize r[0, ..., 7]
#pragma unroll
    for (int j = 0; j < 8; j += 2)
    {
        asm volatile (
            "mul.lo.u32 %0, %1, %2;": "=r"(r[j]): "r"(x[0]), "r"(sk[j])
        );
        asm volatile (
            "mul.hi.u32 %0, %1, %2;": "=r"(r[j + 1]): "r"(x[0]), "r"(sk[j])
        );
    }

    //====================================================================//
    asm volatile (
        "mad.lo.cc.u32 %0, %1, %2, %0;": "+r"(r[1]): "r"(x[0]), "r"(sk[1])
    );
    asm volatile (
        "madc.hi.cc.u32 %0, %1, %2, %0;": "+r"(r[2]): "r"(x[0]), "r"(sk[1])
    );

#pragma unroll
    for (int j = 3; j < 6; j += 2)
    {
        asm volatile (
            "madc.lo.cc.u32 %0, %1, %2, %0;": "+r"(r[j]): "r"(x[0]), "r"(sk[j])
        );
        asm volatile (
            "madc.hi.cc.u32 %0, %1, %2, %0;":
            "+r"(r[j + 1]): "r"(x[0]), "r"(sk[j])
        );
    }

    asm volatile (
        "madc.lo.cc.u32 %0, %1, %2, %0;": "+r"(r[7]): "r"(x[0]), "r"(sk[7])
    );
    // initialize r[8]
    asm volatile (
        "madc.hi.u32 %0, %1, %2, 0;": "=r"(r[8]): "r"(x[0]), "r"(sk[7])
    );

    //====================================================================//
    //  x[i] * sk -> r[i, ..., i + 7, i + 8]
    //====================================================================//
#pragma unroll
    for (int i = 1; i < 8; ++i)
    {
        asm volatile (
            "mad.lo.cc.u32 %0, %1, %2, %0;": "+r"(r[i]): "r"(x[i]), "r"(sk[0])
        );
        asm volatile (
            "madc.hi.cc.u32 %0, %1, %2, %0;":
            "+r"(r[i + 1]): "r"(x[i]), "r"(sk[0])
        );

#pragma unroll
        for (int j = 2; j < 8; j += 2)
        {
            asm volatile (
                "madc.lo.cc.u32 %0, %1, %2, %0;":
                "+r"(r[i + j]): "r"(x[i]), "r"(sk[j])
            );
            asm volatile (
                "madc.hi.cc.u32 %0, %1, %2, %0;":
                "+r"(r[i + j + 1]): "r"(x[i]), "r"(sk[j])
            );
        }

    // initialize r[i + 8]
        asm volatile (
            "addc.u32 %0, 0, 0;": "=r"(r[i + 8])
        );

    //====================================================================//
        asm volatile (
            "mad.lo.cc.u32 %0, %1, %2, %0;":
            "+r"(r[i + 1]): "r"(x[i]), "r"(sk[1])
        );
        asm volatile (
            "madc.hi.cc.u32 %0, %1, %2, %0;":
            "+r"(r[i + 2]): "r"(x[i]), "r"(sk[1])
        );

#pragma unroll
        for (int j = 3; j < 6; j += 2)
        {
            asm volatile (
                "madc.lo.cc.u32 %0, %1, %2, %0;":
                "+r"(r[i + j]): "r"(x[i]), "r"(sk[j])
            );
            asm volatile (
                "madc.hi.cc.u32 %0, %1, %2, %0;":
                "+r"(r[i + j + 1]): "r"(x[i]), "r"(sk[j])
            );
        }

        asm volatile (
            "madc.lo.cc.u32 %0, %1, %2, %0;":
            "+r"(r[i + 7]): "r"(x[i]), "r"(sk[7])
        );
        asm volatile (
            "madc.hi.u32 %0, %1, %2, %0;":
            "+r"(r[i + 8]): "r"(x[i]), "r"(sk[7])
        );
    }

    //====================================================================//
    //  mod q
    //====================================================================//
    uint64_t * y = (uint64_t *)r; 
    uint32_t d[2]; 
    uint32_t med[6];
    uint32_t carry;

    for (int i = 16; i >= 8; i -= 2)
    {
        *((uint64_t *)d) = ((y[i >> 1] << 4) | (y[(i >> 1) - 1] >> 60))
            - (y[i >> 1] >> 60);

        // correct highest 32 bits
        r[i - 1] = (r[i - 1] & 0x0FFFFFFF) | r[i + 1] & 0x10000000;

    //====================================================================//
    //  d * q -> med[0, ..., 5]
    //====================================================================//
        asm volatile (
            "mul.lo.u32 %0, %1, "q0_s";": "=r"(med[0]): "r"(d[0])
        );
        asm volatile (
            "mul.hi.u32 %0, %1, "q0_s";": "=r"(med[1]): "r"(d[0])
        );
        asm volatile (
            "mul.lo.u32 %0, %1, "q2_s";": "=r"(med[2]): "r"(d[0])
        );
        asm volatile (
            "mul.hi.u32 %0, %1, "q2_s";": "=r"(med[3]): "r"(d[0])
        );

    //====================================================================//
        asm volatile (
            "mad.lo.cc.u32 %0, %1, "q1_s", %0;": "+r"(med[1]): "r"(d[0])
        );
        asm volatile (
            "madc.hi.cc.u32 %0, %1, "q1_s", %0;": "+r"(med[2]): "r"(d[0])
        );
        asm volatile (
            "madc.lo.cc.u32 %0, %1, "q3_s", %0;": "+r"(med[3]): "r"(d[0])
        );
        asm volatile (
            "madc.hi.u32 %0, %1, "q3_s", 0;": "=r"(med[4]): "r"(d[0])
        );

    //====================================================================//
        asm volatile (
            "mad.lo.cc.u32 %0, %1, "q0_s", %0;": "+r"(med[1]): "r"(d[1])
        );
        asm volatile (
            "madc.hi.cc.u32 %0, %1, "q0_s", %0;": "+r"(med[2]): "r"(d[1])
        );
        asm volatile (
            "madc.lo.cc.u32 %0, %1, "q2_s", %0;": "+r"(med[3]): "r"(d[1])
        );
        asm volatile (
            "madc.hi.cc.u32 %0, %1," q2_s", %0;": "+r"(med[4]): "r"(d[1])
        );
        asm volatile (
            "addc.u32 %0, 0, 0;": "=r"(med[5])
        );

    //====================================================================//
        asm volatile (
            "mad.lo.cc.u32 %0, %1, "q1_s", %0;": "+r"(med[2]): "r"(d[1])
        );
        asm volatile (
            "madc.hi.cc.u32 %0, %1, "q1_s", %0;": "+r"(med[3]): "r"(d[1])
        );
        asm volatile (
            "madc.lo.cc.u32 %0, %1, "q3_s", %0;": "+r"(med[4]): "r"(d[1])
        );
        asm volatile (
            "madc.hi.u32 %0, %1, "q3_s", %0;": "+r"(med[5]): "r"(d[1])
        );

    //====================================================================//
    //  r[i/2 - 2, i/2 - 3, i/2 - 4] mod q
    //====================================================================//
        asm volatile (
            "sub.cc.u32 %0, %0, %1;": "+r"(r[i - 8]): "r"(med[0])
        );

#pragma unroll
        for (int j = 1; j < 6; ++j)
        {
            asm volatile (
                "subc.cc.u32 %0, %0, %1;": "+r"(r[i + j - 8]): "r"(med[j])
            );
        }

        asm volatile (
            "subc.cc.u32 %0, %0, 0;": "+r"(r[i - 2])
        );

        asm volatile (
            "subc.cc.u32 %0, %0, 0;": "+r"(r[i - 1])
        );

    //====================================================================//
    //  r[i/2 - 2, i/2 - 3, i/2 - 4] correction
    //====================================================================//
        asm volatile (
            "subc.u32 %0, 0, 0;": "=r"(carry)
        );

        carry = 0 - carry;

    //====================================================================//
        asm volatile (
            "mad.lo.cc.u32 %0, %1, "q0_s", %0;": "+r"(r[i - 8]): "r"(carry)
        );

        asm volatile (
            "madc.lo.cc.u32 %0, %1, "q1_s", %0;": "+r"(r[i - 7]): "r"(carry)
        );

        asm volatile (
            "madc.lo.cc.u32 %0, %1, "q2_s", %0;": "+r"(r[i - 6]): "r"(carry)
        );

        asm volatile (
            "madc.lo.cc.u32 %0, %1, "q3_s", %0;": "+r"(r[i - 5]): "r"(carry)
        );

    //====================================================================//
#pragma unroll
        for (int j = 0; j < 3; ++j)
        {
            asm volatile (
                "addc.cc.u32 %0, %0, 0;": "+r"(r[i + j - 4])
            );
        }

        asm volatile (
            "addc.u32 %0, %0, 0;": "+r"(r[i - 1])
        );
    }

    //===================================================================//
    //  Dump result to global memory
    //===================================================================//
#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            hash[(tid << 3) + i] = r[i];
        }

    return;
}

////////////////////////////////////////////////////////////////////////////////
//  Unfinalized hash of message
////////////////////////////////////////////////////////////////////////////////
void initHash(
    // context
    blake2b_ctx * ctx,
    // optional secret key
    const void * key,
    // message
    const void * mes,
    // message length in bytes
    uint32_t meslen
) {
    const uint64_t blake2b_iv[8] = {
        0x6A09E667F3BCC908, 0xBB67AE8584CAA73B,
        0x3C6EF372FE94F82B, 0xA54FF53A5F1D36F1,
        0x510E527FADE682D1, 0x9B05688C2B3E6C1F,
        0x1F83D9ABFB41BD6B, 0x5BE0CD19137E2179
    };

    const uint8_t sigma[192] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3,
        11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4,
        7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8,
        9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13,
        2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9,
        12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11,
        13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10,
        6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5,
        10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3
    };

    int i;
    int j;

    uint64_t v[16];
    uint64_t m[16];

    //====================================================================//
    //  Initialize context
    //====================================================================//
    for (j = 0; j < 8; ++j)
    {
        ctx->h[j] = blake2b_iv[j];
    }

    ctx->h[0] ^= 0x01010000 ^ (0 << 8) ^ NUM_BYTE_SIZE;

    ctx->t[0] = 0;
    ctx->t[1] = 0;
    ctx->c = 0;

    for (j = 0; j < 128; ++j)
    {
        ctx->b[j] = 0;
    }

    //====================================================================//
    //  Hash message
    //====================================================================//
    for (j = 0; j < meslen; ++j)
    {
        if (ctx->c == 128)
        {
            ctx->t[0] += ctx->c;
            ctx->t[1] += (ctx->t[0] < ctx->c)? 1: 0;

            for (i = 0; i < 8; ++i)
            {
                v[i] = ctx->h[i];
                v[i + 8] = blake2b_iv[i];
            }

            v[12] ^= ctx->t[0];
            v[13] ^= ctx->t[1];

            for (i = 0; i < 16; i++)
            {
                m[i] = B2B_GET64(&ctx->b[8 * i]);
            }

            for (i = 0; i < 192; i += 16)
            {
                B2B_G(0, 4,  8, 12, m[sigma[i +  0]], m[sigma[i +  1]]);
                B2B_G(1, 5,  9, 13, m[sigma[i +  2]], m[sigma[i +  3]]);
                B2B_G(2, 6, 10, 14, m[sigma[i +  4]], m[sigma[i +  5]]);
                B2B_G(3, 7, 11, 15, m[sigma[i +  6]], m[sigma[i +  7]]);
                B2B_G(0, 5, 10, 15, m[sigma[i +  8]], m[sigma[i +  9]]);
                B2B_G(1, 6, 11, 12, m[sigma[i + 10]], m[sigma[i + 11]]);
                B2B_G(2, 7,  8, 13, m[sigma[i + 12]], m[sigma[i + 13]]);
                B2B_G(3, 4,  9, 14, m[sigma[i + 14]], m[sigma[i + 15]]);
            }

            for (i = 0; i < 8; ++i)
            {
                ctx->h[i] ^= v[i] ^ v[i + 8];
            }

            ctx->c = 0;
        }

        ctx->b[ctx->c++] = ((const uint8_t *)mes)[j];
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////
//  Block mining                                                               
////////////////////////////////////////////////////////////////////////////////
__global__ void blockMining(
    // hash constants & secret key
    const uint32_t * data,
    // pregenerated nonces
    const uint32_t * non,
    // precalculated hashes
    const uint32_t * hash,
    // results
    uint32_t * res
) {
    uint32_t j;
    uint32_t tid = threadIdx.x;

    // shared memory
    // 8 * B_DIM bytes  
    __shared__ uint32_t shared[2 * B_DIM];

    shared[2 * tid] = data[2 * tid];
    shared[2 * tid + 1] = data[2 * tid + 1];
    __syncthreads();

    // 8 * 64 bits = 64 bytes
    uint64_t * blake2b_iv = (uint64_t *)shared;
    // 192 * 8 bits = 192 bytes
    uint8_t * sigma = (uint8_t *)(shared + 16);
    // 8 * 32 bits = 32 bytes
    uint32_t * sk = shared + 64;

    // local memory
    // 936 bytes
    uint32_t local[118];

    // 128 bytes 
    uint64_t * v = (uint64_t *)local;
    // 128 bytes 
    uint64_t * m = v + 16;
    // (4 * K_LEN) bytes
    uint32_t * ind = local;
    // (NUM_BYTE_SIZE + 4) bytes
    uint8_t * h = (uint8_t *)(ind + K_LEN);
    // 212 bytes 
    blake2b_ctx * ctx = (blake2b_ctx *)(local + 8);

#pragma unroll
    for (int l = 0; l < H_LEN; ++l) 
    {
        ctx = (blake2b_ctx *)(shared + 64 + (NUM_BYTE_SIZE >> 2));

        tid = threadIdx.x + blockDim.x * blockIdx.x
            + l * gridDim.x * blockDim.x;

        const uint8_t * mes = (const uint8_t *)(non + (tid << 3));

    //====================================================================//
    //  Hash nonce
    //====================================================================//
        for (j = 0; ctx->c < 128 && j < NUM_BYTE_SIZE; ++j)
        {
            ctx->b[ctx->c++] = mes[j];
        }

        while (j < NUM_BYTE_SIZE)
        {
            ctx->t[0] += ctx->c;
            ctx->t[1] += 1 - !(ctx->t[0] < ctx->c);

#pragma unroll
            for (int i = 0; i < 8; ++i)
            {
                v[i] = ctx->h[i];
                v[i + 8] = blake2b_iv[i];
            }

            v[12] ^= ctx->t[0];
            v[13] ^= ctx->t[1];

#pragma unroll
            for (int i = 0; i < 16; i++)
            {
                m[i] = B2B_GET64(&ctx->b[8 * i]);
            }

#pragma unroll
            for (int i = 0; i < 192; i += 16)
            {
                B2B_G(0, 4,  8, 12, m[sigma[i +  0]], m[sigma[i +  1]]);
                B2B_G(1, 5,  9, 13, m[sigma[i +  2]], m[sigma[i +  3]]);
                B2B_G(2, 6, 10, 14, m[sigma[i +  4]], m[sigma[i +  5]]);
                B2B_G(3, 7, 11, 15, m[sigma[i +  6]], m[sigma[i +  7]]);
                B2B_G(0, 5, 10, 15, m[sigma[i +  8]], m[sigma[i +  9]]);
                B2B_G(1, 6, 11, 12, m[sigma[i + 10]], m[sigma[i + 11]]);
                B2B_G(2, 7,  8, 13, m[sigma[i + 12]], m[sigma[i + 13]]);
                B2B_G(3, 4,  9, 14, m[sigma[i + 14]], m[sigma[i + 15]]);
            }

#pragma unroll
            for (int i = 0; i < 8; ++i)
            {
                ctx->h[i] ^= v[i] ^ v[i + 8];
            }

            ctx->c = 0;
           
            while (ctx->c < 128 && j < NUM_BYTE_SIZE)
            {
                ctx->b[ctx->c++] = mes[j++];
            }
        }

    //====================================================================//
    //  Finalize hash
    //====================================================================//
        ctx->t[0] += ctx->c;
        ctx->t[1] += 1 - !(ctx->t[0] < ctx->c);

        while (ctx->c < 128)
        {
            ctx->b[ctx->c++] = 0;
        }

#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            v[i] = ctx->h[i];
            v[i + 8] = blake2b_iv[i];
        }

        v[12] ^= ctx->t[0];
        v[13] ^= ctx->t[1];
        v[14] = ~v[14];

#pragma unroll
        for (int i = 0; i < 16; i++)
        {
            m[i] = B2B_GET64(&ctx->b[8 * i]);
        }

#pragma unroll
        for (int i = 0; i < 192; i += 16)
        {
            B2B_G(0, 4,  8, 12, m[sigma[i +  0]], m[sigma[i +  1]]);
            B2B_G(1, 5,  9, 13, m[sigma[i +  2]], m[sigma[i +  3]]);
            B2B_G(2, 6, 10, 14, m[sigma[i +  4]], m[sigma[i +  5]]);
            B2B_G(3, 7, 11, 15, m[sigma[i +  6]], m[sigma[i +  7]]);
            B2B_G(0, 5, 10, 15, m[sigma[i +  8]], m[sigma[i +  9]]);
            B2B_G(1, 6, 11, 12, m[sigma[i + 10]], m[sigma[i + 11]]);
            B2B_G(2, 7,  8, 13, m[sigma[i + 12]], m[sigma[i + 13]]);
            B2B_G(3, 4,  9, 14, m[sigma[i + 14]], m[sigma[i + 15]]);
        }

#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            ctx->h[i] ^= v[i] ^ v[i + 8];
        }

        for (j = 0; j < NUM_BYTE_SIZE; ++j)
        {
            h[j] = (ctx->h[j >> 3] >> ((j & 7) << 3)) & 0xFF;
        }

    //===================================================================//
    //  Generate indices
    //===================================================================//
#pragma unroll
        for (int i = 0; i < 3; ++i)
        {
            h[NUM_BYTE_SIZE + i] = h[i];
        }

#pragma unroll
        for (int i = 0; i < K_LEN; ++i)
        {
            ind[i] = *((uint32_t *)(h + i)) & N_MASK;
        }
        
    //===================================================================//
    //  Calculate result
    //===================================================================//
        uint32_t * p = hashes;
        // 36 bytes
        uint32_t * r = (uint32_t *)h;

        // first addition of hashes -> r
        asm volatile (
            "add.cc.u32 %0, %1, %2;":
                "=r"(r[0]): "r"(p[(ind[0] << 3)]), "r"(p[(ind[1] << 3)])
        );

#pragma unroll
        for (int i = 1; i < 8; ++i)
        {
            asm volatile (
                "addc.cc.u32 %0, %1, %2;":
                "=r"(r[i]): "r"(p[(ind[0] << 3) + i]), "r"(p[(ind[1] << 3) + i])
            );
        }

        asm volatile (
            "addc.u32 %0, 0, 0;": "=r"(r[8])
        );

        // remaining additions
#pragma unroll
        for (int k = 2; k < K_LEN; ++k)
        {
            asm volatile (
                "add.cc.u32 %0, %0, %1;": "+r"(r[0]): "r"(p[ind[k] << 3])
            );

#pragma unroll
            for (int i = 1; i < 8; ++i)
            {
                asm volatile (
                    "addc.cc.u32 %0, %0, %1;":
                    "+r"(r[i]): "r"(p[(ind[k] << 3) + i])
                );
            }

            asm volatile (
                "addc.u32 %0, %0, 0;": "+r"(r[8])
            );
        }

        // subtraction of secret sk
        asm volatile (
            "sub.cc.u32 %0, %0, %1;": "+r"(r[0]): "r"(sk[0])
        );

#pragma unroll
        for (int i = 1; i < 8; ++i)
        {
            asm volatile (
                "subc.cc.u32 %0, %0, %1;": "+r"(r[i]): "r"(sk[i])
            );
        }

        asm volatile (
            "subc.u32 %0, %0, 0;": "+r"(r[8])
        );


    //===================================================================//
    //  Result mod q
    //===================================================================//
        // 20 bytes
        uint32_t * med = ind;
        // 4 bytes
        uint32_t * d = ind + 5; 

        *d = (r[8] << 4) | (r[7] >> 28);
        r[7] &= 0x0FFFFFFF;

    //====================================================================//
        asm volatile (
            "mul.lo.u32 %0, %1, "q0_s";": "=r"(med[0]): "r"(*d)
        );
        asm volatile (
            "mul.hi.u32 %0, %1, "q0_s";": "=r"(med[1]): "r"(*d)
        );
        asm volatile (
            "mul.lo.u32 %0, %1, "q2_s";": "=r"(med[2]): "r"(*d)
        );
        asm volatile (
            "mul.hi.u32 %0, %1, "q2_s";": "=r"(med[3]): "r"(*d)
        );

        asm volatile (
            "mad.lo.cc.u32 %0, %1, "q1_s", %0;": "+r"(med[1]): "r"(*d)
        );
        asm volatile (
            "madc.hi.cc.u32 %0, %1, "q1_s", %0;": "+r"(med[2]): "r"(*d)
        );
        asm volatile (
            "madc.lo.cc.u32 %0, %1, "q3_s", %0;": "+r"(med[3]): "r"(*d)
        );
        asm volatile (
            "madc.hi.u32 %0, %1, "q3_s", 0;": "=r"(med[4]): "r"(*d)
        );

    //====================================================================//
        asm volatile (
            "sub.cc.u32 %0, %0, %1;": "+r"(r[0]): "r"(med[0])
        );

#pragma unroll
        for (int i = 1; i < 5; ++i)
        {
            asm volatile (
                "subc.cc.u32 %0, %0, %1;": "+r"(r[i]): "r"(med[i])
            );
        }

#pragma unroll
        for (int i = 5; i < 8; ++i)
        {
            asm volatile (
                "subc.cc.u32 %0, %0, 0;": "+r"(r[i])
            );
        }

    //====================================================================//
        uint32_t * carry = ind + 6;

        asm volatile (
            "subc.u32 %0, 0, 0;": "=r"(*carry)
        );

        *carry = 0 - *carry;

        asm volatile (
            "mad.lo.cc.u32 %0, %1, "q0_s", %0;": "+r"(r[0]): "r"(*carry)
        );

        asm volatile (
            "madc.lo.cc.u32 %0, %1, "q1_s", %0;": "+r"(r[1]): "r"(*carry)
        );

        asm volatile (
            "madc.lo.cc.u32 %0, %1, "q2_s", %0;": "+r"(r[2]): "r"(*carry)
        );

        asm volatile (
            "madc.lo.cc.u32 %0, %1, "q3_s", %0;": "+r"(r[3]): "r"(*carry)
        );

#pragma unroll
        for (int i = 0; i < 3; ++i)
        {
            asm volatile (
                "addc.cc.u32 %0, %0, 0;": "+r"(r[i + 4])
            );
        }

        asm volatile (
            "addc.u32 %0, %0, 0;": "+r"(r[7])
        );

    //===================================================================//
    //  Dump result to global memory
    //===================================================================//
#pragma unroll
        for (int i = 0; i < 8; ++i)
        {
            res[(tid << 3) + i] = r[i];
        }
    }

    return;
}

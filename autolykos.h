#ifndef AUTOLYKOS_H
#define AUTOLYKOS_H

#include <stdint.h>
#include <stddef.h>

// Parameters
#define KEY_LEN 32       // in bytes
#define NON_LEN 32       // in bytes
#define HASH_LEN 32      // in bytes
#define K_SIZE 32
#define B_SIZE 10
#define N_SIZE 0x4000000 // 2^26
#define H_SIZE 4         // hashes per thread
#define L_SIZE 0x3D090   // H_SIZE * 10^6
#define GDIM 15625
#define BDIM 64          // GDIM * BDIM = 10^6

// 64 bits
#define Q1 0x14DEF9DEA2F79CD6
#define Q0 0x5812631A5CF5D3ED

// state context
typedef struct {
    // input buffer
    uint8_t b[128];
    // chained state
    uint64_t h[8];
    // total number of bytes
    uint64_t t[2];
    // counter for b
    uint32_t c;
} blake2b_ctx;

void partialHash(
    // context
    blake2b_ctx * ctx,
    // optional secret key
    const void * key,
    // message
    const void * mes,
    uint32_t meslen
);

__global__ void blockMining(
    // context
    blake2b_ctx * ctx,
    // pregenerated nonces
    const void * non,
    // results
    uint32_t * res
);

__global__ void hash(
    // optional secret key
    const void * key,
    uint32_t keylen,
    // message
    const void * mes,
    uint32_t meslen,
    // pregenerated nonces
    const void * non,
    uint32_t nonlen,
    // hashes
    void * out,
    uint32_t outlen,
    // results
    uint32_t * res
);

#endif // AUTOLYKOS_H

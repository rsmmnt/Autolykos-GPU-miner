#ifndef DEFINITIONS_H
#define DEFINITIONS_H

/*******************************************************************************

    DEFINITIONS -- Constants, Structs and Macros

*******************************************************************************/

#include "jsmn.h" 
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <atomic>
#include <mutex>

////////////////////////////////////////////////////////////////////////////////
//  PARAMETERS: Autolykos algorithm
////////////////////////////////////////////////////////////////////////////////
// constant message size
#define CONST_MES_SIZE_8   8192 // 2^10

// prehash continue position 
#define CONTINUE_POS       36

// number of indices
#define K_LEN              32

// number of precalculated hashes
#define N_LEN              0x4000000 // 2^26

// mod 2^26 mask
#define N_MASK             (N_LEN - 1)

////////////////////////////////////////////////////////////////////////////////
//  PARAMETERS: Heuristic prehash CUDA kernel parameters
////////////////////////////////////////////////////////////////////////////////
// number of hashes per thread
#define THREAD_LEN         1

// total number of hash loads (threads) per iteration
#define LOAD_LEN           (0x400000 / THREAD_LEN) // 2^22

////////////////////////////////////////////////////////////////////////////////
//  CONSTANTS: Autolykos algorithm
////////////////////////////////////////////////////////////////////////////////
// secret keys and hashes size
#define NUM_SIZE_8         32
#define NUM_SIZE_4         (NUM_SIZE_8 << 1)
#define NUM_SIZE_32        (NUM_SIZE_8 >> 2)
#define NUM_SIZE_64        (NUM_SIZE_8 >> 3)
#define NUM_SIZE_32_BLOCK  (1 + (NUM_SIZE_32 - 1) / BLOCK_DIM)
#define NUM_SIZE_8_BLOCK   (NUM_SIZE_32_BLOCK << 2)
#define ROUND_NUM_SIZE_32  (NUM_SIZE_32_BLOCK * BLOCK_DIM)

// public keys size
#define PK_SIZE_8          33
#define PK_SIZE_4          (PK_SIZE_8 << 1)
#define PK_SIZE_32_BLOCK   (1 + NUM_SIZE_32 / BLOCK_DIM)
#define PK_SIZE_8_BLOCK    (PK_SIZE_32_BLOCK << 2)
#define ROUND_PK_SIZE_32   (PK_SIZE_32_BLOCK * BLOCK_DIM)
#define COUPLED_PK_SIZE_32 (((PK_SIZE_8 << 1) + 3) >> 2)

// nonce size
#define NONCE_SIZE_8       8
#define NONCE_SIZE_4       (NONCE_SIZE_8 << 1)
#define NONCE_SIZE_32      (NONCE_SIZE_8 >> 2)

// index size
#define INDEX_SIZE_8       4

// BLAKE2b-256 hash buffer size
#define BUF_SIZE_8         128

struct ctx_t;

// puzzle data size
#define DATA_SIZE_8                                                            \
(                                                                              \
    (1 + (2 * PK_SIZE_8 + 2 + 3 * NUM_SIZE_8 + sizeof(ctx_t) - 1) / BLOCK_DIM) \
    * BLOCK_DIM                                                                \
)

// mes || w sizes
#define NP_SIZE_32_BLOCK   (1 + (NUM_SIZE_32 << 1) / BLOCK_DIM)
#define NP_SIZE_8_BLOCK    (NP_SIZE_32_BLOCK << 2)
#define ROUND_NP_SIZE_32   (NP_SIZE_32_BLOCK * BLOCK_DIM)

// pk || mew || w sizes
#define PNP_SIZE_32_BLOCK                                                      \
(1 + (COUPLED_PK_SIZE_32 + NUM_SIZE_32 - 1) / BLOCK_DIM)

#define PNP_SIZE_8_BLOCK   (PNP_SIZE_32_BLOCK << 2)
#define ROUND_PNP_SIZE_32  (PNP_SIZE_32_BLOCK * BLOCK_DIM)

// x || ctx sizes
#define NC_SIZE_32_BLOCK                                                       \
(1 + (NUM_SIZE_32 + sizeof(ctx_t) - 1) / BLOCK_DIM)

#define NC_SIZE_8_BLOCK    (NC_SIZE_32_BLOCK << 2)
#define ROUND_NC_SIZE_32   (NC_SIZE_32_BLOCK * BLOCK_DIM)

////////////////////////////////////////////////////////////////////////////////
//  CONSTANTS: Q definition 64-bits and 32-bits words
////////////////////////////////////////////////////////////////////////////////
// Q definition for CUDA ptx pseudo-assembler commands
// 32 bits
#define qhi_s              "0xFFFFFFFF"
#define q4_s               "0xFFFFFFFE"
#define q3_s               "0xBAAEDCE6"
#define q2_s               "0xAF48A03B"
#define q1_s               "0xBFD25E8C"
#define q0_s               "0xD0364141"

// Autolykos valid range
// Q itself is multiplier-of-Q floor of 2^256
// 64 bits
#define Q3                 0xFFFFFFFFFFFFFFFF
#define Q2                 0xFFFFFFFFFFFFFFFE
#define Q1                 0xBAAEDCE6AF48A03B
#define Q0                 0xBFD25E8CD0364141

////////////////////////////////////////////////////////////////////////////////
//  CONSTANTS: Curl http & JSMN specifiers
////////////////////////////////////////////////////////////////////////////////

// CURL number of solution POST retries if failed

#define MAX_POST_RETRIES 5

// URL max size 
#define MAX_URL_SIZE       1024

// default request capacity
#define JSON_CAPACITY      256

// maximal request capacity
#define MAX_JSON_CAPACITY  8192

// total JSON objects count
#define REQ_LEN            7

// curl JSON position of message
#define MES_POS            2

// curl JSON position of bound
#define BOUND_POS          4

// curl JSON position of public key
#define PK_POS             6

// total JSON objects count for config file
#define CONF_LEN           7

// config JSON position of secret key
#define SEED_POS           2

// config JSON position of latest block adress
#define NODE_POS           4

// config JSON position of keep prehash option
#define KEEP_POS           6

////////////////////////////////////////////////////////////////////////////////
//  Error messages
////////////////////////////////////////////////////////////////////////////////
#define ERROR_STAT         "stat"
#define ERROR_ALLOC        "Host memory allocation"
#define ERROR_IO           "I/O"
#define ERROR_CURL         "Curl"
#define ERROR_OPENSSL      "OpenSSL"

////////////////////////////////////////////////////////////////////////////////
//  Structs
////////////////////////////////////////////////////////////////////////////////
typedef unsigned int uint_t;

// autolukos puzzle state
typedef enum
{
    STATE_CONTINUE = 0,
    STATE_KEYGEN = 1,
    STATE_REHASH = 2,
    STATE_INTERRUPT = 3
}
state_t;

// puzzle global info
struct info_t
{
    // Mutex for reading/writing data from info_t safely
    std::mutex info_mutex;

    // Mutex for curl usage/maybe future websocket
    // not used now
    // std::mutex io_mutex;

    // Puzzle data to read
    uint8_t bound_h[NUM_SIZE_8];
    uint8_t mes_h[NUM_SIZE_8];
    uint8_t sk_h[NUM_SIZE_8];
    uint8_t pk_h[PK_SIZE_8];
    char skstr[NUM_SIZE_4];
    char pkstr[PK_SIZE_4 + 1];
    int keepPrehash;
    char to[MAX_URL_SIZE];

    // Increment when new block is sent by node
    std::atomic<uint_t> blockId; 
};

// json string for curl http requests and config 
struct json_t
{
    size_t cap;
    size_t len;
    char * ptr;
    jsmntok_t * toks;

    json_t(const int strlen, const int toklen);
    json_t(const json_t & newjson);
    ~json_t(void);

    // reset len to zero
    void Reset(void) { len = 0; return; }

    // tokens access methods
    int GetTokenStartPos(const int pos) { return toks[pos].start; }
    int GetTokenEndPos(const int pos) { return toks[pos].end; }
    int GetTokenLen(const int pos) { return toks[pos].end - toks[pos].start; }

    char * GetTokenStart(const int pos) { return ptr + toks[pos].start; }
    char * GetTokenEnd(const int pos) { return ptr + toks[pos].end; }
};

// BLAKE2b-256 hash state context
struct ctx_t
{
    // input buffer
    uint8_t b[BUF_SIZE_8];
    // chained state
    uint64_t h[8];
    // total number of bytes
    uint64_t t[2];
    // counter for b
    uint32_t c;
};

// BLAKE2b-256 packed uncomplete hash state context 
struct uctx_t
{
    // chained state
    uint64_t h[8];
    // total number of bytes
    uint64_t t[2];
};

////////////////////////////////////////////////////////////////////////////////
//  BLAKE2b-256 hashing procedures macros
////////////////////////////////////////////////////////////////////////////////
// initialization vector
#define B2B_IV(v)                                                              \
do                                                                             \
{                                                                              \
    ((uint64_t *)(v))[0] = 0x6A09E667F3BCC908;                                 \
    ((uint64_t *)(v))[1] = 0xBB67AE8584CAA73B;                                 \
    ((uint64_t *)(v))[2] = 0x3C6EF372FE94F82B;                                 \
    ((uint64_t *)(v))[3] = 0xA54FF53A5F1D36F1;                                 \
    ((uint64_t *)(v))[4] = 0x510E527FADE682D1;                                 \
    ((uint64_t *)(v))[5] = 0x9B05688C2B3E6C1F;                                 \
    ((uint64_t *)(v))[6] = 0x1F83D9ABFB41BD6B;                                 \
    ((uint64_t *)(v))[7] = 0x5BE0CD19137E2179;                                 \
}                                                                              \
while (0)

// cyclic right rotation
#define ROTR64(x, y) (((x) >> (y)) ^ ((x) << (64 - (y))))

// G mixing function
#define B2B_G(v, a, b, c, d, x, y)                                             \
do                                                                             \
{                                                                              \
    ((uint64_t *)(v))[a] += ((uint64_t *)(v))[b] + x;                          \
    ((uint64_t *)(v))[d]                                                       \
        = ROTR64(((uint64_t *)(v))[d] ^ ((uint64_t *)(v))[a], 32);             \
    ((uint64_t *)(v))[c] += ((uint64_t *)(v))[d];                              \
    ((uint64_t *)(v))[b]                                                       \
        = ROTR64(((uint64_t *)(v))[b] ^ ((uint64_t *)(v))[c], 24);             \
    ((uint64_t *)(v))[a] += ((uint64_t *)(v))[b] + y;                          \
    ((uint64_t *)(v))[d]                                                       \
        = ROTR64(((uint64_t *)(v))[d] ^ ((uint64_t *)(v))[a], 16);             \
    ((uint64_t *)(v))[c] += ((uint64_t *)(v))[d];                              \
    ((uint64_t *)(v))[b]                                                       \
        = ROTR64(((uint64_t *)(v))[b] ^ ((uint64_t *)(v))[c], 63);             \
}                                                                              \
while (0)

// mixing rounds
#define B2B_MIX(v, m)                                                          \
do                                                                             \
{                                                                              \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[ 0], ((uint64_t *)(m))[ 1]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[ 2], ((uint64_t *)(m))[ 3]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[ 4], ((uint64_t *)(m))[ 5]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[ 6], ((uint64_t *)(m))[ 7]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[ 8], ((uint64_t *)(m))[ 9]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[10], ((uint64_t *)(m))[11]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[12], ((uint64_t *)(m))[13]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[14], ((uint64_t *)(m))[15]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[14], ((uint64_t *)(m))[10]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[ 4], ((uint64_t *)(m))[ 8]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[ 9], ((uint64_t *)(m))[15]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[13], ((uint64_t *)(m))[ 6]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[ 1], ((uint64_t *)(m))[12]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[ 0], ((uint64_t *)(m))[ 2]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[11], ((uint64_t *)(m))[ 7]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[ 5], ((uint64_t *)(m))[ 3]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[11], ((uint64_t *)(m))[ 8]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[12], ((uint64_t *)(m))[ 0]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[ 5], ((uint64_t *)(m))[ 2]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[15], ((uint64_t *)(m))[13]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[10], ((uint64_t *)(m))[14]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[ 3], ((uint64_t *)(m))[ 6]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[ 7], ((uint64_t *)(m))[ 1]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[ 9], ((uint64_t *)(m))[ 4]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[ 7], ((uint64_t *)(m))[ 9]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[ 3], ((uint64_t *)(m))[ 1]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[13], ((uint64_t *)(m))[12]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[11], ((uint64_t *)(m))[14]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[ 2], ((uint64_t *)(m))[ 6]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[ 5], ((uint64_t *)(m))[10]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[ 4], ((uint64_t *)(m))[ 0]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[15], ((uint64_t *)(m))[ 8]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[ 9], ((uint64_t *)(m))[ 0]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[ 5], ((uint64_t *)(m))[ 7]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[ 2], ((uint64_t *)(m))[ 4]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[10], ((uint64_t *)(m))[15]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[14], ((uint64_t *)(m))[ 1]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[11], ((uint64_t *)(m))[12]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[ 6], ((uint64_t *)(m))[ 8]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[ 3], ((uint64_t *)(m))[13]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[ 2], ((uint64_t *)(m))[12]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[ 6], ((uint64_t *)(m))[10]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[ 0], ((uint64_t *)(m))[11]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[ 8], ((uint64_t *)(m))[ 3]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[ 4], ((uint64_t *)(m))[13]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[ 7], ((uint64_t *)(m))[ 5]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[15], ((uint64_t *)(m))[14]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[ 1], ((uint64_t *)(m))[ 9]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[12], ((uint64_t *)(m))[ 5]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[ 1], ((uint64_t *)(m))[15]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[14], ((uint64_t *)(m))[13]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[ 4], ((uint64_t *)(m))[10]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[ 0], ((uint64_t *)(m))[ 7]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[ 6], ((uint64_t *)(m))[ 3]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[ 9], ((uint64_t *)(m))[ 2]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[ 8], ((uint64_t *)(m))[11]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[13], ((uint64_t *)(m))[11]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[ 7], ((uint64_t *)(m))[14]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[12], ((uint64_t *)(m))[ 1]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[ 3], ((uint64_t *)(m))[ 9]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[ 5], ((uint64_t *)(m))[ 0]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[15], ((uint64_t *)(m))[ 4]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[ 8], ((uint64_t *)(m))[ 6]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[ 2], ((uint64_t *)(m))[10]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[ 6], ((uint64_t *)(m))[15]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[14], ((uint64_t *)(m))[ 9]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[11], ((uint64_t *)(m))[ 3]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[ 0], ((uint64_t *)(m))[ 8]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[12], ((uint64_t *)(m))[ 2]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[13], ((uint64_t *)(m))[ 7]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[ 1], ((uint64_t *)(m))[ 4]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[10], ((uint64_t *)(m))[ 5]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[10], ((uint64_t *)(m))[ 2]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[ 8], ((uint64_t *)(m))[ 4]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[ 7], ((uint64_t *)(m))[ 6]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[ 1], ((uint64_t *)(m))[ 5]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[15], ((uint64_t *)(m))[11]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[ 9], ((uint64_t *)(m))[14]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[ 3], ((uint64_t *)(m))[12]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[13], ((uint64_t *)(m))[ 0]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[ 0], ((uint64_t *)(m))[ 1]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[ 2], ((uint64_t *)(m))[ 3]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[ 4], ((uint64_t *)(m))[ 5]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[ 6], ((uint64_t *)(m))[ 7]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[ 8], ((uint64_t *)(m))[ 9]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[10], ((uint64_t *)(m))[11]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[12], ((uint64_t *)(m))[13]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[14], ((uint64_t *)(m))[15]);      \
                                                                               \
    B2B_G(v, 0, 4,  8, 12, ((uint64_t *)(m))[14], ((uint64_t *)(m))[10]);      \
    B2B_G(v, 1, 5,  9, 13, ((uint64_t *)(m))[ 4], ((uint64_t *)(m))[ 8]);      \
    B2B_G(v, 2, 6, 10, 14, ((uint64_t *)(m))[ 9], ((uint64_t *)(m))[15]);      \
    B2B_G(v, 3, 7, 11, 15, ((uint64_t *)(m))[13], ((uint64_t *)(m))[ 6]);      \
    B2B_G(v, 0, 5, 10, 15, ((uint64_t *)(m))[ 1], ((uint64_t *)(m))[12]);      \
    B2B_G(v, 1, 6, 11, 12, ((uint64_t *)(m))[ 0], ((uint64_t *)(m))[ 2]);      \
    B2B_G(v, 2, 7,  8, 13, ((uint64_t *)(m))[11], ((uint64_t *)(m))[ 7]);      \
    B2B_G(v, 3, 4,  9, 14, ((uint64_t *)(m))[ 5], ((uint64_t *)(m))[ 3]);      \
}                                                                              \
while (0)

// blake2b initialization
#define B2B_INIT(ctx, aux)                                                     \
do                                                                             \
{                                                                              \
    ((uint64_t *)(aux))[0] = ((ctx_t *)(ctx))->h[0];                           \
    ((uint64_t *)(aux))[1] = ((ctx_t *)(ctx))->h[1];                           \
    ((uint64_t *)(aux))[2] = ((ctx_t *)(ctx))->h[2];                           \
    ((uint64_t *)(aux))[3] = ((ctx_t *)(ctx))->h[3];                           \
    ((uint64_t *)(aux))[4] = ((ctx_t *)(ctx))->h[4];                           \
    ((uint64_t *)(aux))[5] = ((ctx_t *)(ctx))->h[5];                           \
    ((uint64_t *)(aux))[6] = ((ctx_t *)(ctx))->h[6];                           \
    ((uint64_t *)(aux))[7] = ((ctx_t *)(ctx))->h[7];                           \
                                                                               \
    B2B_IV(aux + 8);                                                           \
                                                                               \
    ((uint64_t *)(aux))[12] ^= ((ctx_t *)(ctx))->t[0];                         \
    ((uint64_t *)(aux))[13] ^= ((ctx_t *)(ctx))->t[1];                         \
}                                                                              \
while (0)

// blake2b mixing
#define B2B_FINAL(ctx, aux)                                                    \
do                                                                             \
{                                                                              \
    ((uint64_t *)(aux))[16] = ((uint64_t *)(((ctx_t *)(ctx))->b))[ 0];         \
    ((uint64_t *)(aux))[17] = ((uint64_t *)(((ctx_t *)(ctx))->b))[ 1];         \
    ((uint64_t *)(aux))[18] = ((uint64_t *)(((ctx_t *)(ctx))->b))[ 2];         \
    ((uint64_t *)(aux))[19] = ((uint64_t *)(((ctx_t *)(ctx))->b))[ 3];         \
    ((uint64_t *)(aux))[20] = ((uint64_t *)(((ctx_t *)(ctx))->b))[ 4];         \
    ((uint64_t *)(aux))[21] = ((uint64_t *)(((ctx_t *)(ctx))->b))[ 5];         \
    ((uint64_t *)(aux))[22] = ((uint64_t *)(((ctx_t *)(ctx))->b))[ 6];         \
    ((uint64_t *)(aux))[23] = ((uint64_t *)(((ctx_t *)(ctx))->b))[ 7];         \
    ((uint64_t *)(aux))[24] = ((uint64_t *)(((ctx_t *)(ctx))->b))[ 8];         \
    ((uint64_t *)(aux))[25] = ((uint64_t *)(((ctx_t *)(ctx))->b))[ 9];         \
    ((uint64_t *)(aux))[26] = ((uint64_t *)(((ctx_t *)(ctx))->b))[10];         \
    ((uint64_t *)(aux))[27] = ((uint64_t *)(((ctx_t *)(ctx))->b))[11];         \
    ((uint64_t *)(aux))[28] = ((uint64_t *)(((ctx_t *)(ctx))->b))[12];         \
    ((uint64_t *)(aux))[29] = ((uint64_t *)(((ctx_t *)(ctx))->b))[13];         \
    ((uint64_t *)(aux))[30] = ((uint64_t *)(((ctx_t *)(ctx))->b))[14];         \
    ((uint64_t *)(aux))[31] = ((uint64_t *)(((ctx_t *)(ctx))->b))[15];         \
                                                                               \
    B2B_MIX(aux, aux + 16);                                                    \
                                                                               \
    ((ctx_t *)(ctx))->h[0] ^= ((uint64_t *)(aux))[0] ^ ((uint64_t *)(aux))[ 8];\
    ((ctx_t *)(ctx))->h[1] ^= ((uint64_t *)(aux))[1] ^ ((uint64_t *)(aux))[ 9];\
    ((ctx_t *)(ctx))->h[2] ^= ((uint64_t *)(aux))[2] ^ ((uint64_t *)(aux))[10];\
    ((ctx_t *)(ctx))->h[3] ^= ((uint64_t *)(aux))[3] ^ ((uint64_t *)(aux))[11];\
    ((ctx_t *)(ctx))->h[4] ^= ((uint64_t *)(aux))[4] ^ ((uint64_t *)(aux))[12];\
    ((ctx_t *)(ctx))->h[5] ^= ((uint64_t *)(aux))[5] ^ ((uint64_t *)(aux))[13];\
    ((ctx_t *)(ctx))->h[6] ^= ((uint64_t *)(aux))[6] ^ ((uint64_t *)(aux))[14];\
    ((ctx_t *)(ctx))->h[7] ^= ((uint64_t *)(aux))[7] ^ ((uint64_t *)(aux))[15];\
}                                                                              \
while (0)

// blake2b intermediate mixing procedure on host
#define HOST_B2B_H(ctx, aux)                                                   \
do                                                                             \
{                                                                              \
    ((ctx_t *)(ctx))->t[0] += BUF_SIZE_8;                                      \
    ((ctx_t *)(ctx))->t[1] += 1 - !(((ctx_t *)(ctx))->t[0] < BUF_SIZE_8);      \
                                                                               \
    B2B_INIT(ctx, aux);                                                        \
    B2B_FINAL(ctx, aux);                                                       \
                                                                               \
    ((ctx_t *)(ctx))->c = 0;                                                   \
}                                                                              \
while (0)

// blake2b intermediate mixing procedure on host
#define HOST_B2B_H_LAST(ctx, aux)                                              \
do                                                                             \
{                                                                              \
    ((ctx_t *)(ctx))->t[0] += ((ctx_t *)(ctx))->c;                             \
    ((ctx_t *)(ctx))->t[1]                                                     \
        += 1 - !(((ctx_t *)(ctx))->t[0] < ((ctx_t *)(ctx))->c);                \
                                                                               \
    while (((ctx_t *)(ctx))->c < BUF_SIZE_8)                                   \
    {                                                                          \
        ((ctx_t *)(ctx))->b[((ctx_t *)(ctx))->c++] = 0;                        \
    }                                                                          \
                                                                               \
    B2B_INIT(ctx, aux);                                                        \
                                                                               \
    ((uint64_t *)(aux))[14] = ~((uint64_t *)(aux))[14];                        \
                                                                               \
    B2B_FINAL(ctx, aux);                                                       \
}                                                                              \
while (0)

// blake2b intermediate mixing procedure
#define DEVICE_B2B_H(ctx, aux)                                                 \
do                                                                             \
{                                                                              \
    asm volatile (                                                             \
        "add.cc.u32 %0, %0, 128;": "+r"(((uint32_t *)((ctx_t *)(ctx))->t)[0])  \
    );                                                                         \
    asm volatile (                                                             \
        "addc.cc.u32 %0, %0, 0;": "+r"(((uint32_t *)((ctx_t *)(ctx))->t)[1])   \
    );                                                                         \
    asm volatile (                                                             \
        "addc.cc.u32 %0, %0, 0;": "+r"(((uint32_t *)((ctx_t *)(ctx))->t)[2])   \
    );                                                                         \
    asm volatile (                                                             \
        "addc.u32 %0, %0, 0;": "+r"(((uint32_t *)((ctx_t *)(ctx))->t)[3])      \
    );                                                                         \
                                                                               \
    B2B_INIT(ctx, aux);                                                        \
    B2B_FINAL(ctx, aux);                                                       \
                                                                               \
    ((ctx_t *)(ctx))->c = 0;                                                   \
}                                                                              \
while (0)

// blake2b last mixing procedure
#define DEVICE_B2B_H_LAST(ctx, aux)                                            \
do                                                                             \
{                                                                              \
    asm volatile (                                                             \
        "add.cc.u32 %0, %0, %1;":                                              \
        "+r"(((uint32_t *)((ctx_t *)(ctx))->t)[0]):                            \
        "r"(((ctx_t *)(ctx))->c)                                               \
    );                                                                         \
    asm volatile (                                                             \
        "addc.cc.u32 %0, %0, 0;":                                              \
        "+r"(((uint32_t *)((ctx_t *)(ctx))->t)[1])                             \
    );                                                                         \
    asm volatile (                                                             \
        "addc.cc.u32 %0, %0, 0;":                                              \
        "+r"(((uint32_t *)((ctx_t *)(ctx))->t)[2])                             \
    );                                                                         \
    asm volatile (                                                             \
        "addc.u32 %0, %0, 0;":                                                 \
        "+r"(((uint32_t *)((ctx_t *)(ctx))->t)[3])                             \
    );                                                                         \
                                                                               \
    while (((ctx_t *)(ctx))->c < BUF_SIZE_8)                                   \
    {                                                                          \
        ((ctx_t *)(ctx))->b[((ctx_t *)(ctx))->c++] = 0;                        \
    }                                                                          \
                                                                               \
    B2B_INIT(ctx, aux);                                                        \
                                                                               \
    ((uint64_t *)(aux))[14] = ~((uint64_t *)(aux))[14];                        \
                                                                               \
    B2B_FINAL(ctx, aux);                                                       \
}                                                                              \
while (0)

////////////////////////////////////////////////////////////////////////////////
//  Little-Endian to Big-Endian convertation
////////////////////////////////////////////////////////////////////////////////
#define REVERSE_ENDIAN(p)                                                      \
    ((((uint64_t)((uint8_t *)(p))[0]) << 56) ^                                 \
    (((uint64_t)((uint8_t *)(p))[1]) << 48) ^                                  \
    (((uint64_t)((uint8_t *)(p))[2]) << 40) ^                                  \
    (((uint64_t)((uint8_t *)(p))[3]) << 32) ^                                  \
    (((uint64_t)((uint8_t *)(p))[4]) << 24) ^                                  \
    (((uint64_t)((uint8_t *)(p))[5]) << 16) ^                                  \
    (((uint64_t)((uint8_t *)(p))[6]) << 8) ^                                   \
    ((uint64_t)((uint8_t *)(p))[7]))

#define INPLACE_REVERSE_ENDIAN(p)                                              \
do                                                                             \
{                                                                              \
    *((uint64_t *)(p))                                                         \
    = ((((uint64_t)((uint8_t *)(p))[0]) << 56) ^                               \
    (((uint64_t)((uint8_t *)(p))[1]) << 48) ^                                  \
    (((uint64_t)((uint8_t *)(p))[2]) << 40) ^                                  \
    (((uint64_t)((uint8_t *)(p))[3]) << 32) ^                                  \
    (((uint64_t)((uint8_t *)(p))[4]) << 24) ^                                  \
    (((uint64_t)((uint8_t *)(p))[5]) << 16) ^                                  \
    (((uint64_t)((uint8_t *)(p))[6]) << 8) ^                                   \
    ((uint64_t)((uint8_t *)(p))[7]));                                          \
}                                                                              \
while (0)

////////////////////////////////////////////////////////////////////////////////
//  Wrappers for function calls
////////////////////////////////////////////////////////////////////////////////
#define FREE(x)                                                                \
do                                                                             \
{                                                                              \
    if (x)                                                                     \
    {                                                                          \
        free(x);                                                               \
        (x) = NULL;                                                            \
    }                                                                          \
}                                                                              \
while (0)

#define CUDA_CALL(x)                                                           \
do                                                                             \
{                                                                              \
    if ((x) != cudaSuccess)                                                    \
    {                                                                          \
        fprintf(stderr, "ERROR:  CUDA failed at %s: %d\n",__FILE__,__LINE__);  \
        fprintf(                                                               \
            stderr, "Miner is now terminated\n"                                \
            "========================================"                         \
            "========================================\n"                       \
        );                                                                     \
        exit(EXIT_FAILURE);                                                    \
    }                                                                          \
}                                                                              \
while (0)

#define CALL(func, name)                                                       \
do                                                                             \
{                                                                              \
    if (!(func))                                                               \
    {                                                                          \
        fprintf(stderr, "ERROR:  " name " failed at %s: %d\n",__FILE__,__LINE__);\
        exit(EXIT_FAILURE);                                                    \
    }                                                                          \
}                                                                              \
while (0)

#define FUNCTION_CALL(res, func, name)                                         \
do                                                                             \
{                                                                              \
    if (!((res) = (func)))                                                     \
    {                                                                          \
        fprintf(stderr, "ERROR:  " name " failed at %s: %d\n",__FILE__,__LINE__);\
        exit(EXIT_FAILURE);                                                    \
    }                                                                          \
}                                                                              \
while (0)

#define CALL_STATUS(func, name, status)                                        \
do                                                                             \
{                                                                              \
    if ((func) != (status))                                                    \
    {                                                                          \
        fprintf(stderr, "ERROR:  " name " failed at %s: %d\n",__FILE__,__LINE__);\
        exit(EXIT_FAILURE);                                                    \
    }                                                                          \
}                                                                              \
while (0)

#define FUNCTION_CALL_STATUS(res, func, name, status)                          \
do                                                                             \
{                                                                              \
    if ((res = func) != (status))                                              \
    {                                                                          \
        fprintf(stderr, "ERROR:  " name " failed at %s: %d\n",__FILE__,__LINE__);\
        exit(EXIT_FAILURE);                                                    \
    }                                                                          \
}                                                                              \
while (0)

#define PERSISTENT_CALL(func)                                                  \
do {} while (!(func))

#define PERSISTENT_FUNCTION_CALL(res, func)                                    \
do {} while (!((res) = (func)))

#define PERSISTENT_CALL_STATUS(func, status)                                   \
do {} while ((func) != (status))

#define PERSISTENT_FUNCTION_CALL_STATUS(func, status)                          \
do {} while (((res) = (func)) != (status))

#endif // DEFINITIONS_H

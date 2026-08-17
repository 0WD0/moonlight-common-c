#pragma once

#include <stdbool.h>

#ifdef USE_MBEDTLS
#include <mbedtls/cipher.h>
#else
// Hide the real OpenSSL definition from other code
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;
#endif

typedef struct _PLT_CRYPTO_CONTEXT {
#ifdef USE_MBEDTLS
    mbedtls_cipher_context_t ctx;
    bool initialized;
#else
    EVP_CIPHER_CTX* ctx;
    bool initialized;
#endif
} PLT_CRYPTO_CONTEXT, *PPLT_CRYPTO_CONTEXT;

#define ROUND_TO_PKCS7_PADDED_LEN(x) ((((x) + 15) / 16) * 16)

/**
 * @brief Creates a reusable symmetric cipher context.
 *
 * A context is bound to the first key and algorithm used with it. Callers must
 * create separate contexts for encryption and decryption.
 *
 * @return The allocated context, or `NULL` if allocation fails.
 */
PPLT_CRYPTO_CONTEXT PltCreateCryptoContext(void);

/**
 * @brief Destroys a symmetric cipher context.
 *
 * @param ctx The context to destroy. `NULL` is accepted.
 */
void PltDestroyCryptoContext(PPLT_CRYPTO_CONTEXT ctx);

#define ALGORITHM_AES_CBC 1
#define ALGORITHM_AES_GCM 2

#define CIPHER_FLAG_RESET_IV          0x01
#define CIPHER_FLAG_FINISH            0x02
#define CIPHER_FLAG_PAD_TO_BLOCK_SIZE 0x04

/**
 * @brief Encrypts a message with AES-128-CBC or AES-128-GCM.
 *
 * @param ctx The encryption context.
 * @param algorithm One of `ALGORITHM_AES_CBC` or `ALGORITHM_AES_GCM`.
 * @param flags A bitwise combination of `CIPHER_FLAG_*` values.
 * @param key The 16-byte AES key.
 * @param keyLength The key length, which must be 16.
 * @param iv The initialization vector.
 * @param ivLength The initialization vector length.
 * @param tag The 16-byte GCM tag output, or `NULL` for CBC.
 * @param tagLength The tag length, which must be 16 for GCM and 0 for CBC.
 * @param inputData The plaintext input.
 * @param inputDataLength The plaintext length.
 * @param outputData The ciphertext output.
 * @param outputDataLength Receives the ciphertext length. It is set to zero on
 * failure.
 * @return `true` on success, otherwise `false`.
 */
bool PltEncryptMessage(PPLT_CRYPTO_CONTEXT ctx, int algorithm, int flags,
                       unsigned char* key, int keyLength,
                       unsigned char* iv, int ivLength,
                       unsigned char* tag, int tagLength,
                       unsigned char* inputData, int inputDataLength,
                       unsigned char* outputData, int* outputDataLength);

/**
 * @brief Decrypts and authenticates an AES-128-CBC or AES-128-GCM message.
 *
 * GCM plaintext must not be consumed unless this function returns `true`.
 *
 * @param ctx The decryption context.
 * @param algorithm One of `ALGORITHM_AES_CBC` or `ALGORITHM_AES_GCM`.
 * @param flags A bitwise combination of `CIPHER_FLAG_*` values.
 * @param key The 16-byte AES key.
 * @param keyLength The key length, which must be 16.
 * @param iv The initialization vector.
 * @param ivLength The initialization vector length.
 * @param tag The 16-byte GCM authentication tag, or `NULL` for CBC.
 * @param tagLength The tag length, which must be 16 for GCM and 0 for CBC.
 * @param inputData The ciphertext input.
 * @param inputDataLength The ciphertext length.
 * @param outputData The plaintext output. Produced bytes are cleared when
 * authentication or finalization fails.
 * @param outputDataLength Receives the plaintext length. It is set to zero on
 * failure.
 * @return `true` on successful decryption and authentication, otherwise
 * `false`.
 */
bool PltDecryptMessage(PPLT_CRYPTO_CONTEXT ctx, int algorithm, int flags,
                       unsigned char* key, int keyLength,
                       unsigned char* iv, int ivLength,
                       unsigned char* tag, int tagLength,
                       unsigned char* inputData, int inputDataLength,
                       unsigned char* outputData, int* outputDataLength);

/**
 * @brief Fills a buffer with cryptographically secure random bytes.
 *
 * @param data The output buffer.
 * @param length The positive number of bytes to generate.
 * @return `true` when all bytes were generated, otherwise `false`.
 */
bool PltGenerateRandomData(unsigned char* data, int length);

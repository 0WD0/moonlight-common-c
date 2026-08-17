#include "Limelight-internal.h"

#ifdef USE_MBEDTLS
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/version.h>

mbedtls_entropy_context EntropyContext;
mbedtls_ctr_drbg_context CtrDrbgContext;
bool RandomStateInitialized = false;

#if MBEDTLS_VERSION_MAJOR > 2 || (MBEDTLS_VERSION_MAJOR == 2 && MBEDTLS_VERSION_MINOR >= 25)
#define USE_MBEDTLS_CRYPTO_EXT
#endif

#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

static int addPkcs7PaddingInPlace(unsigned char* plaintext, int plaintextLen) {
    int paddedLength = ROUND_TO_PKCS7_PADDED_LEN(plaintextLen);
    unsigned char paddingByte = (unsigned char)(16 - (plaintextLen % 16));

    memset(&plaintext[plaintextLen], paddingByte, paddedLength - plaintextLen);

    return paddedLength;
}

static void clearBuffer(unsigned char* data, int dataLength) {
    volatile unsigned char* output = data;
    int i;

    for (i = 0; i < dataLength; i++) {
        output[i] = 0;
    }
}

static void clearOutput(unsigned char* outputData, int outputDataLength,
                        int* reportedOutputDataLength) {
    clearBuffer(outputData, outputDataLength);

    if (reportedOutputDataLength != NULL) {
        *reportedOutputDataLength = 0;
    }
}

static bool validateCipherArguments(PPLT_CRYPTO_CONTEXT ctx, int algorithm, int flags,
                                    unsigned char* key, int keyLength,
                                    unsigned char* iv, int ivLength,
                                    unsigned char* tag, int tagLength,
                                    unsigned char* inputData, int inputDataLength,
                                    unsigned char* outputData, int* outputDataLength) {
    if (ctx == NULL || key == NULL || keyLength != 16 || iv == NULL ||
            (inputData == NULL && inputDataLength != 0) || inputDataLength < 0 ||
            outputData == NULL ||
            outputDataLength == NULL ||
            (flags & ~(CIPHER_FLAG_RESET_IV | CIPHER_FLAG_FINISH |
                       CIPHER_FLAG_PAD_TO_BLOCK_SIZE)) != 0) {
        return false;
    }

    if (algorithm == ALGORITHM_AES_CBC) {
        return ivLength == 16 && tag == NULL && tagLength == 0;
    }

    if (algorithm == ALGORITHM_AES_GCM) {
        return ivLength > 0 && ivLength <= 16 && tag != NULL && tagLength == 16 &&
                (flags & (CIPHER_FLAG_FINISH | CIPHER_FLAG_PAD_TO_BLOCK_SIZE)) == 0;
    }

    return false;
}

// When CIPHER_FLAG_PAD_TO_BLOCK_SIZE is used, inputData buffer must be allocated such that
// the buffer length is at least ROUND_TO_PKCS7_PADDED_LEN(inputDataLength) and inputData
// buffer may be modified!
// For GCM, the IV can change from message to message without CIPHER_FLAG_RESET_IV.
// CIPHER_FLAG_RESET_IV is only required for GCM when the IV length changes.
// Changing the key between encrypt/decrypt calls on a single context is not supported.
bool PltEncryptMessage(PPLT_CRYPTO_CONTEXT ctx, int algorithm, int flags,
                       unsigned char* key, int keyLength,
                       unsigned char* iv, int ivLength,
                       unsigned char* tag, int tagLength,
                       unsigned char* inputData, int inputDataLength,
                       unsigned char* outputData, int* outputDataLength) {
    if (!validateCipherArguments(ctx, algorithm, flags, key, keyLength, iv, ivLength,
                                 tag, tagLength, inputData, inputDataLength,
                                 outputData, outputDataLength)) {
        if (outputDataLength != NULL) {
            *outputDataLength = 0;
        }
        return false;
    }
    *outputDataLength = 0;

#ifdef USE_MBEDTLS
    mbedtls_cipher_mode_t cipherMode;
    size_t outLength = 0;

    switch (algorithm) {
    case ALGORITHM_AES_CBC:
        LC_ASSERT(tag == NULL);
        LC_ASSERT(tagLength == 0);
        cipherMode = MBEDTLS_MODE_CBC;
        break;
    case ALGORITHM_AES_GCM:
        LC_ASSERT(tag != NULL);
        LC_ASSERT(tagLength > 0);
        cipherMode = MBEDTLS_MODE_GCM;
        break;
    default:
        LC_ASSERT(false);
        return false;
    }

    if (!ctx->initialized) {
        if (mbedtls_cipher_setup(&ctx->ctx, mbedtls_cipher_info_from_values(MBEDTLS_CIPHER_ID_AES, keyLength * 8, cipherMode)) != 0) {
            return false;
        }

        if (algorithm == ALGORITHM_AES_CBC &&
                mbedtls_cipher_set_padding_mode(&ctx->ctx, MBEDTLS_PADDING_PKCS7) != 0) {
            return false;
        }

        if (mbedtls_cipher_setkey(&ctx->ctx, key, keyLength * 8, MBEDTLS_ENCRYPT) != 0) {
            return false;
        }

        ctx->initialized = true;
    }

    if (tag != NULL) {
#ifdef USE_MBEDTLS_CRYPTO_EXT
        // In mbedTLS, tag is always after ciphertext, while we need to put tag BEFORE ciphertext here
        // To avoid frequent heap allocation, we will use some evil tricks...
        // We only support 16 bytes sized tag
        LC_ASSERT(tagLength == 16);
        // Assume outputData is right after tag
        LC_ASSERT(outputData == tag + tagLength);
#ifndef LC_DEBUG
        if (tagLength != 16 || outputData != tag + tagLength) {
            return false;
        }
#endif
        size_t encryptedLength = 0;
        unsigned char * encryptedData = tag;
        size_t encryptedCapacity = inputDataLength + tagLength;
        if (mbedtls_cipher_auth_encrypt_ext(&ctx->ctx, iv, ivLength, NULL, 0, inputData, inputDataLength, encryptedData,
                                            encryptedCapacity, &encryptedLength, tagLength) != 0) {
            clearBuffer(tag, tagLength);
            clearOutput(outputData, inputDataLength, outputDataLength);
            return false;
        }
        outLength = encryptedLength - tagLength;

        unsigned char tagTemp[16];
        // Copy the tag to temp buffer
        memcpy(tagTemp, encryptedData + outLength, tagLength);
        // Move ciphertext to the end
        memmove(encryptedData + tagLength, encryptedData, outLength);
        // Copy back tag
        memcpy(encryptedData, tagTemp, tagLength);
#else
        if (mbedtls_cipher_auth_encrypt(&ctx->ctx, iv, ivLength, NULL, 0, inputData, inputDataLength, outputData, &outLength, tag, tagLength) != 0) {
            clearBuffer(tag, tagLength);
            clearOutput(outputData, inputDataLength, outputDataLength);
            return false;
        }
#endif
    }
    else {
        if (flags & CIPHER_FLAG_RESET_IV) {
            if (mbedtls_cipher_set_iv(&ctx->ctx, iv, ivLength) != 0) {
                return false;
            }

            mbedtls_cipher_reset(&ctx->ctx);
        }

        if (flags & CIPHER_FLAG_PAD_TO_BLOCK_SIZE) {
            inputDataLength = addPkcs7PaddingInPlace(inputData, inputDataLength);
        }

        if (mbedtls_cipher_update(&ctx->ctx, inputData, inputDataLength, outputData, &outLength) != 0) {
            clearOutput(outputData, (int)outLength, outputDataLength);
            return false;
        }

        if (flags & CIPHER_FLAG_FINISH) {
            size_t finishLength;

            if (mbedtls_cipher_finish(&ctx->ctx, &outputData[outLength], &finishLength) != 0) {
                clearOutput(outputData, (int)outLength, outputDataLength);
                return false;
            }

            outLength += finishLength;
        }
    }

    *outputDataLength = outLength;
    return true;
#else
    LC_ASSERT(keyLength == 16);

    if (algorithm == ALGORITHM_AES_GCM) {
        LC_ASSERT(tag != NULL);
        LC_ASSERT(tagLength > 0);

        if (!ctx->initialized || (flags & CIPHER_FLAG_RESET_IV)) {
            // Perform a full initialization. This codepath also allows
            // us to change the IV length if required.
            if (EVP_EncryptInit_ex(ctx->ctx, EVP_aes_128_gcm(), NULL, NULL, NULL) != 1) {
                return false;
            }

            if (EVP_CIPHER_CTX_ctrl(ctx->ctx, EVP_CTRL_GCM_SET_IVLEN, ivLength, NULL) != 1) {
                return false;
            }

            if (EVP_EncryptInit_ex(ctx->ctx, NULL, NULL, key, iv) != 1) {
                return false;
            }

            ctx->initialized = true;
        }
        else {
            // Calling with cipher == NULL results in a parameter change
            // without requiring a reallocation of the internal cipher ctx.
            if (EVP_EncryptInit_ex(ctx->ctx, NULL, NULL, NULL, iv) != 1) {
                return false;
            }
        }
    }
    else if (algorithm == ALGORITHM_AES_CBC) {
        LC_ASSERT(tag == NULL);
        LC_ASSERT(tagLength == 0);

        if (!ctx->initialized) {
            // Perform a full initialization
            if (EVP_EncryptInit_ex(ctx->ctx, EVP_aes_128_cbc(), NULL, key, iv) != 1) {
                return false;
            }

            ctx->initialized = true;
        }
        else if (flags & CIPHER_FLAG_RESET_IV) {
            // Calling with cipher == NULL results in a parameter change
            // without requiring a reallocation of the internal cipher ctx.
            if (EVP_EncryptInit_ex(ctx->ctx, NULL, NULL, NULL, iv) != 1) {
                return false;
            }
        }

        if (flags & CIPHER_FLAG_PAD_TO_BLOCK_SIZE) {
            inputDataLength = addPkcs7PaddingInPlace(inputData, inputDataLength);
        }
    }
    else {
        LC_ASSERT(false);
        return false;
    }

    if (EVP_EncryptUpdate(ctx->ctx, outputData, outputDataLength, inputData, inputDataLength) != 1) {
        clearOutput(outputData, *outputDataLength, outputDataLength);
        return false;
    }

    if (algorithm == ALGORITHM_AES_GCM) {
        int len;

        // GCM encryption won't ever fill ciphertext here but we have to call it anyway
        if (EVP_EncryptFinal_ex(ctx->ctx, outputData, &len) != 1) {
            clearOutput(outputData, *outputDataLength, outputDataLength);
            return false;
        }
        LC_ASSERT(len == 0);

        if (EVP_CIPHER_CTX_ctrl(ctx->ctx, EVP_CTRL_GCM_GET_TAG, tagLength, tag) != 1) {
            clearBuffer(tag, tagLength);
            clearOutput(outputData, *outputDataLength, outputDataLength);
            return false;
        }
    }
    else if (flags & CIPHER_FLAG_FINISH) {
        int len;

        if (EVP_EncryptFinal_ex(ctx->ctx, &outputData[*outputDataLength], &len) != 1) {
            clearOutput(outputData, *outputDataLength, outputDataLength);
            return false;
        }

        *outputDataLength += len;
    }

    return true;
#endif
}

// When CBC is used, outputData buffer must be allocated such that the buffer length is
// at least ROUND_TO_PKCS7_PADDED_LEN(inputDataLength) to allow room for PKCS7 padding.
// For GCM, the IV can change from message to message without CIPHER_FLAG_RESET_IV.
// CIPHER_FLAG_RESET_IV is only required for GCM when the IV length changes.
// Changing the key between encrypt/decrypt calls on a single context is not supported.
bool PltDecryptMessage(PPLT_CRYPTO_CONTEXT ctx, int algorithm, int flags,
                       unsigned char* key, int keyLength,
                       unsigned char* iv, int ivLength,
                       unsigned char* tag, int tagLength,
                       unsigned char* inputData, int inputDataLength,
                       unsigned char* outputData, int* outputDataLength) {
    if (!validateCipherArguments(ctx, algorithm, flags, key, keyLength, iv, ivLength,
                                 tag, tagLength, inputData, inputDataLength,
                                 outputData, outputDataLength)) {
        if (outputDataLength != NULL) {
            *outputDataLength = 0;
        }
        return false;
    }
    *outputDataLength = 0;

#ifdef USE_MBEDTLS
    mbedtls_cipher_mode_t cipherMode;
    size_t outLength = 0;

    switch (algorithm) {
    case ALGORITHM_AES_CBC:
        LC_ASSERT(tag == NULL);
        LC_ASSERT(tagLength == 0);
        cipherMode = MBEDTLS_MODE_CBC;
        break;
    case ALGORITHM_AES_GCM:
        LC_ASSERT(tag != NULL);
        LC_ASSERT(tagLength > 0);
        cipherMode = MBEDTLS_MODE_GCM;
        break;
    default:
        LC_ASSERT(false);
        return false;
    }

    if (!ctx->initialized) {
        if (mbedtls_cipher_setup(&ctx->ctx, mbedtls_cipher_info_from_values(MBEDTLS_CIPHER_ID_AES, keyLength * 8, cipherMode)) != 0) {
            return false;
        }

        if (algorithm == ALGORITHM_AES_CBC &&
                mbedtls_cipher_set_padding_mode(&ctx->ctx, MBEDTLS_PADDING_PKCS7) != 0) {
            return false;
        }

        if (mbedtls_cipher_setkey(&ctx->ctx, key, keyLength * 8, MBEDTLS_DECRYPT) != 0) {
            return false;
        }

        ctx->initialized = true;
    }

    if (tag != NULL) {
#ifdef USE_MBEDTLS_CRYPTO_EXT
        // We only support 16 bytes sized tag
        LC_ASSERT(tagLength == 16);
        // Assume inputData is right after tag
        LC_ASSERT(inputData == tag + tagLength);
#ifndef LC_DEBUG
        if (tagLength != 16 || inputData != tag + tagLength) {
            return false;
        }
#endif
        unsigned char * encryptedData = tag;
        size_t encryptedDataLen = inputDataLength + tagLength;
        unsigned char tagTemp[16];
        // Copy the tag to temp buffer
        memcpy(tagTemp, encryptedData, tagLength);
        // Move ciphertext to the beginning
        memmove(encryptedData, encryptedData + tagLength, inputDataLength);
        // Copy back tag to the end
        memcpy(encryptedData + inputDataLength, tagTemp, tagLength);
        if (mbedtls_cipher_auth_decrypt_ext(&ctx->ctx, iv, ivLength, NULL, 0, encryptedData, encryptedDataLen,
                                            outputData, inputDataLength, &outLength, tagLength) != 0) {
            clearOutput(outputData, inputDataLength, outputDataLength);
            return false;
        }
#else
        if (mbedtls_cipher_auth_decrypt(&ctx->ctx, iv, ivLength, NULL, 0, inputData, inputDataLength, outputData, &outLength, tag, tagLength) != 0) {
            clearOutput(outputData, inputDataLength, outputDataLength);
            return false;
        }
#endif
    }
    else {
        if (flags & CIPHER_FLAG_RESET_IV) {
            if (mbedtls_cipher_set_iv(&ctx->ctx, iv, ivLength) != 0) {
                return false;
            }

            mbedtls_cipher_reset(&ctx->ctx);
        }

        if (mbedtls_cipher_update(&ctx->ctx, inputData, inputDataLength, outputData, &outLength) != 0) {
            clearOutput(outputData, (int)outLength, outputDataLength);
            return false;
        }

        if (flags & CIPHER_FLAG_FINISH) {
            size_t finishLength;

            if (mbedtls_cipher_finish(&ctx->ctx, &outputData[outLength], &finishLength) != 0) {
                clearOutput(outputData, (int)outLength, outputDataLength);
                return false;
            }

            outLength += finishLength;
        }
    }

    *outputDataLength = outLength;
    return true;
#else
    LC_ASSERT(keyLength == 16);

    if (algorithm == ALGORITHM_AES_GCM) {
        LC_ASSERT(tag != NULL);
        LC_ASSERT(tagLength > 0);

        if (!ctx->initialized || (flags & CIPHER_FLAG_RESET_IV)) {
            // Perform a full initialization. This codepath also allows
            // us to change the IV length if required.
            if (EVP_DecryptInit_ex(ctx->ctx, EVP_aes_128_gcm(), NULL, NULL, NULL) != 1) {
                return false;
            }

            if (EVP_CIPHER_CTX_ctrl(ctx->ctx, EVP_CTRL_GCM_SET_IVLEN, ivLength, NULL) != 1) {
                return false;
            }

            if (EVP_DecryptInit_ex(ctx->ctx, NULL, NULL, key, iv) != 1) {
                return false;
            }

            ctx->initialized = true;
        }
        else {
            // Calling with cipher == NULL results in a parameter change
            // without requiring a reallocation of the internal cipher ctx.
            if (EVP_DecryptInit_ex(ctx->ctx, NULL, NULL, NULL, iv) != 1) {
                return false;
            }
        }
    }
    else if (algorithm == ALGORITHM_AES_CBC) {
        LC_ASSERT(tag == NULL);
        LC_ASSERT(tagLength == 0);

        if (!ctx->initialized) {
            // Perform a full initialization
            if (EVP_DecryptInit_ex(ctx->ctx, EVP_aes_128_cbc(), NULL, key, iv) != 1) {
                return false;
            }

            ctx->initialized = true;
        }
        else if (flags & CIPHER_FLAG_RESET_IV) {
            // Calling with cipher == NULL results in a parameter change
            // without requiring a reallocation of the internal cipher ctx.
            if (EVP_DecryptInit_ex(ctx->ctx, NULL, NULL, NULL, iv) != 1) {
                return false;
            }
        }
    }
    else {
        LC_ASSERT(false);
        return false;
    }

    if (EVP_DecryptUpdate(ctx->ctx, outputData, outputDataLength, inputData, inputDataLength) != 1) {
        clearOutput(outputData, *outputDataLength, outputDataLength);
        return false;
    }

    if (algorithm == ALGORITHM_AES_GCM) {
        int len;

        // Set the GCM tag before calling EVP_DecryptFinal_ex()
        if (EVP_CIPHER_CTX_ctrl(ctx->ctx, EVP_CTRL_GCM_SET_TAG, tagLength, tag) != 1) {
            clearOutput(outputData, *outputDataLength, outputDataLength);
            return false;
        }

        // GCM will never have additional plaintext here, but we need to call it to
        // ensure that the GCM authentication tag is correct for this data.
        if (EVP_DecryptFinal_ex(ctx->ctx, outputData, &len) != 1) {
            clearOutput(outputData, *outputDataLength, outputDataLength);
            return false;
        }
        LC_ASSERT(len == 0);
    }
    else if (flags & CIPHER_FLAG_FINISH) {
        int len;

        if (EVP_DecryptFinal_ex(ctx->ctx, &outputData[*outputDataLength], &len) != 1) {
            clearOutput(outputData, *outputDataLength, outputDataLength);
            return false;
        }

        *outputDataLength += len;
    }

    return true;
#endif
}

PPLT_CRYPTO_CONTEXT PltCreateCryptoContext(void) {
    PPLT_CRYPTO_CONTEXT ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->initialized = false;

#ifdef USE_MBEDTLS
    mbedtls_cipher_init(&ctx->ctx);
#else
    ctx->ctx = EVP_CIPHER_CTX_new();
    if (!ctx->ctx) {
        free(ctx);
        return NULL;
    }
#endif

    return ctx;
}

void PltDestroyCryptoContext(PPLT_CRYPTO_CONTEXT ctx) {
    if (ctx == NULL) {
        return;
    }

#ifdef USE_MBEDTLS
    mbedtls_cipher_free(&ctx->ctx);
#else
    EVP_CIPHER_CTX_free(ctx->ctx);
#endif
    free(ctx);
}

bool PltGenerateRandomData(unsigned char* data, int length) {
    if (data == NULL || length <= 0) {
        return false;
    }

#ifdef USE_MBEDTLS
    // FIXME: This is not thread safe...
    if (!RandomStateInitialized) {
        mbedtls_entropy_init(&EntropyContext);
        mbedtls_ctr_drbg_init(&CtrDrbgContext);
        if (mbedtls_ctr_drbg_seed(&CtrDrbgContext, mbedtls_entropy_func, &EntropyContext, NULL, 0) != 0) {
            // Nothing we can really do here...
            Limelog("Seeding MbedTLS random number generator failed!\n");
            LC_ASSERT(false);
            return false;
        }

        RandomStateInitialized = true;
    }

    return mbedtls_ctr_drbg_random(&CtrDrbgContext, data, length) == 0;
#else
    return RAND_bytes(data, length) == 1;
#endif
}

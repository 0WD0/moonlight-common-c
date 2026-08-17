#include "PlatformCrypto.h"

#include <stdio.h>
#include <string.h>

#define EXPECT_TRUE(expression)                                                   \
    do {                                                                          \
        if (!(expression)) {                                                       \
            fprintf(stderr, "%s:%d: expected true: %s\n", __FILE__, __LINE__,     \
                    #expression);                                                  \
            return false;                                                          \
        }                                                                          \
    } while (0)

#define EXPECT_FALSE(expression) EXPECT_TRUE(!(expression))

#define EXPECT_EQUAL(expected, actual)                                             \
    do {                                                                          \
        if ((expected) != (actual)) {                                               \
            fprintf(stderr, "%s:%d: expected %d, got %d\n", __FILE__, __LINE__,    \
                    (int)(expected), (int)(actual));                                \
            return false;                                                          \
        }                                                                          \
    } while (0)

#define EXPECT_BYTES(expected, actual, length)                                     \
    do {                                                                          \
        if (memcmp((expected), (actual), (length)) != 0) {                         \
            fprintf(stderr, "%s:%d: byte comparison failed\n", __FILE__,           \
                    __LINE__);                                                      \
            return false;                                                          \
        }                                                                          \
    } while (0)

static bool testAes128CbcKnownAnswer(void) {
    static unsigned char key[16] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    };
    static unsigned char iv[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    };
    static unsigned char plaintext[16] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    };
    static const unsigned char expectedCiphertext[16] = {
        0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
        0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
    };
    unsigned char ciphertext[32] = {0};
    int ciphertextLength = -1;
    PPLT_CRYPTO_CONTEXT encryptContext = PltCreateCryptoContext();

    EXPECT_TRUE(encryptContext != NULL);
    EXPECT_TRUE(PltEncryptMessage(
        encryptContext, ALGORITHM_AES_CBC, CIPHER_FLAG_RESET_IV,
        key, sizeof(key), iv, sizeof(iv), NULL, 0, plaintext,
        sizeof(plaintext), ciphertext, &ciphertextLength));
    EXPECT_EQUAL(sizeof(expectedCiphertext), ciphertextLength);
    EXPECT_BYTES(expectedCiphertext, ciphertext, sizeof(expectedCiphertext));

    PltDestroyCryptoContext(encryptContext);
    return true;
}

static bool testAes128CbcRoundTripAndPaddingFailure(void) {
    unsigned char key[16] = {0};
    unsigned char iv[16] = {0};
    unsigned char plaintext[32];
    unsigned char ciphertext[32] = {0};
    unsigned char corruptedCiphertext[32];
    unsigned char decrypted[32] = {0};
    int ciphertextLength = -1;
    int decryptedLength = -1;
    int i;
    PPLT_CRYPTO_CONTEXT encryptContext = PltCreateCryptoContext();
    PPLT_CRYPTO_CONTEXT decryptContext = PltCreateCryptoContext();

    for (i = 0; i < 31; i++) {
        plaintext[i] = (unsigned char)(i + 1);
    }

    EXPECT_TRUE(encryptContext != NULL);
    EXPECT_TRUE(decryptContext != NULL);
    EXPECT_TRUE(PltEncryptMessage(
        encryptContext, ALGORITHM_AES_CBC,
        CIPHER_FLAG_RESET_IV | CIPHER_FLAG_PAD_TO_BLOCK_SIZE, key,
        sizeof(key), iv, sizeof(iv), NULL, 0, plaintext, 31, ciphertext,
        &ciphertextLength));
    EXPECT_EQUAL(sizeof(ciphertext), ciphertextLength);

    EXPECT_TRUE(PltDecryptMessage(
        decryptContext, ALGORITHM_AES_CBC,
        CIPHER_FLAG_RESET_IV | CIPHER_FLAG_FINISH, key, sizeof(key), iv,
        sizeof(iv), NULL, 0, ciphertext, ciphertextLength, decrypted,
        &decryptedLength));
    EXPECT_EQUAL(31, decryptedLength);
    EXPECT_BYTES(plaintext, decrypted, 31);

    memcpy(corruptedCiphertext, ciphertext, sizeof(ciphertext));
    corruptedCiphertext[15] ^= 1;
    memset(decrypted, 0xff, sizeof(decrypted));
    decryptedLength = -1;
    EXPECT_FALSE(PltDecryptMessage(
        decryptContext, ALGORITHM_AES_CBC,
        CIPHER_FLAG_RESET_IV | CIPHER_FLAG_FINISH, key, sizeof(key), iv,
        sizeof(iv), NULL, 0, corruptedCiphertext, sizeof(corruptedCiphertext),
        decrypted, &decryptedLength));
    EXPECT_EQUAL(0, decryptedLength);
    EXPECT_BYTES(iv, decrypted, sizeof(iv));

    PltDestroyCryptoContext(encryptContext);
    PltDestroyCryptoContext(decryptContext);
    return true;
}

static bool testAes128GcmKnownAnswerAndAuthenticationFailure(void) {
    static unsigned char key[16] = {0};
    static unsigned char iv[12] = {0};
    static unsigned char plaintext[16] = {0};
    static const unsigned char expectedCiphertext[16] = {
        0x03, 0x88, 0xda, 0xce, 0x60, 0xb6, 0xa3, 0x92,
        0xf3, 0x28, 0xc2, 0xb9, 0x71, 0xb2, 0xfe, 0x78,
    };
    static const unsigned char expectedTag[16] = {
        0xab, 0x6e, 0x47, 0xd4, 0x2c, 0xec, 0x13, 0xbd,
        0xf5, 0x3a, 0x67, 0xb2, 0x12, 0x57, 0xbd, 0xdf,
    };
    unsigned char sealedMessage[32] = {0};
    unsigned char decrypted[16] = {0xff};
    int ciphertextLength = -1;
    int decryptedLength = -1;
    PPLT_CRYPTO_CONTEXT encryptContext = PltCreateCryptoContext();
    PPLT_CRYPTO_CONTEXT decryptContext = PltCreateCryptoContext();

    EXPECT_TRUE(encryptContext != NULL);
    EXPECT_TRUE(decryptContext != NULL);
    EXPECT_TRUE(PltEncryptMessage(
        encryptContext, ALGORITHM_AES_GCM, CIPHER_FLAG_RESET_IV,
        key, sizeof(key), iv, sizeof(iv), sealedMessage, 16, plaintext,
        sizeof(plaintext), &sealedMessage[16], &ciphertextLength));
    EXPECT_EQUAL(sizeof(expectedCiphertext), ciphertextLength);
    EXPECT_BYTES(expectedTag, sealedMessage, sizeof(expectedTag));
    EXPECT_BYTES(expectedCiphertext, &sealedMessage[16],
                 sizeof(expectedCiphertext));

    EXPECT_TRUE(PltDecryptMessage(
        decryptContext, ALGORITHM_AES_GCM, CIPHER_FLAG_RESET_IV,
        key, sizeof(key), iv, sizeof(iv), sealedMessage, 16,
        &sealedMessage[16], ciphertextLength, decrypted, &decryptedLength));
    EXPECT_EQUAL(sizeof(plaintext), decryptedLength);
    EXPECT_BYTES(plaintext, decrypted, sizeof(plaintext));

    memcpy(sealedMessage, expectedTag, sizeof(expectedTag));
    sealedMessage[0] ^= 1;
    memcpy(&sealedMessage[16], expectedCiphertext, sizeof(expectedCiphertext));
    memset(decrypted, 0xff, sizeof(decrypted));
    decryptedLength = -1;
    EXPECT_FALSE(PltDecryptMessage(
        decryptContext, ALGORITHM_AES_GCM, CIPHER_FLAG_RESET_IV,
        key, sizeof(key), iv, sizeof(iv), sealedMessage, 16,
        &sealedMessage[16], ciphertextLength, decrypted, &decryptedLength));
    EXPECT_EQUAL(0, decryptedLength);
    EXPECT_BYTES(plaintext, decrypted, sizeof(decrypted));

    PltDestroyCryptoContext(encryptContext);
    PltDestroyCryptoContext(decryptContext);
    return true;
}

static bool testZeroLengthGcmMessage(void) {
    static unsigned char key[16] = {0};
    static unsigned char iv[12] = {0};
    static const unsigned char expectedTag[16] = {
        0x58, 0xe2, 0xfc, 0xce, 0xfa, 0x7e, 0x30, 0x61,
        0x36, 0x7f, 0x1d, 0x57, 0xa4, 0xe7, 0x45, 0x5a,
    };
    unsigned char sealedMessage[17] = {0};
    int outputLength = -1;
    PPLT_CRYPTO_CONTEXT context = PltCreateCryptoContext();

    EXPECT_TRUE(context != NULL);
    EXPECT_TRUE(PltEncryptMessage(
        context, ALGORITHM_AES_GCM, CIPHER_FLAG_RESET_IV, key, sizeof(key),
        iv, sizeof(iv), sealedMessage, 16, NULL, 0, &sealedMessage[16],
        &outputLength));
    EXPECT_EQUAL(0, outputLength);
    EXPECT_BYTES(expectedTag, sealedMessage, sizeof(expectedTag));

    PltDestroyCryptoContext(context);
    return true;
}

static bool testInvalidArgumentsAreRejected(void) {
    unsigned char key[16] = {0};
    unsigned char iv[16] = {0};
    unsigned char tag[16] = {0};
    unsigned char input[16] = {0};
    unsigned char output[32] = {0};
    int outputLength = 7;
    PPLT_CRYPTO_CONTEXT context = PltCreateCryptoContext();

    EXPECT_TRUE(context != NULL);
    EXPECT_FALSE(PltEncryptMessage(
        NULL, ALGORITHM_AES_CBC, 0, key, sizeof(key), iv, sizeof(iv), NULL,
        0, input, sizeof(input), output, &outputLength));
    EXPECT_EQUAL(0, outputLength);
    outputLength = 7;
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_CBC, 0, NULL, sizeof(key), iv, sizeof(iv),
        NULL, 0, input, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_CBC, 0, key, sizeof(key) - 1, iv, sizeof(iv),
        NULL, 0, input, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_CBC, 0, key, sizeof(key), NULL, sizeof(iv),
        NULL, 0, input, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_CBC, 0, key, sizeof(key), iv, sizeof(iv), NULL,
        0, NULL, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_CBC, 0, key, sizeof(key), iv, sizeof(iv), NULL,
        0, input, -1, output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_CBC, 0, key, sizeof(key), iv, sizeof(iv), NULL,
        0, input, sizeof(input), NULL, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_CBC, 0, key, sizeof(key), iv, sizeof(iv), NULL,
        0, input, sizeof(input), output, NULL));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_CBC, 0x80, key, sizeof(key), iv, sizeof(iv),
        NULL, 0, input, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_CBC, 0, key, sizeof(key), iv, sizeof(iv) - 1,
        NULL, 0, input, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_CBC, 0, key, sizeof(key), iv, sizeof(iv), tag,
        sizeof(tag), input, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_GCM, 0, key, sizeof(key), iv, 0, tag,
        sizeof(tag), input, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_GCM, 0, key, sizeof(key), iv, sizeof(iv) + 1,
        tag, sizeof(tag), input, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_GCM, 0, key, sizeof(key), iv, sizeof(iv), NULL,
        sizeof(tag), input, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_GCM, 0, key, sizeof(key), iv, sizeof(iv), tag,
        sizeof(tag) - 1, input, sizeof(input), output, &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, ALGORITHM_AES_GCM, CIPHER_FLAG_FINISH, key, sizeof(key), iv,
        sizeof(iv), tag, sizeof(tag), input, sizeof(input), output,
        &outputLength));
    EXPECT_FALSE(PltEncryptMessage(
        context, 0, 0, key, sizeof(key), iv, sizeof(iv), NULL, 0, input,
        sizeof(input), output, &outputLength));

    PltDestroyCryptoContext(context);
    PltDestroyCryptoContext(NULL);
    return true;
}

static bool testRandomGeneration(void) {
    unsigned char randomData[32] = {0};
    unsigned char zeroData[32] = {0};

    EXPECT_FALSE(PltGenerateRandomData(NULL, sizeof(randomData)));
    EXPECT_FALSE(PltGenerateRandomData(randomData, 0));
    EXPECT_FALSE(PltGenerateRandomData(randomData, -1));
    EXPECT_TRUE(PltGenerateRandomData(randomData, sizeof(randomData)));
    EXPECT_FALSE(memcmp(randomData, zeroData, sizeof(randomData)) == 0);
    return true;
}

int main(void) {
    if (!testAes128CbcKnownAnswer() ||
        !testAes128CbcRoundTripAndPaddingFailure() ||
        !testAes128GcmKnownAnswerAndAuthenticationFailure() ||
        !testZeroLengthGcmMessage() ||
        !testInvalidArgumentsAreRejected() ||
        !testRandomGeneration()) {
        return 1;
    }

    return 0;
}

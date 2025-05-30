#ifndef _QS_COMMON_H_
#define _QS_COMMON_H_


#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/decoder.h>
#include <openssl/encoder.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/trace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

#include "oqs/oqs.h"

/* limit testing to just this guy */
#define OQSPROV_PROPQ "provider=oqsprovider"

/* For controlled success */
#define T(e)                                                                   \
    if (!(e)) {                                                                \
        ERR_print_errors_fp(stderr);                                           \
        OPENSSL_die(#e, __FILE__, __LINE__);                                   \
    }
/* For controlled failure */
#define TF(e)                                                                  \
    if ((e)) {                                                                 \
        ERR_print_errors_fp(stderr);                                           \
    } else {                                                                   \
        OPENSSL_die(#e, __FILE__, __LINE__);                                   \
    }
#define cRED "\033[1;31m"
#define cDRED "\033[0;31m"
#define cGREEN "\033[1;32m"
#define cDGREEN "\033[0;32m"
#define cBLUE "\033[1;34m"
#define cDBLUE "\033[0;34m"
#define cNORM "\033[m"
#define PROVIDER_NAME_OQS "oqsprovider"
#define TEST_ASSERT(e)                                                         \
    {                                                                          \
        if (!(test = (e)))                                                     \
            printf(cRED "  Test FAILED" cNORM "\n");                           \
        else                                                                   \
            printf(cGREEN "  Test passed" cNORM "\n");                         \
    }

void hexdump(const void *ptr, size_t len);
int alg_is_enabled(const char *algname);

/** \brief Loads the default provider.
 *
 * \param libctx Top-level OpenSSL context.
 *
 * \returns The default provider. */
OSSL_PROVIDER *load_default_provider(OSSL_LIB_CTX *libctx);

/* Loads the oqs-provider. */
void load_oqs_provider(OSSL_LIB_CTX *libctx, const char *modulename,
                       const char *configfile);

/** \brief Indicates if a signature algorithm is hybrid or not.
 *
 * \param alg Algorithm name.
 *
 * \returns 1 if hybrid, else 0. */
int is_signature_algorithm_hybrid(const char *_alg_);

/** \brief Indicates if a signature algorithm is composite or not.
 *
 * \param alg Algorithm name.
 *
 * \returns 1 if hybrid, else 0. */
int is_signature_algorithm_composite(const char *_alg_);

/** \brief Indicates if an kem algorithm is hybrid or not.
 *
 * \param alg Algorithm name.
 *
 * \returns 1 if hybrid, else 0. */
int is_kem_algorithm_hybrid(const char *_alg_);

/** \brief Extracts an octet string from a parameter of an EVP_PKEY.
 *
 * \param key The EVP_PKEY;
 * \param param_name Name of the parameter.
 * \param[out] buf Out buffer.
 * \param[out] buf_len Size of out buffer.
 *
 * \returns 0 on success. */
int get_param_octet_string(const EVP_PKEY *key, const char *param_name,
                           uint8_t **buf, size_t *buf_len);

int set_param_octet_string(EVP_PKEY *key, const char *param_name,
                            uint8_t *buf, size_t buf_len);

#define PEM_BUFF_LEN 8192

int read_file_to_buffer(uint8_t* buff, int max_buff_len, char* file_path);

#define MAXLOOPS 1000000


struct KeyPair {
    /** \brief The public key. */
    uint8_t *pubkey;

    /** \brief The public key length, in bytes. */
    size_t pubkey_len;

    /** \brief The private key. */
    uint8_t *privkey;

    /** \brief The private key length, in bytes. */
    size_t privkey_len;

    /** \brief Indicates if the pair of keys is from a quantum-resistant
     * algorithm (1) or not (0). */
    int is_pq;
};

#endif
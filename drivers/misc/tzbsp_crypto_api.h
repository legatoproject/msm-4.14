/* kernel/drivers/misc/tzbsp_crypto_api.h
 *
 * Copyright (C) 2012 Sierra Wireless, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This is a Sierra created file and it is adapted and should keep sync to TZ code:
 *   trustzone_images/core/securemsm/trustzone/qsee/include/crypto/tzbsp_crypto_api.h
 *
 */

#ifndef TZBSP_CRYPTO_API_H
#define TZBSP_CRYPTO_API_H

typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef int64_t int64;
typedef int32_t int32;
typedef int16_t int16;
typedef int8_t int8;

#define TZ_STORAGE_SVC_MAX_KEYLEN    CRYPTO_ASYM_KEY_SIZE_MAX

#define TZ_SVC_CRYPTO             10

#define TZ_SYSCALL_CREATE_CMD_ID(s, f) \
  ((uint32)(((s & 0x3ff) << 10) | (f & 0x3ff)))

#define TZ_CRYPTO_SERVICE_SYM_ID                         \
  TZ_SYSCALL_CREATE_CMD_ID(TZ_SVC_CRYPTO, 0x02)

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/
#define TZ_CRYPTO_SVC_CLOCK_ENABLE 1
#define TZ_CRYPTO_SVC_CLOCK_DISABLE 0
#define TZ_CRYPTO_SVC_NONCE_LEN 8
#define TZ_CRYPTO_SVC_MAC_LEN   16
#define TZ_CRYPTO_SVC_MAX_PLAIN_DLEN       0x800

#define CRYPTO_ASYM_MAGIC_NUM (0x4B4D4B42)    /* "" CRYPOT_API MAGIC NUMBER FOR SYM_KEY*/
/* Endianness converter Macro */
#define crypto_asym_htonl(x)                                             \
  (((((uint32)(x) & 0x000000FFU) << 24) |                              \
  (((uint32)(x) & 0x0000FF00U) <<  8) |                                 \
  (((uint32)(x) & 0x00FF0000U) >>  8) |                                 \
  (((uint32)(x) & 0xFF000000U) >> 24)))

#define CRYPTO_ASYM_AES128_KEY_SIZE 16
#define CRYPTO_ASYM_AES256_KEY_SIZE 32
#define CRYPTO_ASYM_HMAC_KEY_SIZE   32

#define CRYPTO_ASYM_KEY_SIZE_MAX  (512+16)          /* 4096 bits */
#define CRYPTO_ASYM_PUB_EXP_SIZE_MAX 5              /* 5 bytes max public exponent size supported */

//#define CRYPTO_ASYM_KEY_SIZE_MAX  (512+16)           /* 4096 bits */
#define CRYPTO_ASYM_IV_LENGTH     (16)            /* AES128 CBC IV */
#define CRYPTO_ASYM_HMAC_LENGTH   (32)            /* SHA2 will be used for HMAC  */
typedef uint32 CRYPTO_ASYM_BLONG;

typedef uint32 KM_BLONG;
#define KM_MAX_KEY_SIZE          4128                                              ///< Maximum key size in bits
#define KM_BLONG_SIZE            sizeof(KM_BLONG)                                ///< Bytes per digit
#define KM_BN_BITS_PER_WORD      (KM_BLONG_SIZE * 8)                             ///< Bit per digit
#define KM_BLONGS_PER_KEY        (KM_MAX_KEY_SIZE + 8*KM_BLONG_SIZE - 1)/(8*KM_BLONG_SIZE)  ///< Digits per key
#define CRYPTO_ASYM_RSA_KEY_SIZE(key) ((key)->bi.n * BLONG_SIZE)


typedef enum {
  CRYPTO_STORAGE_SUCCESS=0,
  CRYPTO_STORAGE_FAILURE=1,
  CRYPTO_STORAGE_KDF_ERROR=2,
  CRYPTO_STORAGE_AES_CCM_FAILURE=3,
  ERRORMAX     =0x7FFFFFFF,
} CRYPTO_STORAGE_API_ERROR;

typedef enum
{
  CRYPTO_SERVICE_KEY_LENGTH_FAILURE=0x00000002,
  CRYPTO_SERVICE_MEMORY_ALLOC_FAILURE=0x00000003,
  CRYPTO_SERVICE_MEMORY_IN_SECURE_REGION=0x00000004,
  CRYPTO_SERVICE_CMD_RSP_PARAM_INCORRECT=0x00000005,
  CRYPTO_SERVICE_NULL_POINTER=0x00000006,
}CRYPTO_ERROR_CODE;

typedef struct
{
  void*        key_material;
  uint32       key_material_len;
} tz_storage_service_key_blob_t;

/**
 * Commands supported
 */
typedef enum   {
  TZ_STOR_SVC_GENERATE_KEY   = 0x00000001,
  TZ_STOR_SVC_SEAL_DATA      = 0x00000002,
  TZ_STOR_SVC_UNSEAL_DATA    = 0x00000003,
}tz_storage_service_cmd_t;

typedef enum {
  TZ_STOR_SVC_AES_CCM_ENCRYPT=0,
  TZ_STOR_SVC_AES_CCM_DECRYPT=1,
  TZ_STOR_SVC_MAX=0x7FFFFFFF,
}TZ_STOR_SVC_AES_ENCRYPT_DECRYPT;
/**
 * Generate Key Data Structures
 */
typedef struct tz_storage_service_gen_key_cmd {
  tz_storage_service_cmd_t        cmd_id;
  tz_storage_service_key_blob_t   key_blob;
} tz_storage_service_gen_key_cmd_t;

typedef struct tz_storage_service_gen_key_resp {
  tz_storage_service_cmd_t    cmd_id;
  int32                       status;
  uint32                      key_blob_size;
} tz_storage_service_gen_key_resp_t;

typedef enum
{
  TZ_CRYPTO_SYM_KEY_GEN  = 0x00000001,
  TZ_CRYPTO_RSA_KEY_GEN  = 0x00000002,
  TZ_CRYPTO_HMAC_KEY_GEN = 0x00000003,
  CRYPTO_KDF_KEY_MAX     =0x7FFFFFFF,
} TZ_CRYPTO_KDF_SUPPORTED_T;

typedef struct tz_storage_service_seal_data_cmd_t {
  tz_storage_service_cmd_t      cmd_id;
  tz_storage_service_key_blob_t key_blob;
  uint8*                        plain_data;
  uint32                        plain_dlen;
  uint8*                        output_buffer;
  uint32                        output_len;
} tz_storage_service_seal_data_cmd_t;

typedef struct tz_storage_service_seal_data_resp {
  tz_storage_service_cmd_t    cmd_id;
  int32                       status;
  uint32                      sealed_data_len;
}tz_storage_service_seal_data_resp_t;


/**
 * Unseal Data Structures
 */
typedef struct tz_storage_service_unseal_data_cmd {
  tz_storage_service_cmd_t      cmd_id;
  tz_storage_service_key_blob_t key_blob;
  uint8*                        sealed_data;
  uint32                        sealed_dlen;
  uint8*                        output_buffer;
  uint32                        output_len;
} tz_storage_service_unseal_data_cmd_t;

/**
 * Unseal Data Structures
 */
typedef struct tz_storage_service_unseal_data_resp {
  tz_storage_service_cmd_t      cmd_id;
  int32                         status;
  uint32                        unsealed_data_len;
} tz_storage_service_unseal_data_resp_t;

/** Supported HMAC algorithms   */

typedef struct CRYPTO_STORAGE_RSA_PSS_PAD_INFO
{
  /** index of Hash & Mask generation function desired */
  int hashidx;
  /** Length of salt */
  int saltlen;
} CRYPTO_STORAGE_RSA_PSS_PAD_INFO;

/**
 * Asymmetric key pair types.
 */
typedef enum
{
  TYPE_RSA = 1UL,
  TYPE_LAST_ENTRY = 0x7FFFFFFF
} crypto_asym_keypair_t;

typedef enum
{
  /*
  * List the commands supportedin by the hardware.
  */
  CRYPTO_STORAGE_GENERATE_KEY_OLD=0x00000001,
  CRYPTO_STORAGE_EXPORT_PUBKEY=0x00000002,
  CRYPTO_STORAGE_SIGN_DATA_OLD=0x00000003,
  CRYPTO_STORAGE_VERIFY_DATA_OLD=0x00000004,
  CRYPTO_STORAGE_IMPORT_KEY=0x00000005,
  CRYPTO_STORAGE_LAST_CMD_ENTRY = 0x7FFFFFFF,
} crypto_asym_cmd_t;

typedef enum {
  TZ_RSA_DIGEST_PADDING_NONE=0x00000000,
  TZ_RSA_PKCS115_SHA2_256=0x00000001,
  TZ_RSA_PSS_SHA2_256=0x00000002,
  TZ_RSA_MAX_VAL=0x7FFFFFFF,
} tz_rsa_digest_pad_algo_t;


typedef struct
{
  uint32 magic_num; /* Validate the integrity of keyblob content */
  uint32 version; /* API version number */
  tz_rsa_digest_pad_algo_t digest_padding;
  uint8 modulus[CRYPTO_ASYM_KEY_SIZE_MAX]; /* Modulus (N) array [MSB...LSB] */
  uint32 modulus_size; /* Modulus array length */
  uint8 public_exponent[CRYPTO_ASYM_KEY_SIZE_MAX]; /* public exponent (e) array [MSB...LSB] */
  uint32 public_exponent_size; /* public exponent array length */
  uint8 iv[CRYPTO_ASYM_IV_LENGTH]; /* Initial vector */
  uint8 encrypted_private_exponent[CRYPTO_ASYM_KEY_SIZE_MAX]; /* Encrypted Private Exponent (d) array [MSB...LSB] */
  uint32 encrypted_private_exponent_size; /* Encrypted Private Exponent array length */

  uint8 hmac[CRYPTO_ASYM_HMAC_LENGTH]; /* HMAC array */

} crypto_rsa_key_type;


typedef struct
{
  crypto_rsa_key_type *key_material;
  uint32 key_material_len;
} crypto_rsa_key_blob_type;


/**
 * Commands supported
 */

/**
 * Parameters needed to generate an RSA key.
 */
typedef struct
{
  uint32 modulus_size;
  uint64 public_exponent;
  tz_rsa_digest_pad_algo_t digest_pad_type;
} crypto_asym_rsa_keygen_params_t;

/**
 * Digest type used for RSA operations.
 */

/**
 * Type of padding used for RSA operations.
 */

/**
 * Command to Generate a public and private key. The key data returned
 * (by secure app) is in shared buffer at offset of "key_blob" and is opaque
 *
 * cmd_id       : Command issue to secure app
 * key_type     : Currently on RSA_TYPE is supported
 * rsa_params   : Parameters needed to generate an RSA key
 */
struct crypto_asym_gen_keypair_cmd {
  crypto_asym_cmd_t cmd_id;
  crypto_rsa_key_blob_type key_blob;
  crypto_asym_rsa_keygen_params_t rsa_params;
};
typedef struct crypto_asym_gen_keypair_cmd crypto_asym_gen_keypair_cmd_t;

/**
 * Response to Generate a public and private key. The key data returned
 * (by secure app) is in shared buffer at offset of "key_blob" and is opaque
 *
 * cmd_id       : Command issue to secure app
 * key_blob     : key blob data
 * key_blob_len : Total length of key blob information
 * status       : Result (success 0, or failure -1)
 */
struct crypto_asym_gen_keypair_resp {
  crypto_asym_cmd_t cmd_id;
  int32 status;
  unsigned int key_blob_size;
};
typedef struct crypto_asym_gen_keypair_resp crypto_asym_gen_keypair_resp_t;

/**
 * Command to import a public and private key pair. (CRYPTO_STORAGE_IMPORT_KEY)
 *
  * cmd_id       : Command issue to secure service
  */
struct crypto_storage_rsa_import_key_cmd
{
  crypto_asym_cmd_t           cmd_id;
  uint8                       modulus[CRYPTO_ASYM_KEY_SIZE_MAX];
  uint32                      modulus_size;
  uint8                       public_exponent[CRYPTO_ASYM_PUB_EXP_SIZE_MAX];
  uint32                      public_exponent_size;
  uint8                       private_exponent[CRYPTO_ASYM_KEY_SIZE_MAX];
  uint32                      private_exponent_size;
  tz_rsa_digest_pad_algo_t    digest_pad_type;
  crypto_rsa_key_blob_type    key_blob;  // Output
};
typedef struct crypto_storage_rsa_import_key_cmd  crypto_storage_rsa_import_key_cmd_t;

/**
 * Response to import a public and private key.
 *
 * cmd_id        : Command issue to secure service
 * status        : Result
 */
struct crypto_storage_rsa_import_key_resp
{
  crypto_asym_cmd_t   cmd_id;
  int32               status;
};
typedef struct crypto_storage_rsa_import_key_resp  crypto_storage_rsa_import_key_resp_t;

/**
* Command to sign data using a key info generated before. This can use either
* an asymmetric key or a secret key.
* The signed data is returned (by secure app) at offset of data + dlen.
*
* cmd_id  : Command issue to secure app
* sign_param:
* key_blob: key data information (in shared buffer)
* data    : Pointer to plain data buffer
* dlen    : Plain data length
*/

struct crypto_asym_sign_data_cmd {
  crypto_asym_cmd_t cmd_id;
  crypto_rsa_key_blob_type key_blob;
  uint8* data;
  size_t dlen;
  uint8* signeddata;
  uint32 signeddata_len;
};

typedef struct crypto_asym_sign_data_cmd crypto_asym_sign_data_cmd_t;

/**
* Response to sign data command
*
* cmd_id      : Command issue to secure app
* signed_data : Signature
* sig_len     : Signed data length
* status      : Result (success 0, or failure -1)
*/
struct crypto_asym_sign_data_resp {
  crypto_asym_cmd_t cmd_id;
  size_t sig_len;
  int32 status;
};

 typedef struct crypto_asym_sign_data_resp crypto_asym_sign_data_resp_t;

 /**
  * Command to verify data using a key info generated before. This can use either
  * an asymmetric key or a secret key.
  *
  * cmd_id      : Command issue to secure app
  * sign_param:
  * key_blob    : Key data information (in shared buffer)
  * signed_data : Pointer to signed data buffer
  * signed_dlen : Signed data length
  * signature   : Offset to the signature data buffer (from signed data buffer)
  * slen        : Signature data length
  */
struct crypto_asym_verify_data_cmd {
  crypto_asym_cmd_t cmd_id;
  crypto_rsa_key_blob_type key_blob;
  uint8 *signed_data;
  size_t signed_dlen;
  uint8 *signature;
  size_t slen;
};

typedef struct crypto_asym_verify_data_cmd crypto_asym_verify_data_cmd_t;
/**
* Response to verify data
*
* cmd_id        : Command issue to secure app
* status        : Result (success 0, or failure -1)
*/
struct crypto_asym_verify_data_resp {
  crypto_asym_cmd_t cmd_id;
  int32 status;
};

typedef struct crypto_asym_verify_data_resp crypto_asym_verify_data_resp_t;

typedef enum {
  CRYPTO_STORAGE_FORMAT_RAW_BYTES = 1,
} crypto_storage_rsa_key_format_t;

typedef struct crypto_storage_rsa_export_key_cmd {
  crypto_asym_cmd_t           cmd_id;
  crypto_rsa_key_blob_type    key_blob;
  crypto_storage_rsa_key_format_t  export_format;
  uint8                       *modulus;
  uint32                      modulus_size;
  uint8                       *public_exponent;
  uint32                      public_exponent_size;
} crypto_storage_rsa_export_key_cmd_t;

typedef struct  crypto_storage_rsa_export_key_resp {
  crypto_asym_cmd_t   cmd_id;
  int32               status;
  uint32              modulus_size;
  uint32              public_exponent_size;
  uint32              exported_key_len;
} crypto_storage_rsa_export_key_resp_t;

 /*===========================================================================
  GLOBAL VARIABLES
  ===========================================================================*/


typedef struct
{
  CRYPTO_ASYM_BLONG a[KM_BLONGS_PER_KEY];
  int n;
} CRYPTO_ASYM_BigInt;

/** KM_S_BIGINT type */
typedef struct CRYPTO_ASYM_S_BIGINT_
{
   CRYPTO_ASYM_BigInt bi;
   int sign;
} CRYPTO_ASYM_S_BIGINT;

typedef enum {
  /*** Generic outcomes of operations ***/
  E_CE_STOR_SUCCESS           =  0,/* Operation successful */
  E_CE_STOR_FAILURE           =  1,/* Operation failed due to unknown err */
  E_CE_STOR_NO_MEMORY         =  2,/* Allocation from a memory pool failed  */
  E_CE_STOR_NULL_PARAM        =  3,/* Allocation from a memory pool failed  */
  E_CE_STOR_MEM_ALLOC_FALURE  =  4,
  E_CE_STOR_INVALID_ARG       = 10,/* Arg is not recognized */
  E_CE_STOR_OUT_OF_RANGE      = 11,/* Arg value is out of range */
  E_CE_STOR_BAD_ADDRESS       = 12,/* Ptr arg is bad address */
  E_CE_STOR_NO_DATA           = 13,/* Expected data, received none */
  E_CE_STOR_BAD_DATA          = 14,/* Data failed sanity check, e.g. CRC */
  E_CE_STOR_DATA_INVALID      = 15,/* Data is correct, but contents invalid */
  E_CE_STOR_DATA_EXPIRED      = 16,/* Data is not yet or not any more valid */
  E_CE_STOR_DATA_TOO_LARGE    = 17,/* Data is too large to process */
  E_CE_STOR_INVALID_ARG_LEN   = 18,
  E_CE_STOR_RESERVED          = 0x7FFFFFFF
} E_CE_STOR_ErrnoType;


typedef struct {
  tz_storage_service_cmd_t cmd_id;
}tzbsp_crypto_common_cmd_t;

#endif /* TZBSP_CRYPTO_API_H */



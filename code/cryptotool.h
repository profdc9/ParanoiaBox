#ifndef _CRYPTOTOOL_H
#define _CRYPTOTOOL_H

#include <stdlib.h>
#include <stdint.h>
#include <AES.h>
#include <GCM.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KEY_DERIVATION_HASHES 1000
#define KEY_MAX_PASSPHRASE_LENGTH 256
#define KEYMANAGER_HASHLEN 32
#define KEYMANAGER_KEYBYTES 32
#define KEYMANAGER_PUBLICKEY_LEN 32
#define KEYMANAGER_PRIVATEKEY_LEN 32
#define AES_KEYLEN 32
#define AES_BLOCKLEN 16
#define AES_GCM_TAG_LENGTH 16
#define KEYMANAGER_SYMMETRICKEY_LEN AES_KEYLEN
#define KEYMANAGER_MAX_SECRET_LEN KEYMANAGER_PUBLICKEY_LEN

/* BASE 64 encoding */

typedef int (*base64_readdata)(void *v);
typedef int (*base64_writedata)(int c, void *v);

int base64_encode(base64_readdata rd, void *vrd, base64_writedata wd, void *vwd);
int base64_decode(base64_readdata rd, void *vrd, base64_writedata wd, void *vwd);
int is_base64_char(char ch);
		
/* Crypto tools assist */

int ctblake2s( void *out, size_t outlen, const void *in, size_t inlen, const void *key, size_t keylen );
int ctblake2srehash( void *hash, size_t inlen, int numhash);
int aes256_memcrypt(bool encrypt, void *aes_key, void *aes_iv, void *buffer, size_t inlen);
int aes256_gcm_memcrypt(bool encrypt, void *aes_key, void *aes_iv, void *tag, void *buffer, size_t inlen);
int key_derivation_function(void *hash, void *passphrase, size_t passphrase_len, void *salt, size_t salt_len);
uint32_t calc_crc16(uint8_t *addr, uint32_t num);
int heap_stack_distance();

#ifdef __cplusplus
}
#endif

#if 0
typedef struct _hmac_inner_outer
{
  uint8_t inner_mac[KEYMANAGER_HASHLEN];
  uint8_t outer_mac[KEYMANAGER_HASHLEN];
} hmac_inner_outer;

int hmac_compute_keys(BLAKE2s &inner_mac, BLAKE2s &outer_mac, const uint8_t *derived_key);
int hmac_compute_inner_outer_hash(BLAKE2s &inner_mac, BLAKE2s &outer_mac, hmac_inner_outer *hic);
#endif

#endif  /* _CRYPTOTOOL_H */

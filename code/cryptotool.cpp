/* BASE 64 encoding */

#include <stdlib.h>
#include <string.h>
#include <BLAKE2s.h>
#include <AES.h>
#include <CTR.h>
#include "cryptotool.h"

#ifdef __cplusplus
extern "C" {
#endif

static const char encoding_table[] = { 
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
            'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
            'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
            'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
            'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
            'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
            'w', 'x', 'y', 'z', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '+', '/' };


static const uint8_t decoding_table[256] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3e, 0xFF, 0xFE, 0xFF, 0x3f,
            0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
            0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
            0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

int is_base64_char(char ch)
{
	return (decoding_table[(uint8_t)ch] != 0xFF);
}

int base64_encode(base64_readdata rd, void *vrd, base64_writedata wd, void *vwd)
{
			int len;
            do {
				int ch1, ch2, ch3;
				len = 0;
				ch1 = rd(vrd);
				if (ch1 < 0) ch1 = 0;
					else
					{
						len++;
						ch2 = rd(vrd);
						if (ch2 < 0) ch2 = 0;
						else 
						{	len++;
							ch3 = rd(vrd);
							if (ch3 < 0) ch3 = 0;
								else len++;
						}
					}
                uint32_t triple = ((((uint32_t)ch1) << 0x10) + 
				                   (((uint32_t)ch2) << 0x08) + 
								    ((uint32_t)ch3));
				if (len > 0)
				{
					wd(encoding_table[(triple >> 3 * 6) & 0x3F],vwd);
					wd(encoding_table[(triple >> 2 * 6) & 0x3F],vwd);
					wd(len > 1 ? encoding_table[(triple >> 1 * 6) & 0x3F] : '=',vwd);
					wd(len > 2 ? encoding_table[(triple >> 0 * 6) & 0x3F] : '=',vwd);
				}
            } while (len>2);
			return 1;
};

int base64_decode(base64_readdata rd, void *vrd, base64_writedata wd, void *vwd)
{        
			int len;
            do {
				int ch1, ch2, ch3, ch4;
				len = 0;
				ch1 = rd(vrd);
				if ((ch1 >= 0) && (ch1 != '=')) len++;
				ch2 = rd(vrd);
				if ((ch2 >= 0) && (ch2 != '=')) len++;
				ch3 = rd(vrd);
				if ((ch3 >= 0) && (ch3 != '=')) len++;
				ch4 = rd(vrd);
				if ((ch4 >= 0) && (ch4 != '=')) len++;
                uint32_t triple = (((uint32_t)decoding_table[(uint8_t)ch1]) << 3 * 6)
                    + (((uint32_t)decoding_table[(uint8_t)ch2]) << 2 * 6)
                    + (((uint32_t)decoding_table[(uint8_t)ch3]) << 1 * 6)
                    + (((uint32_t)decoding_table[(uint8_t)ch4]) << 0 * 6);
				if (len>1) 
				{	
					wd((triple >> 2 * 8) & 0xFF,vwd);
					if (len>2) 
					{
						wd((triple >> 1 * 8) & 0xFF,vwd);
						if (len>3) wd((triple >> 0 * 8) & 0xFF,vwd);
					}
				}
            } while (len>3);
			return 1;
}

int ctblake2s( void *out, size_t outlen, const void *in, size_t inlen, const void *key, size_t keylen )
{
	BLAKE2s blake2s;
	if (keylen > 0)
		blake2s.reset(key, keylen, outlen);
	else
		blake2s.reset(outlen);
	blake2s.update(in, inlen);
	blake2s.finalize(out, outlen);
	return 0;
}

int key_derivation_function(void *hash, void *passphrase, size_t passphrase_len, void *salt, size_t salt_len)
{
	int n;
	uint8_t new_salt[KEYMANAGER_HASHLEN];
	ctblake2s(hash, KEYMANAGER_HASHLEN, passphrase, passphrase_len, salt, salt_len);
	for (n=0;n<KEY_DERIVATION_HASHES;n++)
	{
		memcpy((void *)new_salt, hash, sizeof(new_salt));
		ctblake2s(hash, KEYMANAGER_HASHLEN, (void *)passphrase, passphrase_len, new_salt, sizeof(new_salt));
	}
	return 0;
}

int aes256_memcrypt(bool encrypt, void *aes_key, void *aes_iv, void *buffer, size_t inlen)
{
	CTR<AES256> cipher;
    cipher.setKey((const uint8_t *)aes_key, cipher.keySize());
    cipher.setIV((const uint8_t *)aes_iv, cipher.ivSize());
	if (encrypt)
	    cipher.encrypt((uint8_t *)buffer,(uint8_t *)buffer, inlen);
	else
	    cipher.decrypt((uint8_t *)buffer,(uint8_t *)buffer, inlen);
	return 0;
}

void * __builtin_return_address (unsigned int level);
char* sbrk(int incr);

int heap_stack_distance()
{
  char *v = (char *)__builtin_frame_address (0);
  char *v2 = sbrk(0);
  return ((uint32_t)v)-((uint32_t)v2);
}

#define poly 0x1021

uint32_t calc_crc16(uint8_t *addr, uint32_t num)
{
  int i;
  uint32_t crc = 0;
  for (; num>0; num--)              
    {
    crc = crc ^ (((uint32_t)*addr++) << 8);      
    for (i=0; i<8; i++)              
      {
      crc = crc << 1;                
      if (crc & 0x10000)             
        crc = (crc ^ poly) & 0xFFFF; 
                                    
      }                              
    }                                
    return(crc);                     
}

#ifdef __cplusplus
}
#endif

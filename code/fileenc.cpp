/*
 * Copyright (c) 2020 Daniel Marks

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
 */

#include "Arduino.h"
#include <stdarg.h>
#include <ff.h>
#include <BLAKE2s.h>
#include <AES.h>
#include <CTR.h>
#include "consoleio.h"
#include "fileop.h"
#include "fileenc.h"
#include "keymanager.h"
#include "cryptotool.h"
#include "random.h"

#define USE_MINIPRINTF

#ifdef USE_MINIPRINTF
#include "mini-printf.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif  

int fileenc_check_key_selected(void)
{
  if (current_key_private.entry_type == KEY_TYPE_AES) return 1;
  if (current_key_private.entry_type == KEY_TYPE_ECDH_PRIVATE)
  {
    if (current_key_public.entry_type == KEY_TYPE_ECDH_PUBLIC) return 1;
      else
    {
      file_report_error("No public key selected");
      return 0;
    }
  }
  file_report_error("No private or symmetric key selected");
  return 0;
}

int fileenc_hash_derived_key(void *hash, void *secret, size_t secretlen, void *salt, size_t saltlen)
{
  key_derivation_function((void *)hash, (void *)secret, secretlen, (void *)salt, saltlen);
}

#define FILEENC_READBUF_SIZE (AES_BLOCKLEN*64)

#define FILEENC_WRITEBUF_SIZE 36

typedef struct _fileenc_readbuf
{
  CTR<AES256> *read_cipher;
  BLAKE2s *inner_mac;
  BLAKE2s *outer_mac;
  FIL read_file;
  uint8_t  read_buf[FILEENC_READBUF_SIZE];
  uint16_t read_filled;
  uint16_t read_curpos;
  FSIZE_t read_progress;
  FIL write_file;
  uint8_t  write_buf[FILEENC_WRITEBUF_SIZE];
  uint16_t write_curpos;  
  fileenc_total_header fth;
} fileenc_state;

int fileenc_base64_readdata(void *v)
{
  fileenc_state *fr = (fileenc_state *)v;
  if (fr->read_curpos >= fr->read_filled)
  {
    UINT br;
    FRESULT res = f_read(&fr->read_file,fr->read_buf,FILEENC_READBUF_SIZE,&br);
    if ((res != FR_OK) || (br == 0)) return -1;
    fr->read_cipher->encrypt(fr->read_buf, fr->read_buf, br);
    fr->inner_mac->update(fr->read_buf, br);
    fr->read_filled = br;
    fr->read_curpos = 0;
    if ((f_tell(&fr->read_file)-fr->read_progress) >= FILEENC_DISPLAY_INCREMENT)
    {
      console_puts("Reading ");
      console_printuint(f_tell(&fr->read_file));
      console_putch('/');
      console_printuint(f_size(&fr->read_file));
      console_printcrlf();
      fr->read_progress = f_tell(&fr->read_file);
    }
  }
  return fr->read_buf[fr->read_curpos++];
}

int fileenc_base64_writedata(int c, void *v)
{
  fileenc_state *fw = (fileenc_state *)v;

  if (c >= 0)
    fw->write_buf[fw->write_curpos++] = c; 
  if ((fw->write_curpos == FILEENC_WRITEBUF_SIZE) || (c < 0))
  {
    UINT br;
    f_write(&fw->write_file,fw->write_buf,fw->write_curpos,&br);
    f_write(&fw->write_file,"\n",1,&br);
    fw->write_curpos = 0;
  }
}

const char *fileenc_filename(const char *c)
{
  const char *d = &c[strlen_n(c)];
  while (d>c)
  {
    d--;
    if ((*d == '/') || (*d == ':')) return (d+1);
  }
  return d;
}

void fileenc_encrypt_state(fileenc_state *fs)
{
  char filename_plaintext[256];
  if (!fileenc_check_key_selected()) return;
  {
    char filename_ciphertext[256];
    if (!file_select_plaintext("Select plaintext file", 0, filename_plaintext, sizeof(filename_plaintext)-1)) return;
    if (!file_select_ciphertext("Select directory for ciphertext", 1, filename_ciphertext, sizeof(filename_ciphertext)-1)) return;
    if (!file_enter_filename("Filename for ciphertext output:", filename_ciphertext, sizeof(filename_ciphertext)-1)) return;
    FRESULT fres;
    fres = f_open(&fs->read_file, filename_plaintext, FA_READ);
    if (fres != FR_OK)
    {
      file_report_error("Could not open plaintext file");
      return;
    }
    fres = f_open(&fs->write_file, filename_ciphertext, FA_WRITE | FA_CREATE_NEW);
    if (fres != FR_OK)
    {
      file_report_error("Could not create ciphertext file");
      f_close(&fs->read_file);
      return;    
    }
  }
  console_clrscr();
  console_puts("Converting file:\r\n");
  {
    uint8_t secret[KEYMANAGER_MAX_SECRET_LEN];
    uint8_t hash[KEYMANAGER_HASHLEN];
    hmac_inner_outer hic;
    int secretlen;
    if (keymanager_compute_secret(secret, &secretlen))
    {
      CTR<AES256> read_cipher;
      BLAKE2s inner_mac;
      BLAKE2s outer_mac;

      memset((void *)&fs->fth,'\000',sizeof(fs->fth));
      randomness_get_whitened_bits(fs->fth.salt1, sizeof(fs->fth.salt1));
      randomness_get_whitened_bits(fs->fth.iv1, sizeof(fs->fth.iv1));
      randomness_get_whitened_bits(fs->fth.salt2, sizeof(fs->fth.salt2));
      randomness_get_whitened_bits(fs->fth.iv2, sizeof(fs->fth.iv2));

      fs->read_cipher = &read_cipher;
      fs->inner_mac = &inner_mac;
      fs->outer_mac = &outer_mac;

      fileenc_hash_derived_key((void *)hash, (void *)secret, secretlen, (void *)fs->fth.salt1, sizeof(fs->fth.salt1));
      fs->fth.fhpu.fhp.id   =         FILEENC_EXPORT_ID;
      fs->fth.fhpu.fhp.entry_type =   current_key_private.entry_type;
      fs->fth.fhpu.fhp.vers =         FILEENC_EXPORT_VERSION;
      fs->fth.fhpu.fhp.len =          sizeof(fs->fth.fhpu.fhp);
      fs->fth.fhpu.fhp.totallen =     sizeof(fs->fth.fhpu);
      fs->fth.fhpu.fhp.file_length =  f_size(&fs->read_file);
      strcpy_n(fs->fth.fhpu.fhp.filename, fileenc_filename(filename_plaintext), sizeof(fs->fth.fhpu.fhp.filename)-1);
      aes256_memcrypt(1, (void *)hash, (void *)fs->fth.iv1, (void *)&fs->fth.fhpu, sizeof(fs->fth.fhpu));
      hmac_compute_keys(*fs->inner_mac, *fs->outer_mac, (const uint8_t *)hash);
      fs->inner_mac->update((const uint8_t *) &fs->fth.fhpu, sizeof(fs->fth.fhpu));
      hmac_compute_inner_outer_hash(*fs->inner_mac, *fs->outer_mac, &fs->fth.hmac_first_block);
      file_write_block(&fs->write_file, "PARANOIABOX-FILEHEADER", (void *)&fs->fth, sizeof(fs->fth));

      fileenc_hash_derived_key((void *)hash, (void *)secret, secretlen, (void *)fs->fth.salt2, sizeof(fs->fth.salt2));
      file_write_header(&fs->write_file,"PARANOIABOX-PAYLOAD",0);
      fs->read_filled = fs->read_curpos = 0;
      fs->write_curpos = 0;
      fs->read_progress = 0;
      fs->read_cipher->setKey((const uint8_t *)hash, fs->read_cipher->keySize());
      fs->read_cipher->setIV((const uint8_t *)fs->fth.iv2, fs->read_cipher->ivSize());
      hmac_compute_keys(*fs->inner_mac, *fs->outer_mac, (const uint8_t *)hash);
      base64_encode(fileenc_base64_readdata,(void *)fs,  fileenc_base64_writedata, (void *)fs);
      fileenc_base64_writedata(-1, (void *)fs); 
      hmac_compute_inner_outer_hash(*fs->inner_mac, *fs->outer_mac, &hic);
      file_write_header(&fs->write_file,"PARANOIABOX-PAYLOAD",1);
      file_write_block(&fs->write_file, "PARANOIABOX-ENDBLOCK", (void *)&hic, sizeof(hic));      
    } else file_report_error("Bad secret key");
  }  
  f_close(&fs->write_file);
  f_close(&fs->read_file);
}

void fileenc_encrypt(void)
{
  fileenc_state *fs = (fileenc_state *)malloc(sizeof(fileenc_state));
  if (fs == NULL) return;
  fileenc_encrypt_state(fs);
  free(fs);
}


#define FILEDEC_READBUF_SIZE 512

#define FILEDEC_WRITEBUF_SIZE (AES_BLOCKLEN*64)

typedef struct _filedec_readbuf
{
  FIL          read_file;
  FSIZE_t      read_last_block;
  uint8_t      read_buf[FILEDEC_READBUF_SIZE];
  uint16_t     read_filled;
  uint16_t     read_curpos;
  FIL          write_file;
  uint8_t      write_buf[FILEDEC_WRITEBUF_SIZE];
  uint16_t     write_curpos; 
  FSIZE_t      write_progress;
  FSIZE_t      write_total;
  CTR<AES256>  *write_cipher;
  BLAKE2s      *inner_mac;
  BLAKE2s      *outer_mac;
  fileenc_total_header fth;
} filedec_state;

int filedec_base64_readdata(void *v)
{
  filedec_state *fr = (filedec_state *)v;
  for (;;)
  {
    if (fr->read_curpos >= fr->read_filled)
    {
      UINT br;
      fr->read_last_block = f_tell(&fr->read_file);
      FRESULT res = f_read(&fr->read_file,fr->read_buf,FILEDEC_READBUF_SIZE,&br);
      if ((res != FR_OK) || (br == 0)) return -1;
      fr->read_filled = br;
      fr->read_curpos = 0;
    }
    uint8_t ch = fr->read_buf[fr->read_curpos++];
    if (ch == '-')
    {
      f_lseek(&fr->read_file, fr->read_last_block);
      return -1;
    }
    if (is_base64_char(ch)) return ch;
  }
}

int filedec_base64_writedata(int c, void *v)
{
  filedec_state *fw = (filedec_state *)v;

  if (c >= 0) 
    fw->write_buf[fw->write_curpos++] = c; 
  if ((fw->write_curpos == FILEDEC_WRITEBUF_SIZE) || (c < 0))
  {
    UINT br;
    fw->inner_mac->update(fw->write_buf, fw->write_curpos);
    fw->write_cipher->decrypt(fw->write_buf, fw->write_buf, fw->write_curpos);
    f_write(&fw->write_file,fw->write_buf,fw->write_curpos,&br);
    fw->write_curpos = 0;
    if ((f_tell(&fw->write_file)-fw->write_progress) >= FILEENC_DISPLAY_INCREMENT)
    {
      console_puts("Writing ");
      console_printuint(fw->write_progress);
      console_putch('/');
      console_printuint(fw->write_total);
      console_printcrlf();
      fw->write_progress = f_tell(&fw->write_file);
    }
  }
  return 0;
}

void fileenc_decrypt_state(filedec_state *fs)
{
  FSIZE_t destroy_output = 0;
  if (!fileenc_check_key_selected()) return;
  {
    char filename_ciphertext[256];
    char filename_plaintext[256];
    if (!file_select_ciphertext("Select ciphertext file", 0, filename_ciphertext, sizeof(filename_ciphertext)-1)) return;
    if (!file_select_plaintext("Select directory for plaintext", 1, filename_plaintext, sizeof(filename_plaintext)-1)) return;
    if (!file_enter_filename("Filename for plaintext output:", filename_plaintext, sizeof(filename_plaintext)-1)) return;
    FRESULT fres;
    fres = f_open(&fs->read_file, filename_ciphertext, FA_READ);
    if (fres != FR_OK)
    {
      file_report_error("Could not open ciphertext file");
      return;
    }
    fres = f_open(&fs->write_file, filename_plaintext, FA_WRITE | FA_CREATE_NEW);
    if (fres != FR_OK)
    {
      file_report_error("Could not create plaintext file");
      f_close(&fs->read_file);
      return;    
    } 
  }
  console_clrscr();
  console_puts("Converting file:\r\n");
  {
    uint8_t secret[KEYMANAGER_MAX_SECRET_LEN];
    uint8_t hash[KEYMANAGER_MAX_SECRET_LEN];
    hmac_inner_outer hic;
    int secretlen;
    if (keymanager_compute_secret(secret, &secretlen))
    {
      CTR<AES256>  write_cipher;
      BLAKE2s      inner_mac;
      BLAKE2s      outer_mac;

      memset((void *)&fs->fth,'\000',sizeof(fs->fth));
      fs->write_cipher = &write_cipher;
      fs->inner_mac = &inner_mac;
      fs->outer_mac = &outer_mac;
      if(file_read_block(&fs->read_file, "PARANOIABOX-FILEHEADER", (void *)&fs->fth, sizeof(fs->fth)))
      {
         fileenc_hash_derived_key((void *)hash, (void *)secret, secretlen, (void *)fs->fth.salt1, sizeof(fs->fth.salt1));
         hmac_compute_keys(*fs->inner_mac, *fs->outer_mac, (const uint8_t *)hash);      
         fs->inner_mac->update((const uint8_t *) &fs->fth.fhpu, sizeof(fs->fth.fhpu));
         hmac_compute_inner_outer_hash(*fs->inner_mac, *fs->outer_mac, &hic);
         if (memcmp(&hic, &fs->fth.hmac_first_block, sizeof(hic)) == 0)
         {  
           aes256_memcrypt(0, (void *)hash, (void *)fs->fth.iv1, (void *)&fs->fth.fhpu, sizeof(fs->fth.fhpu));
           if ((fs->fth.fhpu.fhp.id == FILEENC_EXPORT_ID) && (fs->fth.fhpu.fhp.vers == FILEENC_EXPORT_VERSION) &&
               (fs->fth.fhpu.fhp.len == sizeof(fs->fth.fhpu.fhp)) && (fs->fth.fhpu.fhp.entry_type == current_key_private.entry_type))
           {
              fs->fth.fhpu.fhp.filename[sizeof(fs->fth.fhpu.fhp.filename)-1] = '\000';
              fileenc_hash_derived_key((void *)hash, (void *)secret, secretlen, (void *)fs->fth.salt2, sizeof(fs->fth.salt2));
              if (file_skip_header(&fs->read_file,"PARANOIABOX-PAYLOAD",0))
              {
                fs->read_filled = fs->read_curpos = 0;
                fs->write_curpos = 0;
                fs->write_progress = 0;
                fs->write_total = fs->fth.fhpu.fhp.file_length;
                fs->write_cipher->setKey((const uint8_t *)hash, fs->write_cipher->keySize());
                fs->write_cipher->setIV((const uint8_t *)fs->fth.iv2, fs->write_cipher->ivSize());
                hmac_compute_keys(*fs->inner_mac, *fs->outer_mac, (const uint8_t *)hash);      
                base64_decode(filedec_base64_readdata,(void *)fs,  filedec_base64_writedata, (void *)fs);
                filedec_base64_writedata(-1, (void *)fs); 
                hmac_compute_inner_outer_hash(*fs->inner_mac, *fs->outer_mac, &hic);
                if (f_tell(&fs->write_file) == fs->fth.fhpu.fhp.file_length)
                {
                  if (file_skip_header(&fs->read_file,"PARANOIABOX-PAYLOAD",1))
                  {
                    hmac_inner_outer hicverify;
                    if(file_read_block(&fs->read_file, "PARANOIABOX-ENDBLOCK", (void *)&hicverify, sizeof(hicverify)))
                    { 
                      if (memcmp((void *)&hic,(void *)&hicverify,sizeof(hic)) == 0)
                      {
                         destroy_output = fs->fth.fhpu.fhp.file_length;
                      }
                      else file_report_error("Payload HMAC is invalid");
                    } else file_report_error("End block not found");
                  } else file_report_error("End of payload not found");                
                } else file_report_error("Payload length does not match header");
              } else file_report_error("No payload found");
           } else file_report_error("Wrong version of header");
         } else file_report_error("Header HMAC is invalid");
      } else file_report_error("Could not read file header");
    }  else file_report_error("Bad secret key");
  }
  f_lseek(&fs->write_file,destroy_output);
  f_truncate(&fs->write_file);
  f_close(&fs->write_file);
  f_close(&fs->read_file);
}

void fileenc_decrypt(void)
{
  filedec_state *fs = (filedec_state *)malloc(sizeof(filedec_state));
  if (fs == NULL) return;
  fileenc_decrypt_state(fs);
  free(fs);
}

#ifdef __cplusplus
}
#endif  

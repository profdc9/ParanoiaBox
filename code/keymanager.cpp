/*
   Copyright (c) 2018 Daniel Marks

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
#include <RNG.h>
#include <Curve25519.h>
#include "consoleio.h"
#include "editor.h"
#include "fileop.h"
#include "keymanager.h"
#include "random.h"
#include "flashstruct.h"

#define USE_MINIPRINTF

#ifdef USE_MINIPRINTF
#include "mini-printf.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

static uint8_t active_key;
static uint8_t invalidate_passphrase;
static uint8_t keyflash_changed;

key_entry current_key_private;
key_entry current_key_public;

static uint8_t passphrase_hash[KEYMANAGER_HASHLEN];
static key_storage *ks = NULL;

int keymanager_compute_secret(uint8_t *secret, int *secretlen)
{
  uint8_t temp_private_key[KEYMANAGER_PUBLICKEY_LEN];
  if (current_key_private.entry_type == KEY_TYPE_AES)
  {
    *secretlen = sizeof(current_key_private.ksu.sym.symmetric_key);
    memcpy((void *)secret, (void *)current_key_private.ksu.sym.symmetric_key, sizeof(current_key_private.ksu.sym.symmetric_key));
    return 1;
  }
  if ((current_key_private.entry_type != KEY_TYPE_ECDH_PRIVATE) && (current_key_public.entry_type != KEY_TYPE_ECDH_PUBLIC)) return 0;
  *secretlen = sizeof(current_key_public.ksu.pub.public_key);
  memcpy((void *)secret, (void *) &current_key_public.ksu.pub.public_key, sizeof(current_key_public.ksu.pub.public_key));
  memcpy((void *)temp_private_key, (void *)current_key_private.ksu.priv.private_key, KEYMANAGER_PUBLICKEY_LEN);
  return Curve25519::dh2(secret, temp_private_key);
}

void keymanager_initialize(void)
{
  active_key = 0;
  current_key_public.entry_type = current_key_private.entry_type = KEY_TYPE_EMPTY;
}

void keymanager_read_storage(void)
{
  void *vp[1];
  int b[1];

  vp[0] = (void *)ks;
  b[0] = sizeof(key_storage);
  readflashstruct((void *)KEYMANAGER_FLASH_STORAGE_ADDRESS, 1, vp, b);
}

void keymanager_write_storage(void)
{
  void *vp[1];
  int b[1];

  vp[0] = (void *)ks;
  b[0] = sizeof(key_storage);
  writeflashstruct((void *)KEYMANAGER_FLASH_STORAGE_ADDRESS, 1, vp, b);
}

void keymanager_key_derivation_function(const char *passphrase, uint8_t *hash)
{
  key_derivation_function((void *)hash, (void *)passphrase, strlen_n(passphrase), (void *)ks->salt, sizeof(ks->salt));
}

void keymanager_display_message(const char *message)
{
  console_clrscr();
  console_gotoxy(1, 5);
  console_puts(message);
}

void keymanager_enter_passphrase(const char *prompt, char *passphrase)
{
  keymanager_display_message(prompt);
  console_getstring(passphrase, KEY_MAX_PASSPHRASE_LENGTH - 1, 7, 1, 35);
}

int keymanager_initialize_database()
{
  uint8_t bits[64];
  char passphrase[KEY_MAX_PASSPHRASE_LENGTH];
  memset(ks, '\000', sizeof(key_storage));
  keymanager_enter_passphrase("Enter new database passphrase:", passphrase);
  randomness_get_whitened_bits(bits, sizeof(bits));
  memcpy((void *)ks->salt, (void *)bits, sizeof(ks->salt));
  randomness_get_whitened_bits(bits, sizeof(bits));
  memcpy((void *)ks->key_entry_iv, (void *)bits, sizeof(ks->key_entry_iv));
  keymanager_key_derivation_function(passphrase, passphrase_hash);
  return 1;
}

void keymanager_recalculate_hashes(void)
{
  uint8_t keh[KEYMANAGER_HASHLEN];
  uint8_t bits[64];

  randomness_get_whitened_bits(bits, sizeof(bits));
  memcpy((void *)ks->key_entry_iv, (void *)bits, sizeof(ks->key_entry_iv));
  aes256_gcm_memcrypt(1, (void *)passphrase_hash, (void *)ks->key_entry_iv, (void *)ks->key_entry_tag, (void *)&ks->keu, sizeof(ks->keu));
}

int keymanager_get_passphrase(void)
{
  uint8_t keh[KEYMANAGER_HASHLEN];

  if (!active_key)
  {
    char passphrase[KEY_MAX_PASSPHRASE_LENGTH];
    keymanager_enter_passphrase("Enter passphrase:", passphrase);
    keymanager_key_derivation_function(passphrase, passphrase_hash);
  }
  if (!aes256_gcm_memcrypt(0, (void *)passphrase_hash, (void *)ks->key_entry_iv, (void *)ks->key_entry_tag, (void *)&ks->keu, sizeof(ks->keu)))
  {
    char destructcode[13];
    console_gotoxy(1, 15);
    console_puts("Invalid passphrase. To destroy any\r\nkeys and reinitialize database,\r\nenter '000DESTRUCT0' here,\r\notherwise press ENTER:");
    console_getstring(destructcode, sizeof(destructcode) - 1, 20, 1, 20);
    if ((strcmp(destructcode, "000DESTRUCT0")) || (!keymanager_initialize_database())) return 0;
    keyflash_changed = 1;
    return 1;
  }
  active_key = 1;
  return 1;
}

#define KEYMANAGER_SELECT_DISPLAY 16

const char *keymanager_types[] = { "EMPTY", "AES", "PRIVATE", "PUBLIC" };

void keymanager_display_key(int entno, key_entry *ke)
{
  if (entno >= 0)
  {
    console_printint(entno+1);
    console_puts(". ");
  }
  console_puts(keymanager_types[ke->entry_type]);
  console_puts(" ");
  console_puts(ke->description);
}

int keymanager_erase_this_key(const char *message, int entno, key_entry *ke, char *description, int len)
{
  keymanager_display_message("Erase this key?\r\n\r\n");
  keymanager_display_key(entno, ke);
  console_gotoxy(1, 4);
  console_puts(message);
  if (console_yes(18)) return 1;

  if (description != NULL)
  {
    keymanager_display_message("Enter new key name:");
    console_getstring(description,len,7,1,30);
  }
  return 0;
}

void keymanager_new_private_key(int entno, key_entry *ke)
{
  uint8_t private_key[KEYMANAGER_PRIVATEKEY_LEN];
  uint8_t public_key[KEYMANAGER_PUBLICKEY_LEN];
  
  char description[KEY_DESCRIPTION_LEN];
  if (keymanager_erase_this_key("Generate new private/public key", entno, ke, description, sizeof(description)-1)) return;

  random_stir_in_entropy();
  Curve25519::dh1(public_key, private_key);
  memset((void *)&ke->ksu,'\000',sizeof(ke->ksu));
  memcpy((void *)ke->description,(void *)description,sizeof(ke->description));
  memcpy((void *)ke->ksu.priv.private_key, (void *)private_key, sizeof(ke->ksu.priv.private_key));
  memcpy((void *)ke->ksu.priv.public_key, (void *)public_key, sizeof(ke->ksu.priv.public_key));
  ke->entry_type = KEY_TYPE_ECDH_PRIVATE;
  keyflash_changed = 1;
}

void keymanager_export_public_key(int entno, key_entry *ke)
{
  FIL f;
  {
    char filename_key[256];
    if (ke->entry_type != KEY_TYPE_ECDH_PRIVATE) return;
    if (!file_select_ciphertext("Select directory for exported key", 1, filename_key, sizeof(filename_key)-1)) return;
    if (!file_enter_filename("Filename for exported key:", filename_key, sizeof(filename_key)-1)) return;
    FRESULT fres = f_open(&f, filename_key, FA_WRITE | FA_CREATE_NEW);
    if (fres != FR_OK)
    {
      file_report_error("Could not key file");
      f_close(&f);
      return;    
    }
  }
  {
    key_export_public kep;
    memset((void *)&kep,'\000',sizeof(kep));
    kep.id = KEY_EXPORT_ID;
    kep.vers = KEY_EXPORT_VERSION;
    kep.len = sizeof(kep);
    memcpy((void *)kep.description, (void *)ke->description, sizeof(ke->description));  
    memcpy((void *)kep.public_key, (void *)ke->ksu.priv.public_key, sizeof(ke->ksu.priv.public_key));
    kep.crc16 = calc_crc16((uint8_t *)&kep,sizeof(kep));
    file_write_block(&f, "PARANOIABOX-PUBLICKEY", (void *)&kep, sizeof(kep));
    f_close(&f);
  }
}

void keymanager_import_public_key(int entno, key_entry *ke)
{
  FIL f;
  if (keymanager_erase_this_key("Import public key", entno, ke, NULL, 0)) return;
  {
    char filename_key[256];
    if (!file_select_ciphertext("Select filename of imported key", 0, filename_key, sizeof(filename_key)-1)) return;
    FRESULT fres = f_open(&f, filename_key, FA_READ);
    if (fres != FR_OK)
    {
      file_report_error("Could not key file");
      f_close(&f);
      return;    
    }
  }
  {
    key_export_public kep;
    memset((void *)&kep,'\000',sizeof(kep));
    if (file_read_block(&f, "PARANOIABOX-PUBLICKEY", (void *)&kep, sizeof(kep)))
    {
      uint16_t read_crc16 = kep.crc16;
      kep.crc16 = 0;
      uint16_t compare_crc16 = calc_crc16((uint8_t *)&kep,sizeof(kep));
      if (read_crc16 == compare_crc16)
      {
        if ((kep.id == KEY_EXPORT_ID) && (kep.vers == KEY_EXPORT_VERSION) && (kep.len == sizeof(kep)))
        {
          memset((void *)ke, '\000', sizeof(*ke));
          memcpy((void *)ke->description, (void *)kep.description, sizeof(ke->description));  
          memcpy((void *)ke->ksu.pub.public_key, (void *)kep.public_key, sizeof(ke->ksu.priv.public_key));
          ke->entry_type = KEY_TYPE_ECDH_PUBLIC;
          keyflash_changed = 1;
        } else
          file_report_error("Version on file is invalid");
      } else
        file_report_error("CRC on file is invalid");
    } else
      file_report_error("Could not read key file");
  }
  f_close(&f);
}

void keymanager_new_passphrase_key(int entno, key_entry *ke)
{
  int crc16;
  char s[15];
  char passphrase[KEY_MAX_PASSPHRASE_LENGTH];
  uint8_t keh[KEYMANAGER_HASHLEN];
  char description[KEY_DESCRIPTION_LEN];
  if (keymanager_erase_this_key("Generate new passphrase key", entno, ke, description, sizeof(description)-1)) return;
  
  keymanager_enter_passphrase("Enter new key passphrase:", passphrase);
  console_gotoxy(1,16);
  console_puts("Name:\r\n");
  console_puts(description);
  crc16 = calc_crc16((uint8_t *)passphrase,strlen_n(passphrase));
  mini_snprintf(s, sizeof(s)-1,"%04X", crc16);
  console_puts("\r\nCRC16 of passphrase: ");
  console_puts(s);
  if (console_yes(19)) return;

  memset((void *)&ke->ksu,'\000',sizeof(ke->ksu));
  key_derivation_function((void *)keh, (void *)passphrase, strlen_n(passphrase), NULL, 0);
  memcpy((void *)ke->description,(void *)description,sizeof(ke->description));
  memcpy((void *)ke->ksu.sym.symmetric_key, (void *)keh, sizeof(ke->ksu.sym.symmetric_key));
  ke->entry_type = KEY_TYPE_AES;
  keyflash_changed = 1;
}

void keymanager_select_key(void)
{
  int key_no = 0;
  int top_key = 0;
  for (;;)
  {
    int n;
    console_clrscr();
    console_puts("Select key:");
    key_entry *ke; 
    if (top_key > (KEY_NUMBER-KEYMANAGER_SELECT_DISPLAY))
      top_key = KEY_NUMBER-KEYMANAGER_SELECT_DISPLAY;
    if (top_key < 0)
      top_key = 0;
    for (n=0;n<KEYMANAGER_SELECT_DISPLAY;n++)
    {
      int entno = n + top_key;
      key_entry *ke = &ks->keu.kes[entno];
      if (entno == key_no) console_highvideo();
      console_gotoxy(1,n+3);
      keymanager_display_key(entno,ke);
      console_lowvideo();
    }
    console_gotoxy(1,20);
    console_puts("Q-quits, K-invalidate passphrase\r\nUp/Down/Enter select key\r\nN-New Passphrase Key, P-New Private Key\r\nI-Import Public Key, E-Export Public Key");
    int ch = toupper(console_getch());
    ke = &ks->keu.kes[key_no];
    if (ch == 'Q') break;
    if (ch == 'K')
    {
      invalidate_passphrase = 1;
      break;
    }
    if (ch == 'A')
    {
      if (key_no > 0) key_no--;
      if (top_key > key_no) top_key = key_no;
    }
    if (ch == 'B')
    {
      if (key_no < (KEY_NUMBER-1)) key_no++;
      if (top_key < (key_no - (KEYMANAGER_SELECT_DISPLAY-1))) top_key = (key_no - (KEYMANAGER_SELECT_DISPLAY-1));
    }
    if (ch == '\r')
    {
      if ((ke->entry_type == KEY_TYPE_AES) || (ke->entry_type == KEY_TYPE_ECDH_PUBLIC) || (ke->entry_type == KEY_TYPE_ECDH_PRIVATE))
      {
        if (ke->entry_type == KEY_TYPE_ECDH_PUBLIC)
          current_key_public = *ke;
        else
          current_key_private = *ke;
        console_clrscr();
        console_gotoxy(1,5);
        keymanager_display_message("Selected symmetric/private key:\r\n");
        keymanager_display_key(-1,&current_key_private);
        console_puts("\r\n\r\nSelected public key:\r\n");
        keymanager_display_key(-1,&current_key_public);
        console_press_space();
      }
    }
    if (ch == 'N')
      keymanager_new_passphrase_key(key_no,ke);
    if (ch == 'P')
      keymanager_new_private_key(key_no,ke);
    if (ch == 'E')
      keymanager_export_public_key(key_no,ke);
    if (ch == 'I')
      keymanager_import_public_key(key_no,ke);
    if (ch == 'Z')
    {
      uint8_t secret[32];
      int secretlen;
      keymanager_compute_secret(secret,&secretlen);
    }
  }
}

void keymanager(void)
{
  ks = (key_storage *)malloc(sizeof(key_storage));
  if (ks == NULL) return;
  keymanager_read_storage();
  keyflash_changed = 0;
  invalidate_passphrase = 0;
  if (keymanager_get_passphrase())
  {
    keymanager_select_key();
    if (keyflash_changed)
    {
      keymanager_recalculate_hashes();
      keymanager_write_storage();
    }
  }
  memset(ks, '\000', sizeof(key_storage));
  free(ks);
  ks = NULL;
  if (invalidate_passphrase)
  {
    memset(passphrase_hash,'\000',sizeof(passphrase_hash));
    memset((void *)&current_key_private,'\000',sizeof(current_key_private));
    active_key = 0;
  }
}

#ifdef __cplusplus
}
#endif

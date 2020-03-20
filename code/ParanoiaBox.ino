#include <Arduino.h>
#include <malloc.h>
#include <SPI.h>
#include <ff.h>

#include "consoleio.h"
#include "debugmsg.h"
#include "mini-printf.h"
#include "editor.h"
#include "random.h"
#include "fileop.h"
#include "keymanager.h"
#include "fileenc.h"

void setup() {
  digitalWrite(PB8, HIGH); // Put CS lines for card HIGH right away to avoid confusion
  digitalWrite(PB9, HIGH);
  pinMode(PB8, OUTPUT);
  pinMode(PB9, OUTPUT);
  Serial.begin(115200);
  random_initialize();
  console_init();
  keymanager_initialize();
}

const char mainmenu[] =
  "\r\n\r\nM - Mount Drives\r\n\
K - Key manager\r\n\
R - Randomness Test\r\n\
T - Text Editor\r\n\
N - Text Edit New File\r\n\
V - View File\r\n\
E - Encrypt File\r\n\
D - Decrypt File\r\n\
X - Delete File\r\n\
\r\n\r\nOption: ";

const char mainmenuoptions[] = "MKRTNVEDZX";

void loop()
{
  console_clrscr();
  console_highvideo();
  console_puts("ParanoiaBox");
  console_lowvideo();
  console_puts(" by D. Marks\r\n");
  console_puts("Ciphertext card ");
  console_highvideo();
  console_puts(fs0_mounted ? "present" : "absent");
  console_lowvideo();
  console_puts("\r\nPlaintext card ");
  console_highvideo();
  console_puts(fs1_mounted ? "present" : "absent");
  console_lowvideo();
  console_puts("\r\nSymmetric/Private Key:\r\n");
  console_highvideo();
  keymanager_display_key(-1, &current_key_private);
  console_lowvideo();
  console_puts("\r\nPublic Key:\r\n");
  console_highvideo();
  keymanager_display_key(-1, &current_key_public);
  console_lowvideo();
/*  console_puts(" heapstack: ");
  console_printint(heap_stack_distance());
  console_printcrlf(); */
  int option = console_selectmenu(mainmenu, mainmenuoptions);
  delay(500);
  switch (option)
  {
    case 'R': randomness_test();
      break;
    case 'K': keymanager();
      break;
    case 'X': file_delete();
      break;
    case 'N': file_new();
      break;
    case 'T': file_edit();
      break;
    case 'M': file_mount_volume(0);
      break;
    case 'V': file_view();
      break;
    case 'E': fileenc_encrypt();
      break;
    case 'D': fileenc_decrypt();
      break;
    case 'Z': randomness_show();
      break;

  }
}

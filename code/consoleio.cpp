/*
 * Copyright (c) 2018 Daniel Marks

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

#include <Arduino.h>
#include <stdarg.h>
#include <TNTSChar.h>
#include <TNTSCAnsi.h>
#include <PS2Keyboard.h>
#include "consoleio.h"
#include "mini-printf.h"

Stream *externalSerial = NULL;

PS2Keyboard kbd;

#ifdef __cplusplus
extern "C" {
#endif
int console_inchar(void)
{
  int ch;
  if ((ch=kbd.getkey()) >= 0) return ch;
  if ((externalSerial != NULL) && (externalSerial->available())) return externalSerial->read();
  return -1;
}

int console_getch(void)
{
  int ch;
  while ((ch=kbd.getkey()) < 0);
  return ch;
}

void console_putch(char c)
{
  TNTSCAnsi.output_character(c);
  if (externalSerial != NULL) externalSerial->print(c);
}

void console_puts(const char *c)
{
  while (*c) console_putch(*c++);
}

void console_init(void)
{
	uint16_t *xpos_pos, *ypos_pos;
	TNTSChar.begin(1,1); // 第2引数でSPI 1,2を指定(デフォルト 1))
	TNTSChar.adjust(0);  // 垂直同期信号補正(デフォルト 0)
	TNTSChar.get_cursor_ptr((volatile uint16_t **)&xpos_pos,(volatile uint16_t **)&ypos_pos);
	TNTSCAnsi.begin(TNTSChar.screendata(),TNTSChar.rows(),TNTSChar.cols());
	TNTSCAnsi.set_cursor_ptr(xpos_pos, ypos_pos);
	TNTSCAnsi.clear_virtscreen(1);
	kbd.begin(PB4,PB5);
}

void console_printcrlf(void)
{
  console_puts("\r\n");
}

void console_clreol(void)
{ 
  console_puts("\033[K");
}

void console_clrscr(void)
{
	console_puts("\033[H\033[2J");
}

void console_gotoxy(int x, int y)
{
  console_puts("\033[");
	console_printint(y);
	console_putch(';');
	console_printint(x);
	console_putch('H');
}

void console_highvideo(void)
{
	console_puts("\033[1m");
}

void console_lowvideo(void)
{
	console_puts("\033[0m");
}

char *myltoa(char *p, long n)
{
   mini_itoa(n, 10, 0, 0, p, 0); 
   return p;
}

long mystrtol(const char *str, const char **end)
{
	unsigned long n=0;
	int neg;

	while (*str == ' ') str++;
	if (neg=(*str == '-')) str++;
	if (isdigit(*str)) {
		{
			for (;;) {
				if ((*str>='0') && (*str<='9'))
					n = (n*10) + (*str++ - '0');
				else
					break;
			}
		}
	}
	if (end) *end=str;
	return neg ? -((long)n) : (long) n;
}

void console_printint(int n)
{
  char s[20];
  mini_itoa(n, 10, 0, 0, s, 0); 
  console_puts(s);
}

void console_printuint(unsigned int n)
{
  char s[20];
  mini_itoa(n, 10, 0, 1, s, 0); 
  console_puts(s);
}

void console_delayframes(unsigned short fr)
{
	unsigned short fr1 = TNTSChar.framect();
	while (((unsigned short)(TNTSChar.framect() - fr1)) < fr);

}

int console_getstring(char *c, int len, int row, int col, int wrap)
{
  int n = 0;
  for (;;)
  {
    console_gotoxy((n % wrap)+col, (n/wrap)+row);
    int ch = console_getch();
    if (ch == '\r')
    {
      c[n] = '\000';
      return n;
    }
    if ((ch >= ' ') && (ch < 127) && (n < len))
    {
      console_putch(ch);
      c[n++] = ch;
    }
    if ((ch == '\010') && (n > 0))
    {
      n--;
      console_gotoxy((n % wrap)+col, (n/wrap)+row);
      console_puts(" \010");
    }
  }
}

char console_selectmenu(const char *menutext, const char *options)
{
  console_puts(menutext);
  for (;;)
  {
    int ch = toupper(console_getch());
    const char *scanoptions = options;
    while (*scanoptions)
    {
      if (*scanoptions == ch)
      {
        console_putch(ch);
        return ch;
      }
      scanoptions++;
    }
  }  
}

void console_press_space(void)
{
  console_puts("\r\n\r\nPress SPACE to continue");
  while (console_getch() != ' ');
}

int console_yes(int row)
{
  char response[4];
  console_gotoxy(1,row);
  console_puts("Type YES to continue: ");
  console_getstring(response,3,row,23,10);
  return strcmp(response,"YES");
}

void console_print_array(const char *c, unsigned char *a, int n)
{
  console_printcrlf();
  console_puts(c);
  for (int i=0;i<n;i++)
  {
    if ((i % 8) == 0)
        console_printcrlf();
    console_printint(a[i]);
    console_puts(" ");
  }
  console_printcrlf();
}


char *strcpy_n(char *dest, const char *src, size_t len)
{
   while ((len > 0) && (*src))
   {
     *dest++ = *src++;
     len--;
   }
   *dest = '\000';
}

char *strcat_n(char *dest, const char *src, size_t len)
{
  while ((len > 0) && (*dest)) 
  { 
    dest++; len--;
  }
  while ((len > 0) && (*src))
  {
    *dest++ = *src++;
  }
  *dest = '\000';
}

size_t strlen_n(const char *d)
{
  const char *p = d;
  while (*p) p++;
  return size_t (p-d);
}

#ifdef __cplusplus
}
#endif

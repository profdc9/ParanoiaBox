#ifndef _CONSOLEIO_H
#define _CONSOLEIO_H

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
 
#define MY_ITOASIZE 40

#ifdef __cplusplus
extern "C" {
#endif
int console_inchar(void);
int console_getch(void);
void console_putch(char c);
void console_puts(const char *c);
void console_init(void);
void console_printcrlf(void);
void console_clreol(void);
void console_clrscr(void);
void console_gotoxy(int x, int y);
void console_highvideo(void);
void console_lowvideo(void);
void console_printint(int n);
void console_printuint(unsigned int n);
void console_delayframes(unsigned short fr);
char console_selectmenu(const char *menutext, const char *options);
int console_getstring(char *c, int len, int row, int col, int wrap);
void console_press_space(void);
int console_yes(int row);
char *myltoa(char *p, long n);
long mystrtol(const char *str, const char **end);
char *strcpy_n(char *dest, const char *src, size_t len);
char *strcat_n(char *dest, const char *src, size_t len);
size_t strlen_n(const char *d);

#ifdef __cplusplus
}
#endif

#endif  /* _CONSOLEIO_H */

/* Jumentum-SOC

  Copyright (C) 2007 by Daniel Marks

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
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
  
  Daniel L. Marks profdc9@gmail.com

*/

/* ee.c, Yijun Ding, Copyright 1991

Compile with TC: tcc ee.c
Compile in unix: gcc ee.c -lcursesX

Prototype are listed in the order defined. They are essentially grouped
as cursor move, screen, search, files, misc, mainloop, main.

EE is public domain with source code. It has all of basic editing
functions like search/replace, block delete/paste, word
wrap/paragraph fill etc. It is small enough to maintain when you
have a new OS, or just changed your taste of style.

extensive modifications made by DLM for microcontroller use
including shrinking the size, removing features
not necessary, removing library dependencies, etc.

*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "consoleio.h"
#include "editor.h"

#ifdef __cplusplus
extern "C" {
#endif	

#define EDITOR_GETCH() console_getch()
#define EDITOR_PUTCH(x) console_putch(x)
#define EDITOR_CPUTS(x) console_puts(x)
#define EDITOR_TTOPEN()
#define EDITOR_TTCLOSE()

#define NLEN  30    /* buffer line for file name */

#define EOL '\0'    /* end of line marker */
#define BLK ' '     /* blank */
#define LF  '\n'    /* new line */
#define LMAX  255   /* max line length */
#define XINC  20    /* increament of x */
#define HLP 28

#define CHG 0
#define FIL 1   /* fill */
#define OVR 2   /* insert */
#define CAS 3   /* case sensative */
#define TAB 4   /* tab */
#define POS 5   /* show pos */
#define ALT 6   /* meta_key */
#define SHW 7   /* screen */
#define NEW 8   /* new file */
#define EDT 9   /* quit edit */
#define NTS 10  /* note posted */
ESTATIC char  flag[NTS+1];
char const  fsym[]="|foctp~";

/* order of \n\r is important for curses */
#define HELP_STR1 "\n\r\
Help\n\r\
\n\
A set mark, append file\n\r\
B format, right margin\n\r\
C play macro, record\n\r\
D cursor right, line end\n\r\
E cursor up, file top\n\r\
F forward search, backward search\n\r\
G goto line, goto column\n\r\
H delete before, delete under\n\r\
I tab, tab insert\n\r\
K block copy, block cut\n\r\
L refresh screen, status\n\r\
N toggle insert mode\n\r\
O toggle mode\n\r\
P command prefix\n\r\
Q command prefix\n\r\
\n\nPress any key to continue ..."

#define HELP_STR2 "\n\r\
Help\n\r\
\n\
R replace, replace all\n\r\
S cursor left, line begin\n\r\
T delete word, del eol\n\r\
U block paste, block write\n\r\
V page down, page up\n\r\
W save and/or exit\n\r\
X cursor down, file bottom\n\r\
Y block delete line\n\r\
\n\
Each letter represent a control key.\n\r\
Each key corresponds one or two\n\r\
editing commands (separated by comma).\n\r\
Press the control key for the\n\r\
first editing function, or control-P\n\r\
and the control key for the second\n\r\
editing function.\n\n\r\
Press any key to continue ..."

ESTATIC int swhfull = 22;                  /* screen physical height */
ESTATIC int x;
ESTATIC int sww = 38;                   /* screen position 1 <= x <= sww */
ESTATIC int y, swh;                   /* screen size 0 <= y <= swh */
ESTATIC int y1, y2;                   /* 1st, 2nd line of window */
ESTATIC int tabsize=8;                /* tab size */

ESTATIC char  *sbuf, *rbuf;           /* search buffer, replace buffer */
ESTATIC char  *ae, *aa;               /* main buffer */
ESTATIC char  *bb, *mk;               /* block buffer, mark */
ESTATIC unsigned blen;
ESTATIC char  *dp, *ewb;              /* current pos, line */

ESTATIC int xtru, ytru;               /* file position */
ESTATIC int ytot;                     /* 0 <= ytru <= ytot */

readfile rf;
void *rf_ptr;
writefile wf;
void *wf_ptr;

ESTATIC void cursor_up(), cursor_down(), cursor_left(), cursor_right();
ESTATIC void show_rest(int len, char *s);
ESTATIC void show_scr(int fr, int to);
ESTATIC void show_sup(int line), show_sdn(int line);
ESTATIC void show_flag(int x, int g);
ESTATIC void show_note(char *prp, int slp);
ESTATIC int  show_gets(char *prp, char *buf, int len);
ESTATIC void show_top(), show_help(), show_mode(), show_status();
ESTATIC void file_read(readfile r, void *v);
ESTATIC char *file_ltab(char *s);
ESTATIC int  file_write(writefile w, void *v, char *s, char *e);
ESTATIC void file_save(void);
ESTATIC int file_rs(char *s, char *d);
ESTATIC void goto_x(int xx), goto_y(int yy);
ESTATIC void goto_ptr(char *s);
ESTATIC void goto_row(), goto_col();
ESTATIC int  str_cmp(char *s);
ESTATIC char *goto_find(char *s, int  back);
ESTATIC void goto_search(int back);
ESTATIC void goto_replace(int whole);
ESTATIC void window_size();
ESTATIC void block_put(), block_get(), block_mark();
ESTATIC void block_copy(int delete);
ESTATIC void block_paste(), block_line();
ESTATIC int  block_fill();
ESTATIC void block_format();
ESTATIC void key_return(), key_deleol(char *s), key_delete();
ESTATIC void key_backspace(), key_delword(int eol);
ESTATIC void key_tab(int tabi);
ESTATIC void key_normal(int key);
ESTATIC void key_macros(int record);
ESTATIC void main_meta(int key), main_exec(int key);
ESTATIC void main_loop();

/* defintions linking to rest of project */

#define YTOP  1		/* first line */

ESTATIC void insline(void)
{
	EDITOR_CPUTS("\033[L");
}

ESTATIC void delline(void)
{
	EDITOR_CPUTS("\033[M");
}


ESTATIC void sec_sleep(int n)
{	
#if 0
	int i,j;
	for (i=0;i<n;i++)
		for (j=0;j<480;j++)
			EDITOR_PUTCH(0);
#else
	console_delayframes(n*60);
#endif
}

ESTATIC void cshownum(int p, int n)
{
  int i;
  char s[MY_ITOASIZE];
  char *c = myltoa(s,n);
  for (i=strlen(c);i>p;i--) EDITOR_PUTCH(' ');
  EDITOR_CPUTS(c);
}

/* honest get a key */
ESTATIC int get_key()
{
  static char const k1[]="ABCDrpl";
  static char const k2[]="EXDSdsh";
  int key;
  char  *s;
  key = EDITOR_GETCH();
  if(key == 127) return 8;
  if(key == 27) {
    EDITOR_GETCH();  /* aa nothing */
    if((s=strchr(k1, EDITOR_GETCH())) != NULL && (key = k2[s-k1]) > 'a')
      flag[ALT]++;
    return key&0x1F;
  }
  return key;
}

/* cursor movement ----------------------------------------- */
ESTATIC void cursor_up()
{
  if(ytru == 0) return;
  ytru--;
  while(*--ewb != EOL) ;
  y--;
}

ESTATIC void cursor_down()
{
  if(ytru == ytot) return;
  ytru++;
  while(*++ewb != EOL) ;
  y++;
}

/* cursor left & right: x, xtru */
ESTATIC void cursor_left()
{
  if(xtru == 1) return;
  xtru--;
  if(--x < 1) {
    x += XINC;
    flag[SHW]++;
  }
}

ESTATIC void cursor_right()
{
  if(xtru == LMAX) return;
  xtru++;
  if(++x > sww) {
    x -= XINC;
    flag[SHW]++;
  }
}

#define cursor_pageup() {int i; for(i=1; i<swh; ++i) cursor_up();}
#define cursor_pagedown(){int i; for(i=1; i<swh; ++i) cursor_down();}

/* dispaly --------------------------------------------------------*/
/* assuming cursor in correct position: show_rest(sww-x,ewb+xtru) */
ESTATIC void show_rest(len, s)
int  len;
char *s;
{
  char save;
  save = s[len];
  s[len] = 0;
  EDITOR_CPUTS(s);
  s[len] = save;
  console_clreol();
}

/* ewb and y correct */
ESTATIC void show_scr(fr,to)
int fr, to;
{
  char *s=ewb;
  int  len=sww-1, i=fr;
  unsigned xl=xtru-x;

  /* calculate s */
  for(; i<y; i++) while(*--s != EOL) ;
  for(; i>y; i--) while(*++s != EOL) ;

  /* first line */
  s++;
  do {
    console_gotoxy(1, fr+y2);
    if(s<ae && strlen(s) > xl) show_rest(len, s+xl);
    else console_clreol();
    while(*s++) ;
  } while(++fr <= to);
}

ESTATIC void show_sup(line)
int line;
{
  console_gotoxy(1, y2+line);
  delline();
  show_scr(swh, swh);
}

ESTATIC void show_sdn(line)
int line;
{
  console_gotoxy(1, y2+line);
  insline();
  show_scr(line, line);
}

ESTATIC void show_flag(x, g)
int x, g;
{
  console_gotoxy(14+x, y1);
  EDITOR_PUTCH(g? fsym[x]: '.');
  flag[x] = g;
}

ESTATIC void show_note(prp, slp)
char *prp;
int slp;
{
  console_gotoxy(17+ALT, y1);
  EDITOR_CPUTS(prp);
  console_clreol();
  flag[NTS]++;
  sec_sleep(slp);
}

ESTATIC int show_gets(prp, buf, len)
char *prp, *buf;
int len;
{
  int key;
  int col=(-1);
  show_note(prp,0);
  EDITOR_CPUTS(": ");
  EDITOR_CPUTS(buf);
  for(;;) {
    key = get_key();
    if(key >= BLK) {
      if(col < 0) {
        col++;
        show_note(prp,0);
        EDITOR_CPUTS(": ");
      }
      if (col<len) {
		buf[col++] = key;
		EDITOR_PUTCH(key);
	  }
    }
    else if(key == 8) {
      if(col < 0) col = strlen(buf);
      if(col == 0) continue;
      col--;
	  EDITOR_CPUTS("\010 \010");
    }
    else break;
  }
  flag[ALT] = 0;
  if(col > 0) buf[col] = 0;
  return (key == 27 || *buf == 0);
}

ESTATIC void show_top()
{
  int i;
  console_gotoxy(1, y1);
  console_highvideo();
  EDITOR_CPUTS(EDITORTITLE);
  console_lowvideo();
  console_clreol();
  for(i=0; i<=ALT; i++)
    show_flag(i, flag[i]);
  flag[NTS]++;
}

ESTATIC void show_help()
{
  console_clrscr();
  EDITOR_CPUTS(HELP_STR1);
  get_key();
  console_clrscr();
  EDITOR_CPUTS(HELP_STR2);
  get_key();
  show_top();
  flag[SHW]++;
}

ESTATIC void show_mode()
{
  char *d;
  int  k;
  show_note(fsym,2);
  EDITOR_PUTCH(BLK);
  k = get_key()|0x60;
  if((d=strchr(fsym, k)) != 0) {
    k = d-fsym;
    show_flag(k, !flag[k]);
  }
}

#define SHOWSTATUS

#ifdef SHOWSTATUS

ESTATIC void strcatint(char *t, char *r, int n)
{
  char s[MY_ITOASIZE];
  strcat(t, r);
  strcat(t, myltoa(s,n));
}

ESTATIC void show_status()
{
  char tbuf[80];
  tbuf[0] = 0;
  strcatint(tbuf, "[", ytru+1);
  strcatint(tbuf, "/", ytot);
  strcatint(tbuf, ",C", xtru);
  strcatint(tbuf, "]#",dp-aa);
  strcatint(tbuf, "/", ae-aa);
  show_note(tbuf,2);
}
#endif

/* file operation ---*/
ESTATIC void file_read(readfile rf, void *v)
{
  int  c;
  char *col;
  char *end = aa+(AMAX-BMAX-1);
  ewb = aa;
  ae = mk = col = aa+1;
  xtru = x = 1;
  ytru = y = ytot = 0;
  if (!rf)
    return;

  /* read complete line */
  do {
    c =(*rf)(v);
    if(c == 0) {
      break;
    }
    if(c == 9) {    /* tab */
      if(flag[TAB] == 0) show_flag(TAB, 1);
      do (*ae++ = BLK);
      while( (((ae-col) % tabsize) != 0) && (ae < end));
    }
    else if(c == LF) {
      *ae++ = EOL;
      col = ae;
      ytot++;
    }
    else if ((c>=' ') && (c<='~')) *ae++ = c;
  } while((ae < end) || (c != LF)); 
  *ae = EOL;
}

/* compress one line from end */
ESTATIC char *file_ltab(s)
char *s;
{
  char *d, *e;
  e = d = strchr(s--, EOL);
  while(*--e == BLK) ;  /* trailing blank */
  while(e > s) {
    if(e[0] == BLK && (e-s)%tabsize == 0 && e[-1] == BLK) {
      *--d = 9;
      while(*--e == BLK && (e-s)%tabsize != 0) ;
    }
    else *--d = *e--;
  }
  return d;
}

/* routine used for write block file also, this makes it more complicated */
ESTATIC int file_write(writefile w, void *v, char *s, char *e)
{
  if (!w) return 1;
  do {
    if(flag[TAB] && *s != EOL) s = file_ltab(s);
    while (*s != EOL) (*w)(*s++,v);
    s++;
    (*w)(LF,v);
  } while(s < e);
  (*w)(0,v);
  (*w)(-1,v);
  return 0;
}

ESTATIC void file_save(void)
{
  int k='n';
  if(flag[CHG] )
  do {
    show_note("Save (y/n/cncl): ",0);
    k = toupper(get_key());
    if (k == 'C') return;
  } while(k != 'Y' && k != 'N');
  flag[CHG] = 0;
  flag[EDT] = 1;
  if(k == 'N') {
    return;
  }
  file_write(wf, wf_ptr, aa+1,ae);
  show_note("Saved",2);
}

ESTATIC int file_rs(s, d)
char  *d, *s;
{
  char  *e = ae;
  unsigned i = e-s;
  char *temp;

  /* immediate problem only when block buffer on disk too big */
  temp = ae + (d-s);
  if (temp >= (aa+AMAX)) {
    show_note("Main buffer full",2);
    return 0;
  }
  ae = temp;
  if(s < d) { /* expand */
    d += e - s;
    s = e;
    while(i-- > 0) *--d = *--s;
    /* while(j-- > 0) if((*--d = *--str) == EOL) ytot++; */
  }
  else {
    /* adjust ytot when shrink */
    for(e=d; e<s; e++) if(*e == EOL) ytot--;
    while(i-- > 0) *d++ = *s++;
  }
  *ae = EOL;  /* last line may not complete */
  if(!flag[CHG] ) {
    show_flag(CHG, 1);
    console_gotoxy(x, y+y2);
  }
  return 1;
}

/* search and goto */
/* xx >= 1, yy >= 0 */
ESTATIC void goto_x(xx)
int  xx;
{
  int i, n;
  n = xtru;
  for(i=xx; i<n; i++) cursor_left();
  for(i=n; i<xx; i++) cursor_right();
}

ESTATIC void goto_y(yy)
int  yy;
{
  int i, n;
  n = ytru;
  for(i=yy; i<n; i++) cursor_up();
  for(i=n; i<yy; i++) cursor_down();
}

ESTATIC void goto_ptr(s)
char *s;
{
  /* find ewb <= s */
  char  *s1 = s;
  while(*--s1 != EOL) ;
  while(ewb > s) cursor_up();
  while(ewb < s1) cursor_down();
  goto_x(s-ewb);
  if(y > swh) y = flag[SHW] = swh/4;
}

ESTATIC void goto_row()
{
  char rbuf[6];
  rbuf[0] = 0;
  show_gets("Goto line", rbuf, sizeof(rbuf)-1);
  goto_y(mystrtol(rbuf,NULL)-1);
}

ESTATIC void goto_col()
{
  char cbuf[6];
  cbuf[0] = 0;
  show_gets("Goto Column", cbuf, sizeof(cbuf)-1);
  goto_x(mystrtol(cbuf,NULL)-1 );
}

/* compare to sbuf. used by search */
ESTATIC int str_cmp(s)
char *s;
{
  char  *d = sbuf;
  if(flag[CAS] ) {
    while(*d ) if(*s++ != *d++ ) return 1;
    return 0;
  }
  while(*d ) if(toupper(*s++) != toupper(*d++)) return 1;
  return 0;
}

/* back / forward search */
ESTATIC char *goto_find(s, back)
char *s;
int  back;
{
  do {
    if(back ) {
      if(--s <= aa) return 0;
    }
    else if(++s >= ae) return 0;
  } while(str_cmp(s) );
  goto_ptr(s);
  return s;
}

ESTATIC void goto_search(back)
int  back;
{
  if(show_gets("Search for", sbuf, NLEN-1) ) return;
  goto_find(dp, back);
}

ESTATIC void goto_replace(whole)
int whole;
{
  char  *s=dp;
  int rlen, slen = strlen(sbuf);
  if(str_cmp(s) || str_cmp(rbuf) == 0 ||
  show_gets("Replace with", rbuf, NLEN-1) ) return;
  rlen = strlen(rbuf);
  do {
    if (!file_rs(s+slen, s+rlen))
      break;
    memmove(s, rbuf, rlen);
  }
  while(whole && (s=goto_find(s, 0)) != 0) ;
  if(whole) flag[SHW]++;
  else {
    console_gotoxy(x, y+y2);
    show_rest(sww-x, s);
  }
}

ESTATIC void window_size()
{
  char wbuf[8];
  strcpy(wbuf,"68");
  if(show_gets("Right margin",wbuf,sizeof(wbuf)-1) == 0) {
    sww = mystrtol(wbuf,NULL);
    flag[SHW]++;
  }
}

/* block and format ---*/
/* use blen, mk, bb */
ESTATIC void block_put()
{
  
  memmove(bb, mk, blen=(blen < BMAX ?  blen : BMAX));
  show_note("Block copied",2);
}

ESTATIC void block_get()
{
  int i;
  memmove(mk, bb, blen);
  /* calculate ytot */
  for(i=0; i<blen; i++) if(mk[i] == EOL) ytot++;
}

ESTATIC void block_mark()
{
  if(*dp == EOL)
    show_note("Invalid Pos.",2);
  else {
    show_note("Mark Set",2);
    mk = dp;
  }
}

ESTATIC void block_copy(delete)
int delete;
{
  if (mk > dp) {
	  char *s = mk;
	  mk = dp;
	  dp = s;
  }
  blen = dp - mk;
  block_put();
  dp = blen + mk;
  if(delete) {
    goto_ptr(mk);
    file_rs(dp, mk);
    flag[SHW]++;
  }
}

ESTATIC void block_paste()
{
  if (!file_rs(dp, dp+blen))
    return;
  mk = dp;
  block_get();  
  /* if it is a line */
  if(xtru == 1 && strlen(mk) == blen-1) show_sdn(y);
  else {
    show_scr(y, swh);
    goto_ptr(mk+blen);
  }
}

ESTATIC void block_line()
{
  if(ytru == ytot) return;
  goto_x(1 );
  for(blen = 0; ewb[++blen] != EOL; ) ;
  mk = ewb+1;
  block_put();
  file_rs(ewb+blen, ewb);
  show_sup(y);
}

/* fill current line; does not change corsur or file len */
ESTATIC int block_fill()
{
  int i=sww;
  while(ewb[--i] > BLK) ;
  if(i == 0) i = sww-1;
  ewb[i] = EOL;
  ytot++;
  cursor_down();
  return i;
  /* screen position to console_clreol() */
}

/* format paragraph */
ESTATIC void block_format()
{
  char  *s=ewb;
  int ytmp = y;
  goto_x(1);
  while(s = strchr(++s, EOL), ytru < ytot && s[1] != EOL) {
    s[0] = BLK;
    ytot--;
  }
  while(strlen(ewb+1) >= sww) block_fill();
  while(ewb[xtru] ) cursor_right();
  if( flag[SHW] == 0) show_scr(ytmp, swh);
}

/* key actions ---*/
/* update file part, then screen ... */
ESTATIC void key_return()
{
  char  *s=dp;
  if(flag[OVR] ) {
    cursor_down();
    goto_x(1);
    return;
  }
  if (!file_rs(s, s+1)) return;
  goto_x(1);
  *s = EOL;
  ytot++;
  cursor_down();
  if(flag[SHW] == 0) {
    console_clreol();
    if(y < sww) show_sdn(y);
  }
}

/* used by next two */
ESTATIC void key_deleol(s)
char *s;
{
  if(ytru == ytot) return;
  goto_x(s-ewb);
  if (!file_rs(s+1, s)) return;
  if(flag[SHW] ) return;
  if(y < 0) {   /* y = -1 */
    y = 0;
    show_scr(0,0);
  }
  else {
    console_gotoxy(x, y+y2);
    show_rest(sww-x, s);
    show_sup(y+1);
  }
}

/* delete under */
ESTATIC void key_delete()
{
  char  *s=dp;
  if( *s == EOL) {
    key_deleol(s);
    return;
  }
  if (file_rs(s+1, s))
    show_rest(sww-x, s);
}

ESTATIC void key_backspace()
{
  char  *s=dp;
  if(*--s == EOL) { /* delete EOL */
    if(ytru == 0) return;
    cursor_up();
    key_deleol(s);
    return;
  }
  while(ewb+xtru > s) cursor_left();
  /* delete tab space */
  if(*s == BLK) {
    while(*--s == BLK && (xtru%tabsize) != 1) cursor_left();
    s++;
  }
  file_rs(dp, s);
  if(!flag[SHW] ) {
    console_gotoxy(x, y+y2);
    show_rest(sww-x, s);
  }
}

ESTATIC void key_delword(eol)
int  eol;
{
  char  *d=dp;
  if(*d == EOL) {
    key_deleol(d);
    return;
  }
  if(eol) while(*d != EOL) d++;
  else {
    while(*d > BLK) d++;
    while(*d == BLK) d++;
  }
  show_rest(sww-x, d);
  file_rs(d, dp);
}

ESTATIC void key_tab(tabi)
int tabi;
{
  char  *s = ewb+xtru;
  int xtmp=x;
  do cursor_right();
  while((xtru%tabsize) != 1);
  if(!tabi && s==dp) {
    s = ewb+xtru;
    if (file_rs(dp, s)) { /* may change cursor_position */
      while(s > dp) *--s = BLK;
      console_gotoxy(xtmp, y+y2);
      show_rest(sww-xtmp, s);
    }
  }
}

ESTATIC void key_normal(key)
int key;
{
  char  *s=ewb+xtru;
  int xtmp;
  if(dp < s) {
    if (file_rs(dp, s))
      while(dp < s) *dp++ = BLK;
    else
      return;
  }
  if(flag[OVR] && *s != EOL) {
    EDITOR_PUTCH(*s = key);
    flag[CHG] = 1;
  }
  else {
    if (!file_rs(s, ++dp))
      return;
    *s = key;
    show_rest(sww-x, s);
  }
  cursor_right();
  if(!flag[FIL] || xtru<sww) return;

  xtmp = block_fill();  /* cursor_down */
  if(xtru > sww) flag[SHW]++;
  xtru = x = xtru - xtmp;
  if(flag[SHW] == 0) {
    console_gotoxy(xtmp, y2+y-1);
    console_clreol();
    show_sdn(y);
  }
}

ESTATIC void key_macros(record)
int record;
{
  static char mbuf[60];
  char  *s=mbuf;
  void  main_exec();
  int k;
  if(!record) {
    while(*s != 0) main_exec(*s++);
    return;
  }
  show_note("^Z to end: ",0);
  while( (k = get_key()) != 'Z'-'@') {
    *s++ = k;
    if(k < BLK) {
      EDITOR_PUTCH('^');
      k |= 0x40;
    }
    EDITOR_PUTCH(k);
  }
  *s = 0;
  s = mbuf;
}

/* main function */
ESTATIC void main_meta(key)
int key;
{
  switch(key | 0x60) {
    /*  case 'a': file_more(); break; */
  case 'b': window_size(); break;
  case 'd': goto_x(strlen(ewb+1)+1); break;
  case 'e': goto_y(0); break;
  case 'f': goto_search(1); break;
  case 'g': goto_col(); break;
  case 'h': key_delete(); break;
  case 'i': key_tab(0); break;
  case 'j': key_macros(1); break;
  case 'k': block_copy(1); break;
#ifdef SHOWSTATUS
  case 'l': show_status(); break;
#endif
  case 'm':
  case 'n':
  case 'o':
  case 'p': key_normal(get_key() ); break;
  case 'q': break;
  case 'r': goto_replace(1); break;
  case 's': goto_x(1); break;
  case 't': key_delword(1); break;
    /*  case 'u': block_write(); break;*/
  case 'v': cursor_pageup(); break;
  case 'x': goto_y(ytot); break;
  case 'y': break;
  }
  show_flag(ALT, 0);
}

ESTATIC void main_exec(key)
int key;
{
  int i = xtru;
  dp = ewb;
  while(*++dp && --i>0) ;
  if(flag[ALT] ) main_meta(key);
  else if(key >= BLK) key_normal(key);
  else if(key == 0x7F) key_backspace();
  else switch(key|0x60) {
  case 'a': block_mark(); break;
  case 'b': block_format(); break;
  case 'c': key_macros(0); break; 
  case 'd': cursor_right(); break;
  case 'e': cursor_up(); break;
  case 'f': goto_search(0); break;
  case 'g': goto_row(); break;
  case 'h': key_backspace(); break;
  case 'i': key_tab(1); break;
  case 'k': block_copy(0); break;
  case 'l': if(ytru > swh/4) { y = swh/4; flag[SHW]++; } break;
  case 'm':
  case 'j': key_return(); break;
  case 'n': show_flag(OVR, !flag[OVR]); break;
  case 'o': show_mode(); break;
  case 'p':
  case 'q': show_flag(ALT, 1); break;
  case 'r': goto_replace(0); break;
  case 's': cursor_left(); break;
  case 't': key_delword(0); break;
  case 'u': block_paste(); break;
  case 'v': cursor_pagedown(); break;
  case 'w': file_save(); break;
  case 'x': cursor_down(); break;
  case 'y': block_line(); break;
  case HLP|0x60: show_help();   /* | */
  }
}

/* win is only thing it knows */
ESTATIC void main_loop()
{
  int  yold, xold;

  show_top();
  show_note("New file",0);
  flag[NEW]++;
  file_read(rf,rf_ptr);

  while(flag[EDT] == 0) {
    if(y <= -1 || y >= swh) {      /* change here if no hardware scroll */
      if(y == -1) {
        y++; show_sdn(0);
      }
      else if(y == swh) {
        y--; show_sup(0);
      }
      else {
        y = y < 0? 0: swh-1;
        show_scr(0, swh);
        flag[SHW] = 0;
      }
    }
    else if(flag[SHW] ) {
      show_scr(0, swh);
      flag[SHW] = 0;
    }
    if(flag[NTS] ) {
      show_note(flag[POS]? "Line      Col": "Ctrl-\\ Help",0);
      yold = xold = (-1);
      flag[NTS] = 0;
    }
    if(flag[POS] ) {
      if(ytru != yold) {
        yold = ytru;
        console_gotoxy(22+ALT, y1);
	cshownum(4,ytru+1);
      }
      if(xtru != xold) {
        xold = xtru;
        console_gotoxy(31+ALT, y1);
		cshownum(3,xtru);
      }
    }
    console_gotoxy(x, y+y2);
    main_exec(get_key() );
  }
}

void editor(void)
{
  sbuf=malloc(sizeof(char)*NLEN);
  rbuf=malloc(sizeof(char)*NLEN);
  aa=malloc(sizeof(char)*AMAX);
  bb=malloc(sizeof(char)*BMAX);
  if (sbuf && rbuf && aa && bb) {

    sbuf[0] = rbuf[0] = aa[0] = bb[0] = EOL;
    
    EDITOR_TTOPEN();
	  console_clrscr();
    swh = swhfull;
    y1 = YTOP;
    
    y2 = y1+1;
    flag[SHW]++;
    flag[NEW] = flag[EDT] = 0;
    main_loop();
    
    console_gotoxy(1, swhfull+2);
    EDITOR_TTCLOSE();
  } else {
	EDITOR_CPUTS("Unable to allocate memory\nReset unit\n");
  }
  if (sbuf) free(sbuf);
  if (rbuf) free(rbuf);
  if (aa) free(aa);
  if (bb) free(bb);
  return;
}

#ifdef __cplusplus
}
#endif	

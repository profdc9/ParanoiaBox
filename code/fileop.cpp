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

#include "Arduino.h"
#include <stdarg.h>
#include <ff.h>
#include "consoleio.h"
#include "editor.h"
#include "fileop.h"
#include "cryptotool.h"

#define USE_MINIPRINTF

#ifdef USE_MINIPRINTF
#include "mini-printf.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif  

uint8_t fs0_mounted = 0;
uint8_t fs1_mounted = 0;
static FATFS fs0, fs1;

typedef struct
{
  FIL fil;
} file_edit_struct;

static int file_editreadfile(void *v)
{
  file_edit_struct *fes = (file_edit_struct *)v;
  for (;;)
  {
    unsigned char ch;
    UINT readbytes;
    FRESULT res = f_read(&fes->fil, (void *) &ch, sizeof(ch), &readbytes);
    if ((readbytes < 1) || (ch == 0))
    {
      f_lseek(&fes->fil, 0);
      return 0;
    }
    if (ch < 127) return ch;
  }
}

static int file_editwritefile(int c, void *v)
{
  file_edit_struct *fes = (file_edit_struct *)v;
  if (c < 0)
  {
    f_truncate(&fes->fil);
    return 0;
  }
  unsigned char ch = c;
  UINT writebytes;
  f_write(&fes->fil, (void *)&ch, sizeof(ch),&writebytes);
  return 0;
}

void file_edit(void)
{
  file_edit_struct fes;
  FRESULT fres;
  {
    char filename[256];
    if (!file_select_card("Select file to edit",filename,sizeof(filename)-1,0)) return;
    FRESULT fres = f_open(&fes.fil, filename, FA_READ|FA_WRITE);
    if (fres != FR_OK)
    {
      file_report_error("Unable to open file");
      return;
    }
  }
  rf_ptr = wf_ptr = &fes;
  rf = file_editreadfile;
  wf = file_editwritefile;
  editor();  
  f_close(&fes.fil);
}

int file_enter_filename(const char *message, char *filename, int len)
{
  console_clrscr();
  console_gotoxy(1,4);
  console_puts(message);
  console_gotoxy(1,6);
  strcat_n(filename,"/",len);
  size_t flen = strlen_n(filename);
  console_getstring(&filename[flen],len-flen,6,1,35);
  return filename[flen] != '\000';
}

void file_new(void)
{
  file_edit_struct fes;
  FRESULT fres;
  {
    char filename[256];
    if (!file_select_card("Select directory for new file",filename,sizeof(filename)-1,1)) return;
    if (!file_enter_filename("Filename of new file:", filename, sizeof(filename)-1)) return;
    FRESULT fres = f_open(&fes.fil, filename, FA_CREATE_NEW|FA_WRITE);
    if (fres != FR_OK)
    {
      file_report_error("Unable to create file");
      return;
    }
  }
  rf = NULL;
  rf_ptr = NULL;
  wf_ptr = &fes;
  wf = file_editwritefile;
  editor();  
  f_close(&fes.fil);
}

void file_report_error(const char *error_message)
{
  console_clrscr();
  console_gotoxy(0,10);
  console_puts(error_message);
  console_press_space();
}

void file_mount_volume(uint8_t unmount)
{
  f_mount(NULL,"0:",0);
  f_mount(NULL,"1:",0);
  fs0_mounted = fs1_mounted = 0;
  if (unmount) return;
  fs0_mounted = (f_mount(&fs0,"0:",1) == FR_OK);
  fs1_mounted = (f_mount(&fs1,"1:",1) == FR_OK);
}

#define FILE_SELECT_ENTRY_LEN 31
#define FILE_SELECT_MAX_ENTRIES 16

typedef struct
{
  BYTE attrib;
  FSIZE_t filesize;
  char name[FILE_SELECT_ENTRY_LEN+1];
} file_select_entry;

int file_strip_pathname(char *filename)
{
  char *c = filename;
  while (*c) c++;
  while ((c > filename) && (*c != '/')) c--;
  if (c == filename) return 0;
  *c = '\000';
  return 1;
}

int file_select(const char *message, const char *dir, uint8_t seldir, char *selected, int maxlen)
{
  int cur_page_entry = 0;
  file_select_entry *fse;
  
  fse = (file_select_entry *)malloc(sizeof(file_select_entry)*FILE_SELECT_MAX_ENTRIES);
  if (fse == NULL) return 0;
  strcpy_n(selected,dir,maxlen);
  int selitem = 0;
  for (;;)
  {
    DIR dp;
    FRESULT fres = f_opendir(&dp,selected);
    if (fres != FR_OK)
    {
      file_report_error("Could not open directory"); 
      free(fse);
      return 0;
    }
    int n = 0, m = 0;
    for (;;)
    {
      FILINFO nfo;
      fres = f_readdir(&dp,&nfo);
      if (fres != FR_OK)
      {
        file_report_error("Could not open directory entry"); 
        free(fse);
        return 0;
      }
      if (nfo.fname[0] == '\000') break;
      if ((++m) < cur_page_entry) continue;
      file_select_entry *f = &fse[n];
      strcpy_n(f->name,nfo.fname,sizeof(f->name)-1);
      f->attrib = nfo.fattrib;
      f->filesize = nfo.fsize;  
      if ((++n) >= FILE_SELECT_MAX_ENTRIES) break;
    }
    f_closedir(&dp);
    if (n == 0)
    {
      if (cur_page_entry >= FILE_SELECT_MAX_ENTRIES)
        {
          cur_page_entry -= FILE_SELECT_MAX_ENTRIES;
          continue;
        } else
        {
          console_gotoxy(1,4);
          console_puts("No files to select");
        } 
    }
    console_clrscr();
    console_puts(message);
    console_puts("\r\nSelect ");
    console_puts(seldir ? "directory:\r\n" : "file:\r\n");
    console_puts(selected);
    console_gotoxy(0,22);
    console_puts("Up/Down move, Enter Select, Right Enter\r\nDir, Left Leave Dir, Q Quit");
    for (;;)
    {
      for (int p=0;p<n;p++)
      {        
        file_select_entry *f = &fse[p];
        if (p == selitem) console_highvideo();
          else console_lowvideo();
        console_gotoxy(1,p+5);
        console_puts(f->name);
        console_lowvideo();
        console_gotoxy(32,p+5);
        if (f->attrib & AM_DIR)
        {
          console_puts("<DIR>");
        } else console_printuint(f->filesize);
      }
      int ch = toupper(console_getch());
      if (selitem < n)
      {
        if (ch == 'A')
        {
          if (selitem > 0) --selitem;
          else
          {
            if (cur_page_entry >= FILE_SELECT_MAX_ENTRIES)
            {
              cur_page_entry -= FILE_SELECT_MAX_ENTRIES;
              selitem = FILE_SELECT_MAX_ENTRIES-1;
              break;
            }
          }
        }
        if (ch == 'B')
        {
          if (selitem < (n-1)) ++selitem;
          else {
            if (selitem == (FILE_SELECT_MAX_ENTRIES-1))
            {
              cur_page_entry += FILE_SELECT_MAX_ENTRIES;
              selitem = 0;
              break;
            }
          }
        }
        if (ch == 'C')
        {
          if (fse[selitem].attrib & AM_DIR)
          {
            strcat_n(selected,"/",maxlen);
            strcat_n(selected,fse[selitem].name,maxlen);
            cur_page_entry = selitem = 0;
            break;        
          }
        }
      }
      if (ch == 'D')
      {
        if (file_strip_pathname(selected))
        {
          selitem = 0;
          break;
        }
      }
      if (ch == '\r')
      {
        if (seldir || (!(fse[selitem].attrib & AM_DIR) && (!seldir)))
        {
          if (!seldir)
          {
            strcat_n(selected,"/",maxlen);
            strcat_n(selected,fse[selitem].name,maxlen);
          }
          free(fse);
          return (1);
        }
      }
      if (ch == 'Q')
      {
        free(fse);
        return (0);
      }
    }    
  }
}

int file_select_ciphertext(const char *message, uint8_t seldir, char *selected, int maxlen)
{
  return file_select(message, "0:", seldir, selected, maxlen);
}

int file_select_plaintext(const char *message, uint8_t seldir, char *selected, int maxlen)
{
  return file_select(message, "1:", seldir, selected, maxlen);
}

int file_select_card(const char *message, char *filename, int maxlen, int seldir)
{
  console_clrscr();
  console_gotoxy(1,5);
  console_puts("Use (P)laintext or (C)iphertext card?");
  for (;;)
  {
    int ch = toupper(console_getch());
    if (ch == 'C')
        return file_select_ciphertext(message, seldir, filename, maxlen);
    if (ch == 'P')
        return file_select_plaintext(message, seldir, filename, maxlen);
    if (ch == 'Q')
        return 0;
  }
}

void file_count_lf(FIL *fil, FSIZE_t first_ofs, FSIZE_t last_ofs, FSIZE_t line_ofs[], int numofs, int cols, int maxlines)
{
  int i, curcol=0;
  for (i=0;i<numofs;i++) line_ofs[i] = first_ofs;
  f_lseek(fil, first_ofs);
  for (;;)
  {
    unsigned char ch;
    UINT readbytes;
    FRESULT res = f_read(fil, (void *) &ch, sizeof(ch), &readbytes);
    if ((readbytes < 1) || (f_tell(fil) >= last_ofs)) return;
    if ((ch >= ' ')&& (ch <= '~')) curcol++;
    if ((curcol >= cols) || (ch == '\n'))
    {
      curcol = 0;
      for (i=numofs;i>1;)
      {
        i--; line_ofs[i] = line_ofs[i-1];
      }
      line_ofs[0] = f_tell(fil);
      if ((--maxlines) <= 0) return;
    }
  }
}   

void file_display_lines(FIL *fil, FSIZE_t ofs, int lines, int cols)
{
  int curcol = 0;
  f_lseek(fil,ofs);
  for (;;)
  {
    unsigned char ch;
    UINT readbytes;
    FRESULT res = f_read(fil, (void *) &ch, sizeof(ch), &readbytes);
    if (readbytes < 1) return;
    if ((ch >= ' ')&& (ch <= '~')) 
    {
      console_putch(ch);
      curcol++;
    }
    if ((curcol > cols) || (ch == '\n'))
    {
      curcol = 0;
      console_printcrlf();
      if ((--lines)<=0) return;
    }
  }
} 

#define FILE_VIEW_DISPLAY_MAXROWS 25

void file_view_display(const char *filename, int rows, int cols)
{ 
  FIL fil;
  FRESULT fres;
  FSIZE_t line_ofs[FILE_VIEW_DISPLAY_MAXROWS];
  FSIZE_t current_offset = 0;
  int maxscreenchars = rows*cols;

  if (rows > FILE_VIEW_DISPLAY_MAXROWS) return;
  fres = f_open(&fil, filename, FA_READ);
  if (fres != FR_OK)
  {
    file_report_error("Could not open file");
    return;
  }
  for (;;)
  {
    console_clrscr();
    console_highvideo();
    console_puts(filename);
    console_lowvideo();
    console_printcrlf();
    console_printcrlf();
    file_display_lines(&fil, current_offset, rows, cols);
    for (;;)
    {
      int ch = toupper(console_getch());
      if (ch == 'Q')
      {
        f_close(&fil);
        return;
      }
      if (ch == 'A')
      {
         FSIZE_t new_offset;
         if (current_offset < maxscreenchars)
            new_offset = 0;
         else
            new_offset = current_offset - maxscreenchars;
         file_count_lf(&fil, new_offset, current_offset, line_ofs, rows, cols, maxscreenchars);
         current_offset = line_ofs[rows/2-1];
         break;        
      }
      if (ch == 'B')
      {
         FSIZE_t ahead_offset = current_offset + (rows*cols);
         if (ahead_offset > f_size(&fil)) ahead_offset = f_size(&fil);
         file_count_lf(&fil, current_offset, ahead_offset, line_ofs, rows, cols, rows/2);
         current_offset = line_ofs[0];
         break;        
      }
    }
  }
}

void file_view(void)
{
  char filename[256];
  if (!file_select_card("Select file to view",filename,sizeof(filename)-1,0)) return;
  file_view_display(filename, 20, 38);
}

void file_delete(void)
{
  char filename[256];
  if (!file_select_card("Select file to delete",filename,sizeof(filename)-1,0)) return;
  console_clrscr();
  console_gotoxy(1,4);
  console_puts("Delete file?\r\n");
  console_puts(filename);
  if (console_yes(18)) return;
  f_unlink(filename);
}

static const char dashes[] = "----";

void file_create_header(const char *header, int term, char *line, int len)
{
  strcpy_n(line,dashes,len);
  strcat_n(line,header,len);
  if (term) strcat_n(line,dashes,len);
}

void file_write_header(FIL *f, const char *header, int term)
{
  char line[80];
  UINT br;

  if (term) f_write(f,"\n",1,&br);
  file_create_header(header, term, line, sizeof(line)-1);
  f_write(f,line,strlen_n(line),&br);
  f_write(f,"\n",1,&br);
  return;
}

int file_getline(FIL *f, char *line, int len)
{
  int pos = 0;
  for (;;)
  {
    char ch = 0;
    UINT br;
    FRESULT fres = f_read(f, &ch, sizeof(ch), &br);
    if (fres != FR_OK) return 0;
    if ((br == 0) || (ch == '\n'))
    {
      line[pos] = '\000';
      return 1;
    }
    if ((ch > ' ') && (ch <= '~') && (pos < len))
      line[pos++] = ch;
  } 
}

int file_skip_header(FIL *f, const char *header, int term)
{
  char matchline[80];
  UINT br;

  file_create_header(header, term, matchline, sizeof(matchline)-1);
  while (!f_eof(f))
  {
    char line[80];
    if (!file_getline(f, line, sizeof(line)-1)) return 0;
    if (!strcmp(line,matchline)) return 1;
  }
  return 0;
}

#define FILEBLOCKENCODE_WRITEBUF_SIZE 36

typedef struct _fileblockencode
{
  uint8_t *read_buf;
  uint16_t read_total;
  uint16_t read_curpos;
  FIL     *write_file;
  uint8_t  write_buf[FILEBLOCKENCODE_WRITEBUF_SIZE];
  uint16_t write_curpos;  
} fileblockencode;

int fileblockencode_readdata(void *v)
{
  fileblockencode *f = (fileblockencode *)v;
  if (f->read_curpos >= f->read_total) return -1;
  return f->read_buf[f->read_curpos++];
}

int fileblockencode_writedata(int c, void *v)
{
  fileblockencode *f = (fileblockencode *)v;

  if (c >= 0)
    f->write_buf[f->write_curpos++] = c; 
  if ((f->write_curpos == FILEBLOCKENCODE_WRITEBUF_SIZE) || (c < 0))
  {
    UINT br;
    f_write(f->write_file,f->write_buf,f->write_curpos,&br);
    f_write(f->write_file,"\n",1,&br);
    f->write_curpos = 0;
  }
}

int file_write_block(FIL *f, const char *header, void *v, uint16_t len)
{
  fileblockencode *fs;

  fs = (fileblockencode *) malloc(sizeof(fileblockencode));
  if (fs == NULL) return 0;

  fs->read_buf = (uint8_t *)v;
  fs->read_total = len;
  fs->read_curpos = 0;
  fs->write_file = f;
  fs->write_curpos = 0;
  
  file_write_header(f,header,0);
  base64_encode(fileblockencode_readdata,(void *)fs,  fileblockencode_writedata, (void *)fs);  
  fileblockencode_writedata(-1,fs);
  file_write_header(f,header,1);
  free(fs);
  return 1;
}

#define FILEBLOCKDECODE_READBUF_SIZE 256

typedef struct _fileblockdecode
{
  FIL     *read_file;
  FSIZE_t  read_last_block;
  uint8_t  read_buf[FILEBLOCKDECODE_READBUF_SIZE];
  uint16_t read_filled;
  uint16_t read_curpos;
  uint8_t *write_buf;
  uint16_t write_curpos;  
  uint16_t write_total;
  uint16_t write_end;
} fileblockdecode;

int fileblockdecode_readdata(void *v)
{
  fileblockdecode *f = (fileblockdecode *)v;
  
  if (f->write_end) 
  {
    f_lseek(f->read_file, f->read_last_block);
    return -1;
  }
  for (;;)
  {
    if (f->read_curpos >= f->read_filled)
    {
      UINT br;
      f->read_last_block = f_tell(f->read_file);
      FRESULT res = f_read(f->read_file,f->read_buf,FILEBLOCKDECODE_READBUF_SIZE,&br);
      if ((res != FR_OK) || (br == 0)) return -1;
      f->read_filled = br;
      f->read_curpos = 0;
    }
    uint8_t ch = f->read_buf[f->read_curpos++];
    if (is_base64_char(ch)) return ch;
  }
}

int fileblockdecode_writedata(int c, void *v)
{
  fileblockdecode *f = (fileblockdecode *)v;

  if (f->write_end) return 0;
  if (c >= 0)
    f->write_buf[f->write_curpos++] = c;
  if (f->write_curpos >= f->write_total)
    f->write_end = 1;
  return 1;
}

int file_read_block(FIL *f, const char *header, void *v, uint16_t len)
{
  fileblockdecode *fd;

  fd = (fileblockdecode *)malloc(sizeof(fileblockdecode));
  if (fd == NULL) return 0;

  fd->read_file = f;
  fd->read_filled = 0;
  fd->read_curpos = 0;
  fd->write_buf = (uint8_t *) v;
  fd->write_total = len;
  fd->write_curpos = 0;
  fd->write_end = 0;
  if (file_skip_header(f,header,0))
  {
    base64_decode(fileblockdecode_readdata,(void *)fd,  fileblockdecode_writedata, (void *)fd);
    if (file_skip_header(f,header,1))
     {
        free(fd);
        return 1;
     }
  }
  free(fd);
  return 0;
}
    
#ifdef __cplusplus
}
#endif  

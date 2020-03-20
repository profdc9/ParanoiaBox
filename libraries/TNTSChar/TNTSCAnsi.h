/*

  Copyright (C) 2020 by Daniel Marks

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

#ifndef _TNTSCANSI_H
#define _TNTSCANSI_H

#define loc_in_virtscreen(cur,y,x) (((cur).data)+(((y)*((cur).columns))+(x)))
#define MAX_ANSI_ELEMENTS 16
#define char_to_virtscreen(cur,ch) (((cur).next_char_send)((cur),(ch)))

#undef VTATTRIB

#ifdef VTATTRIB
typedef unsigned short screenchartype;
#else
typedef unsigned char screenchartype;
#endif

class TNTSCAnsi_class;

typedef void(TNTSCAnsi_class::*TNTSCAnsi_char_send_function)(unsigned char ch);

typedef struct _virtscreen
{
  int rows;
  int columns; 
  screenchartype *data;

  int xpos;
  int ypos;
  int top_scroll;
  int bottom_scroll;
  int attrib;
  int old_attrib;
  int old_xpos;
  int old_ypos;

  int cur_ansi_number;
  int ansi_elements;
  int ansi_reading_number;
  int ansi_element[MAX_ANSI_ELEMENTS];
  unsigned short *xpos_cursor;
  unsigned short *ypos_cursor;
  TNTSCAnsi_char_send_function next_char_send;
} virtscreen;

class TNTSCAnsi_class {    
  public:
	void begin(void *buf, int rows, int columns);
    void end();                            
//  private:
    struct _virtscreen cur;
	void clear_region(int y1, int x1, int y2, int x2, int atrb);
	void clear_virtscreen(int mode);
	void clear_to_eol(int mode);
	void move_cursor();
	void position_console(int ypos, int xpos, int rel);
	void delete_chars_in_line(int char_move);
	void scroll_virt_up_at_cursor(int dir);
	void scroll_virtscreen();
	void set_scroll_region(int low, int high);
	void change_attribute(int attrib);
	void change_character(int ch);
	void special_ansi_charset_0(unsigned char ch);
	void special_ansi_charset_1(unsigned char ch);
	void std_interpret_ansi(unsigned char ch);
	void std_interpret_char(unsigned char ch);
	void special_reading_ansi(unsigned char ch);
	void output_character(unsigned char ch);
	void set_cursor_ptr(unsigned short *xpos, unsigned short *ypos);
};

extern TNTSCAnsi_class TNTSCAnsi; 

extern virtscreen *newscr;


#endif  /* _TNTSCANSI_H */

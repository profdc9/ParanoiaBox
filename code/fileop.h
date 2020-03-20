#ifndef _FILEOP_H
#define _FILEOP_H

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

#include <ff.h>

#ifdef __cplusplus
extern "C" {
#endif  

void file_report_error(const char *error_message);
void file_edit(void);
void file_new(void);
void file_mount_volume(uint8_t unmount);
int file_select(const char *message, const char *dir, uint8_t seldir, char *selected, int maxlen);
int file_select_card(const char *message, char *filename, int maxlen, int seldir);
void file_view_display(const char *filename, int rows, int cols);
void file_view(void);
void file_delete(void);
int file_select_ciphertext(const char *message, uint8_t seldir, char *selected, int maxlen);
int file_select_plaintext(const char *message,uint8_t seldir, char *selected, int maxlen);
int file_enter_filename(const char *message, char *filename, int len);
void file_write_header(FIL *f, const char *header, int term);
int file_skip_header(FIL *f, const char *header, int term);
int file_write_block(FIL *f, const char *header, void *v, uint16_t len);
int file_read_block(FIL *f, const char *header, void *v, uint16_t len);

extern uint8_t fs0_mounted;
extern uint8_t fs1_mounted;

#ifdef __cplusplus
}
#endif  

#endif  /* _FILEOP_H */

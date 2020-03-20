#ifndef _ee_h
#define _ee_h

#ifdef __cplusplus
extern "C" {
#endif	

void editor(void);

#define ESTATIC static
#define EDITORTITLE "Editor"

typedef int (*readfile)(void *v);
typedef int (*writefile)(int c, void *v);

extern readfile rf;
extern void *rf_ptr;
extern writefile wf;
extern void *wf_ptr;

#define AMAX  0x1800	/* main buffer size */
#define BMAX  0x200 	/* block size */

#ifdef __cplusplus
}
#endif	

#endif /* _ee_h */

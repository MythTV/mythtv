#ifndef LANG_H
#define LANG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vt.h"

extern int latin1;
extern const char lang_chars[][16]; /* from vbilut.cpp */

struct enhance
{
    int next_des;      // next expected designation code
    unsigned int trip[13*16];  // tripplets
};

void lang_init(void);
void conv2latin(unsigned char *p, int n, int lang);

void init_enhance(struct enhance *eh);
void add_enhance(struct enhance *eh, int dcode, unsigned int *data);
void enhance(struct enhance *eh, struct vt_page *vtp);

#ifdef __cplusplus
}
#endif

#endif // LANG_H


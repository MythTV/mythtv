#ifndef LANG_H
#define LANG_H

#include "captions/vbilut.h"
#include "vt.h"

extern int isLatin1;

struct enhance
{
    int next_des;      // next expected designation code
    unsigned int trip[13*16];  // tripplets
};

void lang_init(void);
void conv2latin(unsigned char *p, int n, int lang);

void init_enhance(struct enhance *eh);
void add_enhance(struct enhance *eh, int dcode, std::array<unsigned int,13>& data);
void do_enhancements(struct enhance *eh, struct vt_page *vtp);

#endif // LANG_H


#ifndef _VBILUT_H_
#define _VBILUT_H_

#include <stdint.h>

extern "C" const char  lang_chars[][16];
extern const char           chartab_original[];
extern const char           chartab_swedish[];
extern const unsigned short hammtab[];
extern const uint8_t        hamm84tab[];
extern const uint8_t        unham84tab[];
extern const uint8_t        vbi_bit_reverse[];
extern const char          *formats[];
extern const char          *subtitles[];
extern const char           hamm24par[][256];
extern const char           hamm24val[];
extern const short          hamm24err[];
extern const int            hamm24cor[];

enum vbimode
{
    VBI_IVTV,        /// < IVTV packet
    VBI_DVB,         /// < DVB packet
    VBI_DVB_SUBTITLE /// < DVB subtitle packet
};

int hamm8(const uint8_t *p, int *err);
int hamm84(const uint8_t *p, int *err);
int hamm16(const uint8_t *p, int *err);

#endif // _VBILUT_H_

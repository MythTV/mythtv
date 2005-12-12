#ifndef _VBILUT_H_
#define _VBILUT_H_

#include <stdint.h>

extern const unsigned char  lang_chars[][16];
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

#endif // _VBILUT_H_

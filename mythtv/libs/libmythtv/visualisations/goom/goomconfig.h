#ifndef GOOMCONFIG_H
#define GOOMCONFIG_H

#include <cstdint>

//#define VERSION "1.9.2"
//#define _DEBUG

#define COLOR_BGRA
//#define COLOR_ARGB

enum COLOR : uint8_t {
#ifdef COLOR_BGRA
/** position des composantes **/
    ROUGE = 2,
    BLEU = 0,
    VERT = 1,
    ALPHA = 3,
#else
    ROUGE = 1,
    BLEU = 3,
    VERT = 2,
    ALPHA = 0,
#endif
};
		

// target
#define XMMS_PLUGIN
//#define STANDALONE

//#define HAVE_ATHLON

//#define VERBOSE

#endif // GOOMCONFIG_H

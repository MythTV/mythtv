
//#define VERSION "1.9.2"
//#define _DEBUG

#define COLOR_BGRA
//#define COLOR_ARGB

#ifdef COLOR_BGRA
/** position des composantes **/
    #define ROUGE 2
    #define BLEU 0
    #define VERT 1
    #define ALPHA 3
#else
    #define ROUGE 1
    #define BLEU 3
    #define VERT 2
    #define ALPHA 0
#endif
		

// target
#define XMMS_PLUGIN
//#define STANDALONE

//#define POWERPC

//#define HAVE_ATHLON

//#define VERBOSE

#ifndef guint32
#define guint8 unsigned char
#define guin16 unsigned short
#define guint32 unsigned int
#define gint8 signed char
#define gint16 signed short int
#define gint32 signed int
#endif

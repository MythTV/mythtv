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
		
// for pc users with mmx processors.
#define HAVE_MMX

//#define VERBOSE

#ifndef guint32
#define guint8 unsigned char
#define guin16 unsigned short
#define guint32 unsigned int
#define gint8 signed char
#define gint16 signed short int
#define gint32 signed int
#endif

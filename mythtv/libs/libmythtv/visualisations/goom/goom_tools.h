#ifndef GOOMTOOLS_H
#define GOOMTOOLS_H

#include <cinttypes>
#include <cstddef>
#include <cstdlib>

#if !defined( M_PI ) 
static constexpr double M_PI    {  3.14159265358979323846  };
#endif
static constexpr float  M_PI_F  { static_cast<float>(M_PI) };

static constexpr size_t NB_RAND { 0x10000 };

/* in graphic.c */
extern int *rand_tab;
static unsigned short rand_pos;

static inline void RAND_INIT(int i)
{
    srand (i) ;
    if (!rand_tab)
        rand_tab = (int *) malloc (NB_RAND * sizeof(int)) ;
    rand_pos = 1 ;
    while (rand_pos != 0)
        rand_tab[rand_pos++] = rand () ;
}

static inline int RAND(void)
{
    ++rand_pos;
    return rand_tab[rand_pos];
}

static inline void RAND_CLOSE()
{
    free(rand_tab);
    rand_tab = nullptr;
}

//#define iRAND(i) ((guint32)((float)i * RAND()/RAND_MAX))
static inline unsigned int iRAND(int i)
    { return static_cast<unsigned int>(RAND()) % i; }

//inline unsigned int RAND(void);
//inline unsigned int iRAND(int i);

#endif // GOOMTOOLS_H

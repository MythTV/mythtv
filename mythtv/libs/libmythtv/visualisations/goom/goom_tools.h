#ifndef GOOMTOOLS_H
#define GOOMTOOLS_H

#if !defined( M_PI ) 
 #define M_PI 3.14159265358979323846 
#endif
#define M_PI_F ((float)M_PI)

#define NB_RAND 0x10000

/* in graphic.c */
extern int *rand_tab;
static unsigned short rand_pos;

#define RAND_INIT(i) \
	srand (i) ;\
	if (!rand_tab) rand_tab = (int *) malloc (NB_RAND * sizeof(int)) ;\
	rand_pos = 1 ;\
	while (rand_pos != 0) rand_tab [rand_pos++] = rand () ;


static inline int RAND(void) {
	++rand_pos;
	return rand_tab[rand_pos];
}

#define RAND_CLOSE()\
	free (rand_tab);\
	rand_tab = nullptr;


//#define iRAND(i) ((guint32)((float)i * RAND()/RAND_MAX))
#define iRAND(i) (((uint)RAND())%(i))

//inline unsigned int RAND(void);
//inline unsigned int iRAND(int i);

#endif // GOOMTOOLS_H

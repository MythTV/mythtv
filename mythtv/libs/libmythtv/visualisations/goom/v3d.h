#ifndef V3D_H
#define V3D_H

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "mathtools.h"

struct v3d {
	float x,y,z;
};

struct v2d {
	int x,y;
};

/* 
 * projete le vertex 3D sur le plan d'affichage
 * retourne (0,0) si le point ne doit pas etre affiche.
 *
 * bonne valeur pour distance : 256
 */
#define V3D_TO_V2D(v3,v2,width,height,distance) \
{ \
	int Xp; \
	int Yp; \
  if ((v3).z > 2) { \
	 F2I(((distance) * (v3).x / (v3).z),Xp) ; \
	 F2I(((distance) * (v3).y / (v3).z),Yp) ; \
	 (v2).x = Xp + ((width)>>1); \
	 (v2).y = -Yp + ((height)>>1); \
  } \
  else (v2).x=(v2).y=-666; \
}

/*
 * rotation selon Y du v3d vi d'angle a (cosa=cos(a), sina=sin(a))
 * centerz = centre de rotation en z
 */
#define Y_ROTATE_V3D(vi,vf,sina,cosa)\
{\
 (vf).x = ((vi).x * (cosa)) - ((vi).z * (sina));\
 (vf).z = ((vi).x * (sina)) + ((vi).z * (cosa));\
 (vf).y = (vi).y;\
}

/*
 * translation
 */
#define TRANSLATE_V3D(vsrc,vdest)\
{\
 (vdest).x += (vsrc).x;\
 (vdest).y += (vsrc).y;\
 (vdest).z += (vsrc).z;\
}

#define MUL_V3D(lf,v) {(v).x*=(lf);(v).y*=(lf);(v).z*=(lf);}

#endif // V3D_H

#ifndef _SURFACE_H
#define _SURFACE_H

struct Surface {
  int * buf;
  int width;
  int height;
  int size;

  int * realstart;
};

Surface * surface_new (int w, int h) ;
void surface_delete (Surface **s) ;

#endif

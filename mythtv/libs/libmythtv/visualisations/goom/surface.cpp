#include "surface.h"
#include <stdlib.h>
#include <inttypes.h>	// added for amd64 support

Surface * surface_new (int w, int h) {
  Surface * s = (Surface*)malloc(sizeof(Surface));
  s->realstart = (int*)malloc(w*h*4 + 128);
  s->buf = (int*)((uintptr_t)s->realstart + 128 - (((uintptr_t)s->realstart) % 128));
  s->size = w*h;
  s->width = w;
  s->height = h;
  return s;
}

void surface_delete (Surface **s) {
  free ((*s)->realstart);
  free (*s);
  *s = NULL;
}

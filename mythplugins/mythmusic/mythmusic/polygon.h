#ifndef POLYGON_H_
#define POLYGON_H_

#include <string.h>

template<class Pixel>
struct Bitmap {
  int width, height, extra;
  Pixel *data;

  Bitmap(int e=0) : extra(e), data(0) { };
  ~Bitmap() { delete[] data; };

  void size(int w,int h) {
    delete[] data;
    width = w;
    height = h;
    data = new Pixel[w*h+extra];
    clear();
  }
  
  void clear() {
    memset(data,0,sizeof(Pixel)*(width*height+extra));
  }
};
#endif

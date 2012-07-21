#ifndef POLYGON_H_
#define POLYGON_H_

#include <string.h>

template<typename Pixel>
class Bitmap
{
  public:
    int width, height, extra;
    Pixel *data;

    Bitmap(int e = 0) : width(0), height(0), extra(e), data(NULL) { }
    ~Bitmap() { delete[] data; }

    void size(int w,int h)
    {
        delete[] data;
        width = w;
        height = h;
        data = new Pixel[2*w*h+extra];
        clear();
    }
  
    void clear()
    {
        memset(data,0,sizeof (Pixel)*(2*width*height+extra));
    }
};
#endif

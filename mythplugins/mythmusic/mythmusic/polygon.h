#ifndef POLYGON_H_
#define POLYGON_H_

#include <cstring>

template<typename Pixel>
class Bitmap
{
  public:
    int width   { 0 };
    int height  { 0 };
    int extra;
    Pixel *data { nullptr };

    explicit Bitmap(int e = 0) : extra(e) {}
    ~Bitmap() { delete[] data; }

    void size(int w,int h)
    {
        delete[] data;
        width = w;
        height = h;
        data = new Pixel[(2*w*h)+extra];
        clear();
    }
  
    void clear()
    {
        memset(data,0,sizeof (Pixel)*(2*width*height+extra));
    }
};
#endif

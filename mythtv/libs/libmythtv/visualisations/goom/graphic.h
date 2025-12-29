#ifndef GRAPHIC_H
#define GRAPHIC_H

struct Color
{
	unsigned short r, v, b;
};

extern const Color BLACK;
extern const Color WHITE;
extern const Color RED;
extern const Color BLUE;
extern const Color GREEN;
extern const Color YELLOW;
extern const Color ORANGE;
extern const Color VIOLET;

//inline void setPixelRGB (unsigned int * buffer, unsigned int x, unsigned int y, Color c);
//inline void getPixelRGB (unsigned int * buffer, unsigned int x, unsigned int y, Color * c);

#endif /* GRAPHIC_H */

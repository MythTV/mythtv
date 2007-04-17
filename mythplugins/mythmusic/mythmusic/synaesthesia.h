#ifndef SYNAETHESIA
#define SYNAETHESIA

#include <mythtv/visual.h>
#include "polygon.h"
#include "mainvisual.h"
#include "config.h"

#ifdef SDL_SUPPORT
#include <SDL.h>
#endif

class QImage;

#define LogSize 10
#define Brightness 150
#define NumSamples (1<<LogSize)

#define Flame 0
#define Wave 1
#define Stars 2

class Synaesthesia : public VisualBase
{
public:
    Synaesthesia(long int winid);
    virtual ~Synaesthesia();

    void resize(const QSize &size);
    bool process(VisualNode *node);
    bool draw(QPainter *p, const QColor &back);
    void handleKeyPress(const QString &action) {(void) action;}

private:
    void setupPalette(void);
    void coreInit(void);
    int bitReverser(int i);
    void fft(double *x, double *y);
    void setStarSize(double lsize);

    inline void addPixel(int x, int y, int br1, int br2);
    inline void addPixelFast(unsigned char *p, int br1, int br2);
    inline unsigned char getPixel(int x, int y, int where);

    inline void fadePixelWave(int x, int y, int where, int step);
    void fadeWave(void);
    inline void fadePixelHeat(int x, int y, int where, int step);
    void fadeHeat(void);
    void fadeFade(void);
    void fade(void);

    QSize size;

    double cosTable[NumSamples];
    double negSinTable[NumSamples];
    int bitReverse[NumSamples];
    int scaleDown[256];
    int maxStarRadius;
    int fadeMode;
    bool pointsAreDiamonds;
    double brightnessTwiddler;
    double starSize;

    int outWidth;
    int outHeight;

    Bitmap<unsigned short> outputBmp, lastOutputBmp, lastLastOutputBmp;
    QImage *outputImage;

    unsigned char palette[768];
    double fgRedSlider, fgGreenSlider, bgRedSlider, bgGreenSlider;

    double energy_avg;
#ifdef SDL_SUPPORT
    SDL_Surface *surface;
#endif
};

#endif // SYNAETHESIA

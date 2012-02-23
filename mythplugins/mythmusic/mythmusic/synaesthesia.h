#ifndef SYNAETHESIA
#define SYNAETHESIA

#include <visual.h>
#include "polygon.h"
#include "mainvisual.h"
#include "config.h"

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
    Synaesthesia(void);
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

    QSize m_size;

    double m_cosTable[NumSamples];
    double m_negSinTable[NumSamples];
    int m_bitReverse[NumSamples];
    int m_scaleDown[256];
    int m_maxStarRadius;
    int m_fadeMode;
    bool m_pointsAreDiamonds;
    double m_brightnessTwiddler;
    double m_starSize;

    int m_outWidth;
    int m_outHeight;

    Bitmap<unsigned short> m_outputBmp, m_lastOutputBmp, m_lastLastOutputBmp;
    QImage *m_outputImage;

    unsigned char m_palette[768];
    double m_fgRedSlider, m_fgGreenSlider, m_bgRedSlider, m_bgGreenSlider;

    double m_energy_avg;
};

#endif // SYNAETHESIA

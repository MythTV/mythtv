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

    void resize(const QSize &size) override; // VisualBase
    bool process(VisualNode *node) override; // VisualBase
    bool draw(QPainter *p, const QColor &back) override; // VisualBase
    void handleKeyPress(const QString &action) override // VisualBase
        {(void) action;}

private:
    void setupPalette(void);
    void coreInit(void);
    static int bitReverser(int i);
    void fft(double *x, double *y);
    void setStarSize(double lsize);

    inline void addPixel(int x, int y, int br1, int br2);
    static inline void addPixelFast(unsigned char *p, int br1, int br2);
    inline unsigned char getPixel(int x, int y, int where);

    inline void fadePixelWave(int x, int y, int where, int step);
    void fadeWave(void);
    inline void fadePixelHeat(int x, int y, int where, int step);
    void fadeHeat(void);
    void fadeFade(void);
    void fade(void);

    QSize m_size                 {0,0};

    double m_cosTable[NumSamples];
    double m_negSinTable[NumSamples];
    int    m_bitReverse[NumSamples];
    int    m_scaleDown[256];
    int    m_maxStarRadius       {1};
    int    m_fadeMode            {Stars};
    bool   m_pointsAreDiamonds   {true};
    double m_brightnessTwiddler  {0.3};
    double m_starSize            {0.5};

    int    m_outWidth            {0};
    int    m_outHeight           {0};

    Bitmap<unsigned short> m_outputBmp, m_lastOutputBmp, m_lastLastOutputBmp;
    QImage *m_outputImage        {nullptr};

    unsigned char m_palette[768];
    double m_fgRedSlider         {0.0};
    double m_fgGreenSlider       {0.5};
    double m_bgRedSlider         {0.75};
    double m_bgGreenSlider       {0.4};

    double m_energy_avg          {80.0};
};

#endif // SYNAETHESIA

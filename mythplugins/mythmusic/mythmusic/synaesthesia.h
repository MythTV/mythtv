#ifndef SYNAETHESIA
#define SYNAETHESIA

// C++
#include <array>

// MythTV
#include <libmyth/visual.h>

// MythMusic
#include "mainvisual.h"
#include "polygon.h"

class QImage;

static constexpr size_t  LogSize    {         10 };
static constexpr size_t  NumSamples { 1<<LogSize };
static constexpr uint8_t Brightness {        150 };

using samp_dbl_array = std::array<double,NumSamples>;
using samp_int_array = std::array<int,NumSamples>;

enum Mode : std::uint8_t {
    Flame = 0,
    Wave  = 1,
    Stars = 2
};

class Synaesthesia : public VisualBase
{
public:
    Synaesthesia(void);
    ~Synaesthesia() override;

    void resize(const QSize &size) override; // VisualBase
    bool process(VisualNode *node) override; // VisualBase
    bool draw(QPainter *p, const QColor &back) override; // VisualBase
    void handleKeyPress([[maybe_unused]] const QString &action) override {}; // VisualBase

private:
    void setupPalette(void);
    void coreInit(void);
    static int bitReverser(int i);
    void fft(double *x, double *y);
    void setStarSize(double lsize);

    inline void addPixel(int x, int y, int br1, int br2) const;
    static inline void addPixelFast(unsigned char *p, int br1, int br2);
    inline unsigned char getPixel(int x, int y, int where) const;

    inline void fadePixelWave(int x, int y, int where, int step);
    void fadeWave(void);
    inline void fadePixelHeat(int x, int y, int where, int step);
    void fadeHeat(void);
    void fadeFade(void) const;
    void fade(void);

    QSize m_size                 {0,0};

    samp_dbl_array      m_cosTable    {};
    samp_dbl_array      m_negSinTable {};
    samp_int_array      m_bitReverse  {};
    std::array<int,256> m_scaleDown   {};
    int    m_maxStarRadius       {1};
    int    m_fadeMode            {Stars};
    bool   m_pointsAreDiamonds   {true};
    double m_brightnessTwiddler  {0.3};
    double m_starSize            {0.5};

    int    m_outWidth            {0};
    int    m_outHeight           {0};

    Bitmap<unsigned short> m_outputBmp;
    Bitmap<unsigned short> m_lastOutputBmp;
    Bitmap<unsigned short> m_lastLastOutputBmp;
    QImage *m_outputImage        {nullptr};

    std::array<uint8_t,768>  m_palette {};
    double m_fgRedSlider         {0.0};
    double m_fgGreenSlider       {0.5};
    double m_bgRedSlider         {0.75};
    double m_bgGreenSlider       {0.4};

    double m_energyAvg           {80.0};
};

#endif // SYNAETHESIA

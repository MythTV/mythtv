// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//
// modified 12-2004 by Kyle Schlansker to add 64 bit support
//

// C++
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

// Qt
#include <QCoreApplication>
#include <QPainter>
#include <QImage>

// MythTV
#include <libmythbase/compat.h>
#include <libmythbase/mythlogging.h>

// MythMusic
#include "mainvisual.h"
#include "synaesthesia.h"

Synaesthesia::Synaesthesia(void)
{
    m_fps = 29;

    coreInit();              // init cosTable, negSinTable, bitReverse
    setStarSize(m_starSize); // init scaleDown, maxStarRadius
    setupPalette();          // init palette
}

Synaesthesia::~Synaesthesia()
{
    delete m_outputImage;
}

//static constexpr uint8_t sBOUND(int x)
//    { return std::clamp(x, 0, 255); };
//static constexpr uint8_t sPEAKIFY(int x)
//    {return sBOUND(x - x*(255-x)/255/2); };

void Synaesthesia::setupPalette(void)
{
    double fgRed = m_fgRedSlider;
    double fgGreen = m_fgGreenSlider;
    double fgBlue = 1.0 - std::max(m_fgRedSlider,m_fgGreenSlider);
    //double scale = std::max({fgRed,fgGreen,fgBlue});
    double scale = (fgRed + fgGreen + fgBlue) / 2.0;
    fgRed /= scale;
    fgGreen /= scale;
    fgBlue /= scale;
  
    double bgRed = m_bgRedSlider;
    double bgGreen = m_bgGreenSlider;
    double bgBlue = 1.0 - std::max(m_bgRedSlider,m_bgGreenSlider);
    //scale = std::max({bgRed,bgGreen,bgBlue});
    scale = (bgRed + bgGreen + bgBlue) / 2.0;
    bgRed /= scale;
    bgGreen /= scale;
    bgBlue /= scale;

    for (int i = 0; i < 256; i++) {
        int f = i & 15;
        int b = i / 16;
        //m_palette[i * 3 + 0] = sPEAKIFY(b*bgRed*16+f*fgRed*16);
        //m_palette[i * 3 + 1] = sPEAKIFY(b*bgGreen*16+f*fgGreen*16);
        //m_palette[i * 3 + 2] = sPEAKIFY(b*bgBlue*16+f*fgBlue*16);

        double red = (b * bgRed * 16) + (f * fgRed * 16);
        double green = (b * bgGreen * 16) + (f * fgGreen * 16);
        double blue = (b * bgBlue * 16) + (f * fgBlue * 16);

        double excess = 0.0;
        for (int j = 0; j < 5; j++)
        {
            red += excess / 3;
            green += excess / 3;
            blue += excess / 3;
            excess = 0.0;


            if (red > 255) { excess += red - 255; red = 255; }
            if (green > 255) { excess += green - 255; green = 255; }
            if (blue > 255) { excess += blue - 255; blue = 255; }
        }

        double scale2 = (0.5 + (red + green + blue) / 768.0) / 1.5;
        red *= scale2;
        green *= scale2;
        blue *= scale2;

        m_palette[(i * 3) + 0] = std::clamp(int(red), 0, 255);
        m_palette[(i * 3) + 1] = std::clamp(int(green), 0, 255);
        m_palette[(i * 3) + 2] = std::clamp(int(blue), 0, 255);
    }
}

void Synaesthesia::resize(const QSize &newsize)
{
    m_size = newsize;

    m_size.setHeight(m_size.height() / 2);
    m_size.setWidth((m_size.width() / 4) * 4);
    m_outputBmp.size(m_size.width(), m_size.height());
    m_lastOutputBmp.size(m_size.width(), m_size.height());
    m_lastLastOutputBmp.size(m_size.width(), m_size.height());
    m_outWidth = m_size.width();
    m_outHeight = m_size.height();

    delete m_outputImage;

    m_size.setHeight(m_size.height() * 2);
    m_outputImage = new QImage(m_size, QImage::Format_Indexed8);

    if (!m_outputImage) 
    {
        LOG(VB_GENERAL, LOG_ERR,
            "outputImage in Synaesthesia::resize() is NULL");
        return;
    }

    for (size_t i = 0; i < 256; i++)
        m_outputImage->setColor(i, qRgba(m_palette[i * 3], m_palette[(i * 3) + 1],
                                      m_palette[(i * 3) + 2], 255));

#if 0
    surface = SDL_SetVideoMode(size.width(), size.height(), 8, 0);

    if (!surface)
    {
        LOG(VB_GENERAL, LOG_ERR, "Couldn't get SDL surface");
        return;
    }

    SDL_Color sdlPalette[256];
    
    for (int i = 0; i < 256; i++)
    {
        sdlPalette[i].r = m_palette[i * 3];
        sdlPalette[i].g = m_palette[i * 3 + 1];
        sdlPalette[i].b = m_palette[i * 3 + 2];
    }

    SDL_SetColors(surface, sdlPalette, 0, 256);
#endif
}

int Synaesthesia::bitReverser(int i)
{
    int sum = 0;
    for (size_t j = 0; j < LogSize; j++)
    {
        sum = (i & 1) + (sum * 2);
        i >>= 1;
    }
    
    return sum;
}

void Synaesthesia::fft(double *x, double *y)
{
    size_t n2 = NumSamples;
    for (size_t twoToTheK = 1; twoToTheK < NumSamples; twoToTheK *= 2)
    {
        size_t n1 = n2;
        n2 /= 2;
        for (size_t j = 0; j < n2; j++)
        {
            double c = m_cosTable[(j * twoToTheK) & (NumSamples - 1)];
            double s = m_negSinTable[(j * twoToTheK) & (NumSamples - 1)];
            for (size_t i = j; i < NumSamples; i += n1)
            {
                size_t l = i + n2;
                double xt = x[i] - x[l];
                x[i] = (x[i] + x[l]);
                double yt = y[i] - y[l];
                y[i] = (y[i] + y[l]);
                x[l] = (xt * c) - (yt * s);
                y[l] = (xt * s) + (yt * c);
            }
        }
    }
}

void Synaesthesia::setStarSize(double lsize)
{
    double fadeModeFudge { 0.78 };
    if (m_fadeMode == Wave)
        fadeModeFudge = 0.4;
    else if (m_fadeMode == Flame)
        fadeModeFudge = 0.6;

    int factor = 0;
    if (lsize > 0.0)
        factor = int(exp(log(fadeModeFudge) / (lsize * 8.0)) * 255);
    factor = std::min(factor, 255);

    for (int i = 0; i < 256; i++)
        m_scaleDown[i] = i * factor>>8;

    m_maxStarRadius = 1;
    for (int i = 255; i; i = m_scaleDown[i])
        m_maxStarRadius++;
}

void Synaesthesia::coreInit(void)
{
    for (size_t i = 0; i < NumSamples; i++)
    {
        m_negSinTable[i] = -sin(3.141592 * 2.0 / NumSamples * i);
        m_cosTable[i] = cos(3.141592 * 2.0 / NumSamples * i);
        m_bitReverse[i] = bitReverser(i);
    }
}

#define output ((unsigned char*)m_outputBmp.data)
#define lastOutput ((unsigned char*)m_lastOutputBmp.data)
#define lastLastOutput ((unsigned char*)m_lastLastOutputBmp.data)

void Synaesthesia::addPixel(int x, int y, int br1, int br2) const
{
    if (x < 0 || x > m_outWidth || y < 0 || y >= m_outHeight)
        return;

    unsigned char *p = output + (static_cast<ptrdiff_t>(x) * 2) +
        (static_cast<ptrdiff_t>(y) * m_outWidth * 2);
    if (p[0] + br1 < 255)
        p[0] += br1;
    else
        p[0] = 255;
    if (p[1] + br2 < 255)
        p[1] += br2;
    else
        p[1] = 255;
}

void Synaesthesia::addPixelFast(unsigned char *p, int br1, int br2)
{
    if (p[0] + br1 < 255)
        p[0] += br1;
    else
        p[0] = 255;
    if (p[1] + br2 < 255)
        p[1] += br2;
    else
        p[1] = 255;
}

unsigned char Synaesthesia::getPixel(int x, int y, int where) const
{
    if (x < 0 || y < 0 || x >= m_outWidth || y >= m_outHeight)
        return 0;
    
    return lastOutput[where];
}

void Synaesthesia::fadeFade(void) const
{
    auto *ptr = (uint32_t *)output;
    for (int i = static_cast<ptrdiff_t>(m_outWidth) * m_outHeight * 2 / sizeof(uint32_t);
         i > 0; i--)
    {
        uint32_t x = *ptr;
        if (x)
        {
            *(ptr++) = x - ((x & (uintptr_t)0xf0f0f0f0) >> 4) -
                           ((x & (uintptr_t)0xe0e0e0e0) >> 5);
        }
        else
        {
            ptr++;
        }
    }
}

void Synaesthesia::fadePixelWave(int x, int y, int where, int step)
{
    short j = short((int(getPixel(x - 1, y, where - 2)) +
                     int(getPixel(x + 1, y, where + 2)) + 
                     int(getPixel(x, y - 1, where - step)) + 
                     int(getPixel(x, y + 1, where + step))) >> 2) +
        lastOutput[where];

    if (!j)
    {
        output[where] = 0;
        return;
    }
    j = j - lastLastOutput[where] - 1;
    if (j < 0)
        output[where] = 0;
    else if (j & (255 * 256))
        output[where] = 255;
    else
        output[where] = j;
}

void Synaesthesia::fadeWave(void) 
{
    unsigned short *t = m_lastLastOutputBmp.data;
    m_lastLastOutputBmp.data = m_lastOutputBmp.data;
    m_lastOutputBmp.data = m_outputBmp.data;
    m_outputBmp.data = t;

    int step = m_outWidth*2;
    for (int x = 0, i = 0, j = m_outWidth * (m_outHeight - 1) * 2;
         x < m_outWidth; x++, i += 2, j += 2) 
    {
        fadePixelWave(x, 0, i, step);
        fadePixelWave(x, 0, i + 1, step);
        fadePixelWave(x, m_outHeight - 1, j, step);
        fadePixelWave(x, m_outHeight - 1, j + 1, step);
    }

    for (int y = 1, i = m_outWidth * 2, j = (m_outWidth * 4) - 2;
         y < m_outHeight; y++, i += step, j += step) 
    {
        fadePixelWave(0, y, i, step);
        fadePixelWave(0, y, i + 1, step);
        fadePixelWave(m_outWidth - 1, y, j, step);
        fadePixelWave(m_outWidth - 1, y, j + 1, step);
    }

    for (int y = 1, start = (m_outWidth * 2) + 2, end = (m_outWidth * 4) - 2;
         y < m_outHeight - 1; y++, start += step, end += step) 
    {
        for (int i2 = start; i2 < end; i2++)
        {
            short j2 = short((int(lastOutput[i2 - 2]) +
                              int(lastOutput[i2 + 2]) +
                              int(lastOutput[i2 - step]) +
                              int(lastOutput[i2 + step])) >> 2) +
                lastOutput[i2];
            if (!j2)
            {
                output[i2] = 0;
            }
            else
            {
                j2 = j2 - lastLastOutput[i2] - 1;
                if (j2 < 0)
                    output[i2] = 0;
                else if (j2 & (255*256))
                    output[i2] = 255;
                else
                    output[i2] = j2;
            }
        }
    }
}

void Synaesthesia::fadePixelHeat(int x, int y, int where, int step) 
{
    short j = short((int(getPixel(x - 1, y, where - 2)) +
                     int(getPixel(x + 1, y, where + 2)) +
                     int(getPixel(x, y - 1, where - step)) +
                     int(getPixel(x, y + 1, where + step))) >> 2) +
        lastOutput[where];
    if (!j) 
    { 
        output[where] = 0; 
        return; 
    }
    j = j -lastLastOutput[where] - 1;
    if (j < 0) 
        output[where] = 0;
    else if (j & (255 * 256)) 
        output[where] = 255;
    else 
        output[where] = j;
}

void Synaesthesia::fadeHeat(void) 
{
    unsigned short *t = m_lastLastOutputBmp.data;
    m_lastLastOutputBmp.data = m_lastOutputBmp.data;
    m_lastOutputBmp.data = m_outputBmp.data;
    m_outputBmp.data = t;

    int step = m_outWidth * 2;
    for (int x = 0, i = 0, j = m_outWidth * (m_outHeight - 1) * 2;
         x < m_outWidth; x++, i += 2, j += 2) 
    {
        fadePixelHeat(x, 0, i, step);
        fadePixelHeat(x, 0, i + 1, step);
        fadePixelHeat(x, m_outHeight - 1, j, step);
        fadePixelHeat(x, m_outHeight - 1, j + 1, step);
    }

    for(int y = 1, i = m_outWidth * 2, j = (m_outWidth * 4) - 2; y < m_outHeight;
        y++, i += step, j += step) 
    {
        fadePixelHeat(0, y, i, step);
        fadePixelHeat(0, y, i + 1, step);
        fadePixelHeat(m_outWidth - 1, y, j, step);
        fadePixelHeat(m_outWidth - 1, y, j + 1, step);
    }

    for(int y = 1, start = (m_outWidth * 2) + 2, end = (m_outWidth * 4) - 2;
        y < m_outHeight - 1; y++, start += step, end += step) 
    {
        for (int i2 = start; i2 < end; i2++)
        {
            short j2 = short((int(lastOutput[i2 - 2]) +
                              int(lastOutput[i2 + 2]) +
                              int(lastOutput[i2 - step]) +
                              int(lastOutput[i2 + step])) >> 2) +
                lastOutput[i2];
            if (!j2)
                output[i2] = 0;
            else
            {
                j2 = j2 - lastLastOutput[i2] +
                    ((lastLastOutput[i2] - lastOutput[i2]) >> 2) - 1;
                if (j2 < 0)
                    output[i2] = 0;
                else if (j2 & (255*256))
                    output[i2] = 255;
                else
                    output[i2] = j2;
            }
        };
    }
}

void Synaesthesia::fade(void) 
{
    switch(m_fadeMode)
    {
        case Stars: fadeFade(); break;
        case Flame: fadeHeat(); break;
        case Wave: fadeWave(); break;
        default: break;
    }
}

bool Synaesthesia::process(VisualNode *node)
{
    fade();

    if (!node)
        return false;

    samp_dbl_array x {};
    samp_dbl_array y {};
    samp_dbl_array a {};
    samp_dbl_array b {};
    samp_int_array clarity {};

    int brightFactor = int(Brightness * m_brightnessTwiddler / (m_starSize + 0.01));

    size_t numSamps = NumSamples;
    if (node->m_length < NumSamples)
        numSamps = node->m_length;

    for (size_t i = 0; i < numSamps; i++)
    {
        x[i] = node->m_left[i];
        if (node->m_right)
            y[i] = node->m_right[i];
        else
            y[i] = x[i];
    }

    fft(x.data(), y.data());

    double energy = 0.0;

    for (size_t i = 0 + 1; i < NumSamples / 2; i++)
    {
        double x1 = x[m_bitReverse[i]];
        double y1 = y[m_bitReverse[i]];
        double x2 = x[m_bitReverse[NumSamples - i]];
        double y2 = y[m_bitReverse[NumSamples - i]];
        double aa = ((x1 + x2) * (x1 + x2)) + ((y1 - y2) * (y1 - y2));
        double bb = ((x1 - x2) * (x1 - x2)) + ((y2 + y2) * (y1 + y2));
        a[i] = sqrt(aa);
        b[i] = sqrt(bb);
        if (aa + bb != 0.0)
        {
            clarity[i] = (int)(((x1 + x2) * (x1 - x2) + (y1 + y2) * (y1 - y2)) /
                         (aa + bb) * 256);
        }
        else
        {
            clarity[i] = 0;
        }

        energy += (aa + bb) * i * i;
    }

    energy = sqrt(energy / NumSamples) / 65536.0;

    //int heightFactor = NumSamples / 2 / outHeight + 1;
    //int actualHeight = NumSamples / 2 / heightFactor;
    //int heightAdd = (outHeight + actualHeight) >> 1;

    double brightFactor2 = (brightFactor / 65536.0 / NumSamples) *
                           sqrt(m_outHeight * m_outWidth / (320.0 * 200.0));

    m_energyAvg = (m_energyAvg * 0.95) + (energy * 0.05);
    if (m_energyAvg > 0.0)
        brightFactor2 *= 80.0 / (m_energyAvg + 5.0);

    for (size_t i = 1; i < NumSamples / 2; i++)
    {
        if (a[i] > 0 || b[i] > 0)
        {
            int h = (int)(b[i] * m_outWidth / (a[i] + b[i]));
            int br = (int)((a[i] + b[i]) * i * brightFactor2);
            int br1 = br * (clarity[i] + 128) >> 8;
            int br2 = br * (128 - clarity[i]) >> 8;
            if (br1 < 0) br1 = 0; else if (br1 > 255) br1 = 255;
            if (br2 < 0) br2 = 0; else if (br2 > 255) br2 = 255;

            int px = h;
            int py = m_outHeight - (i * m_outHeight / (NumSamples / 2));

            if (m_pointsAreDiamonds)
            {
                addPixel(px, py, br1, br2);
                br1 = m_scaleDown[br1];
                br2 = m_scaleDown[br2];

                for(int j = 1; br1 > 0 || br2 > 0;
                    j++, br1 = m_scaleDown[br1], br2 = m_scaleDown[br2])
                {
                    for (int k = 0; k < j; k++)
                    {
                        addPixel(px - j + k,py - k, br1, br2);
                        addPixel(px + k, py - j + k, br1, br2);
                        addPixel(px + j - k, py + k, br1, br2);
                        addPixel(px - k, py + j - k, br1, br2);
                    }
                }
            } 
            else 
            {
                if (px < m_maxStarRadius || py < m_maxStarRadius ||
                    px > m_outWidth - m_maxStarRadius || 
                    py > m_outHeight - m_maxStarRadius) 
                {
                    addPixel(px, py, br1, br2);
                    for(int j = 1; br1 > 0 || br2 > 0;
                        j++, br1 = m_scaleDown[br1], br2 = m_scaleDown[br2])
                    {
                        addPixel(px + j, py, br1, br2);
                        addPixel(px, py + j, br1, br2);
                        addPixel(px - j, py, br1, br2);
                        addPixel(px, py - j, br1, br2);
                    }
                }
                else
                {
                    unsigned char *p = output + (static_cast<ptrdiff_t>(px) * 2) +
                        (static_cast<ptrdiff_t>(py) * m_outWidth * 2);
                    unsigned char *p1 = p;
                    unsigned char *p2 = p;
                    unsigned char *p3 = p;
                    unsigned char *p4 = p;
                    addPixelFast(p, br1, br2);
                    for (; br1 > 0 || br2 > 0; 
                         br1 = m_scaleDown[br1], br2 = m_scaleDown[br2])
                    {
                        p1 += 2;
                        addPixelFast(p1, br1, br2);
                        p2 -= 2;
                        addPixelFast(p2, br1, br2);
                        p3 += static_cast<ptrdiff_t>(m_outWidth) * 2;
                        addPixelFast(p3, br1, br2);
                        p4 -= static_cast<ptrdiff_t>(m_outWidth) * 2;
                        addPixelFast(p4, br1, br2);
                    }
                }
            }
        }
    }

    return false;
}

bool Synaesthesia::draw(QPainter *p, [[maybe_unused]] const QColor &back)
{
    if (!m_outputImage)
        return true;

    auto *ptrOutput = (uint32_t *)output;

    for (int j = 0; j < m_outHeight * 2; j += 2) 
    {
        auto *ptrTop = (uint32_t *)(m_outputImage->scanLine(j));
        auto *ptrBot = (uint32_t *)(m_outputImage->scanLine(j+1));

        for (int i = m_outWidth / 4; i > 0; i--)
        {
            unsigned int const r1 = *(ptrOutput++);
            unsigned int const r2 = *(ptrOutput++);

            unsigned int const v = ((r1 & 0x000000f0UL) >> 4) |
                                   ((r1 & 0x0000f000UL) >> 8) |
                                   ((r1 & 0x00f00000UL) >> 12) |
                                   ((r1 & 0xf0000000UL) >> 16);

            *(ptrTop++) = v | (((r2 & 0x000000f0UL) << 12) |
                               ((r2 & 0x0000f000UL) << 8) |
                               ((r2 & 0x00f00000UL) << 4) |
                               ((r2 & 0xf0000000UL)));

            *(ptrBot++) = v | (((r2 & 0x000000f0UL) << 12) |
                               ((r2 & 0x0000f000UL) << 8) |
                               ((r2 & 0x00f00000UL) << 4) |
                               ((r2 & 0xf0000000UL)));
        }
    }

    p->drawImage(0, 0, *m_outputImage);

    return true;
}

static class SynaesthesiaFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "Synaesthesia");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create([[maybe_unused]] MainVisual *parent,
                       [[maybe_unused]] const QString &pluginName) const override // VisFactory
    {
        return new Synaesthesia();
    }
} SynaesthesiaFactory;

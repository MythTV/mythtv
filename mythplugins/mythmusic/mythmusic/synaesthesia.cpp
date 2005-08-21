// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//
// modified 12-2004 by Kyle Schlansker to add 64 bit support
//

#include "mainvisual.h"
#include "synaesthesia.h"

#include <qpainter.h>
#include <qpixmap.h>
#include <qimage.h>

#include <math.h>
#include <stdlib.h>

#include "config.h"
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <inttypes.h>

#include <iostream>
using namespace std;

Synaesthesia::Synaesthesia(long int winid)
{
    fps = 29;
    fadeMode = Stars;
    pointsAreDiamonds = true;
    energy_avg = 80.0;

    coreInit();
    starSize = 0.5;
    setStarSize(starSize);
    brightnessTwiddler = 0.3;
    outputImage = NULL;
    fgRedSlider = 0.0;
    fgGreenSlider = 0.5;
    bgRedSlider = 0.75;
    bgGreenSlider = 0.4;

#ifdef SDL_SUPPORT
    surface = NULL;

    char SDL_windowhack[32];
    sprintf(SDL_windowhack, "%ld", winid);
    setenv("SDL_WINDOWID", SDL_windowhack, 1);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0)
    {
        cerr << "Unable to init SDL\n";
        return;
    }

    SDL_ShowCursor(0);
#endif

    setupPalette();
}

Synaesthesia::~Synaesthesia()
{
    if (outputImage)
        delete outputImage;
#ifdef SDL_SUPPORT
    SDL_Quit();
    unsetenv("SDL_WINDOWID");
#endif
}

void Synaesthesia::setupPalette(void)
{
#define sBOUND(x) ((x) > 255 ? 255 : (x))
#define sPEAKIFY(x) int(sBOUND((x) - (x)*(255-(x))/255/2))
#define sMAX(x,y) ((x) > (y) ? (x) : (y))
    int i;
    double scale, fgRed, fgGreen, fgBlue, bgRed, bgGreen, bgBlue;
    fgRed = fgRedSlider;
    fgGreen = fgGreenSlider;
    fgBlue = 1.0 - sMAX(fgRedSlider,fgGreenSlider);
    //scale = sMAX(sMAX(fgRed,fgGreen),fgBlue);
    scale = (fgRed + fgGreen + fgBlue) / 2.0;
    fgRed /= scale;
    fgGreen /= scale;
    fgBlue /= scale;
  
    bgRed = bgRedSlider;
    bgGreen = bgGreenSlider;
    bgBlue = 1.0 - sMAX(bgRedSlider,bgGreenSlider);
    //scale = sMAX(sMAX(bgRed,bgGreen),bgBlue);
    scale = (bgRed + bgGreen + bgBlue) / 2.0;
    bgRed /= scale;
    bgGreen /= scale;
    bgBlue /= scale;

    for (i = 0; i < 256; i++) {
        int f = i & 15, b = i / 16;
        //palette[i * 3 + 0] = sPEAKIFY(b*bgRed*16+f*fgRed*16);
        //palette[i * 3 + 1] = sPEAKIFY(b*bgGreen*16+f*fgGreen*16);
        //palette[i * 3 + 2] = sPEAKIFY(b*bgBlue*16+f*fgBlue*16);

        double red = b * bgRed * 16 + f * fgRed * 16;
        double green = b * bgGreen * 16 + f * fgGreen * 16;
        double blue = b * bgBlue * 16 + f * fgBlue * 16;

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

        double scale = (0.5 + (red + green + blue) / 768.0) / 1.5;
        red *= scale;
        green *= scale;
        blue *= scale;

        palette[i * 3 + 0] = sBOUND(int(red));
        palette[i * 3 + 1] = sBOUND(int(green));
        palette[i * 3 + 2] = sBOUND(int(blue));
    }
}

void Synaesthesia::resize(const QSize &newsize)
{
    size = newsize;

    size.setHeight(size.height() / 2);
    size.setWidth((size.width() / 4) * 4);
    outputBmp.size(size.width(), size.height());
    lastOutputBmp.size(size.width(), size.height());
    lastLastOutputBmp.size(size.width(), size.height());
    outWidth = size.width();
    outHeight = size.height();
 
    if (outputImage)
        delete outputImage;

    size.setHeight(size.height() * 2);
    outputImage = new QImage(size, 8, 256);

    if (!outputImage) 
    {
        cerr << "outputImage in Synaesthesia::resize() is NULL" << endl;
        return;
    }

    for (int i = 0; i < 256; i++)
        outputImage->setColor(i, qRgb(palette[i * 3], palette[i * 3 + 1], 
                                      palette[i * 3 + 2]));

#ifdef SDL_SUPPORT
    surface = SDL_SetVideoMode(size.width(), size.height(), 8, 0);

    if (!surface)
    {
        cerr << "Couldn't get SDL surface\n";
        return;
    }

    SDL_Color sdlPalette[256];
    
    for (int i = 0; i < 256; i++)
    {
        sdlPalette[i].r = palette[i * 3];
        sdlPalette[i].g = palette[i * 3 + 1];
        sdlPalette[i].b = palette[i * 3 + 2];
    }

    SDL_SetColors(surface, sdlPalette, 0, 256);
#endif
}

int Synaesthesia::bitReverser(int i)
{
    int sum = 0;
    for (int j = 0; j < LogSize; j++)
    {
        sum = (i & 1) + sum * 2;
        i >>= 1;
    }
	
    return sum;
}

void Synaesthesia::fft(double *x, double *y)
{
    int n2 = NumSamples, n1;
    for (int twoToTheK = 1; twoToTheK < NumSamples; twoToTheK *= 2)
    {
        n1 = n2;
        n2 /= 2;
        for (int j = 0; j < n2; j++)
        {
            double c = cosTable[j * twoToTheK & (NumSamples - 1)],
                   s = negSinTable[j * twoToTheK & (NumSamples - 1)];
            for (int i = j; i < NumSamples; i += n1)
            {
                int l = i + n2;
                double xt = x[i] - x[l];
                x[i] = (x[i] + x[l]);
                double yt = y[i] - y[l];
                y[i] = (y[i] + y[l]);
                x[l] = xt * c - yt * s;
                y[l] = xt * s + yt * c;
            }
        }
    }
}

void Synaesthesia::setStarSize(double lsize)
{
    double fadeModeFudge = (fadeMode == Wave ? 0.4 :
                           (fadeMode == Flame ? 0.6 : 0.78));

    int factor;
    if (lsize > 0.0)
        factor = int(exp(log(fadeModeFudge) / (lsize * 8.0)) * 255);
    else
        factor = 0;

    if (factor > 255) 
        factor = 255;

    for (int i = 0; i < 256; i++)
        scaleDown[i] = i * factor>>8;

    maxStarRadius = 1;
    for (int i = 255; i; i = scaleDown[i])
        maxStarRadius++;
}

void Synaesthesia::coreInit(void)
{
    for (int i = 0; i < NumSamples; i++)
    {
        negSinTable[i] = -sin(3.141592 * 2.0 / NumSamples * i);
        cosTable[i] = cos(3.141592 * 2.0 / NumSamples * i);
        bitReverse[i] = bitReverser(i);
    }
}

#define output ((unsigned char*)outputBmp.data)
#define lastOutput ((unsigned char*)lastOutputBmp.data)
#define lastLastOutput ((unsigned char*)lastLastOutputBmp.data)

void Synaesthesia::addPixel(int x, int y, int br1, int br2)
{
    if (x < 0 || x > outWidth || y < 0 || y >= outHeight)
        return;

    unsigned char *p = output + x * 2 + y * outWidth * 2;
    if (p[0] < 255 - br1)
        p[0] += br1;
    else
        p[0] = 255;
    if (p[1] < 255 - br2)
        p[1] += br2;
    else
        p[1] = 255;
}

void Synaesthesia::addPixelFast(unsigned char *p, int br1, int br2)
{
    if (p[0] < 255 - br1)
        p[0] += br1;
    else
        p[0] = 255;
    if (p[1] < 255 - br2)
        p[1] += br2;
    else
        p[1] = 255;
}

unsigned char Synaesthesia::getPixel(int x, int y, int where)
{
    if (x < 0 || y < 0 || x >= outWidth || y >= outHeight)
        return 0;
	
    return lastOutput[where];
}

void Synaesthesia::fadeFade(void)
{
    register uint32_t *ptr = (uint32_t *)output;
    int i = outWidth * outHeight * 2 / sizeof(uint32_t);
    do {
        uint32_t x = *ptr;
        if (x)
            *(ptr++) = x - ((x & (uintptr_t)0xf0f0f0f0) >> 4) -
                           ((x & (uintptr_t)0xe0e0e0e0) >> 5);
        else
            ptr++;
    } while (--i > 0);
}

void Synaesthesia::fadePixelWave(int x, int y, int where, int step)
{
    short j = (short(getPixel(x - 1, y, where - 2)) + 
                     getPixel(x + 1, y, where + 2) + 
                     getPixel(x, y - 1, where - step) + 
                     getPixel(x, y + 1, where + step) >> 2) + lastOutput[where];
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
    unsigned short *t = lastLastOutputBmp.data;
    lastLastOutputBmp.data = lastOutputBmp.data;
    lastOutputBmp.data = outputBmp.data;
    outputBmp.data = t;

    int x, y, i, j, start, end;
    int step = outWidth*2;
    for (x = 0, i = 0, j = outWidth * (outHeight - 1) * 2; 
         x < outWidth; x++, i += 2, j += 2) 
    {
        fadePixelWave(x, 0, i, step);
        fadePixelWave(x, 0, i + 1, step);
        fadePixelWave(x, outHeight - 1, j, step);
        fadePixelWave(x, outHeight - 1, j + 1, step);
    }

    for (y = 1, i = outWidth * 2, j = outWidth * 4 - 2; 
         y < outHeight; y++, i += step, j += step) 
    {
        fadePixelWave(0, y, i, step);
        fadePixelWave(0, y, i + 1, step);
        fadePixelWave(outWidth - 1, y, j, step);
        fadePixelWave(outWidth - 1, y, j + 1, step);
    }

    for (y = 1, start = outWidth * 2 + 2, end = outWidth * 4 - 2; 
         y < outHeight - 1; y++, start += step, end += step) 
    {
        int i = start;
        do {
            short j = (short(lastOutput[i - 2]) + lastOutput[i + 2] +
                             lastOutput[i - step] +
                             lastOutput[i + step] >> 2) + lastOutput[i];
            if (!j) {
                output[i] = 0;
            } else {
                j = j - lastLastOutput[i] - 1;
                if (j < 0) 
                    output[i] = 0;
                else if (j & (255*256)) 
                    output[i] = 255;
                else 
                    output[i] = j;
            }
        } while(++i < end);
    }
}

void Synaesthesia::fadePixelHeat(int x, int y, int where, int step) 
{
    short j = (short(getPixel(x - 1, y, where - 2)) + 
                     getPixel(x + 1, y, where + 2) +
                     getPixel(x, y - 1, where - step) +
                     getPixel(x, y + 1, where + step) >> 2) + lastOutput[where];
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
    unsigned short *t = lastLastOutputBmp.data;
    lastLastOutputBmp.data = lastOutputBmp.data;
    lastOutputBmp.data = outputBmp.data;
    outputBmp.data = t;

    int x, y, i, j, start, end;
    int step = outWidth * 2;
    for (x = 0, i = 0, j = outWidth * (outHeight - 1) * 2; 
         x < outWidth; x++, i += 2, j += 2) 
    {
        fadePixelHeat(x, 0, i, step);
        fadePixelHeat(x, 0, i + 1, step);
        fadePixelHeat(x, outHeight - 1, j, step);
        fadePixelHeat(x, outHeight - 1, j + 1, step);
    }

    for(y = 1, i = outWidth * 2, j = outWidth * 4 - 2; y < outHeight;
        y++, i += step, j += step) 
    {
        fadePixelHeat(0, y, i, step);
        fadePixelHeat(0, y, i + 1, step);
        fadePixelHeat(outWidth - 1, y, j, step);
        fadePixelHeat(outWidth - 1, y, j + 1, step);
    }

    for(y = 1, start = outWidth * 2 + 2, end = outWidth * 4 - 2; 
        y < outHeight - 1; y++, start += step, end += step) 
    {
        int i = start;
        do {
            short j = (short(lastOutput[i - 2]) + lastOutput[i + 2] +
                       lastOutput[i - step] + lastOutput[i + step] >> 2) +
                       lastOutput[i];
            if (!j) 
                output[i] = 0;
            else 
            {
                j = j - lastLastOutput[i] + 
                    (lastLastOutput[i] - lastOutput[i] >> 2) - 1;
                if (j < 0) 
                    output[i] = 0;
                else if (j & (255*256)) 
                    output[i] = 255;
                else 
                    output[i] = j;
            }
        } while(++i < end);
    }
}

void Synaesthesia::fade(void) 
{
    switch(fadeMode) {
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
        return true;

    double x[NumSamples], y[NumSamples];
    double a[NumSamples], b[NumSamples];
    double energy;
    int clarity[NumSamples];
    int i, j, k;

    int brightFactor = int(Brightness * brightnessTwiddler / (starSize + 0.01));

    int numSamps = NumSamples;
    if (node->length < NumSamples)
        numSamps = node->length;

    memset(x, 0, sizeof(x));
    memset(y, 0, sizeof(y));

    for (i = 0; i < numSamps; i++) 
    {
        x[i] = node->left[i];
        if (node->right)
            y[i] = node->right[i];
        else
            y[i] = x[i];
    }
   
    fft(x, y);

    energy = 0.0;

    for (i = 0 + 1; i < NumSamples / 2; i++)
    {
        double x1 = x[bitReverse[i]], 
               y1 = y[bitReverse[i]],
               x2 = x[bitReverse[NumSamples - i]],
               y2 = y[bitReverse[NumSamples - i]],
               aa, bb;
        a[i] = sqrt(aa = (x1 + x2) * (x1 + x2) + (y1 - y2) * (y1 - y2));
        b[i] = sqrt(bb = (x1 - x2) * (x1 - x2) + (y2 + y2) * (y1 + y2));
        if (aa + bb != 0.0)
            clarity[i] = (int)(((x1 + x2) * (x1 - x2) + (y1 + y2) * (y1 - y2)) /
                         (aa + bb) * 256);
        else
            clarity[i] = 0;

        energy += (aa + bb) * i * i;
    }

    energy = sqrt(energy / NumSamples) / 65536.0;

    //int heightFactor = NumSamples / 2 / outHeight + 1;
    //int actualHeight = NumSamples / 2 / heightFactor;
    //int heightAdd = (outHeight + actualHeight) >> 1;

    double brightFactor2 = (brightFactor / 65536.0 / NumSamples) *
                           sqrt(outHeight * outWidth / (320.0 * 200.0));

    energy_avg = energy_avg * 0.95 + energy * 0.05;
    if (energy_avg > 0.0)
        brightFactor2 *= 80.0 / (energy_avg + 5.0);

    for(i = 1; i < NumSamples / 2; i++) 
    {
        if (a[i] > 0 || b[i] > 0) 
        {
            int h = (int)(b[i] * outWidth / (a[i] + b[i]));
            int br1, br2, br = (int)((a[i] + b[i]) * i * brightFactor2);
            br1 = br * (clarity[i] + 128) >> 8;
            br2 = br * (128 - clarity[i]) >> 8;
            if (br1 < 0) br1 = 0; else if (br1 > 255) br1 = 255;
            if (br2 < 0) br2 = 0; else if (br2 > 255) br2 = 255;

            int px = h,
                py = outHeight - i * outHeight / (NumSamples / 2);

            if (pointsAreDiamonds) 
            {
                addPixel(px, py, br1, br2);
                br1 = scaleDown[br1];
                br2 = scaleDown[br2];

                for(j = 1; br1 > 0 || br2 > 0; 
                    j++, br1 = scaleDown[br1], br2 = scaleDown[br2]) 
                {
                    for (k = 0; k < j; k++) 
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
                if (px < maxStarRadius || py < maxStarRadius ||
                    px > outWidth - maxStarRadius || 
                    py > outHeight - maxStarRadius) 
                {
                    addPixel(px, py, br1, br2);
                    for( j = 1; br1 > 0 || br2 > 0;
                        j++, br1 = scaleDown[br1], br2 = scaleDown[br2]) 
                    {
                        addPixel(px + j, py, br1, br2);
                        addPixel(px, py + j, br1, br2);
                        addPixel(px - j, py, br1, br2);
                        addPixel(px, py - j, br1, br2);
                    }
                } 
                else 
                {
                    unsigned char *p = output + px * 2 + py * outWidth * 2, 
                                  *p1=p, *p2=p, *p3=p, *p4=p;
                    addPixelFast(p, br1, br2);
                    for (; br1 > 0 || br2 > 0; 
                         br1 = scaleDown[br1], br2=scaleDown[br2]) 
                    {
                        p1 += 2;
                        addPixelFast(p1, br1, br2);
                        p2 -= 2;
                        addPixelFast(p2, br1, br2);
                        p3 += outWidth * 2;
                        addPixelFast(p3, br1, br2);
                        p4 -= outWidth*2;
                        addPixelFast(p4, br1, br2);
                    }
                }
            }
        }
    }

    return false;
}

bool Synaesthesia::draw(QPainter *p, const QColor &back)
{
    (void)p;
    (void)back;
#ifdef SDL_SUPPORT
    if (!surface)
    {
        cerr << "No sdl surface\n";
        return false;
    }

    SDL_LockSurface(surface);

    register uint32_t *ptrOutput = (uint32_t *)output;

    for (int j = 0; j < outHeight * 2; j += 2) 
    {		
        uint32_t *ptrTop = (uint32_t *)(surface->pixels) + outWidth / 4 * j;
	uint32_t *ptrBot = (uint32_t *)(surface->pixels) + 
                                                         outWidth / 4 * (j + 1);
		
        int i = outWidth / 4;

        do {
            register unsigned int const r1 = *(ptrOutput++);
            register unsigned int const r2 = *(ptrOutput++);
            register unsigned int const v = ((r1 & 0x000000f0ul) >> 4)  |
                                            ((r1 & 0x0000f000ul) >> 8)  |
                                            ((r1 & 0x00f00000ul) >> 12) |
                                            ((r1 & 0xf0000000ul) >> 16);
            *(ptrTop++) = v | ( ((r2 & 0x000000f0ul) << 12) |
                                ((r2 & 0x0000f000ul) << 8 ) |
                                ((r2 & 0x00f00000ul) << 4 ) |
                                ((r2 & 0xf0000000ul)));

            *(ptrBot++) = v | ( ((r2 & 0x000000f0ul) << 12) |
                                ((r2 & 0x0000f000ul) << 8 ) |
                                ((r2 & 0x00f00000ul) << 4 ) |
                                ((r2 & 0xf0000000ul)));
        }while(--i);
    }

    SDL_UnlockSurface(surface);
    SDL_Flip(surface);

    return false;
#else

    if (!outputImage)
        return false;

    register uint32_t *ptrOutput = (uint32_t *)output;

    for (int j = 0; j < outHeight * 2; j += 2) 
    {
        uint32_t *ptrTop = (uint32_t *)(outputImage->scanLine(j));
        uint32_t *ptrBot = (uint32_t *)(outputImage->scanLine(j+1));

        int i = outWidth / 4;

        do
        {
            register unsigned int const r1 = *(ptrOutput++);
            register unsigned int const r2 = *(ptrOutput++);

            register unsigned int const v = ((r1 & 0x000000f0ul) >> 4) |
                                            ((r1 & 0x0000f000ul) >> 8) |
                                            ((r1 & 0x00f00000ul) >> 12) |
                                            ((r1 & 0xf0000000ul) >> 16);

            *(ptrTop++) = v | (((r2 & 0x000000f0ul) << 12) |
                               ((r2 & 0x0000f000ul) << 8) |
                               ((r2 & 0x00f00000ul) << 4) |
                               ((r2 & 0xf0000000ul)));

            *(ptrBot++) = v | (((r2 & 0x000000f0ul) << 12) |
                               ((r2 & 0x0000f000ul) << 8) |
                               ((r2 & 0x00f00000ul) << 4) |
                               ((r2 & 0xf0000000ul)));
        } while (--i);
    }

    p->drawImage(QRect(0, 0, 800, 600), *outputImage);
   
    return true;
#endif
}

const QString &SynaesthesiaFactory::name(void) const
{
    static QString name("Synaesthesia");
    return name;
}

const QString &SynaesthesiaFactory::description(void) const
{
    static QString name(QObject::tr("Synaesthesia"));
    return name;
}

VisualBase *SynaesthesiaFactory::create(MainVisual *parent, long int winid)
{
    (void)parent;
    return new Synaesthesia(winid);
}

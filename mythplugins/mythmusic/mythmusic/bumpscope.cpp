#include "mainvisual.h"
#include "bumpscope.h"

#include <compat.h>
#include <mythlogging.h>

// This was:
// Bump Scope - Visualization Plugin for XMMS
// Copyright (C) 1999-2001 Zinx Verituse

#ifdef SDL_SUPPORT

#include <cmath>
#include <cstdlib>

#include <iostream>
using namespace std;

#include <QPainter>

BumpScope::BumpScope(long int winid) :
    size(0,0),

    surface(NULL),

    m_color(0x7ACCFF),
    m_x(0), m_y(0), m_width(800), m_height(600),
    m_phongrad(800),

    color_cycle(true),
    moving_light(true),
    diamond(false),

    bpl(0),

    rgb_buf(NULL),

    iangle(0), ixo(0), iyo(0), ixd(0), iyd(0), ilx(0), ily(0),
    was_moving(0), was_color(0),
    ih(0.0), is(0.0), iv(0.0), isd(0.0), ihd(0),
    icolor(0)
{
    fps = 15;

    for (unsigned int i = 255; i > 0; i--)
    {
        intense1[i] = cos(((double)(255 - i) * M_PI) / 512.0);
        intense2[i] = pow(intense1[i], 250) * 150;
    }
    intense1[0] = intense1[1];
    intense2[0] = intense2[1];

    static char SDL_windowhack[32];
    sprintf(SDL_windowhack, "SDL_WINDOWID=%ld", winid);
    putenv(SDL_windowhack);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unable to init SDL");
        return;
    }

    SDL_ShowCursor(0);
}

BumpScope::~BumpScope()
{
    if (rgb_buf)
        delete [] rgb_buf;

    for (unsigned int i = 0; i < phongdat.size(); i++)
        phongdat[i].resize(0);
    phongdat.resize(0);

    SDL_Quit();
}

void BumpScope::resize(const QSize &newsize)
{
    size = newsize;

    size.setHeight((size.height() / 2) * 2);
    size.setWidth((size.width() / 4) * 4);

    if (rgb_buf)
        delete [] rgb_buf;

    int bufsize = (size.height() + 2) * (size.width() + 2);

    rgb_buf = new unsigned char[bufsize];

    bpl = size.width() + 2;

    surface = SDL_SetVideoMode(size.width(), size.height(), 8, 0);

    if (!surface)
    {
        LOG(VB_GENERAL, LOG_ERR, "Couldn't get SDL surface");
        return;
    }

    m_width = size.width();
    m_height = size.height();
    m_phongrad = m_width;

    m_x = m_width / 2;
    m_y = m_height;
    
    phongdat.resize(m_phongrad * 2);
    for (unsigned int i = 0; i < phongdat.size(); i++)
        phongdat[i].resize(m_phongrad * 2);

    generate_phongdat();
    generate_cmap(m_color);
}

void BumpScope::blur_8(unsigned char *ptr, int w, int h, int bpl)
{
    (void)w;

    register unsigned int i, sum;
    register unsigned char *iptr;

    iptr = ptr + bpl + 1;
    i = bpl * h;
 
    while (i--)
    {
        sum = (iptr[-bpl] + iptr[-1] + iptr[1] + iptr[bpl]) >> 2;
        if (sum > 2)
            sum -= 2;
        *(iptr++) = sum;
    }
}

void BumpScope::generate_cmap(unsigned int color)
{
    SDL_Color sdlPalette[256];
    unsigned int i, red, blue, green, r, g, b;

    if (surface)
    {
        red = (unsigned int)(color / 0x10000);
        green = (unsigned int)((color % 0x10000) / 0x100);
        blue = (unsigned int)(color % 0x100);

        for (i = 255; i > 0; i--)
        {
             r = (unsigned int)(((double)(100 * Qt::red / 255) * intense1[i] + intense2[i]));
             if (r > 255) 
                 r = 255;
             g = (unsigned int)(((double)(100 * Qt::green / 255) * intense1[i] + intense2[i]));
             if (g > 255) 
                 g = 255;
             b = (unsigned int)(((double)(100 * Qt::blue / 255) * intense1[i] + intense2[i]));
             if (b > 255) 
                 b = 255;

             sdlPalette[i].r = r;
             sdlPalette[i].g = g;
             sdlPalette[i].b = b;
         }

         sdlPalette[0].r = sdlPalette[1].r;
         sdlPalette[0].g = sdlPalette[1].g;
         sdlPalette[0].b = sdlPalette[1].b;

         SDL_SetColors(surface, sdlPalette, 0, 256);
    }
}

void BumpScope::generate_phongdat(void)
{
    unsigned int y, x;
    double i, i2;

    unsigned int PHONGRES = m_phongrad * 2;

    for (y = 0; y < m_phongrad; y++) 
    {
        for (x = 0; x < m_phongrad; x++) 
        {
            i = (double)x / ((double)m_phongrad) - 1;
            i2 = (double)y / ((double)m_phongrad) - 1;

            if (diamond)
               i = 1 - pow(i*i2,.75) - i*i - i2*i2;
            else
               i = 1 - i*i - i2*i2;

            if (i >= 0) 
            {
                if (diamond)
                    i = i*i*i * 255.0;
                else
                    i = i*i*i * 255.0;
 
                if (i > 255) 
                    i = 255;
                unsigned char uci = (unsigned char)i;

                phongdat[y][x] = uci;
                phongdat[(PHONGRES-1)-y][x] = uci;
                phongdat[y][(PHONGRES-1)-x] = uci;
                phongdat[(PHONGRES-1)-y][(PHONGRES-1)-x] = uci;
            } 
            else 
            {
                phongdat[y][x] = 0;
                phongdat[(PHONGRES-1)-y][x] = 0;
                phongdat[y][(PHONGRES-1)-x] = 0;
                phongdat[(PHONGRES-1)-y][(PHONGRES-1)-x] = 0;
            }
        }
    }
}

void BumpScope::translate(int x, int y, int *xo, int *yo, int *xd, int *yd,
                          int *angle)
{
    unsigned int HEIGHT = m_height;
    unsigned int WIDTH = m_width;

    int wd2 = (int)(WIDTH / 2);
    int hd2 = (int)(HEIGHT / 2);

    /* try setting y to both maxes */
    *yo = HEIGHT/2;
    *angle = (int)(asin((float)(y-(HEIGHT/2))/(float)*yo)/(M_PI/180.0));
    *xo = (int)((x-(WIDTH/2))/cos(*angle*(M_PI/180.0)));

    if (*xo >= -wd2 && *xo <= wd2) {
        *xd = (*xo>0)?-1:1;
        *yd = 0;
        return;
    }

    *yo = -*yo;
    *angle = (int)(asin((float)(y-(HEIGHT/2))/(float)*yo)/(M_PI/180.0));
    *xo = (int)((x-(WIDTH/2))/cos(*angle*(M_PI/180.0)));

    if (*xo >= -wd2 && *xo <= wd2) {
        *xd = (*xo>0)?-1:1;
        *yd = 0;
        return;
    }

    /* try setting x to both maxes */
    *xo = WIDTH/2;
    *angle = (int)(acos((float)(x-(WIDTH/2))/(float)*xo)/(M_PI/180.0));
    *yo = (int)((y-(HEIGHT/2))/sin(*angle*(M_PI/180.0)));

    if (*yo >= -hd2 && *yo <= hd2) {
        *yd = (*yo>0)?-1:1;
        *xd = 0;
        return;
    }

    *xo = -*xo;
    *angle = (int)(acos((float)(x-(WIDTH/2))/(float)*xo)/(M_PI/180.0));
    *yo = (int)((y-(HEIGHT/2))/sin(*angle*(M_PI/180.0)));

    /* if this isn't right, it's out of our range and we don't care */
    *yd = (*yo>0)?-1:1;
    *xd = 0;
}

inline void BumpScope::draw_vert_line(unsigned char *buffer, int x, int y1, 
                                      int y2)
{
    int y;
    unsigned char *p;

    if (y1 < y2)
    {
        p = buffer + ((y1 + 1) * bpl) + x + 1;
        for (y = y1; y <= y2; y++)
        {
            *p = 0xff;
            p += bpl;
        }
    }
    else if (y2 < y1)
    {
        p = buffer + ((y2 + 1) * bpl) + x + 1;
        for (y = y2; y <= y1; y++)
        {
            *p = 0xff;
            p += bpl;
        }
    }
    else
        buffer[((y1 + 1) * bpl) + x + 1] = 0xff;
}

void BumpScope::render_light(int lx, int ly)
{
    int prev_y, out_y, dy, dx, xp, yp;
    unsigned int PHONGRES = m_phongrad * 2;
    unsigned int i, j;

    prev_y = bpl + 1;
    out_y = 0;
    unsigned char *outputbuf = (unsigned char *)(surface->pixels);

    for (dy = (-ly) + (PHONGRES / 2), j = 0; j < m_height; j++, dy++,
         prev_y += bpl - m_width)
    {
        for (dx = (-lx) + (PHONGRES / 2), i = 0; i < m_width; i++, dx++,
             prev_y++, out_y++)
        {
            xp = (rgb_buf[prev_y - 1] - rgb_buf[prev_y + 1]) + dx;
            yp = (rgb_buf[prev_y - bpl] - rgb_buf[prev_y + bpl]) + dy;

            if (yp < 0 || yp >= (int)PHONGRES ||
                xp < 0 || xp >= (int)PHONGRES)
            {
                outputbuf[out_y] = 0;
                continue;
            }

            outputbuf[out_y] = phongdat[yp][xp];
        }
    }
}

void BumpScope::rgb_to_hsv(unsigned int color, double *h, double *s, double *v)
{
  double max, min, delta, r, g, b;

  r = (double)(color>>16) / 255.0;
  g = (double)((color>>8)&0xff) / 255.0;
  b = (double)(color&0xff) / 255.0;

  max = r;
  if (g > max) max = g;
  if (b > max) max = b;

  min = r;
  if (g < min) min = g;
  if (b < min) min = b;

  *v = max;

  if (max != 0.0) *s = (max - min) / max;
  else *s = 0.0;

  if (*s == 0.0) *h = 0.0;
  else
    {
      delta = max - min;

      if (r == max) *h = (g - b) / delta;
      else if (g == max) *h = 2.0 + (b - r) / delta;
      else if (b == max) *h = 4.0 + (r - g) / delta;

      *h = *h * 60.0;

      if (*h < 0.0) *h = *h + 360;
    }
}

void BumpScope::hsv_to_rgb(double h, double s, double v, unsigned int *color)
{
  int i;
  double f, w, q, t, r, g, b;

  if (s == 0.0)
    s = 0.000001;

  if (h == -1.0)
    {
      r = v; g = v; b = v;
    }
  else
    {
      if (h == 360.0) h = 0.0;
      h = h / 60.0;
      i = (int) h;
      f = h - i;
      w = v * (1.0 - s);
      q = v * (1.0 - (s * f));
      t = v * (1.0 - (s * (1.0 - f)));

      switch (i)
        {
        case 0: r = v; g = t; b = w; break;
        case 1: r = q; g = v; b = w; break;
        case 2: r = w; g = v; b = t; break;
        case 3: r = w; g = q; b = v; break;
        case 4: r = t; g = w; b = v; break;
        /*case 5: use default to keep gcc from complaining */
        default: r = v; g = w; b = q; break;
        }
    }

  *color = ((unsigned int)((double)r*255)<<16) | ((unsigned int)((double)g*255)<<8) | ((unsigned int)((double)b*255));
}

bool BumpScope::process(VisualNode *node)
{
    if (!node || node->length == 0 || !surface)
        return true;

    int numSamps = 512;
    if (node->length < 512)
        numSamps = node->length;

    unsigned int i;
    int y, prev_y;

    prev_y = (int)m_height / 2 + ((int)node->left[0] * (int)m_height) / 
             (int)0x10000;

    if (prev_y < 0)
        prev_y = 0;
    if (prev_y >= (int)m_height) prev_y = m_height - 1;

    for (i = 0; i < m_width; i++)
    {
        y = (i * numSamps) / (m_width - 1);
        y = (int)m_height / 2 + ((int)node->left[y] * (int)m_height) / 
            (int)0x10000;

        if (y < 0)
            y = 0;
        if (y >= (int)m_height)
            y = m_height - 1;

        draw_vert_line(rgb_buf, i, prev_y, y);
        prev_y = y;
    }

    blur_8(rgb_buf, m_width, m_height, bpl);
    return false;
}

bool BumpScope::draw(QPainter *p, const QColor &back)
{
    (void)p;
    (void)back;

    if (!surface)
    {
        LOG(VB_GENERAL, LOG_ERR, "No sdl surface");
        return false;
    }

    ilx = m_x;
    ily = m_y;

    if (moving_light)
    {
        if (!was_moving)
        {
            translate(ilx, ily, &ixo, &iyo, &ixd, &iyd, &iangle);
            was_moving = 1;
        }
 
        ilx = (int)(m_width / 2 + cos(iangle * (M_PI / 180.0)) * ixo);
        ily = (int)(m_height / 2 + sin(iangle * (M_PI / 180.0)) * iyo);
 
        iangle += 2;
        if (iangle >= 360)
            iangle = 0;

        ixo += ixd;
        if ((int)ixo > ((int)m_width / 2) || (int)ixo < -((int)m_width / 2))
        {
            ixo = (ixo > 0) ? (m_width / 2) : -(m_width / 2);
            if (random() & 1)
            {
                ixd = (ixd > 0) ? -1 : 1;
                iyd = 0;
            }
            else
            {
                iyd = (iyd > 0) ? -1 : 1;
                ixd = 0;
            }
        }

        iyo += iyd;
        if ((int)iyo > ((int)m_height / 2) || (int)iyo < -((int)m_height / 2))
        {
            iyo = (iyo > 0) ? (m_height / 2) : -(m_height / 2);
            if (random() & 1)
            {
                ixd = (ixd > 0) ? -1 : 1;
                iyd = 0;
            }
            else
            {
                iyd = (iyd > 0) ? -1 : 1;
                ixd = 0;
            }
        }
    }

    if (color_cycle)
    {
        if (!was_color)
        {
            rgb_to_hsv(m_color, &ih, &is, &iv);
            was_color = 1;

            if (random() & 1)
            {
                ihd = (random() & 1) * 2 - 1;
                isd = 0;
            }
            else
            {
                isd = 0.01 * ((random() & 1) * 2 - 1);
                ihd = 0;
            }
        }

        hsv_to_rgb(ih, is, iv, &icolor);

        generate_cmap(icolor);

        if (ihd)
        {
            ih += ihd;
            if (ih >= 360)
                ih = 0;
            if (ih < 0)
                ih = 359;
            if ((random() % 150) == 0)
            {
                if (random() & 1) 
                {
                    ihd = (random() & 1) * 2 - 1;
                    isd = 0;
                }
                else
                {
                    isd = 0.01 * ((random() & 1) * 2 - 1);
                    ihd = 0;
                }
            }
        }
        else
        {
            is += isd;

            if (is <= 0 || is >= 0.5)
            {
                if (is < 0)
                    is = 0;
                if (is > 0.52)
                    isd = -0.01;
                else if (is == 0)
                {
                    ihd = random() % 360;
                    isd = 0.01;
                }
                else
                {
                    if (random() & 1)
                    {
                        ihd = (random() & 1) * 2 - 1;
                        isd = 0;
                    }
                    else
                    {
                        isd = 0.01 * ((random() & 1) * 2 - 1);
                        ihd = 0;
                    }
                }
            }
        }
    }

    render_light(ilx, ily);

    SDL_UpdateRect(surface, 0, 0, 0, 0);

    return false;
}

static class BumpScopeFactory : public VisFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("BumpScope");
        return name;
    }

    uint plugins(QStringList *list) const
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual *parent, long int winid, const QString &pluginName) const
    {
        (void)parent;
        (void)pluginName;
        return new BumpScope(winid);
    }
}BumpScopeFactory;

#endif

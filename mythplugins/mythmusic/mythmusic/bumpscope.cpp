// This was:
// Bump Scope - Visualization Plugin for XMMS
// Copyright (C) 1999-2001 Zinx Verituse

// C++ headers
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

// QT headers
#include <QCoreApplication>
#include <QPainter>

// MythTV headers
#include <libmythbase/compat.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythrandom.h>
#include <libmythbase/sizetliteral.h>

// Mythmusic Headers
#include "bumpscope.h"
#include "mainvisual.h"

BumpScope::BumpScope()
{
    m_fps = 15;

    for (unsigned int i = 255; i > 0; i--)
    {
        m_intense1[i] = cos(((double)(255 - i) * M_PI) / 512.0);
        m_intense2[i] = pow(m_intense1[i], 250) * 150;
    }
    m_intense1[0] = m_intense1[1];
    m_intense2[0] = m_intense2[1];
}

BumpScope::~BumpScope()
{
    delete [] m_rgbBuf;
    delete m_image;
    for (auto & dat : m_phongDat)
        dat.resize(0);
    m_phongDat.resize(0);
}

void BumpScope::resize(const QSize &newsize)
{
    m_size = newsize;

    m_size.setHeight((m_size.height() / 2) * 2);
    m_size.setWidth((m_size.width() / 4) * 4);

    delete [] m_rgbBuf;

    int bufsize = (m_size.height() + 2) * (m_size.width() + 2);

    m_rgbBuf = new unsigned char[bufsize];

    m_bpl = m_size.width() + 2;

    delete m_image;
    m_image = new QImage(m_size.width(), m_size.height(), QImage::Format_Indexed8);

    m_width = m_size.width();
    m_height = m_size.height();
    m_phongRad = m_width;

    m_x = m_width / 2;
    m_y = m_height;

    m_phongDat.resize(m_phongRad * 2_UZ);
    for (auto & dat : m_phongDat)
        dat.resize(m_phongRad * 2_UZ);

    generate_phongdat();
    generate_cmap(m_color);
}

void BumpScope::blur_8(unsigned char *ptr, [[maybe_unused]] int w, int h, ptrdiff_t bpl)
{
    uchar *iptr = ptr + bpl + 1;
    uint i = bpl * h;

    while (i--)
    {
        uint sum = (iptr[-bpl] + iptr[-1] + iptr[1] + iptr[bpl]) >> 2;
        if (sum > 2)
            sum -= 2;
        *(iptr++) = sum;
    }
}

void BumpScope::generate_cmap(unsigned int color)
{
    if (m_image)
    {
        uint red = color / 0x10000;
        uint green = (color % 0x10000) / 0x100;
        uint blue = color % 0x100;

        for (uint i = 255; i > 0; i--)
        {
             uint r = (unsigned int)(((100 * static_cast<double>(red) / 255)
                                * m_intense1[i]) + m_intense2[i]);
             r = std::min<uint>(r, 255);
             uint g = (unsigned int)(((100 * static_cast<double>(green) / 255)
                                * m_intense1[i]) + m_intense2[i]);
             g = std::min<uint>(g, 255);
             uint b = (unsigned int)(((100 * static_cast<double>(blue) / 255)
                                * m_intense1[i]) + m_intense2[i]);
             b = std::min<uint>(b, 255);

             m_image->setColor(i, qRgba(r, g, b, 255));
         }

         m_image->setColor(0, m_image->color(1));
    }
}

void BumpScope::generate_phongdat(void)
{
    unsigned int PHONGRES = m_phongRad * 2;

    for (uint y = 0; y < m_phongRad; y++)
    {
        for (uint x = 0; x < m_phongRad; x++)
        {
            double i = ((double)x / ((double)m_phongRad)) - 1;
            double i2 = ((double)y / ((double)m_phongRad)) - 1;

            //if (m_diamond)
               i = 1 - pow(i*i2,.75) - (i*i) - (i2*i2);
            //else
            //   i = 1 - i*i - i2*i2;

            if (i >= 0)
            {
                //if (m_diamond)
                    i = i*i*i * 255.0;
                //else
                //    i = i*i*i * 255.0;

                i = std::min<double>(i, 255);
                auto uci = (unsigned char)i;

                m_phongDat[y][x] = uci;
                m_phongDat[(PHONGRES-1)-y][x] = uci;
                m_phongDat[y][(PHONGRES-1)-x] = uci;
                m_phongDat[(PHONGRES-1)-y][(PHONGRES-1)-x] = uci;
            }
            else
            {
                m_phongDat[y][x] = 0;
                m_phongDat[(PHONGRES-1)-y][x] = 0;
                m_phongDat[y][(PHONGRES-1)-x] = 0;
                m_phongDat[(PHONGRES-1)-y][(PHONGRES-1)-x] = 0;
            }
        }
    }
}

#define M_PI_F static_cast<float>(M_PI)
void BumpScope::translate(int x, int y, int *xo, int *yo, int *xd, int *yd,
                          int *angle) const
{
    unsigned int HEIGHT = m_height;
    unsigned int WIDTH = m_width;

    int wd2 = (int)(WIDTH / 2);
    int hd2 = (int)(HEIGHT / 2);

    /* try setting y to both maxes */
    *yo = HEIGHT/2;
    *angle = (int)(asinf((float)(y-(HEIGHT/2.0F))/(float)*yo)/(M_PI_F/180.0F));
    *xo = (int)((x-(WIDTH/2.0F))/cosf(*angle*(M_PI_F/180.0F)));

    if (*xo >= -wd2 && *xo <= wd2) {
        *xd = (*xo>0)?-1:1;
        *yd = 0;
        return;
    }

    *yo = -*yo;
    *angle = (int)(asinf((float)(y-(HEIGHT/2.0F))/(float)*yo)/(M_PI_F/180.0F));
    *xo = (int)((x-(WIDTH/2.0F))/cosf(*angle*(M_PI_F/180.0F)));

    if (*xo >= -wd2 && *xo <= wd2) {
        *xd = (*xo>0)?-1:1;
        *yd = 0;
        return;
    }

    /* try setting x to both maxes */
    *xo = WIDTH/2;
    *angle = (int)(acosf((float)(x-(WIDTH/2.0F))/(float)*xo)/(M_PI_F/180.0F));
    *yo = (int)((y-(HEIGHT/2.0F))/sinf(*angle*(M_PI_F/180.0F)));

    if (*yo >= -hd2 && *yo <= hd2) {
        *yd = (*yo>0)?-1:1;
        *xd = 0;
        return;
    }

    *xo = -*xo;
    *angle = (int)(acosf((float)(x-(WIDTH/2.0F))/(float)*xo)/(M_PI_F/180.0F));
    *yo = (int)((y-(HEIGHT/2.0F))/sinf(*angle*(M_PI_F/180.0F)));

    /* if this isn't right, it's out of our range and we don't care */
    *yd = (*yo>0)?-1:1;
    *xd = 0;
}

inline void BumpScope::draw_vert_line(unsigned char *buffer, int x, int y1,
                                      int y2) const
{
    if (y1 < y2)
    {
        uchar *p = buffer + ((y1 + 1) * m_bpl) + x + 1;
        for (int y = y1; y <= y2; y++)
        {
            *p = 0xff;
            p += m_bpl;
        }
    }
    else if (y2 < y1)
    {
        uchar *p = buffer + ((y2 + 1) * m_bpl) + x + 1;
        for (int y = y2; y <= y1; y++)
        {
            *p = 0xff;
            p += m_bpl;
        }
    }
    else
    {
        buffer[((y1 + 1) * m_bpl) + x + 1] = 0xff;
    }
}

void BumpScope::render_light(int lx, int ly)
{
    int dy = 0;
    unsigned int PHONGRES = m_phongRad * 2;
    unsigned int j = 0;

    int prev_y = m_bpl + 1;
    int out_y = 0;
    unsigned char *outputbuf = m_image->bits();

    for (dy = (-ly) + (PHONGRES / 2), j = 0; j < m_height; j++, dy++,
         prev_y += m_bpl - m_width)
    {
        int dx = 0;
        unsigned int i = 0;
        for (dx = (-lx) + (PHONGRES / 2), i = 0; i < m_width; i++, dx++,
             prev_y++, out_y++)
        {
            int xp = (m_rgbBuf[prev_y - 1] - m_rgbBuf[prev_y + 1]) + dx;
            int yp = (m_rgbBuf[prev_y - m_bpl] - m_rgbBuf[prev_y + m_bpl]) + dy;

            if (yp < 0 || yp >= (int)PHONGRES ||
                xp < 0 || xp >= (int)PHONGRES)
            {
                outputbuf[out_y] = 0;
                continue;
            }

            outputbuf[out_y] = m_phongDat[yp][xp];
        }
    }
}

void BumpScope::rgb_to_hsv(unsigned int color, double *h, double *s, double *v)
{
  double r = (double)(color>>16) / 255.0;
  double g = (double)((color>>8)&0xff) / 255.0;
  double b = (double)(color&0xff) / 255.0;

  double max = r;
  max = std::max(g, max);
  max = std::max(b, max);

  double min = r;
  min = std::min(g, min);
  min = std::min(b, min);

  *v = max;

  if (max != 0.0) *s = (max - min) / max;
  else *s = 0.0;

  if (*s == 0.0) *h = 0.0;
  else
    {
      double delta = max - min;

      if (r == max) *h = (g - b) / delta;
      else if (g == max) *h = 2.0 + ((b - r) / delta);
      else if (b == max) *h = 4.0 + ((r - g) / delta);

      *h = *h * 60.0;

      if (*h < 0.0) *h = *h + 360;
    }
}

void BumpScope::hsv_to_rgb(double h, double s, double v, unsigned int *color)
{
  double r = __builtin_nan("");
  double g = __builtin_nan("");
  double b = __builtin_nan("");

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
      int i = (int) h;
      double f = h - i;
      double w = v * (1.0 - s);
      double q = v * (1.0 - (s * f));
      double t = v * (1.0 - (s * (1.0 - f)));

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

  *color = ((unsigned int)(r*255)<<16) | ((unsigned int)(g*255)<<8) | ((unsigned int)(b*255));
}

bool BumpScope::process(VisualNode *node)
{
    if (!node || node->m_length == 0 || !m_image)
        return false;

    int numSamps = 512;
    if (node->m_length < 512)
        numSamps = node->m_length;

    int prev_y = ((int)m_height / 2) +
        (((int)node->m_left[0] * (int)m_height) / 0x10000);

    prev_y = std::max(prev_y, 0);
    if (prev_y >= (int)m_height) prev_y = m_height - 1;

    for (uint i = 0; i < m_width; i++)
    {
        int y = (i * numSamps) / (m_width - 1);
        y = ((int)m_height / 2) +
            (((int)node->m_left[y] * (int)m_height) / 0x10000);

        y = std::max(y, 0);
        if (y >= (int)m_height)
            y = m_height - 1;

        draw_vert_line(m_rgbBuf, i, prev_y, y);
        prev_y = y;
    }

    blur_8(m_rgbBuf, m_width, m_height, m_bpl);

    return false;
}

bool BumpScope::draw(QPainter *p, [[maybe_unused]] const QColor &back)
{
    if (!m_image || m_image->isNull())
    {
        LOG(VB_GENERAL, LOG_ERR, "BumpScope::draw: Bad image");
        return false;
    }

    m_ilx = m_x;
    m_ily = m_y;

    if (m_movingLight)
    {
        if (!m_wasMoving)
        {
            translate(m_ilx, m_ily, &m_ixo, &m_iyo, &m_ixd, &m_iyd, &m_iangle);
            m_wasMoving = 1;
        }

        m_ilx = (int)((m_width / 2.0F) + (cosf(m_iangle * (M_PI_F / 180.0F)) * m_ixo));
        m_ily = (int)((m_height / 2.0F) + (sinf(m_iangle * (M_PI_F / 180.0F)) * m_iyo));

        m_iangle += 2;
        if (m_iangle >= 360)
            m_iangle = 0;

        m_ixo += m_ixd;
        if (m_ixo > ((int)m_width / 2) || m_ixo < -((int)m_width / 2))
        {
            m_ixo = (m_ixo > 0) ? (m_width / 2) : -(m_width / 2);
            if (rand_bool())
            {
                m_ixd = (m_ixd > 0) ? -1 : 1;
                m_iyd = 0;
            }
            else
            {
                m_iyd = (m_iyd > 0) ? -1 : 1;
                m_ixd = 0;
            }
        }

        m_iyo += m_iyd;
        if (m_iyo > ((int)m_height / 2) || m_iyo < -((int)m_height / 2))
        {
            m_iyo = (m_iyo > 0) ? (m_height / 2) : -(m_height / 2);
            if (rand_bool())
            {
                m_ixd = (m_ixd > 0) ? -1 : 1;
                m_iyd = 0;
            }
            else
            {
                m_iyd = (m_iyd > 0) ? -1 : 1;
                m_ixd = 0;
            }
        }
    }

    if (m_colorCycle)
    {
        if (!m_wasColor)
        {
            rgb_to_hsv(m_color, &m_ih, &m_is, &m_iv);
            m_wasColor = 1;

            if (rand_bool())
            {
                m_ihd = rand_bool() ? -1 : 1;
                m_isd = 0;
            }
            else
            {
                m_isd = rand_bool() ? -0.01 : 0.01;
                m_ihd = 0;
            }
        }

        hsv_to_rgb(m_ih, m_is, m_iv, &m_icolor);

        generate_cmap(m_icolor);

        if (m_ihd)
        {
            m_ih += m_ihd;
            if (m_ih >= 360)
                m_ih = 0;
            if (m_ih < 0)
                m_ih = 359;
            if (rand_bool(150))
            {
                if (rand_bool())
                {
                    m_ihd = rand_bool() ? -1 : 1;
                    m_isd = 0;
                }
                else
                {
                    m_isd = rand_bool() ? -0.01 : 0.01;
                    m_ihd = 0;
                }
            }
        }
        else
        {
            m_is += m_isd;

            if (m_is <= 0 || m_is >= 0.5)
            {
                m_is = std::max<double>(m_is, 0);
                if (m_is > 0.52)
                    m_isd = -0.01;
                else if (m_is == 0)
                {
                    m_ihd = MythRandom(0, 360 - 1);
                    m_isd = 0.01;
                }
                else
                {
                    if (rand_bool())
                    {
                        m_ihd = rand_bool() ? -1 : 1;
                        m_isd = 0;
                    }
                    else
                    {
                        m_isd = rand_bool() ? -0.01 : 0.01;
                        m_ihd = 0;
                    }
                }
            }
        }
    }

    render_light(m_ilx, m_ily);

    p->drawImage(0, 0, *m_image);

    return true;
}

static class BumpScopeFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "BumpScope");
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
        return new BumpScope();
    }
}BumpScopeFactory;

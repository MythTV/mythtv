#include "mainvisual.h"
#include "bumpscope.h"

#include <compat.h>
#include <mythlogging.h>

// This was:
// Bump Scope - Visualization Plugin for XMMS
// Copyright (C) 1999-2001 Zinx Verituse

#include <cmath>
#include <cstdlib>

#include <iostream>
using namespace std;

#include <QCoreApplication>
#include <QPainter>

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
    delete [] m_rgb_buf;
    delete m_image;
    for (size_t i = 0; i < m_phongdat.size(); i++)
        m_phongdat[i].resize(0);
    m_phongdat.resize(0);
}

void BumpScope::resize(const QSize &newsize)
{
    m_size = newsize;

    m_size.setHeight((m_size.height() / 2) * 2);
    m_size.setWidth((m_size.width() / 4) * 4);

    delete [] m_rgb_buf;

    int bufsize = (m_size.height() + 2) * (m_size.width() + 2);

    m_rgb_buf = new unsigned char[bufsize];

    m_bpl = m_size.width() + 2;

    delete m_image;
    m_image = new QImage(m_size.width(), m_size.height(), QImage::Format_Indexed8);

    m_width = m_size.width();
    m_height = m_size.height();
    m_phongrad = m_width;

    m_x = m_width / 2;
    m_y = m_height;

    m_phongdat.resize(m_phongrad * 2);
    for (size_t i = 0; i < m_phongdat.size(); i++)
        m_phongdat[i].resize(m_phongrad * 2);

    generate_phongdat();
    generate_cmap(m_color);
}

void BumpScope::blur_8(unsigned char *ptr, int w, int h, int bpl)
{
    (void)w;

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
             uint r = (unsigned int)((100 * static_cast<double>(red) / 255)
                                * m_intense1[i] + m_intense2[i]);
             if (r > 255)
                 r = 255;
             uint g = (unsigned int)((100 * static_cast<double>(green) / 255)
                                * m_intense1[i] + m_intense2[i]);
             if (g > 255)
                 g = 255;
             uint b = (unsigned int)((100 * static_cast<double>(blue) / 255)
                                * m_intense1[i] + m_intense2[i]);
             if (b > 255)
                 b = 255;

             m_image->setColor(i, qRgba(r, g, b, 255));
         }

         m_image->setColor(0, m_image->color(1));
    }
}

void BumpScope::generate_phongdat(void)
{
    unsigned int PHONGRES = m_phongrad * 2;

    for (uint y = 0; y < m_phongrad; y++)
    {
        for (uint x = 0; x < m_phongrad; x++)
        {
            double i = (double)x / ((double)m_phongrad) - 1;
            double i2 = (double)y / ((double)m_phongrad) - 1;

            //if (m_diamond)
               i = 1 - pow(i*i2,.75) - i*i - i2*i2;
            //else
            //   i = 1 - i*i - i2*i2;

            if (i >= 0)
            {
                //if (m_diamond)
                    i = i*i*i * 255.0;
                //else
                //    i = i*i*i * 255.0;

                if (i > 255)
                    i = 255;
                unsigned char uci = (unsigned char)i;

                m_phongdat[y][x] = uci;
                m_phongdat[(PHONGRES-1)-y][x] = uci;
                m_phongdat[y][(PHONGRES-1)-x] = uci;
                m_phongdat[(PHONGRES-1)-y][(PHONGRES-1)-x] = uci;
            }
            else
            {
                m_phongdat[y][x] = 0;
                m_phongdat[(PHONGRES-1)-y][x] = 0;
                m_phongdat[y][(PHONGRES-1)-x] = 0;
                m_phongdat[(PHONGRES-1)-y][(PHONGRES-1)-x] = 0;
            }
        }
    }
}

#define M_PI_F static_cast<float>(M_PI)
void BumpScope::translate(int x, int y, int *xo, int *yo, int *xd, int *yd,
                          int *angle)
{
    unsigned int HEIGHT = m_height;
    unsigned int WIDTH = m_width;

    int wd2 = (int)(WIDTH / 2);
    int hd2 = (int)(HEIGHT / 2);

    /* try setting y to both maxes */
    *yo = HEIGHT/2;
    *angle = (int)(asinf((float)(y-(HEIGHT/2.0F))/(float)*yo)/(M_PI_F/180.0F));
    *xo = (int)((x-(WIDTH/2.0F))/cosf(*angle*(M_PI/180.0)));

    if (*xo >= -wd2 && *xo <= wd2) {
        *xd = (*xo>0)?-1:1;
        *yd = 0;
        return;
    }

    *yo = -*yo;
    *angle = (int)(asin((float)(y-(HEIGHT/2.0F))/(float)*yo)/(M_PI_F/180.0F));
    *xo = (int)((x-(WIDTH/2.0F))/cosf(*angle*(M_PI/180.0)));

    if (*xo >= -wd2 && *xo <= wd2) {
        *xd = (*xo>0)?-1:1;
        *yd = 0;
        return;
    }

    /* try setting x to both maxes */
    *xo = WIDTH/2;
    *angle = (int)(acosf((float)(x-(WIDTH/2.0F))/(float)*xo)/(M_PI_F/180.0F));
    *yo = (int)((y-(HEIGHT/2.0F))/sinf(*angle*(M_PI/180.0)));

    if (*yo >= -hd2 && *yo <= hd2) {
        *yd = (*yo>0)?-1:1;
        *xd = 0;
        return;
    }

    *xo = -*xo;
    *angle = (int)(acosf((float)(x-(WIDTH/2.0F))/(float)*xo)/(M_PI_F/180.0F));
    *yo = (int)((y-(HEIGHT/2.0F))/sinf(*angle*(M_PI/180.0)));

    /* if this isn't right, it's out of our range and we don't care */
    *yd = (*yo>0)?-1:1;
    *xd = 0;
}

inline void BumpScope::draw_vert_line(unsigned char *buffer, int x, int y1,
                                      int y2)
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
        buffer[((y1 + 1) * m_bpl) + x + 1] = 0xff;
}

void BumpScope::render_light(int lx, int ly)
{
    int dx = 0, dy = 0;
    unsigned int PHONGRES = m_phongrad * 2;
    unsigned int i = 0, j = 0;

    int prev_y = m_bpl + 1;
    int out_y = 0;
    unsigned char *outputbuf = m_image->bits();

    for (dy = (-ly) + (PHONGRES / 2), j = 0; j < m_height; j++, dy++,
         prev_y += m_bpl - m_width)
    {
        for (dx = (-lx) + (PHONGRES / 2), i = 0; i < m_width; i++, dx++,
             prev_y++, out_y++)
        {
            int xp = (m_rgb_buf[prev_y - 1] - m_rgb_buf[prev_y + 1]) + dx;
            int yp = (m_rgb_buf[prev_y - m_bpl] - m_rgb_buf[prev_y + m_bpl]) + dy;

            if (yp < 0 || yp >= (int)PHONGRES ||
                xp < 0 || xp >= (int)PHONGRES)
            {
                outputbuf[out_y] = 0;
                continue;
            }

            outputbuf[out_y] = m_phongdat[yp][xp];
        }
    }
}

void BumpScope::rgb_to_hsv(unsigned int color, double *h, double *s, double *v)
{
  double r = (double)(color>>16) / 255.0;
  double g = (double)((color>>8)&0xff) / 255.0;
  double b = (double)(color&0xff) / 255.0;

  double max = r;
  if (g > max) max = g;
  if (b > max) max = b;

  double min = r;
  if (g < min) min = g;
  if (b < min) min = b;

  *v = max;

  if (max != 0.0) *s = (max - min) / max;
  else *s = 0.0;

  if (*s == 0.0) *h = 0.0;
  else
    {
      double delta = max - min;

      if (r == max) *h = (g - b) / delta;
      else if (g == max) *h = 2.0 + (b - r) / delta;
      else if (b == max) *h = 4.0 + (r - g) / delta;

      *h = *h * 60.0;

      if (*h < 0.0) *h = *h + 360;
    }
}

void BumpScope::hsv_to_rgb(double h, double s, double v, unsigned int *color)
{
  double r = NAN, g = NAN, b = NAN;

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

    int prev_y = (int)m_height / 2 +
        ((int)node->m_left[0] * (int)m_height) / 0x10000;

    if (prev_y < 0)
        prev_y = 0;
    if (prev_y >= (int)m_height) prev_y = m_height - 1;

    for (uint i = 0; i < m_width; i++)
    {
        int y = (i * numSamps) / (m_width - 1);
        y = (int)m_height / 2 +
            ((int)node->m_left[y] * (int)m_height) / 0x10000;

        if (y < 0)
            y = 0;
        if (y >= (int)m_height)
            y = m_height - 1;

        draw_vert_line(m_rgb_buf, i, prev_y, y);
        prev_y = y;
    }

    blur_8(m_rgb_buf, m_width, m_height, m_bpl);

    return false;
}

bool BumpScope::draw(QPainter *p, const QColor &back)
{
    if (!m_image || m_image->isNull())
    {
        LOG(VB_GENERAL, LOG_ERR, "BumpScope::draw: Bad image");
        return false;
    }

    (void)back;

    m_ilx = m_x;
    m_ily = m_y;

    if (m_moving_light)
    {
        if (!m_was_moving)
        {
            translate(m_ilx, m_ily, &m_ixo, &m_iyo, &m_ixd, &m_iyd, &m_iangle);
            m_was_moving = 1;
        }

        m_ilx = (int)(m_width / 2.0F + cosf(m_iangle * (M_PI / 180.0)) * m_ixo);
        m_ily = (int)(m_height / 2.0F + sinf(m_iangle * (M_PI / 180.0)) * m_iyo);

        m_iangle += 2;
        if (m_iangle >= 360)
            m_iangle = 0;

        m_ixo += m_ixd;
        if (m_ixo > ((int)m_width / 2) || m_ixo < -((int)m_width / 2))
        {
            m_ixo = (m_ixo > 0) ? (m_width / 2) : -(m_width / 2);
            if (random() & 1)
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
            if (random() & 1)
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

    if (m_color_cycle)
    {
        if (!m_was_color)
        {
            rgb_to_hsv(m_color, &m_ih, &m_is, &m_iv);
            m_was_color = 1;

            if (random() & 1)
            {
                m_ihd = (random() & 1) * 2 - 1;
                m_isd = 0;
            }
            else
            {
                m_isd = 0.01 * ((random() & 1) * 2 - 1);
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
            if ((random() % 150) == 0)
            {
                if (random() & 1)
                {
                    m_ihd = (random() & 1) * 2 - 1;
                    m_isd = 0;
                }
                else
                {
                    m_isd = 0.01 * ((random() & 1) * 2 - 1);
                    m_ihd = 0;
                }
            }
        }
        else
        {
            m_is += m_isd;

            if (m_is <= 0 || m_is >= 0.5)
            {
                if (m_is < 0)
                    m_is = 0;
                if (m_is > 0.52)
                    m_isd = -0.01;
                else if (m_is == 0)
                {
                    m_ihd = random() % 360;
                    m_isd = 0.01;
                }
                else
                {
                    if (random() & 1)
                    {
                        m_ihd = (random() & 1) * 2 - 1;
                        m_isd = 0;
                    }
                    else
                    {
                        m_isd = 0.01 * ((random() & 1) * 2 - 1);
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

    VisualBase *create(MainVisual *parent, const QString &pluginName) const override // VisFactory
    {
        (void)parent;
        (void)pluginName;
        return new BumpScope();
    }
}BumpScopeFactory;

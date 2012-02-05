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

#include <QPainter>

BumpScope::BumpScope() :
    m_image(NULL),

    m_size(0,0),

    m_color(0x2050FF),
    m_x(0), m_y(0), m_width(800), m_height(600),
    m_phongrad(800),

    m_color_cycle(true),
    m_moving_light(true),
    //m_diamond(true),

    m_bpl(0),

    m_rgb_buf(NULL),

    m_iangle(0), m_ixo(0), m_iyo(0), m_ixd(0), m_iyd(0), m_ilx(0), m_ily(0),
    m_was_moving(0), m_was_color(0),
    m_ih(0.0), m_is(0.0), m_iv(0.0), m_isd(0.0), m_ihd(0),
    m_icolor(0)
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
    if (m_rgb_buf)
        delete [] m_rgb_buf;

    if (m_image)
        delete m_image;

    for (unsigned int i = 0; i < m_phongdat.size(); i++)
        m_phongdat[i].resize(0);
    m_phongdat.resize(0);
}

void BumpScope::resize(const QSize &newsize)
{
    m_size = newsize;

    m_size.setHeight((m_size.height() / 2) * 2);
    m_size.setWidth((m_size.width() / 4) * 4);

    if (m_rgb_buf)
        delete [] m_rgb_buf;

    int bufsize = (m_size.height() + 2) * (m_size.width() + 2);

    m_rgb_buf = new unsigned char[bufsize];

    m_bpl = m_size.width() + 2;

    if (m_image)
        delete m_image;

    m_image = new QImage(m_size.width(), m_size.height(), QImage::Format_Indexed8);

    m_width = m_size.width();
    m_height = m_size.height();
    m_phongrad = m_width;

    m_x = m_width / 2;
    m_y = m_height;

    m_phongdat.resize(m_phongrad * 2);
    for (unsigned int i = 0; i < m_phongdat.size(); i++)
        m_phongdat[i].resize(m_phongrad * 2);

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
    unsigned int i, red, blue, green, r, g, b;

    if (m_image)
    {
        red = (unsigned int)(color / 0x10000);
        green = (unsigned int)((color % 0x10000) / 0x100);
        blue = (unsigned int)(color % 0x100);

        for (i = 255; i > 0; i--)
        {
             r = (unsigned int)(((double)(100 * red / 255) * m_intense1[i] + m_intense2[i]));
             if (r > 255)
                 r = 255;
             g = (unsigned int)(((double)(100 * green / 255) * m_intense1[i] + m_intense2[i]));
             if (g > 255)
                 g = 255;
             b = (unsigned int)(((double)(100 * blue / 255) * m_intense1[i] + m_intense2[i]));
             if (b > 255)
                 b = 255;

             m_image->setColor(i, qRgba(r, g, b, 255));
         }

         m_image->setColor(0, m_image->color(1));
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
        p = buffer + ((y1 + 1) * m_bpl) + x + 1;
        for (y = y1; y <= y2; y++)
        {
            *p = 0xff;
            p += m_bpl;
        }
    }
    else if (y2 < y1)
    {
        p = buffer + ((y2 + 1) * m_bpl) + x + 1;
        for (y = y2; y <= y1; y++)
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
    int prev_y, out_y, dy, dx, xp, yp;
    unsigned int PHONGRES = m_phongrad * 2;
    unsigned int i, j;

    prev_y = m_bpl + 1;
    out_y = 0;
    unsigned char *outputbuf = (unsigned char *)(m_image->bits());

    for (dy = (-ly) + (PHONGRES / 2), j = 0; j < m_height; j++, dy++,
         prev_y += m_bpl - m_width)
    {
        for (dx = (-lx) + (PHONGRES / 2), i = 0; i < m_width; i++, dx++,
             prev_y++, out_y++)
        {
            xp = (m_rgb_buf[prev_y - 1] - m_rgb_buf[prev_y + 1]) + dx;
            yp = (m_rgb_buf[prev_y - m_bpl] - m_rgb_buf[prev_y + m_bpl]) + dy;

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
    if (!node || node->length == 0 || !m_image)
        return false;

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

    m_ilx = m_x;
    m_ily = m_y;

    if (m_moving_light)
    {
        if (!m_was_moving)
        {
            translate(m_ilx, m_ily, &m_ixo, &m_iyo, &m_ixd, &m_iyd, &m_iangle);
            m_was_moving = 1;
        }

        m_ilx = (int)(m_width / 2 + cos(m_iangle * (M_PI / 180.0)) * m_ixo);
        m_ily = (int)(m_height / 2 + sin(m_iangle * (M_PI / 180.0)) * m_iyo);

        m_iangle += 2;
        if (m_iangle >= 360)
            m_iangle = 0;

        m_ixo += m_ixd;
        if ((int)m_ixo > ((int)m_width / 2) || (int)m_ixo < -((int)m_width / 2))
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
        if ((int)m_iyo > ((int)m_height / 2) || (int)m_iyo < -((int)m_height / 2))
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

    VisualBase *create(MainVisual *parent, const QString &pluginName) const
    {
        (void)parent;
        (void)pluginName;
        return new BumpScope();
    }
}BumpScopeFactory;

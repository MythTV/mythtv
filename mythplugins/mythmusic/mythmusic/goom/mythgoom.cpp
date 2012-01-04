#include "mythgoom.h"

#include <cmath>
#include <cstdlib>

#include <iostream>
using namespace std;

#include <QPainter>

#include <compat.h>
#include <mythcontext.h>
#include <mythlogging.h>

extern "C" {
#include "goom_tools.h"
#include "goom_core.h"
}

Goom::Goom()
{
    m_fps = 20;

    m_buffer = NULL;

    goom_init(800, 600, 0);

    m_scalew = 1; //gCoreContext->GetNumSetting("VisualScaleWidth", 2);
    m_scaleh = 1; //gCoreContext->GetNumSetting("VisualScaleHeight", 2);

    if (m_scaleh > 2)
        m_scaleh = 2;
    if (m_scaleh < 1)
        m_scaleh = 1;

    if (m_scalew > 2)
        m_scalew = 2;
    if (m_scalew < 1)
        m_scalew = 1;
}

Goom::~Goom()
{
    goom_close();
}

void Goom::resize(const QSize &newsize)
{
    m_size = newsize;

    m_size.setHeight((m_size.height() / 2) * 2);
    m_size.setWidth((m_size.width() / 2) * 2);

    goom_set_resolution(m_size.width() / m_scalew, m_size.height() / m_scaleh, 0);
}

bool Goom::process(VisualNode *node)
{
    if (!node || node->length == 0)
        return false;

    int numSamps = 512;
    if (node->length < 512)
        numSamps = node->length;

    signed short int data[2][512];

    int i = 0;
    for (i = 0; i < numSamps; i++)
    {
        data[0][i] = node->left[i];
        if (node->right)
            data[1][i] = node->right[i];
        else
            data[1][i] = data[0][i];
    }

    for (; i < 512; i++)
    {
        data[0][i] = 0;
        data[1][i] = 0;
    }

    m_buffer = goom_update(data, 0);

    return false;
}

bool Goom::draw(QPainter *p, const QColor &back)
{
    p->fillRect(0, 0, m_size.width(), m_size.height(), back);

    if (!m_buffer)
        return true;

#if 0
    if (scalew != 1 || scaleh != 1)
    {
        register int *d = (int*)surface->pixels;
        register int *s = (int*)buffer;

        int sw = (size.width() / scalew) << 2;
        int sw2 = surface->pitch;
        int swd = sw2 - sw * scalew;

        long fin = (long)s;
        long fd = (long)d + (sw2 * size.height());

        while ((long)d < fd) {
            fin += sw;
            if (scalew == 2)
            {
                while ((long)s < fin) {
                    register long col = *(s++);
                    *(d++) = col; *(d++) = col;
                } 
            }
            else
            {
                while ((long)s < fin) {
                    register long col = *(s++);
                    *(d++) = col;
                }
            }
            d = (int*)((char*)d + swd);

            if (scaleh == 2)
            {
                memcpy(d, ((char*)d) - sw2, sw2);
                d = (int*)((char*)d + sw2);
            }
        }
    }
    else
    {
        SDL_Surface *tmpsurf = SDL_CreateRGBSurfaceFrom(buffer, size.width(),
                                                        size.height(), 32, 
                                                        size.width() * 4,
                                                        0x00ff0000, 0x0000ff00,
                                                        0x000000ff, 0x00000000);
        SDL_BlitSurface(tmpsurf, NULL, surface, NULL);
        SDL_FreeSurface(tmpsurf);
    }

    SDL_UnlockSurface(surface);
    SDL_Flip(surface);
#endif
    QImage *image = new QImage((uchar*) m_buffer,  m_size.width(), m_size.height(), m_size.width() * 4, QImage::Format_ARGB32);

    p->drawImage(0, 0, *image);

    delete image;

    return true;
}

//#if 0
static class GoomFactory : public VisFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("Goom");
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
        return new Goom();
    }
}GoomFactory;
//#endif
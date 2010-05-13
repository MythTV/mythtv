#include "mythgoom.h"

#ifdef SDL_SUPPORT

#include <cmath>
#include <cstdlib>

#include <iostream>
using namespace std;

#include <QPainter>

#include <compat.h>
#include <mythcontext.h>

extern "C" {
#include "goom_tools.h"
#include "goom_core.h"
}

Goom::Goom(long int winid)
{
    fps = 20;

    surface = NULL;
    buffer = NULL;

    char SDL_windowhack[32];
    //char SDL_windowhack[sizeof(long int)];
    sprintf(SDL_windowhack, "%ld", winid);
    setenv("SDL_WINDOWID", SDL_windowhack, 1);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0)
    {
        VERBOSE(VB_IMPORTANT, "Unable to init SDL");
        return;
    }

    SDL_ShowCursor(0);

    goom_init(800, 600, 0);

    scalew = gCoreContext->GetNumSetting("VisualScaleWidth", 2);
    scaleh = gCoreContext->GetNumSetting("VisualScaleHeight", 2);

    if (scaleh > 2)
        scaleh = 2;
    if (scaleh < 1)
        scaleh = 1;

    if (scalew > 2)
        scalew = 2;
    if (scalew < 1)
        scalew = 1;
}

Goom::~Goom()
{
    goom_close();
    SDL_Quit();

    unsetenv("SDL_WINDOWID");
}

void Goom::resize(const QSize &newsize)
{
    size = newsize;

    size.setHeight((size.height() / 2) * 2);
    size.setWidth((size.width() / 2) * 2);

    surface = SDL_SetVideoMode(size.width(), size.height(), 32, 0 /* SDL_ANYFORMAT */);
    goom_set_resolution(size.width() / scalew, size.height() / scaleh, 0);
}

bool Goom::process(VisualNode *node)
{
    if (!node || node->length == 0 || !surface)
        return true;

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

    buffer = goom_update(data, 0);

    return false;
}

bool Goom::draw(QPainter *p, const QColor &back)
{
    (void)p;
    (void)back;

    if (!surface)
    {
        VERBOSE(VB_IMPORTANT, "No sdl surface");
        return false;
    }

    if (!buffer)
        return false;

    if (scalew != 1 || scaleh != 1)
    {
        SDL_LockSurface(surface);

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

    return false;
}

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

    VisualBase *create(MainVisual *parent, long int winid, const QString &pluginName) const
    {
        (void)parent;
        (void)pluginName;
        return new Goom(winid);
    }
}GoomFactory;


#endif

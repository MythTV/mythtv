#include "mainvisual.h"
#include "mythgoom.h"

#ifdef SDL_SUPPORT

extern "C" {
#include "goom_tools.h"
#include "goom_core.h"
}

#include <qpainter.h>

#include <math.h>
#include <stdlib.h>

#include <iostream>
using namespace std;

#include <mythtv/mythcontext.h>

Goom::Goom(long int winid)
{
    fps = 20;

    surface = NULL;

    static char SDL_windowhack[32];
    sprintf(SDL_windowhack, "SDL_WINDOWID=%ld", winid);
    putenv(SDL_windowhack);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0)
    {
        cerr << "Unable to init SDL\n";
        return;
    }

    surface = SDL_SetVideoMode(800, 600, 32, SDL_RESIZABLE);
    SDL_ShowCursor(0);

    goom_init(800, 600, 0);

    scalew = gContext->GetNumSetting("VisualScaleWidth", 2);
    scaleh = gContext->GetNumSetting("VisualScaleHeight", 2);

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
}

void Goom::resize(const QSize &newsize)
{
    size = newsize;

    size.setHeight((size.height() / 2) * 2);
    size.setWidth((size.width() / 2) * 2);

    surface = SDL_SetVideoMode(size.width(), size.height(), 32, SDL_RESIZABLE);
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
        data[1][i] = node->right[i];
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
        cerr << "No sdl surface\n";
        return false;
    }

    if (scalew != 1 || scaleh != 1)
    {
        SDL_LockSurface(surface);

        register int *d = (int*)surface->pixels;
        register int *s = (int*)buffer;

        int sw = (size.width() / scalew) << 2;
        int sw2 = surface->pitch;
        int swd = sw2 - sw * scalew;

        int fin = (int)s;
        int fd = (int)d + (sw2 * size.height());

        while ((int)d < fd) {
            fin += sw;
            if (scalew == 2)
            {
                while ((int)s < fin) {
                    register int col = *(s++);
                    *(d++) = col; *(d++) = col;
                } 
            }
            else
            {
                while ((int)s < fin) {
                    register int col = *(s++);
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

const QString &GoomFactory::name(void) const
{
    static QString name("Goom");
    return name;
}

VisualBase *GoomFactory::create(MainVisual *parent, long int winid)
{
    (void)parent;
    return new Goom(winid);
}

#endif

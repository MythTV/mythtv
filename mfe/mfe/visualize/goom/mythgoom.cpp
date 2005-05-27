#include "mythgoom.h"

extern "C" {
#include "goom_tools.h"
#include "goom_core.h"
}

#include <qpainter.h>
#include <qimage.h>

#include <math.h>
#include <stdlib.h>

#include <iostream>
using namespace std;

#include <mythtv/mythcontext.h>

Goom::Goom()
     :MfeVisualizer()
{
    fps = 40;
    visualization_type = MVT_GOOM;
    buffer = NULL;
    goom_init(800, 600, 0);

    scalew = 1; //gContext->GetNumSetting("VisualScaleWidth", 2);
    scaleh = 1; //gContext->GetNumSetting("VisualScaleHeight", 2);

/*
    if (scaleh > 2)
        scaleh = 2;
    if (scaleh < 1)
        scaleh = 1;

    if (scalew > 2)
        scalew = 2;
    if (scalew < 1)
        scalew = 1;
*/
}

Goom::~Goom()
{
    goom_close();
}

void Goom::resize(QSize newsize, QColor)
{
    size = newsize;

    size.setHeight((size.height() / 2) * 2);
    size.setWidth((size.width() / 2) * 2);

    goom_set_resolution(size.width() / scalew, size.height() / scaleh, 0);
}

bool Goom::update(QPixmap *pixmap_to_draw_on)
{

    if(nodes.size() < 1)
    {
        return false;
    }


    std::deque<VisualNode*>::iterator iter;
    int process_at_most = 0;
    for (iter = nodes.begin(); iter != nodes.end(); iter++)
    {
        if (process_at_most < 1)
        {
            process( (*iter) );
        }
        ++process_at_most;
    }
                         
    nodes.clear();

    QPainter p(pixmap_to_draw_on);
    return draw(&p, Qt::black);
    return true;
}

bool Goom::process(VisualNode *node)
{
    if (!node || node->length == 0)
    {
        return true;
    }

    int numSamps = 256;
    if (node->length < 256)
        numSamps = node->length;

    signed short int data[2][256];
   
    int i = 0;
    for (i = 0; i < numSamps; i++)
    {
        data[0][i] = node->left[i];
        if (node->right)
            data[1][i] = node->right[i];
        else
            data[1][i] = data[0][i];
    }

    for (; i < 256; i++)
    {
        data[0][i] = 0;
        data[1][i] = 0;
    }

    buffer = goom_update(data, 0);

    return false;
}

bool Goom::draw(QPainter *p, const QColor &back)
{
    (void)back;

    if (!buffer)
        return false;


    if (scalew != 1 || scaleh != 1)
    {
        cerr << "should not be here" << endl;
/*
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
*/
    }
    else
    {
        QImage an_image = QImage((uchar *) buffer, size.width(), size.height(), 32, NULL, 256*256*256, QImage::LittleEndian);
        p->drawImage(0,0,an_image); 
    }

    return true;
}


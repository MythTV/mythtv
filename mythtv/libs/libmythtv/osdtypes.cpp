#include <qimage.h>
#include <math.h>

#include <iostream>
using namespace std;

#include "yuv2rgb.h"
#include "osdtypes.h"
#include "ttfont.h"

OSDSet::OSDSet(const QString &name, bool cache, int screenwidth, 
               int screenheight, float wmult, float hmult)
{
    m_name = name;
    m_cache = cache;

    m_framerate = 30;

    m_hasdisplayed = false;
    m_displaying = false;
    m_framesleft = 0;
    m_allowfade = true;

    m_screenwidth = screenwidth;
    m_screenheight = screenheight;
    m_wmult = wmult;
    m_hmult = hmult;

    m_notimeout = false;
    m_fadeframes = -1;
    m_maxfade = -1;
 
    m_xmove = 0;
    m_ymove = 0;

    m_xoff = 0;
    m_yoff = 0;

    m_priority = 5;

    allTypes = new vector<OSDType *>;
}

OSDSet::~OSDSet()
{
    vector<OSDType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        OSDType *type = (*i);
        if (type)
            delete type;
    }
    delete allTypes;
}
 
void OSDSet::AddType(OSDType *type)
{
    typeList[type->Name()] = type;
    allTypes->push_back(type);
    type->SetParent(this);
}

OSDType *OSDSet::GetType(const QString &name)
{
    OSDType *ret = NULL;
    if (typeList.contains(name))
        ret = typeList[name];

    return ret;
}

void OSDSet::Display(void)
{
    m_notimeout = true;
    m_displaying = true;
    m_framesleft = 1;
    m_fadeframes = -1;
    m_xoff = 0;
    m_yoff = 0;
}

void OSDSet::DisplayFor(int frames)
{
    m_framesleft = frames;
    m_displaying = true;
    m_fadeframes = -1;
    m_notimeout = false;
    m_xoff = 0;
    m_yoff = 0;
}
 
void OSDSet::FadeFor(int frames)
{
    if (m_allowfade)
    {
        m_framesleft = -1;
        m_fadeframes = frames;
        m_maxfade = frames;
        m_displaying = true;
        m_notimeout = false;
    }
}

void OSDSet::Hide(void)
{
    m_framesleft = 1;
    m_notimeout = false;
}

void OSDSet::Draw(unsigned char *yuvptr)
{
    vector<OSDType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        OSDType *type = (*i);
        type->Draw(yuvptr, m_screenwidth, m_screenheight, m_fadeframes, 
                   m_maxfade, m_xoff, m_yoff);
    }

    m_hasdisplayed = true;

    if (m_notimeout)
        return;

    if (m_framesleft >= 0)
        m_framesleft--;
    if (m_fadeframes >= 0)
    {
        m_fadeframes--;
        if (m_xmove || m_ymove)
        {
            m_xoff += m_xmove;
            m_yoff += m_ymove;
            m_fadeframes -= 4;
        }
    }
    if (m_framesleft <= 0 && m_fadeframes <= 0)
        m_displaying = false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 

OSDType::OSDType(const QString &name)
{
    m_name = name;
}

OSDType::~OSDType()
{
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeText::OSDTypeText(const QString &name, TTFFont *font, 
                         const QString &text, QRect displayrect)
           : OSDType(name)
{
    m_message = text;
    m_font = font;
    
    m_displaysize = displayrect;
    m_multiline = false;
    m_centered = false;
    m_outline = true;

    m_color = COL_WHITE;
}

OSDTypeText::~OSDTypeText()
{
}

void OSDTypeText::SetText(const QString &text)
{
    m_message = text;
}

void OSDTypeText::Draw(unsigned char *screenptr, int vid_width, int vid_height,
                       int fade, int maxfade, int xoff, int yoff)
{
    int textlength = 0;
    m_font->CalcWidth(m_message, &textlength);

    int maxlength = m_displaysize.width();

    if (m_multiline && textlength > maxlength)
    {
        char *orig = strdup((char *)m_message.ascii());
        int length = 0;
        int lines = 0;
        char line[512];
        memset(line, '\0', 512);

        char *word = strtok(orig, " ");
        while (word)
        {
            if (word[0] == '%') 
            {
                if (word[1] == 'd')
                {
                    int timeleft = (int)(ceil(m_parent->GetFramesLeft() * 1.0 / 
                                              m_parent->GetFrameRate()));
                    if (timeleft > 99)
                        timeleft = 99;
                    if (timeleft < 0)
                        timeleft = 0;
    
                    sprintf(word, "%d", timeleft);
                }
            }
            m_font->CalcWidth(word, &textlength);
            if (textlength + m_font->SpaceWidth() + length > maxlength)
            {
                QRect drawrect = m_displaysize;
                drawrect.setTop(m_displaysize.top() + 
                                m_font->Size() * (lines) * 3 / 2);
                DrawString(screenptr, vid_width, vid_height, drawrect, line, 
                           fade, maxfade, xoff, yoff);
                length = 0;
                memset(line, '\0', 512);
                lines++;
            }
            if (length == 0)
            {
                length = textlength;
                strcpy(line, word);
            }
            else
            {
                length += textlength + m_font->SpaceWidth();
                strcat(line, " ");
                strcat(line, word);
            }
            word = strtok(NULL, " ");
        }
        QRect drawrect = m_displaysize;
        drawrect.setTop(m_displaysize.top() + m_font->Size() * (lines) * 3 / 2);
        DrawString(screenptr, vid_width, vid_height, drawrect, line, fade, 
                   maxfade, xoff, yoff);
        free(orig);                   
    }           
    else        
        DrawString(screenptr, vid_width, vid_height, m_displaysize, m_message, 
                   fade, maxfade, xoff, yoff);
}           
  
void OSDTypeText::DrawString(unsigned char *screenptr, int vid_width,
                             int vid_height, QRect rect, const QString &text, 
                             int fade, int maxfade, int xoff, int yoff)
{
    if (m_centered)
    {
        int textlength = 0;
        m_font->CalcWidth(text, &textlength);

        int xoffset = (rect.width() - textlength) / 2;

        if (xoffset > 0)
            rect.moveBy(xoffset, 0);
    }

    rect.moveBy(xoff, yoff);

    int x = rect.left();
    int y = rect.top();
    int maxx = rect.right();
    int maxy = rect.bottom();

    if (maxx > vid_width)
        maxx = vid_width;

    if (maxy > vid_height)
        maxy = vid_height;

    int alphamod = 255;

    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    if (m_outline)
    {
        m_font->DrawString(screenptr, x - 1, y - 1, text, maxx, maxy, alphamod, 
                           COL_BLACK, false);

        m_font->DrawString(screenptr, x + 1, y - 1, text, maxx, maxy, alphamod,
                           COL_BLACK, false);

        m_font->DrawString(screenptr, x - 1, y + 1, text, maxx, maxy, alphamod,
                           COL_BLACK, false);

        m_font->DrawString(screenptr, x + 1, y + 1, text, maxx, maxy, alphamod,
                           COL_BLACK, false);
    }

    m_font->DrawString(screenptr, x, y, text, maxx, maxy, alphamod, m_color,
                       false);
} 

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeImage::OSDTypeImage(const QString &name, const QString &filename, 
                           QPoint displaypos, float wmult, float hmult, 
                           int scalew, int scaleh)
            : OSDType(name)
{
    m_filename = filename;
    m_displaypos = displaypos;

    m_yuv = m_alpha = NULL;
    m_isvalid = false;

    LoadImage(filename, wmult, hmult, scalew, scaleh);
}

OSDTypeImage::OSDTypeImage(const OSDTypeImage &other)
            : OSDType(other.m_name)
{
    m_filename = other.m_filename;
    m_displaypos = other.m_displaypos;
    m_imagesize = other.m_imagesize;
    m_isvalid = other.m_isvalid;
    m_name = other.m_name;

    m_alpha = m_yuv = NULL;
    if (m_isvalid)
    {
        int size = m_imagesize.width() * m_imagesize.height() * 3 / 2;
        m_yuv = new unsigned char[size];
        memcpy(m_yuv, other.m_yuv, size);


        size = m_imagesize.width() * m_imagesize.height();
        m_alpha = new unsigned char[size];
        memcpy(m_alpha, other.m_alpha, size);

        m_ybuffer = m_yuv;
        m_ubuffer = m_yuv + (m_imagesize.width() * m_imagesize.height());
        m_vbuffer = m_yuv + (m_imagesize.width() * m_imagesize.height() * 
                             5 / 4);
    } 
}

OSDTypeImage::OSDTypeImage(const QString &name)
            : OSDType(name)
{
}

OSDTypeImage::~OSDTypeImage()
{
    if (m_yuv)
        delete [] m_yuv;
    if (m_alpha)
        delete [] m_alpha;
}

void OSDTypeImage::LoadImage(const QString &filename, float wmult, float hmult,
                             int scalew, int scaleh)
{
    if (m_isvalid)
    {
        if (m_yuv)
            delete [] m_yuv;
        if (m_alpha)
            delete [] m_alpha;

        m_isvalid = false;
        m_yuv = NULL;
        m_alpha = NULL;
    }

    QImage tmpimage(filename);
   
    if (tmpimage.width() == 0 || tmpimage.height() == 0)
        return;

    float mult = wmult;

    if (scalew <= 0)
        mult *= 0.91; 

    int width = 0, height = 0;

    if (scalew > 0) 
        width = ((int)(scalew * mult) / 2) * 2;
    else
        width = ((int)(tmpimage.width() * mult) / 2) * 2;

    if (scaleh > 0)
        height = ((int)(scaleh * hmult) / 2) * 2;
    else
        height = ((int)(tmpimage.height() * hmult) / 2) * 2;

    QImage tmp2 = tmpimage.smoothScale(width, height);

    m_isvalid = true;

    m_yuv = new unsigned char[width * height * 3 / 2];
    m_ybuffer = m_yuv;
    m_ubuffer = m_yuv + (width * height);
    m_vbuffer = m_yuv + (width * height * 5 / 4);

    m_alpha = new unsigned char[width * height];

    rgb32_to_yuv420p(m_ybuffer, m_ubuffer, m_vbuffer, m_alpha, tmp2.bits(),
                     width, height, tmp2.width());

    m_imagesize = QRect(0, 0, width, height);
}

void OSDTypeImage::Draw(unsigned char *screenptr, int vid_width, int vid_height,
                        int fade, int maxfade, int xoff, int yoff)
{
    if (!m_isvalid)
        return;

    unsigned char *dest, *src;
    int alpha, tmp1, tmp2;

    int iwidth,width;
    iwidth = width = m_imagesize.width();
    int height = m_imagesize.height();

    int ystart = m_displaypos.y();
    int xstart = m_displaypos.x();

    xstart += xoff;
    ystart += yoff;

    ystart = (ystart / 2) * 2;
    xstart = ((xstart + 1) / 2) * 2;

    if (height + ystart > vid_height)
        height = vid_height - ystart - 1;
    if (width + xstart > vid_width)
        width = vid_width - xstart - 1;

    int ysrcwidth;
    int ydestwidth;

    int alphamod = 255;

    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    for (int y = 0; y < height; y++)
    {
        ysrcwidth = y * width;
        ydestwidth = (y + ystart) * vid_width;

        for (int x = 0; x < width; x++)
        {
            alpha = *(m_alpha + x + ysrcwidth);

            if (alpha == 0)
                continue;

            alpha = ((alpha * alphamod) + 0x80) >> 8;

            dest = screenptr + x + xstart + ydestwidth;
            src = m_ybuffer + x + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;
        }
    }

    width /= 2;
    height /= 2;

    ystart /= 2;
    xstart /= 2;

    unsigned char *destuptr = screenptr + (vid_width * vid_height);
    unsigned char *destvptr = screenptr + (vid_width * vid_height * 5 / 4);

    for (int y = 0; y < height; y++)
    {
        ysrcwidth = y * width;
        ydestwidth = (y + ystart) * (vid_width / 2);

        for (int x = 0; x < width; x++)
        {
            alpha = *(m_alpha + x * 2 + y * 2 * iwidth);

            if (alpha == 0)
                continue;

            alpha = ((alpha * alphamod) + 0x80) >> 8;

            dest = destuptr + x + xstart + ydestwidth;
            src = m_ubuffer + x + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;

            dest = destvptr + x + xstart + ydestwidth;
            src = m_vbuffer + x + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 

OSDTypePosSlider::OSDTypePosSlider(const QString &name,
                                   const QString &filename,
                                   QRect displayrect, float wmult,
                                   float hmult, int scalew, int scaleh)
               : OSDTypeImage(name, filename, displayrect.topLeft(), wmult,
                              hmult, scalew, scaleh)
{
    m_maxval = 1000;
    m_curval = 0;
    m_displayrect = displayrect;
}

OSDTypePosSlider::~OSDTypePosSlider()
{
}

void OSDTypePosSlider::SetPosition(int pos)
{
    m_curval = pos;
    if (m_curval > 1000)
        m_curval = 1000;
    if (m_curval < 0)
        m_curval = 0;

    int xpos = (int)((m_displayrect.width() / 1000.0) * m_curval);
    int width = m_imagesize.width() / 2;

    xpos = m_displayrect.left() + xpos - width;

    m_displaypos.setX(xpos);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeFillSlider::OSDTypeFillSlider(const QString &name, 
                                     const QString &filename,
                                     QRect displayrect, float wmult, 
                                     float hmult, int scalew, int scaleh)
                 : OSDTypeImage(name, filename, displayrect.topLeft(), wmult, 
                                hmult, scalew, scaleh)
{
    m_maxval = 1000;
    m_curval = 0;
    m_drawwidth = 0;
    m_displayrect = displayrect;
}

OSDTypeFillSlider::~OSDTypeFillSlider()
{
}

void OSDTypeFillSlider::SetPosition(int pos)
{
    m_curval = pos;
    if (m_curval > 1000)
        m_curval = 1000;
    if (m_curval < 0)
        m_curval = 0;

    m_drawwidth = (int)((m_displayrect.width() / 1000.0) * m_curval);
}

void OSDTypeFillSlider::Draw(unsigned char *screenptr, int vid_width, 
                             int vid_height, int fade, int maxfade, 
                             int xoff, int yoff)
{
    if (!m_isvalid)
        return;

    unsigned char *dest, *src;
    int alpha, tmp1, tmp2;

    int iwidth,width;
    iwidth = m_imagesize.width();
    width = m_drawwidth;
    int height = m_imagesize.height();

    int ystart = m_displaypos.y();
    int xstart = m_displaypos.x();

    xstart += xoff;
    ystart += yoff;

    ystart = (ystart / 2) * 2;
    xstart = (xstart / 2) * 2;

    if (height + ystart > vid_height)
        height = vid_height - ystart - 1;
    if (width + xstart > vid_width)
        width = vid_width - xstart - 1;

    int ysrcwidth;
    int ydestwidth;

    int alphamod = 255;

    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    for (int y = 0; y < height; y++)
    {
        ysrcwidth = y * iwidth;
        ydestwidth = (y + ystart) * vid_width;

        for (int x = 0; x < width; x++)
        {
            alpha = *(m_alpha + ysrcwidth);

            if (alpha == 0)
                continue;

            alpha = ((alpha * alphamod) + 0x80) >> 8;

            dest = screenptr + x + xstart + ydestwidth;
            src = m_ybuffer + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;
        }
    }

    width /= 2;
    height /= 2;
    iwidth /= 2;

    ystart /= 2;
    xstart /= 2;

    unsigned char *destuptr = screenptr + (vid_width * vid_height);
    unsigned char *destvptr = screenptr + (vid_width * vid_height * 5 / 4);

    for (int y = 0; y < height; y++)
    {
        ysrcwidth = y * iwidth;
        ydestwidth = (y + ystart) * (vid_width / 2);

        for (int x = 0; x < width; x++)
        {
            alpha = *(m_alpha + y * 2 * iwidth);

            if (alpha == 0)
                continue;

            alpha = ((alpha * alphamod) + 0x80) >> 8;

            dest = destuptr + x + xstart + ydestwidth;
            src = m_ubuffer + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;

            dest = destvptr + x + xstart + ydestwidth;
            src = m_vbuffer + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeEditSlider::OSDTypeEditSlider(const QString &name,
                                     const QString &bluefilename,
                                     const QString &redfilename,
                                     QRect displayrect, float wmult,
                                     float hmult, int scalew, int scaleh)
                 : OSDTypeImage(name)
{
    m_maxval = 1000;
    m_curval = 0;
    m_displayrect = displayrect;
    m_drawwidth = displayrect.width();

    m_drawMap = new int[m_drawwidth + 1];
    for (int i = 0; i < m_drawwidth; i++)
         m_drawMap[i] = 0;

    m_displaypos = displayrect.topLeft();
    
    m_yuv = m_alpha = NULL;
    m_isvalid = false;

    m_ryuv = m_ralpha = NULL;
    m_risvalid = false;

    LoadImage(redfilename, wmult, hmult, scalew, scaleh);
    if (m_isvalid)
    {
        m_risvalid = m_isvalid;
        m_ralpha = m_alpha;
        m_ryuv = m_yuv;
        m_rimagesize = m_imagesize;
        m_rybuffer = m_ybuffer;
        m_rubuffer = m_ubuffer;
        m_rvbuffer = m_vbuffer;

        m_isvalid = false;
        m_alpha = m_yuv = NULL;
    }

    LoadImage(bluefilename, wmult, hmult, scalew, scaleh);
}

OSDTypeEditSlider::~OSDTypeEditSlider()
{
    if (m_ryuv)
        delete [] m_ryuv;
    if (m_ralpha)
        delete [] m_ralpha;
}

void OSDTypeEditSlider::ClearAll(void)
{
    for (int i = 0; i < m_drawwidth; i++)
        m_drawMap[i] = 0;
}

void OSDTypeEditSlider::SetRange(int start, int end)
{
    start = (int)((m_drawwidth / 1000.0) * start);
    end = (int)((m_drawwidth / 1000.0) * end);

    if (start < 0)
        start = 0;
    if (start >= m_drawwidth) 
        start = m_drawwidth - 1;
    if (end < 0)
        end = 0;
    if (end >= m_drawwidth)
        end = m_drawwidth - 1;

    if (end < start)
    {
        int tmp = start;
        start = end;
        end = tmp;
    }

    for (int i = start; i < end; i++)
        m_drawMap[i] = 1;
}

void OSDTypeEditSlider::Draw(unsigned char *screenptr, int vid_width,
                             int vid_height, int fade, int maxfade,
                             int xoff, int yoff)
{           
    if (!m_isvalid || !m_risvalid)
        return;
            
    unsigned char *dest, *src;
    int a, tmp1, tmp2; 
            
    int iwidth, riwidth, width;
    iwidth = m_imagesize.width();
    riwidth = m_rimagesize.width();
    width = m_drawwidth;
    int height = m_imagesize.height();

    int ystart = m_displaypos.y();
    int xstart = m_displaypos.x();
            
    xstart += xoff;
    ystart += yoff;

    ystart = (ystart / 2) * 2;
    xstart = (xstart / 2) * 2;

    if (height + ystart > vid_height)
        height = vid_height - ystart - 1;
    if (width + xstart > vid_width)  
        width = vid_width - xstart - 1;
                 
    int ysrcwidth;              
    int ydestwidth;
    
    int alphamod = 255;
    
    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    unsigned char *alpha;
    unsigned char *ybuf;
    unsigned char *ubuf;
    unsigned char *vbuf;

    for (int y = 0; y < height; y++)
    {
        ydestwidth = (y + ystart) * vid_width;

        for (int x = 0; x < width; x++)
        {
            if (m_drawMap[x] == 0)
            {
                alpha = m_alpha;
                ybuf = m_ybuffer;
                ysrcwidth = y * iwidth;
            }
            else
            {
                alpha = m_ralpha;
                ybuf = m_rybuffer;
                ysrcwidth = y * riwidth;
            }

            a = *(alpha + ysrcwidth);
        
            if (a == 0)
                continue;

            a = ((a * alphamod) + 0x80) >> 8;

            dest = screenptr + x + xstart + ydestwidth;
            src = ybuf + ysrcwidth;

            tmp1 = (*src - *dest) * a;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;
        }
    }

    width /= 2;
    height /= 2;
    iwidth /= 2;
    riwidth /= 2;

    ystart /= 2;
    xstart /= 2;

    unsigned char *destuptr = screenptr + (vid_width * vid_height);
    unsigned char *destvptr = screenptr + (vid_width * vid_height * 5 / 4);

    for (int y = 0; y < height; y++)
    {
        ydestwidth = (y + ystart) * (vid_width / 2);

        for (int x = 0; x < width; x++)
        {
            if (m_drawMap[x * 2] == 0)
            {
                alpha = m_alpha;
                ubuf = m_ubuffer;
                vbuf = m_vbuffer;
                ysrcwidth = y * iwidth;
            }
            else
            {
                alpha = m_ralpha;
                ubuf = m_rubuffer;
                vbuf = m_rvbuffer;
                ysrcwidth = y * riwidth;
            }

            a = *(alpha + y * 2 * iwidth);

            if (a == 0)
                continue;

            a = ((a * alphamod) + 0x80) >> 8;

            dest = destuptr + x + xstart + ydestwidth;
            src = ubuf + ysrcwidth;

            tmp1 = (*src - *dest) * a;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;

            dest = destvptr + x + xstart + ydestwidth;
            src = vbuf + ysrcwidth;

            tmp1 = (*src - *dest) * a;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeBox::OSDTypeBox(const QString &name, QRect displayrect) 
          : OSDType(name)
{
    size = displayrect;
}

OSDTypeBox::~OSDTypeBox()
{
}

void OSDTypeBox::Draw(unsigned char *screenptr, int vid_width, int vid_height,
                      int fade, int maxfade, int xoff, int yoff)
{
    unsigned char *src;
    unsigned char alpha = 192;

    QRect disprect = size;
    disprect.moveBy(xoff, yoff);

    int ystart = disprect.top();
    int yend = disprect.bottom();
    int xstart = disprect.left();
    int xend = disprect.right();

    if (xstart < 0)
        xstart = 0;
    if (xend > vid_width)
        xend = vid_width;
    if (ystart < 0)
        ystart = 0;
    if (yend > vid_height)
        yend = vid_height;

    int tmp1, tmp2;
    int alphamod = 255;
    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    alpha = ((alpha * alphamod) + 0x809) >> 8;

    for (int y = ystart; y < yend; y++)
    {
        for (int x = xstart; x < xend; x++)
        {
            src = screenptr + x + y * vid_width;

            tmp1 = (0 - *src) * alpha;
            tmp2 = *src + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *src = tmp2 & 0xff;
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypePositionRectangle::OSDTypePositionRectangle(const QString &name)
          : OSDType(name)
{
    m_numpositions = 0;
    m_curposition = -1;
}       
    
OSDTypePositionRectangle::~OSDTypePositionRectangle()
{
}

void OSDTypePositionRectangle::AddPosition(QRect rect)
{
    positions.push_back(rect);
    m_numpositions++;
}

void OSDTypePositionRectangle::SetPosition(int pos)
{
    m_curposition = pos;
    if (m_curposition >= m_numpositions)
        m_curposition = m_numpositions - 1;
}

void OSDTypePositionRectangle::PositionUp(void)
{
    if (m_curposition > 0)
        m_curposition--;
}

void OSDTypePositionRectangle::PositionDown(void)
{
    if (m_curposition < m_numpositions - 1)
        m_curposition++;
}

void OSDTypePositionRectangle::Draw(unsigned char *screenptr, int vid_width, 
                                    int vid_height, int fade, int maxfade, 
                                    int xoff, int yoff)
{
    vid_height = vid_height;
    fade = fade;
    maxfade = maxfade;
    xoff = xoff;
    yoff = yoff;

    if (m_curposition < 0 || m_curposition >= m_numpositions)
        return;

    QRect rect = positions[m_curposition];

    unsigned char *src;
    int ystart = rect.top();
    int yend = rect.bottom();
    int xstart = rect.left();
    int xend = rect.right();

    for (int y = ystart; y < yend; y++)
    {
        for (int x = xstart; x < xstart + 2; x++)
        {
            src = screenptr + x + y * vid_width;
            *src = 255;
        }
        for (int x = xend - 2; x < xend; x++)
        {
            src = screenptr + x + y * vid_width;
            *src = 255;
        }
    }

    for (int x = xstart; x < xend; x++)
    {
        for (int y = ystart; y < ystart + 2; y++)
        {
            src = screenptr + x + y * vid_width;
            *src = 255;
        }
        for (int y = yend - 2; y < yend; y++)
        {
            src = screenptr + x + y * vid_width;
            *src = 255;
        }
    }
}

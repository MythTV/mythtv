#include <qimage.h>
#include <qmap.h>
#include <qregexp.h>
#include <math.h>

#include <iostream>
using namespace std;

#include "yuv2rgb.h"
#include "osdtypes.h"
#include "ttfont.h"

#include "mythcontext.h"

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

OSDSet::OSDSet(const OSDSet &other)
{
    m_screenwidth = other.m_screenwidth;
    m_screenheight = other.m_screenheight;
    m_framerate = other.m_framerate;
    m_wmult = other.m_wmult;
    m_hmult = other.m_hmult;
    m_cache = other.m_cache;
    m_name = other.m_name;
    m_notimeout = other.m_notimeout;
    m_hasdisplayed = other.m_hasdisplayed;
    m_framesleft = other.m_framesleft;
    m_displaying = other.m_displaying;
    m_fadeframes = other.m_fadeframes;
    m_maxfade = other.m_maxfade;
    m_priority = other.m_priority;
    m_xmove = other.m_xmove;
    m_ymove = other.m_ymove;
    m_xoff = other.m_xoff;
    m_yoff = other.m_yoff;
    m_allowfade = other.m_allowfade;

    allTypes = new vector<OSDType *>;

    vector<OSDType *>::iterator iter = other.allTypes->begin();
    for (;iter != other.allTypes->end(); iter++)
    {
        OSDType *type = (*iter);
        if (OSDTypeText *item = dynamic_cast<OSDTypeText*>(type))
        {
            OSDTypeText *newtext = new OSDTypeText(*item);
            AddType(newtext);
        }
        else if (OSDTypePositionImage *item =
                  dynamic_cast<OSDTypePositionImage*>(type))
        {
            OSDTypePositionImage *newrect = new OSDTypePositionImage(*item);
            AddType(newrect);
        }
        else if (OSDTypeImage *item = dynamic_cast<OSDTypeImage*>(type))
        {
            OSDTypeImage *newimage = new OSDTypeImage(*item);
            AddType(newimage);
        }
        else if (OSDTypeBox *item = dynamic_cast<OSDTypeBox*>(type))
        {
            OSDTypeBox *newbox = new OSDTypeBox(*item);
            AddType(newbox);
        }
        else if (OSDTypePositionRectangle *item = 
                  dynamic_cast<OSDTypePositionRectangle*>(type))
        {
            OSDTypePositionRectangle *newrect = 
                               new OSDTypePositionRectangle(*item);
            AddType(newrect);
        }
        else
        {
            cerr << "Unknown conversion\n";
        }
    }
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

void OSDSet::ClearAllText(void)
{
    vector<OSDType *>::iterator iter = allTypes->begin();
    for (; iter != allTypes->end(); iter++)
    {
        OSDType *type = (*iter);
        if (OSDTypeText *item = dynamic_cast<OSDTypeText *>(type))
            item->SetText(QString(""));
    }
}

void OSDSet::SetTextByRegexp(QMap<QString, QString> &regexpMap)
{
    vector<OSDType *>::iterator iter = allTypes->begin();
    for (; iter != allTypes->end(); iter++)
    {
        OSDType *type = (*iter);
        if (OSDTypeText *item = dynamic_cast<OSDTypeText *>(type))
        {
            QMap<QString, QString>::Iterator riter = regexpMap.begin();
            QString new_text = item->GetDefaultText();
            QString full_regex;

            if ((new_text == "") &&
                (regexpMap.contains(item->Name())))
            {
                new_text = regexpMap[item->Name()];
            }
            else
                for (; riter != regexpMap.end(); riter++)
                {
                    full_regex = "%" + riter.key().upper() + "%";
                    new_text.replace(QRegExp(full_regex), riter.data());
                }

            if (new_text != "")
                item->SetText(new_text);
        }
    }
}

void OSDSet::Display(bool onoff)
{
    if (onoff)
    {
        m_notimeout = true;
        m_displaying = true;
        m_framesleft = 1;
        m_fadeframes = -1;
        m_xoff = 0;
        m_yoff = 0;
    }
    else
    {
        m_displaying = false;
    }
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
    m_default_msg = text;
    m_font = font;

    m_altfont = NULL;
    
    m_displaysize = displayrect;
    m_multiline = false;
    m_centered = false;
    m_right = false;
    m_usingalt = false;
}

OSDTypeText::OSDTypeText(const OSDTypeText &other)
           : OSDType(other.m_name)
{
    m_displaysize = other.m_displaysize;
    m_message = other.m_message;
    m_default_msg = other.m_default_msg;
    m_font = other.m_font;
    m_altfont = other.m_altfont;
    m_centered = other.m_centered;
    m_multiline = other.m_multiline;
    m_usingalt = other.m_usingalt;
    m_right = other.m_right;
}

OSDTypeText::~OSDTypeText()
{
}

void OSDTypeText::SetAltFont(TTFFont *font)
{
    m_altfont = font;
}

void OSDTypeText::SetText(const QString &text)
{
    m_message = text;
}

void OSDTypeText::SetDefaultText(const QString &text)
{
    m_message = text;
    m_default_msg = text;
}

void OSDTypeText::Draw(unsigned char *screenptr, int vid_width, int vid_height,
                       int fade, int maxfade, int xoff, int yoff)
{
    int textlength = 0;
    if (m_message == QString::null)
        return;

    m_font->CalcWidth(m_message, &textlength);

    int maxlength = m_displaysize.width();

    if (m_multiline)
    {
        QString tmp_msg = m_message;
        tmp_msg.replace(QRegExp("%BR%"), "\n");
        tmp_msg.replace(QRegExp("\n")," \n ");

        char *orig = strdup((char *)tmp_msg.ascii());
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

            if ((!length) && (!strcmp(word, "\n")))
            {
                word = strtok(NULL, " ");
                continue;
            }

            m_font->CalcWidth(word, &textlength);
            if ((textlength + m_font->SpaceWidth() + length > maxlength) ||
                (!strcmp(word, "\n")))
            {
                QRect drawrect = m_displaysize;
                drawrect.setTop(m_displaysize.top() + 
                                m_font->Size() * (lines) * 3 / 2);
                DrawString(screenptr, vid_width, vid_height, drawrect, line, 
                           fade, maxfade, xoff, yoff);
                length = 0;
                memset(line, '\0', 512);
                lines++;

                if (!strcmp(word, "\n"))
                {
                    word[0] = '\0';
                    textlength = 0;
                }
            }
            if (length == 0)
            {
                length = textlength;
                strcpy(line, word);
            }
            else
            {
                if (length)
                    strcat(line, " ");
                length += textlength + m_font->SpaceWidth();
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
    if (m_centered || m_right)
    {
        int textlength = 0;
        m_font->CalcWidth(text, &textlength);

        int xoffset = rect.width() - textlength;
        if (m_centered)
            xoffset /= 2;

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

    TTFFont *font = m_font;
    if (m_usingalt && m_altfont)
        font = m_altfont;

    font->DrawString(screenptr, x, y, text, maxx, maxy, alphamod);
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
    m_imagesize = QRect(0, 0, 0, 0);

    m_scalew = scalew;
    m_scaleh = scaleh;

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
    m_scalew = other.m_scalew;
    m_scaleh = other.m_scaleh;

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
    m_yuv = NULL;
    m_alpha = NULL;
    m_ybuffer = NULL;
    m_ubuffer = NULL;
    m_vbuffer = NULL;
    m_isvalid = false;
    m_filename = "";
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

    if (filename.length() < 2)
        return;

    QImage tmpimage(filename);

    if (tmpimage.width() == 0)
    {
        QString url = gContext->GetMasterHostPrefix();
        if (url.length() < 1)
            return;

        url += filename;

        QImage *cached = gContext->CacheRemotePixmap(url, false);
        if (cached)
            tmpimage = *cached;
    }

    if (tmpimage.width() == 0)
        return;

    if (scalew > 0 && m_scalew > 0)
        scalew = m_scalew;
    if (scaleh > 0 && m_scaleh > 0)
        scaleh = m_scaleh;

    int width = 0, height = 0;

    if (scalew > 0) 
        width = ((int)(scalew * wmult) / 2) * 2;
    else
        width = ((int)(tmpimage.width() * wmult) / 2) * 2;

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

    int startline = 0;
    int startcol = 0;

    if (ystart < 0)
    {
        startline = 0 - ystart;
        ystart = 0;
    }

    if (xstart < 0)
    {
        startcol = 0 - xstart;
        xstart = 0;
    }
   
    int ysrcwidth;
    int ydestwidth;

    int alphamod = 255;

    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    for (int y = startline; y < height; y++)
    {
        ysrcwidth = y * width;
        ydestwidth = (y + ystart - startline) * vid_width;

        for (int x = startcol; x < width; x++)
        {
            alpha = *(m_alpha + x + ysrcwidth);

            if (alpha == 0)
                continue;

            alpha = ((alpha * alphamod) + 0x80) >> 8;

            dest = screenptr + (x + xstart - startcol) + ydestwidth;
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

    startline /= 2;
    startcol /= 2;

    unsigned char *destuptr = screenptr + (vid_width * vid_height);
    unsigned char *destvptr = screenptr + (vid_width * vid_height * 5 / 4);

    for (int y = startline; y < height; y++)
    {
        ysrcwidth = y * width;
        ydestwidth = (y + ystart - startline) * (vid_width / 2);

        for (int x = startcol; x < width; x++)
        {
            alpha = *(m_alpha + x * 2 + y * 2 * iwidth);

            if (alpha == 0)
                continue;

            alpha = ((alpha * alphamod) + 0x80) >> 8;

            dest = destuptr + (x + xstart - startcol) + ydestwidth;
            src = m_ubuffer + x + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;

            dest = destvptr + (x + xstart - startcol) + ydestwidth;
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

    int startline = 0;
    int startcol = 0;

    if (ystart < 0)
    {
        startline = 0 - ystart;
        ystart = 0;
    }

    if (xstart < 0)
    {
        startcol = 0 - xstart;
        xstart = 0;
    }

    int ysrcwidth;
    int ydestwidth;

    int alphamod = 255;

    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    for (int y = startline; y < height; y++)
    {
        ysrcwidth = y * iwidth;
        ydestwidth = (y + ystart - startline) * vid_width;

        for (int x = startcol; x < width; x++)
        {
            alpha = *(m_alpha + ysrcwidth);

            if (alpha == 0)
                continue;

            alpha = ((alpha * alphamod) + 0x80) >> 8;

            dest = screenptr + (x + xstart - startcol) + ydestwidth;
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

    startline /= 2;
    startcol /= 2;

    unsigned char *destuptr = screenptr + (vid_width * vid_height);
    unsigned char *destvptr = screenptr + (vid_width * vid_height * 5 / 4);

    for (int y = startline; y < height; y++)
    {
        ysrcwidth = y * iwidth;
        ydestwidth = (y + ystart - startline) * (vid_width / 2);

        for (int x = startcol; x < width; x++)
        {
            alpha = *(m_alpha + y * 2 * iwidth);

            if (alpha == 0)
                continue;

            alpha = ((alpha * alphamod) + 0x80) >> 8;

            dest = destuptr + (x + xstart - startcol) + ydestwidth;
            src = m_ubuffer + ysrcwidth;

            tmp1 = (*src - *dest) * alpha;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;

            dest = destvptr + (x + xstart - startcol) + ydestwidth;
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
    delete [] m_drawMap;

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
                
    int startline = 0;
    int startcol = 0;

    if (ystart < 0)
    {
        startline = 0 - ystart;
        ystart = 0;
    }

    if (xstart < 0)
    {
        startcol = 0 - xstart;
        xstart = 0;
    }
 
    int ysrcwidth;              
    int ydestwidth;
    
    int alphamod = 255;
    
    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    unsigned char *alpha;
    unsigned char *ybuf;
    unsigned char *ubuf;
    unsigned char *vbuf;

    for (int y = startline; y < height; y++)
    {
        ydestwidth = (y + ystart - startline) * vid_width;

        for (int x = startcol; x < width; x++)
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

            dest = screenptr + (x + xstart - startcol) + ydestwidth;
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

    startline /= 2;
    startcol /= 2;

    unsigned char *destuptr = screenptr + (vid_width * vid_height);
    unsigned char *destvptr = screenptr + (vid_width * vid_height * 5 / 4);

    for (int y = startline; y < height; y++)
    {
        ydestwidth = (y + ystart - startline) * (vid_width / 2);

        for (int x = startcol; x < width; x++)
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

            dest = destuptr + (x + xstart - startcol) + ydestwidth;
            src = ubuf + ysrcwidth;

            tmp1 = (*src - *dest) * a;
            tmp2 = *dest + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *dest = tmp2 & 0xff;

            dest = destvptr + (x + xstart - startcol) + ydestwidth;
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

OSDTypeBox::OSDTypeBox(const OSDTypeBox &other)
          : OSDType(other.m_name)
{
    size = other.size;
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

OSDTypePositionIndicator::OSDTypePositionIndicator(void)
{
    m_numpositions = 0;
    m_curposition = -1;
    m_offset = 0;
}

OSDTypePositionIndicator::OSDTypePositionIndicator(
                                      const OSDTypePositionIndicator &other)
{
    m_numpositions = other.m_numpositions;
    m_curposition = other.m_curposition;
    m_offset = other.m_offset;
}

OSDTypePositionIndicator::~OSDTypePositionIndicator()
{
}

void OSDTypePositionIndicator::SetPosition(int pos)
{
    m_curposition = pos + m_offset;
    if (m_curposition >= m_numpositions)
        m_curposition = m_numpositions - 1;
}

void OSDTypePositionIndicator::PositionUp(void)
{
    if (m_curposition > m_offset)
        m_curposition--;
}

void OSDTypePositionIndicator::PositionDown(void)
{
    if (m_curposition < m_numpositions - 1)
        m_curposition++;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


OSDTypePositionRectangle::OSDTypePositionRectangle(const QString &name)
                        : OSDType(name), OSDTypePositionIndicator()
{
}       
   
OSDTypePositionRectangle::OSDTypePositionRectangle(
                                        const OSDTypePositionRectangle &other) 
                        : OSDType(other.m_name), OSDTypePositionIndicator(other)
{
    for (int i = 0; i < m_numpositions; i++)
    {
        QRect tmp = other.positions[i];
        positions.push_back(tmp);
    }
}

OSDTypePositionRectangle::~OSDTypePositionRectangle()
{
}

void OSDTypePositionRectangle::AddPosition(QRect rect)
{
    positions.push_back(rect);
    m_numpositions++;
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
        if (y < 0 || y >= vid_height)
            continue;

        for (int x = xstart; x < xstart + 2; x++)
        {
            if (x < 0 || x >= vid_width)
                continue;
            src = screenptr + x + y * vid_width;
            *src = 255;
        }
        for (int x = xend - 2; x < xend; x++)
        {
            if (x < 0 || x >= vid_width)
                continue;
            src = screenptr + x + y * vid_width;
            *src = 255;
        }
    }

    for (int x = xstart; x < xend; x++)
    {
        if (x < 0 || x >= vid_width)
            continue;

        for (int y = ystart; y < ystart + 2; y++)
        {
            if (y < 0 || y >= vid_height)
                continue;
            src = screenptr + x + y * vid_width;
            *src = 255;
        }
        for (int y = yend - 2; y < yend; y++)
        {
            if (y < 0 || y >= vid_height)
                continue;
            src = screenptr + x + y * vid_width;
            *src = 255;
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypePositionImage::OSDTypePositionImage(const QString &name)
                    : OSDTypeImage(name), OSDTypePositionIndicator()
{
}

OSDTypePositionImage::OSDTypePositionImage(const OSDTypePositionImage &other)
                    : OSDTypeImage(other), OSDTypePositionIndicator(other)
{
    for (int i = 0; i < m_numpositions; i++)
    {
        QPoint tmp = other.positions[i];
        positions.push_back(tmp);
    }
}

OSDTypePositionImage::~OSDTypePositionImage()
{
}

void OSDTypePositionImage::AddPosition(QPoint pos)
{
    positions.push_back(pos);
    m_numpositions++;
}

void OSDTypePositionImage::Draw(unsigned char *screenptr, int vid_width,
                                int vid_height, int fade, int maxfade,
                                int xoff, int yoff)
{
    if (m_curposition < 0 || m_curposition >= m_numpositions)
        return;

    QPoint pos = positions[m_curposition];

    OSDTypeImage::SetPosition(pos);
    OSDTypeImage::Draw(screenptr, vid_width, vid_height, fade, maxfade, xoff, 
                       yoff);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeCC::OSDTypeCC(const QString &name, TTFFont *font, int xoff, int yoff,
                     int dispw, int disph)
         : OSDType(name)
{
    m_font = font;
    m_textlist = NULL;
    xoffset = xoff;
    yoffset = yoff;
    displaywidth = dispw;
    displayheight = disph;

    QRect rect = QRect(0, 0, 0, 0);
    m_box = new OSDTypeBox("cc_background", rect);
}

OSDTypeCC::~OSDTypeCC()
{
    ClearAllCCText();
    delete m_box;
}

void OSDTypeCC::AddCCText(const QString &text, int x, int y, int color, 
                          bool teletextmode)
{
    ccText *cc = new ccText();
    cc->text = text;
    cc->x = x;
    cc->y = y;
    cc->color = color;
    cc->teletextmode = teletextmode;

    if (!m_textlist)
        m_textlist = new vector<ccText *>;

    m_textlist->push_back(cc);
}

void OSDTypeCC::ClearAllCCText()
{
    if (m_textlist)
    {
        vector<ccText*>::iterator i = m_textlist->begin();
        for (; i != m_textlist->end(); i++)
        {
            ccText *cc = (*i);
            if (cc)
                delete cc;
        }
        delete m_textlist;
        m_textlist = NULL;
    }
}

void OSDTypeCC::Draw(unsigned char *screenptr, int vid_width, int vid_height,
                     int fade, int maxfade, int xoff, int yoff)
{
    // not used
    fade = fade;
    maxfade = maxfade;
    xoff = xoff;
    yoff = yoff;

    vector<ccText*>::iterator i = m_textlist->begin();
    for (; i != m_textlist->end(); i++)
    {
        ccText *cc = (*i);

        if (cc && (cc->text != QString::null))
        {
            int textlength = 0;
            m_font->CalcWidth(cc->text, &textlength);

            int x, y;
            if (cc->teletextmode)
            {
                // position as if we use a fixed size font
                // on a 24 row / 40 char grid (teletext expects us to)
                x = cc->y * displaywidth / 40 + xoffset;
                y = cc->x * displayheight / 25 + yoffset;
            }
            else
            {
                x = (cc->x + 1) * displaywidth / 34 + xoffset;
                y = cc->y * displayheight / 18 + yoffset;
            }

            int maxx = x + textlength;
            int maxy = y + m_font->Size() * 3 / 2;

            if (maxx > vid_width)
                maxx = vid_width;

            if (maxy > vid_height)
                maxy = vid_height;

            QRect rect = QRect(0, 0, textlength + 4, 
                               (m_font->Size() * 3 / 2) + 3);
            m_box->SetRect(rect);
            m_box->Draw(screenptr, vid_width, vid_height, 0, 0, x - 2, y - 2);

            m_font->DrawString(screenptr, x, y + 2, cc->text, maxx, maxy, 255); 
        }
    }
}

#include <qapplication.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qcursor.h>
#include <qpainter.h>
#include <qdir.h>
#include <qfileinfo.h>

#include "themesetup.h"
#include "libmyth/mythcontext.h"

using namespace std;

ThemeSetup::ThemeSetup(MythContext *context,  QSqlDatabase *ldb,
                       QWidget *parent, const char *name)
          : MythDialog(context, parent, name)
{
    db = ldb;

    fgcolor = paletteForegroundColor();
    highlightcolor = fgcolor;

    m_font = new QFont("Arial", (int)(context->GetSmallFontSize() * hmult),
                       QFont::Bold);

    thumbw = screenwidth / (THUMBS_W + 1);
    thumbh = screenheight / (THUMBS_H + 1);

    spacew = thumbw / (THUMBS_W + 1);
    spaceh = (thumbh * 5 / 4) / (THUMBS_H + 1);

    thumbh = (screenheight - spaceh * (THUMBS_H + 1)) / THUMBS_H;

    fillList(PREFIX"/share/mythtv/themes");
    screenposition = 0;
    currow = 0;
    curcol = 0;

    WFlags flags = getWFlags();
    setWFlags(flags | Qt::WRepaintNoErase);
}

ThemeSetup::~ThemeSetup()
{
    while (thumbs.size() > 0)
    {
        if (thumbs.back().pixmap)
            delete thumbs.back().pixmap;
        thumbs.pop_back();
    }

    delete m_font;
}

void ThemeSetup::Show(void)
{
    showFullScreen();
}

void ThemeSetup::paintEvent(QPaintEvent *e)
{
    e = e;

    QRect r = QRect(0, 0, screenwidth, screenheight);
    QPainter p(this);

    QPixmap pix(r.size());
    pix.fill(this, r.topLeft());

    QPainter tmp;
    tmp.begin(&pix, this);

    tmp.setFont(*m_font);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
    unsigned int curpos = screenposition;
    for (int y = 0; y < THUMBS_H; y++)
    {
        int ypos = spaceh * (y + 1) + thumbh * y;

        for (int x = 0; x < THUMBS_W; x++)
        {
             if (curpos >= thumbs.size())
                 continue;

             Thumbnail *thumb = &(thumbs[curpos]);

             int xpos = spacew * (x + 1) + thumbw * x;

             if (!thumb->pixmap)
             {
                 loadThumbPixmap(thumb);
             }
             if (thumb->pixmap)
                 tmp.drawPixmap(xpos +
                                (thumbw - thumb->pixmap->width()) / 2,
                                ypos, *thumb->pixmap);      

             tmp.drawText(xpos, ypos + thumbh, thumbw, spaceh,
                          AlignVCenter | AlignCenter, thumb->name);

             if (currow == y && curcol == x)
             {
                 tmp.setPen(QPen(highlightcolor, (int)(3.75 * wmult)));
                 tmp.drawRect(xpos, ypos - (int)(10 * hmult),
                              thumbw, thumbh + spaceh);
                 tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
             }
             curpos++;
        }
    }
 
    tmp.flush();
    tmp.end();

    p.drawPixmap(r.topLeft(), pix);
}

void ThemeSetup::fillList(const QString &dir)
{
    QDir d(dir);

    if (!d.exists())
        return;

    d.setSorting(QDir::Name | QDir::IgnoreCase);
    d.setFilter(QDir::Dirs);

    d.setMatchAllDirs(true);
    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        QString filename = fi->absFilePath();

        Thumbnail thumb;
        thumb.filename = filename;
        thumb.name = fi->baseName();

        if (fi->isDir())
	{
	    thumb.isdir = true;
	    fi->setFile(filename + "/theme.xml");
	}

	if (thumb.isdir == true && fi->exists())
            thumbs.push_back(thumb);
    }
}

void ThemeSetup::loadThumbPixmap(Thumbnail *thumb)
{
    QImage tmpimage(thumb->filename + "/preview.jpg");

    if (tmpimage.width() == 0 || tmpimage.height() == 0)
        return;

    QImage tmp2 = tmpimage.smoothScale(thumbw, thumbh, QImage::ScaleMin);

    thumb->pixmap = new QPixmap();
    thumb->pixmap->convertFromImage(tmp2);
}

void ThemeSetup::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    int oldrow = currow;
    int oldcol = curcol;
    int oldpos = screenposition;

    switch (e->key())
    {
        case Key_Up:
        {
            currow--;
            if (currow < 0)
            {
                if (screenposition > 0)
                    screenposition -= 3;
                currow = 0;
            }
            handled = true;
            break;
        }
        case Key_Left:
        {
            if (curcol == 0 && currow == 0)
            {
                handled = true;
                break;
            }

            curcol--;
            if (curcol < 0)
            {
                currow--;
                if (currow < 0)
                {
                    if (screenposition > 0)
                        screenposition -= 3;
                    currow = 0;
                }
                curcol = THUMBS_W - 1;
            }
            handled = true;
            break;
        }
        case Key_Down:
        {
            currow++;
            if (currow >= THUMBS_H)
            {
                if (screenposition < thumbs.size() - 1)
                    screenposition += 3;
                currow = THUMBS_H - 1;
            }

            if (screenposition + currow * THUMBS_H + curcol >= thumbs.size())
            {
                if (screenposition + currow * THUMBS_H < thumbs.size())
                {
                    curcol = 0;
                    handled = true;
                }
                else
                    handled = false;
            }
            else
                handled = true;
            break;
        }
        case Key_Right:
        {
            curcol++;
            if (curcol >= THUMBS_W)
            {
                currow++;
                if (currow >= THUMBS_H)
                {
                    if (screenposition < thumbs.size() - 1)
                        screenposition += 3;
                    currow = THUMBS_H - 1;
                }
                curcol = 0;
            }

            if (screenposition + currow * THUMBS_H + curcol >= thumbs.size())
                handled = false;
            else
                handled = true;
            break;
        }
        case Key_Space:
        case Key_Enter:
        case Key_Return:
        {
            int pos = screenposition + currow * THUMBS_W + curcol;

            if (thumbs[pos].isdir)
            {
                QFileInfo fi(thumbs[pos].filename);
	        m_context->SetSetting("Theme", fi.fileName());
	        m_context->LoadQtConfig();
                accept();
            }
            handled = true;
            break;
	
        }
        default: break;
    }

    if (handled)
    {
        update();
    }
    else
    {
        screenposition = oldpos;
        currow = oldrow;
        curcol = oldcol;
        QDialog::keyPressEvent(e);
    }
}

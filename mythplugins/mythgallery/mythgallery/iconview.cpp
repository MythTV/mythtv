#include <qapplication.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qcursor.h>
#include <qpainter.h>
#include <qdir.h>
#include <qfileinfo.h>

#include "iconview.h"
#include "singleview.h"

#include <mythtv/settings.h>

extern Settings *globalsettings;

IconView::IconView(const QString &startdir, QWidget *parent, const char *name)
        : QDialog(parent, name)
{
    screenheight = QApplication::desktop()->height();
    screenwidth = QApplication::desktop()->width();

    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");

    wmult = screenwidth / 800.0;
    hmult = screenheight / 600.0;

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setCursor(QCursor(Qt::BlankCursor));

    bool usetheme = globalsettings->GetNumSetting("ThemeQt");

    if (usetheme)
    {
        bgcolor = QColor(globalsettings->GetSetting("BackgroundColor"));
        fgcolor = QColor(globalsettings->GetSetting("ForegroundColor"));

        if (globalsettings->GetSetting("BackgroundPixmap") != "")
        {
            QString pmapname = globalsettings->GetSetting("ThemePathName") +
                               globalsettings->GetSetting("BackgroundPixmap");
            setPaletteBackgroundPixmap(QPixmap(pmapname));
        } 
        else if (globalsettings->GetSetting("TiledBackgroundPixmap") != "")
        {
            QString pmapname = globalsettings->GetSetting("ThemePathName") +
                          globalsettings->GetSetting("TiledBackgroundPixmap");

            QPixmap background(screenwidth, screenheight);
            QPainter tmp(&background);
    
            tmp.drawTiledPixmap(0, 0, screenwidth, screenheight,
                                QPixmap(pmapname));
            tmp.end();
            setPaletteBackgroundPixmap(background);
        }
        else
            setPalette(QPalette(bgcolor));
    }
    else
    {
        bgcolor = QColor("white");
        fgcolor = QColor("black");
        setPalette(QPalette(bgcolor));
    }

    highlightcolor = fgcolor;

    m_font = new QFont("Arial", (int)(13 * hmult), QFont::Bold);

    thumbw = screenwidth / (THUMBS_W + 1);
    thumbh = screenheight / (THUMBS_H + 1);

    spacew = thumbw / (THUMBS_W + 1);
    spaceh = (thumbh * 5 / 4) / (THUMBS_H + 1);

    thumbh = (screenheight - spaceh * (THUMBS_H + 1)) / THUMBS_H;

    fillList(startdir);
    screenposition = 0;
    currow = 0;
    curcol = 0;

    WFlags flags = getWFlags();
    setWFlags(flags | Qt::WRepaintNoErase);

    showFullScreen();
}

IconView::~IconView()
{
    while (thumbs.size() > 0)
    {
        if (thumbs.back().pixmap)
            delete thumbs.back().pixmap;
        thumbs.pop_back();
    }
}

void IconView::paintEvent(QPaintEvent *e)
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

             if (!thumb->pixmap)
                 loadThumbPixmap(thumb);

             int xpos = spacew * (x + 1) + thumbw * x;

             if (thumb->pixmap)
                 tmp.drawPixmap(xpos + (thumbw - thumb->pixmap->width()) / 2, 
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

void IconView::fillList(const QString &dir)
{
    QDir d(dir);

    if (!d.exists())
        return;

    d.setNameFilter("*.jpg *.JPG *.jpeg *.JPEG *.png *.PNG *.tiff *.TIFF "
                    "*.bmp *.BMP *.gif *.GIF");
    d.setSorting(QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);

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
        if (fi->isDir())
            thumb.isdir = true;

        thumb.name = fi->baseName();
        thumbs.push_back(thumb);
    }
}

void IconView::loadThumbPixmap(Thumbnail *thumb)
{
    QImage tmpimage(thumb->filename);

    if (tmpimage.width() == 0 || tmpimage.height() == 0)
        return;

    QImage tmp2 = tmpimage.smoothScale((int)(thumbw * wmult),
                                       (int)(thumbh * hmult), QImage::ScaleMin);

    thumb->pixmap = new QPixmap();
    thumb->pixmap->convertFromImage(tmp2);
}

void IconView::keyPressEvent(QKeyEvent *e)
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
            SingleView sv(thumbs[pos].filename);
            sv.exec();
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


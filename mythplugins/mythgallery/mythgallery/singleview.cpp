#include <qapplication.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qcursor.h>
#include <qpainter.h>

#include "singleview.h"

#include <mythtv/settings.h>

extern Settings *globalsettings;

SingleView::SingleView(const QString &filename, QWidget *parent, 
                       const char *name)
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

    m_font = new QFont("Arial", (int)(13 * hmult), QFont::Bold);

    QImage tmpimage(filename);
    QImage tmp2 = tmpimage.smoothScale((int)(screenwidth * wmult), 
                                       (int)(screenheight * hmult), 
                                       QImage::ScaleMin);
    image = new QImage(tmp2);

    WFlags flags = getWFlags();
    setWFlags(flags | Qt::WRepaintNoErase);

    showFullScreen();
}

SingleView::~SingleView()
{
     if (image)
         delete image;
}

void SingleView::paintEvent(QPaintEvent *e)
{
    e = e;
    QPainter p(this);

    p.drawImage((screenwidth - image->width()) / 2, 
                (screenheight - image->height()) / 2, *image);
}

void SingleView::keyPressEvent(QKeyEvent *e)
{
    QDialog::keyPressEvent(e);
}

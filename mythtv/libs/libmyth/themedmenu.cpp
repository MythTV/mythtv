#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qapplication.h>
#include <qpixmap.h>
#include <qbitmap.h>
#include <qpainter.h>
#include <qimage.h>
#include <qdir.h>
#include <math.h>

#include "themedmenu.h"
#include "settings.h"

extern Settings *globalsettings;

ThemedMenu::ThemedMenu(const char *cdir, const char *menufile, 
                       QWidget *parent, const char *name)
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
    setFixedWidth(screenwidth);
    setFixedHeight(screenheight);

    setPalette(QPalette(QColor(250, 250, 250)));

    QString dir = QString(cdir) + "/";

    QString filename = dir + menufile;

    foundtheme = true;
    QFile filetest(filename);
    if (!filetest.exists())
    {
        foundtheme = false;
        return;
    }

    parseSettings(dir, menufile);
}

ThemedMenu::~ThemedMenu(void)
{
    if (logo)
        delete logo;
    if (buttonnormal)
        delete buttonnormal;
    if (buttonactive)
        delete buttonactive;
    for (unsigned int i = 0; i < buttonList.size(); i++)
    {
        if (buttonList[i].icon)
            delete buttonList[i].icon;
    }
}

void ThemedMenu::parseSettings(QString dir, QString menuname)
{
    QString filename = dir + menuname;

    Settings *settings = new Settings(filename);

    QString setting = settings->GetSetting("Include");
    if (setting.length() > 1)
    {
        filename = QString(dir) + setting;
        settings->ReadSettings(filename);
    }

    logo = NULL;

    bool tiledbackground = false;
    setting = settings->GetSetting("TiledBackground");

    QPixmap *bground = NULL;
    if (setting.length() > 1)
    {
        tiledbackground = true;
        setting = dir + setting;
        bground = scalePixmap(setting);
    }
    else
    {
        setting = settings->GetSetting("Background");
        if (setting.length() > 1)
        {
            setting = dir + setting;
            bground = scalePixmap(setting);
        }
    }

    if (bground)
    {
        QPixmap background(screenwidth, screenheight);
        QPainter tmp(&background);

        if (tiledbackground)
            tmp.drawTiledPixmap(0, 0, screenwidth, screenheight,
                                *bground);
        else
            tmp.drawPixmap(menuRect(), *bground);

        tmp.end();

        setPaletteBackgroundPixmap(background);

        delete bground;
    }

    setting = settings->GetSetting("Logo");
    if (setting.length() > 1)
    {
        setting = dir + setting;
        logo = scalePixmap(setting);
    }

    if (logo)
    {
        logopos = parsePoint(settings->GetSetting("LogoPos"));
        logopos.setX(logopos.x() * wmult);
        logopos.setY(logopos.y() * hmult);
        logoRect = QRect(logopos.x(), logopos.y(), logo->width(), 
                         logo->height());
    }

    setting = dir + settings->GetSetting("ButtonNormal");
    buttonnormal = scalePixmap(setting);
    setting = dir + settings->GetSetting("ButtonActive");
    buttonactive = scalePixmap(setting);

    textRect = parseRect(settings->GetSetting("ButtonTextArea"));
    textRect.setX(textRect.x() * wmult);
    textRect.setY(textRect.y() * hmult);
    textRect.setWidth(textRect.width() * wmult);
    textRect.setHeight(textRect.height() * hmult);
    textRect = QRect(textRect.x(), textRect.y(), 
                     buttonnormal->width() - textRect.width() - textRect.x(), 
                     buttonnormal->height() - textRect.height() - textRect.y());

    int buttons = settings->GetNumSetting("NumButtons");

    for (int i = 0; i < buttons; i++)
    {
        ThemedButton newbutton;
 
        setting = QString("ButtonAction%1").arg(i+1);       
        newbutton.selectioninfo = settings->GetSetting(setting);

        if (newbutton.selectioninfo == "")
            cerr << "Button " << i + 1 << " doesn't have a ButtonAction\n"; 

        setting = QString("ButtonPos%1").arg(i+1);
        
        newbutton.pos = parsePoint(settings->GetSetting(setting));
        newbutton.pos.setX(newbutton.pos.x() * wmult);
        newbutton.pos.setY(newbutton.pos.y() * hmult);
        newbutton.posRect = QRect(newbutton.pos.x(), newbutton.pos.y(), 
                                  buttonnormal->width(), 
                                  buttonnormal->height());

        setting = QString("ButtonText%1").arg(i+1);
        newbutton.text = settings->GetSetting(setting);

        newbutton.icon = NULL;

        setting = QString("ButtonIcon%1").arg(i+1);
        setting = settings->GetSetting(setting);

        if (setting.length() > 1)
        {
            setting = dir + setting;
            newbutton.icon = scalePixmap(setting);
            setting = QString("ButtonIconOffset%1").arg(i+1);

            newbutton.iconPos = parsePoint(settings->GetSetting(setting));
            newbutton.iconPos.setX(newbutton.iconPos.x() * wmult);
            newbutton.iconPos.setY(newbutton.iconPos.y() * hmult);
            newbutton.iconPos += newbutton.pos;
            newbutton.iconRect = QRect(newbutton.iconPos.x(), 
                                       newbutton.iconPos.y(),
                                       newbutton.icon->width(),
                                       newbutton.icon->height());
        }
        buttonList.push_back(newbutton);
    }

    buttonUpDown = parseOrder(settings->GetSetting("ButtonOrderVertical"));
    buttonLeftRight = parseOrder(settings->GetSetting("ButtonOrderHorizontal"));

    if (buttonUpDown.size() != buttonList.size())
    {
        cerr << "Invalid ButtonOrderVertical\n";
        buttonUpDown.clear();
        for (unsigned int j = 1; j <= buttonList.size(); j++)
            buttonUpDown.push_back(j);
    }

    if (buttonLeftRight.size() != buttonList.size())
    {
        cerr << "Invalid ButtonOrderHorizontal\n";
        buttonLeftRight.clear();
        for (unsigned int j = 1; j <= buttonList.size(); j++)
            buttonLeftRight.push_back(j);
    }

    int fontsize = settings->GetNumSetting("ButtonTextSize");
    bool italic = (bool)settings->GetNumSetting("ButtonTextItalics");
    QString fontname = settings->GetSetting("ButtonTextName");
    bool bold = (bool)settings->GetNumSetting("ButtonTextBold");

    int weight = QFont::Normal;
    if (bold)
        weight = QFont::Bold;

    font = QFont(fontname, fontsize * hmult, weight, italic);
    setFont(font);

    textColor = QColor(settings->GetSetting("ButtonTextColor"));

    setCursor(QCursor(Qt::BlankCursor));

    hasshadow = false;
    QString shadowcolor = settings->GetSetting("ButtonShadowColor");
    if (shadowcolor.length() > 1)
    {
        hasshadow = true;
        shadowColor = QColor(shadowcolor);
        shadowOffset = parsePoint(settings->GetSetting("ButtonShadowOffset"));
        shadowalpha = settings->GetNumSetting("ButtonShadowAlpha");
    }

    hasoutline = false;
    int dooutline = settings->GetNumSetting("ButtonTextOutline");
    if (dooutline)
    {
        hasoutline = true;
        QString outlinecolor = settings->GetSetting("ButtonTextOutlineColor");
        outlineColor = QColor(outlinecolor);
        outlinesize = settings->GetNumSetting("ButtonTextOutlineSize");
        outlinesize = (int)(outlinesize * hmult);
    }

    int centered = settings->GetNumSetting("ButtonTextCentered");
    
    if (centered)
        textflags = Qt::AlignTop | Qt::AlignHCenter | WordBreak;
    else
        textflags = Qt::AlignTop | Qt::AlignLeft | WordBreak;

    int startbutton = settings->GetNumSetting("StartButton") - 1;
    if (startbutton < 0)
        startbutton = 0;
    activebutton = startbutton;

    WFlags flags = getWFlags();
    setWFlags(flags | Qt::WRepaintNoErase);

    selection = "";

    update(menuRect());

    delete settings;
}

QPixmap *ThemedMenu::scalePixmap(QString filename)
{
    QPixmap *ret = new QPixmap();

    if (screenwidth != 800 || screenheight != 600)
    {
        QImage tmpimage(filename);
        QImage tmp2 = tmpimage.smoothScale(tmpimage.width() * wmult, 
                                           tmpimage.height() * hmult);
        ret->convertFromImage(tmp2);
    }
    else
        ret->load(filename);

    return ret;
}

void ThemedMenu::Show()
{
    showFullScreen();
    setActiveWindow();
}

QPoint ThemedMenu::parsePoint(QString text)
{
    int x, y;
    QPoint retval;
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);
    return retval;
}

QRect ThemedMenu::parseRect(QString text)
{
    int x, y, w, h;
    QRect retval;
    if (sscanf(text.data(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
        retval = QRect(x, y, w, h);

    return retval;
}

QValueList<int> ThemedMenu::parseOrder(QString text)
{
    QValueList<int> retval;
    
    for (unsigned int i = 0; i < buttonList.size(); i++)
    {
        int value = text.section(",", i, i).toInt();
        if (value != 0)
            retval.push_back(value);     
    }

    return retval;
}

QRect ThemedMenu::menuRect() const
{
    QRect r(0, 0, screenwidth, screenheight);
    return r;
}

void ThemedMenu::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(logoRect))
    {
        paintLogo(&p);
    }

    for (unsigned int i = 0; i < buttonList.size(); i++)
    {
        if (r.intersects(buttonList[i].posRect))
            paintButton(i, &p);
    }
}

void ThemedMenu::paintLogo(QPainter *p)
{
    if (logo)
        p->drawPixmap(logoRect, *logo);
}

void ThemedMenu::paintButton(unsigned int button, QPainter *p)
{
    QRect cr;
    if (buttonList[button].icon)
        cr = buttonList[button].posRect.unite(buttonList[button].iconRect);
    else
        cr = buttonList[button].posRect;

    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());

    QPainter tmp;
    tmp.begin(&pix, this);

    QRect newRect(0, 0, buttonList[button].posRect.width(),
                  buttonList[button].posRect.height());
    newRect.moveBy(buttonList[button].posRect.x() - cr.x(), 
                   buttonList[button].posRect.y() - cr.y());

    if (button == activebutton)
    {
        tmp.drawPixmap(newRect.topLeft(), *buttonactive);
    }
    else
    {
        tmp.drawPixmap(newRect.topLeft(), *buttonnormal);
    }

    QRect buttonTextRect = textRect;
    buttonTextRect.moveBy(newRect.x(), newRect.y());

    if (hasshadow && shadowalpha > 0)
    {
        QPixmap textpix(buttonTextRect.size());
        textpix.fill(QColor(color0));
        textpix.setMask(textpix.createHeuristicMask());

        QRect myrect = buttonTextRect;
        myrect.moveTopLeft(shadowOffset);

        QPainter texttmp;
        texttmp.begin(&textpix, this);
        texttmp.setPen(QPen(shadowColor, 1));
        texttmp.setFont(font);
        texttmp.drawText(myrect, textflags, buttonList[button].text);
        texttmp.end();

        texttmp.begin(textpix.mask());
        texttmp.setPen(QPen(Qt::color1, 1));
        texttmp.setFont(font);
        texttmp.drawText(myrect, textflags, buttonList[button].text);
        texttmp.end();

        QImage im = textpix.convertToImage();

        for (int x = 0; x < im.width(); x++)
        {
            for (int y = 0; y < im.height(); y++)
            {
               uint *p = (uint *)im.scanLine(y) + x;
               QRgb col = *p;
               if (qAlpha(col) > 128)
                   *p = qRgba(qRed(col), qGreen(col), qBlue(col), shadowalpha);
            }
        } 

        textpix.convertFromImage(im);

        myrect.moveTopLeft(QPoint(0, 0));
        bitBlt(&pix, buttonTextRect.topLeft(), &textpix, myrect, Qt::CopyROP);
    }

    tmp.setFont(font);
    drawText(&tmp, buttonTextRect, textflags, buttonList[button].text);

    if (buttonList[button].icon)
    {
        newRect = QRect(0, 0, buttonList[button].iconRect.width(),
                        buttonList[button].iconRect.height());
        newRect.moveBy(buttonList[button].iconRect.x() - cr.x(),     
                       buttonList[button].iconRect.y() - cr.y());

        tmp.drawPixmap(newRect.topLeft(), *(buttonList[button].icon));
    }


    tmp.flush();
    tmp.end();

    p->drawPixmap(cr.topLeft(), pix);
}

void ThemedMenu::drawText(QPainter *p, QRect &rect, int textflags, QString text)
{
    if (hasoutline)
    {
        QRect outlinerect = rect;

        p->setPen(QPen(outlineColor, 1));
        outlinerect.moveBy(0 - outlinesize, 0 - outlinesize);
        p->drawText(outlinerect, textflags, text);

        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            outlinerect.moveBy(1, 0);
            p->drawText(outlinerect, textflags, text);
        }

        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            outlinerect.moveBy(0, 1);
            p->drawText(outlinerect, textflags, text);
        }
  
        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            outlinerect.moveBy(-1, 0);
            p->drawText(outlinerect, textflags, text);
        }

        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            outlinerect.moveBy(0, -1);
            p->drawText(outlinerect, textflags, text);
        }

        p->setPen(QPen(textColor, 1));
        p->drawText(rect, textflags, text);
    }
    else
    {
        p->setPen(QPen(textColor, 1));
        p->drawText(rect, textflags, text);
    }
}

void ThemedMenu::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    unsigned int lastbutton = activebutton;
    unsigned int lrpos = buttonLeftRight.findIndex(lastbutton + 1);
    unsigned int udpos = buttonUpDown.findIndex(lastbutton + 1);

    switch (e->key())
    {
        case Key_Up:
        {
            udpos--;
            if (udpos > buttonList.size())
                udpos = buttonList.size() - 1;
            activebutton = buttonUpDown[udpos] - 1;
            handled = true;
            break;
        }
        case Key_Left:
        {
            lrpos--;
            if (lrpos > buttonList.size())
                lrpos = buttonList.size() - 1;
            activebutton = buttonLeftRight[lrpos] - 1;
            handled = true;
            break;
        }
        case Key_Down:
        {
            udpos++;
            if (udpos == buttonList.size())
                udpos = 0;
            activebutton = buttonUpDown[udpos] - 1;
            handled = true;
            break;
        }
        case Key_Right:
        {
            lrpos++;
            if (lrpos == buttonList.size())
                lrpos = 0;
            activebutton = buttonLeftRight[lrpos] - 1;
            handled = true;
            break;
        }
        case Key_Enter:
        case Key_Return:
        case Key_Space:
        {
            selection = buttonList[activebutton].selectioninfo;
            done(1);
            break;
        }
        default: break;
    }

    if (handled)
    {
        update(buttonList[lastbutton].posRect);
        update(buttonList[activebutton].posRect);
    }
    else
        QDialog::keyPressEvent(e);
} 

QString findThemeDir(QString themename, QString prefix)
{
    char *home = getenv("HOME");
    QString testdir = QString(home) + "/.mythtv/themes/" + themename;

    QDir dir(testdir);
    if (dir.exists())
        return testdir;

    testdir = prefix + "/share/mythtv/themes/" + themename;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;

    testdir = "../menutest/" + themename;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;

    cerr << "Could not find theme: " << themename << endl;
    return "";
}


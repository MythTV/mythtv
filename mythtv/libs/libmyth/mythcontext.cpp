#include <qapplication.h>
#include <qsqldatabase.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qdir.h>
#include <qpainter.h>

#include "mythcontext.h"
#include "settings.h"
#include "themedmenu.h"
#include "guidegrid.h"

using namespace libmyth;

MythContext::MythContext(void)
{
    m_installprefix = PREFIX;
    m_settings = new Settings;

    LoadSettingsFiles("settings.txt");
    LoadSettingsFiles("mysql.txt");
    LoadSettingsFiles("theme.txt");

    qtfontbig = m_settings->GetNumSetting("QtFontBig");
    if (qtfontbig <= 0)
        qtfontbig = 25;

    qtfontmed = m_settings->GetNumSetting("QtFontMedium");
    if (qtfontmed <= 0)
        qtfontmed = 25;

    qtfontsmall = m_settings->GetNumSetting("QtFontSmall");
    if (qtfontsmall <= 0)
        qtfontsmall = 25;
}

MythContext::~MythContext()
{
    if (m_settings)
        delete m_settings;
}

QString MythContext::GetFilePrefix(void)
{
    return m_settings->GetSetting("RecordFilePrefix");
}

void MythContext::LoadSettingsFiles(const QString &filename)
{
    m_settings->LoadSettingsFiles(filename, m_installprefix);
}

void MythContext::LoadQtConfig(void)
{
    QString themename = m_settings->GetSetting("Theme");
    QString themedir = FindThemeDir(themename);
    
    m_settings->SetSetting("ThemePathName", themedir + "/");
    
    themedir += "/qtlook.txt";
    m_settings->ReadSettings(themedir);
}

void MythContext::GetScreenSettings(int &width, float &wmult, 
                                    int &height, float &hmult)
{
    height = QApplication::desktop()->height();
    width = QApplication::desktop()->width();

    if (m_settings->GetNumSetting("GuiWidth") > 0)
        width = m_settings->GetNumSetting("GuiWidth");
    if (m_settings->GetNumSetting("GuiHeight") > 0)
        height = m_settings->GetNumSetting("GuiHeight");

    wmult = width / 800.0;
    hmult = height / 600.0;
}

QString MythContext::FindThemeDir(QString themename)
{
    char *home = getenv("HOME");
    QString testdir = QString(home) + "/.mythtv/themes/" + themename;

    QDir dir(testdir);
    if (dir.exists())
        return testdir;

    testdir = m_installprefix + "/share/mythtv/themes/" + themename;
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

QString MythContext::RunProgramGuide(QString startchannel)
{
    GuideGrid gg(this, startchannel);
    gg.exec();

    return gg.getLastChannel();
}

int MythContext::OpenDatabase(QSqlDatabase *db)
{
    db->setDatabaseName(m_settings->GetSetting("DBName"));
    db->setUserName(m_settings->GetSetting("DBUserName"));
    db->setPassword(m_settings->GetSetting("DBPassword"));
    db->setHostName(m_settings->GetSetting("DBHostName"));
 
    return db->open();
}

QString MythContext::GetSetting(const QString &key) 
{
    return m_settings->GetSetting(key); 
}

int MythContext::GetNumSetting(const QString &key)
{ 
    return m_settings->GetNumSetting(key); 
}

void MythContext::ThemeWidget(QWidget *widget, int screenwidth, 
                              int screenheight, float wmult, float hmult)
{
    bool usetheme = m_settings->GetNumSetting("ThemeQt");
    QColor bgcolor, fgcolor;

    if (usetheme)
    {
        bgcolor = QColor(m_settings->GetSetting("BackgroundColor"));
        fgcolor = QColor(m_settings->GetSetting("ForegroundColor"));

        widget->setPaletteBackgroundColor(bgcolor);
        widget->setPaletteForegroundColor(fgcolor);

        QPixmap *bgpixmap = NULL;

        if (m_settings->GetSetting("BackgroundPixmap") != "")
        {
            QString pmapname = m_settings->GetSetting("ThemePathName") +
                               m_settings->GetSetting("BackgroundPixmap");

            bgpixmap = LoadScalePixmap(pmapname, screenwidth, screenheight,
                                       wmult, hmult);

            if (bgpixmap)
                widget->setPaletteBackgroundPixmap(*bgpixmap);
        }
        else if (m_settings->GetSetting("TiledBackgroundPixmap") != "")
        {
            QString pmapname = m_settings->GetSetting("ThemePathName") +
                               m_settings->GetSetting("TiledBackgroundPixmap");

            bgpixmap = LoadScalePixmap(pmapname, screenwidth, screenheight,
                                       wmult, hmult);

            if (bgpixmap)
            {
                QPixmap background(screenwidth, screenheight);
                QPainter tmp(&background);

                tmp.drawTiledPixmap(0, 0, screenwidth, screenheight, *bgpixmap);
                tmp.end();
                widget->setPaletteBackgroundPixmap(background);
            }
        }

        if (bgpixmap)
            delete bgpixmap;
    }
    else
    {
        bgcolor = QColor("white");
        fgcolor = QColor("black");
        widget->setPalette(QPalette(bgcolor));
    }
}

QPixmap *MythContext::LoadScalePixmap(QString filename, int screenwidth, 
                                      int screenheight, float wmult, 
                                      float hmult)
{               
    QPixmap *ret = new QPixmap();

    if (screenwidth != 800 || screenheight != 600)
    {           
        QImage tmpimage;

        if (!tmpimage.load(filename))
        {
            cerr << "Error loading image file: " << filename << endl;
            delete ret;
            return NULL;
        }
        QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult),
                                           (int)(tmpimage.height() * hmult));
        ret->convertFromImage(tmp2);
    }       
    else
    {
        if (!ret->load(filename))
        {
            cerr << "Error loading image file: " << filename << endl;
            delete ret;
            return NULL;
        }
    }

    return ret;
}


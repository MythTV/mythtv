#include <qapplication.h>
#include <qsqldatabase.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qdir.h>
#include <qpainter.h>
#include <unistd.h>
#include <qsqldatabase.h>

#include "mythcontext.h"
#include "settings.h"
#include "themedmenu.h"
#include "guidegrid.h"

using namespace libmyth;

MythContext::MythContext(bool gui)
{
    m_installprefix = PREFIX;
    m_settings = new Settings;
    m_qtThemeSettings = new Settings;

    m_themeloaded = false;

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

    if (gui)
    {
        m_height = QApplication::desktop()->height();
        m_width = QApplication::desktop()->width();
    }
    else 
    {
        m_height = m_width = 0;
    }

    if (m_settings->GetNumSetting("GuiWidth") > 0)
        m_width = m_settings->GetNumSetting("GuiWidth");
    if (m_settings->GetNumSetting("GuiHeight") > 0)
        m_height = m_settings->GetNumSetting("GuiHeight");

    m_wmult = m_width / 800.0;
    m_hmult = m_height / 600.0;
}

MythContext::~MythContext()
{
    if (m_settings)
        delete m_settings;
    if (m_qtThemeSettings)
        delete m_qtThemeSettings;
}

QString MythContext::GetFilePrefix(void)
{
    return m_settings->GetSetting("RecordFilePrefix");
}

void MythContext::LoadSettingsFiles(const QString &filename)
{
    m_settings->LoadSettingsFiles(filename, m_installprefix);
}

void MythContext::LoadSettingsDatabase(QSqlDatabase *db)
{
    QString thequery = "SELECT * FROM mythsettings";
    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString key = query.value(0).toString();
            QString value = query.value(1).toString();

            if (value == QString::null)
                value = "";

            if (key != QString::null && key != "")
                SetSetting(key, value);
        }
    }
}

void MythContext::LoadQtConfig(void)
{
    if (m_qtThemeSettings)
        delete m_qtThemeSettings;

    m_qtThemeSettings = new Settings;

    QString themename = m_settings->GetSetting("Theme");
    QString themedir = FindThemeDir(themename);
    
    m_settings->SetSetting("ThemePathName", themedir + "/");
    
    themedir += "/qtlook.txt";
    m_qtThemeSettings->ReadSettings(themedir);
    m_themeloaded = false;
}

void MythContext::GetScreenSettings(int &width, float &wmult, 
                                    int &height, float &hmult)
{
    height = m_height;
    width = m_width;

    wmult = m_wmult;
    hmult = m_hmult;
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

QString MythContext::RunProgramGuide(QString startchannel, bool thread)
{
    if (thread)
        qApp->lock();
 
    GuideGrid gg(this, startchannel);

    if (thread)
    {
        gg.show();
        qApp->unlock();

        while (gg.isVisible())
            usleep(50);
    }
    else 
        gg.exec();

    QString chanstr = gg.getLastChannel();
    if (chanstr == QString::null)
        chanstr = "";

    return chanstr;
}

int MythContext::OpenDatabase(QSqlDatabase *db)
{
    db->setDatabaseName(m_settings->GetSetting("DBName"));
    db->setUserName(m_settings->GetSetting("DBUserName"));
    db->setPassword(m_settings->GetSetting("DBPassword"));
    db->setHostName(m_settings->GetSetting("DBHostName"));
 
    return db->open();
}

QString MythContext::GetSetting(const QString &key, const QString &defaultval) 
{
    return m_settings->GetSetting(key, defaultval); 
}

int MythContext::GetNumSetting(const QString &key, int defaultval)
{ 
    return m_settings->GetNumSetting(key, defaultval); 
}

void MythContext::SetPalette(QWidget *widget)
{
    QPalette pal = widget->palette();

    QColorGroup active = pal.active();
    QColorGroup disabled = pal.disabled();
    QColorGroup inactive = pal.inactive();

    const QString names[] = { "Foreground", "Button", "Light", "Midlight",  
                              "Dark", "Mid", "Text", "BrightText", "ButtonText",
                              "Base", "Background", "Shadow", "Highlight",
                              "HighlightedText" };

    QString type = "Active";
    for (int i = 0; i < 13; i++)
    {
        QString color = m_qtThemeSettings->GetSetting(type + names[i]);
        if (color != "")
            pal.setColor(QPalette::Active, (QColorGroup::ColorRole) i,
                         QColor(color));
    }

    type = "Disabled";
    for (int i = 0; i < 13; i++)
    {
        QString color = m_qtThemeSettings->GetSetting(type + names[i]);
        if (color != "")
            pal.setColor(QPalette::Disabled, (QColorGroup::ColorRole) i,
                         QColor(color));
    }

    type = "Inactive";
    for (int i = 0; i < 13; i++)
    {
        QString color = m_qtThemeSettings->GetSetting(type + names[i]);
        if (color != "")
            pal.setColor(QPalette::Inactive, (QColorGroup::ColorRole) i,
                         QColor(color));
    }

    widget->setPalette(pal);
}

void MythContext::ThemeWidget(QWidget *widget)
{
    bool usetheme = m_settings->GetNumSetting("ThemeQt");
    QColor bgcolor, fgcolor;

    if (usetheme)
    {
        if (m_themeloaded)
        {
            widget->setPalette(m_palette);
            if (m_backgroundimage.width() > 0)
                widget->setPaletteBackgroundPixmap(m_backgroundimage);
            return;
        }

        SetPalette(widget);

        m_palette = widget->palette();

        QPixmap *bgpixmap = NULL;

        if (m_qtThemeSettings->GetSetting("BackgroundPixmap") != "")
        {
            QString pmapname = m_settings->GetSetting("ThemePathName") +
                              m_qtThemeSettings->GetSetting("BackgroundPixmap");

            bgpixmap = LoadScalePixmap(pmapname);
            if (bgpixmap)
            {
                widget->setPaletteBackgroundPixmap(*bgpixmap);
                m_backgroundimage = *bgpixmap;
            }
        }
        else if (m_qtThemeSettings->GetSetting("TiledBackgroundPixmap") != "")
        {
            QString pmapname = m_settings->GetSetting("ThemePathName") +
                         m_qtThemeSettings->GetSetting("TiledBackgroundPixmap");

            bgpixmap = LoadScalePixmap(pmapname);
            if (bgpixmap)
            {
                QPixmap background(m_width, m_height);
                QPainter tmp(&background);

                tmp.drawTiledPixmap(0, 0, m_width, m_height, *bgpixmap);
                tmp.end();

                m_backgroundimage = background;
                widget->setPaletteBackgroundPixmap(background);
            }
        }

        m_themeloaded = true;

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

QPixmap *MythContext::LoadScalePixmap(QString filename) 
{               
    QPixmap *ret = new QPixmap();

    if (m_width != 800 || m_height != 600)
    {           
        QImage tmpimage;

        if (!tmpimage.load(filename))
        {
            cerr << "Error loading image file: " << filename << endl;
            delete ret;
            return NULL;
        }
        QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * m_wmult),
                                           (int)(tmpimage.height() * m_hmult));
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

void MythContext::SetSetting(const QString &key, const QString &newValue)
{
    m_settings->SetSetting(key, newValue);
}


#include <qapplication.h>
#include <qsqldatabase.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qdir.h>
#include <qpainter.h>
#include <unistd.h>
#include <qsqldatabase.h>
#include <qurl.h>

#include "mythcontext.h"
//#include "settings.h"
#include "oldsettings.h"
#include "themedmenu.h"
#include "util.h"
#include "remotefile.h"
#include "lcddevice.h"

MythContext::MythContext(const QString &binversion, bool gui)
           : QObject()
{
    if (binversion != MYTH_BINARY_VERSION)
    {
        cerr << "This app was compiled against libmyth version: " << binversion
             << "\nbut the library is version: " << MYTH_BINARY_VERSION << endl;
        cerr << "You probably want to recompile everything, and do a\n"
             << "'make distclean' first.\n";
        cerr << "exiting\n";
        exit(0);
    }

    m_installprefix = PREFIX;
    m_settings = new Settings;
    m_qtThemeSettings = new Settings;

    m_themeloaded = false;

    pthread_mutex_init(&dbLock, NULL);
    m_db = QSqlDatabase::addDatabase("QMYSQL3", "MythContext");

    char localhostname[1024];
    if (gethostname(localhostname, 1024))
    {
        cerr << "Error getting local hostname\n";
        exit(0);
    }
    m_localhostname = localhostname;

    LoadSettingsFiles("mysql.txt");

    if (gui)
    {
        m_height = QApplication::desktop()->height();
        m_width = QApplication::desktop()->width();
    }
    else 
    {
        m_height = m_width = 0;
    }

    if (GetNumSetting("GuiWidth") > 0)
        m_width = GetNumSetting("GuiWidth");
    if (GetNumSetting("GuiHeight") > 0)
        m_height = GetNumSetting("GuiHeight");

    serverSock = NULL;
    expectingReply = false;
	
    lcd_device = new LCD();
}

MythContext::~MythContext()
{
    imageCache.clear();

    if (m_settings)
        delete m_settings;
    if (m_qtThemeSettings)
        delete m_qtThemeSettings;
    if (serverSock)
    {
        serverSock->close();
        delete serverSock;
    }
}

bool MythContext::ConnectServer(const QString &hostname, int port)
{
    pthread_mutex_init(&serverSockLock, NULL);
    serverSock = new QSocket();

    cout << "connecting to backend server: " << hostname << ":" << port << endl;
    serverSock->connectToHost(hostname, port);

    int num = 0;
    while (serverSock->state() == QSocket::HostLookup ||
           serverSock->state() == QSocket::Connecting)
    {
        qApp->processEvents();
        usleep(50);
        num++;
        if (num > 100)
        {
            cerr << "Connection timed out.\n";
            cerr << "You probably should modify the Master Server settings\n";
            cerr << "in the setup program and set the proper IP address.\n";
            exit(0);
        }
    }

    if (serverSock->state() != QSocket::Connected)
    {
        cout << "Could not connect to backend server\n";
        exit(0);
    }

    QString str = QString("ANN Playback %1 %2").arg(m_localhostname).arg(true);
    QStringList strlist = str;
    WriteStringList(serverSock, strlist);
    ReadStringList(serverSock, strlist);

    connect(serverSock, SIGNAL(readyRead()), this, SLOT(readSocket()));

    return true;
}

QString MythContext::GetMasterHostPrefix(void)
{
    if (!serverSock)
        return "";

    QString ret = QString("myth://%1:%2/").arg(serverSock->peerName())
                                          .arg(serverSock->peerPort());
    return ret;
}

QString MythContext::GetFilePrefix(void)
{
    return GetSetting("RecordFilePrefix");
}

void MythContext::LoadSettingsFiles(const QString &filename)
{
    m_settings->LoadSettingsFiles(filename, m_installprefix);
}

void MythContext::LoadQtConfig(void)
{
    m_height = QApplication::desktop()->height();
    m_width = QApplication::desktop()->width();

    if (GetNumSetting("GuiWidth") > 0)
        m_width = GetNumSetting("GuiWidth");
    if (GetNumSetting("GuiHeight") > 0)
        m_height = GetNumSetting("GuiHeight");

    if (m_qtThemeSettings)
        delete m_qtThemeSettings;

    m_qtThemeSettings = new Settings;

    QString themename = GetSetting("Theme");
    QString themedir = FindThemeDir(themename);
    
    m_themepathname = themedir + "/";
    
    themedir += "/qtlook.txt";
    m_qtThemeSettings->ReadSettings(themedir);
    m_themeloaded = false;
}

void MythContext::GetScreenSettings(int &width, float &wmult, 
                                    int &height, float &hmult)
{
    height = GetNumSetting("GuiHeight", m_height);
    width = GetNumSetting("GuiWidth", m_width);

    if (height == 0)
        height = m_height;
    if (width == 0)
        width = m_width;

    if (height < 160 || width < 160)
    {
        cerr << "Somehow, your screen size settings are bad.\n";
        cerr << "GuiHeight: " << GetNumSetting("GuiHeight") << endl;
        cerr << "GuiWidth: " << GetNumSetting("GuiWidth") << endl;
        cerr << "m_height: " << m_height << endl;
        cerr << "m_width: " << m_width << endl;
        cerr << "Falling back to 640x480\n";

        width = 640;
        height = 480;
    }

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

int MythContext::OpenDatabase(QSqlDatabase *db)
{
    pthread_mutex_lock(&dbLock);
    if (!m_db->isOpen()) {
        m_db->setDatabaseName(m_settings->GetSetting("DBName"));
        m_db->setUserName(m_settings->GetSetting("DBUserName"));
        m_db->setPassword(m_settings->GetSetting("DBPassword"));
        m_db->setHostName(m_settings->GetSetting("DBHostName"));
        m_db->open();
    }
    pthread_mutex_unlock(&dbLock);
        
    db->setDatabaseName(m_settings->GetSetting("DBName"));
    db->setUserName(m_settings->GetSetting("DBUserName"));
    db->setPassword(m_settings->GetSetting("DBPassword"));
    db->setHostName(m_settings->GetSetting("DBHostName"));
 
    return db->open();
}

void MythContext::KickDatabase(QSqlDatabase *db)
{
    QString query("SELECT NULL;");
    for(unsigned int i = 0 ; i < 2 ; ++i, usleep(50000)) {
        QSqlQuery result = db->exec(query);
        if (result.isActive())
            break;
        else
            DBError("KickDatabase", result);
    }
}

void MythContext::DBError(QString where, const QSqlQuery& query) 
{
    if(query.lastError().type())
    {
        cerr << "DB Error (" << where << "):" << endl;
    }
    else
    {
        cerr << "MythContext::DBError() saw non-error in "
             << where << ":" << endl;
    }

    cerr << "Query was:" << endl
         << query.lastQuery() << endl
         << "Driver error was ["
             << query.lastError().type()
             << "/"
             << query.lastError().number()
         << "]:" << endl
         << query.lastError().driverText() << endl
         << "Database error was:" << endl
         << query.lastError().databaseText() << endl;
}

void MythContext::SaveSetting(QString key, int newValue)
{
    QString strValue = QString("%1").arg(newValue);

    SaveSetting(key, strValue);
}

void MythContext::SaveSetting(QString key, QString newValue)
{
    pthread_mutex_lock(&dbLock);

    if (m_db->isOpen()) 
    {
        KickDatabase(m_db);

        QString querystr = QString("DELETE FROM settings WHERE value = '%1' "
                                "AND hostname = '%2';")
                               .arg(key).arg(m_localhostname);

        QSqlQuery result = m_db->exec(querystr);
        if (!result.isActive())
            MythContext::DBError("Clear setting", querystr);

        querystr = QString("INSERT settings ( value, data, hostname ) "
                           "VALUES ( '%1', '%2', '%3' );")
                           .arg(key).arg(newValue).arg(m_localhostname);

        result = m_db->exec(querystr);
        if (!result.isActive())
            MythContext::DBError("Save new setting", querystr);
    }

    pthread_mutex_unlock(&dbLock);
}

QString MythContext::GetSetting(const QString &key, const QString &defaultval) 
{
    bool found = false;
    QString value;

    pthread_mutex_lock(&dbLock);

    if (m_db->isOpen()) 
    {
        KickDatabase(m_db);

        QString query = QString("SELECT data FROM settings WHERE value = '%1' "
                                "AND hostname = '%2';")
                               .arg(key).arg(m_localhostname);

        QSqlQuery result = m_db->exec(query);

        if (result.isActive() && result.numRowsAffected() > 0) 
        {
            result.next();
            value = result.value(0).toString();
            found = true;
        }
        else
        {
            query = QString("SELECT data FROM settings WHERE value = '%1' AND "
                            "hostname IS NULL;").arg(key);

            result = m_db->exec(query);

            if (result.isActive() && result.numRowsAffected() > 0) 
            {
                result.next();
                value = result.value(0).toString();
                found = true;
            }
        }
    }
    pthread_mutex_unlock(&dbLock);

    if (found)
        return value;
    return m_settings->GetSetting(key, defaultval); 
}

int MythContext::GetNumSetting(const QString &key, int defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSetting(key, val);

    return retval.toInt();
}

QString MythContext::GetSettingOnHost(const QString &key, const QString &host,
                                      const QString &defaultval)
{
    bool found = false;
    QString value = defaultval;

    pthread_mutex_lock(&dbLock);

    if (m_db->isOpen())
    {
        KickDatabase(m_db);

        QString query = QString("SELECT data FROM settings WHERE value = '%1' "
                                "AND hostname = '%2';").arg(key).arg(host);

        QSqlQuery result = m_db->exec(query);

        if (result.isActive() && result.numRowsAffected() > 0)
        {
            result.next();
            value = result.value(0).toString();
            found = true;
        }
    }

    pthread_mutex_unlock(&dbLock);

    return value;
}

int MythContext::GetNumSettingOnHost(const QString &key, const QString &host,
                                     int defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSettingOnHost(key, host, val);

    return retval.toInt();
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
    bool usetheme = GetNumSetting("ThemeQt", 1);
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
            QString pmapname = m_themepathname +
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
            QString pmapname = m_themepathname +
                         m_qtThemeSettings->GetSetting("TiledBackgroundPixmap");

            int width, height;
            float wmult, hmult;

            GetScreenSettings(width, wmult, height, hmult);


            bgpixmap = LoadScalePixmap(pmapname);
            if (bgpixmap)
            {
                QPixmap background(width, height);
                QPainter tmp(&background);

                tmp.drawTiledPixmap(0, 0, width, height, *bgpixmap);
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

QImage *MythContext::LoadScaleImage(QString filename)
{
    if (filename.left(5) == "myth:")
        return NULL;

    QImage *ret = NULL;

    int width, height;
    float wmult, hmult;

    GetScreenSettings(width, wmult, height, hmult);

    if (width != 800 || height != 600)
    {
        QImage tmpimage;

        if (!tmpimage.load(filename))
        {
            cerr << "Error loading image file: " << filename << endl;
            return NULL;
        }
        QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult),
                                           (int)(tmpimage.height() * hmult));
        ret = new QImage(tmp2);
    }
    else
    {
        ret = new QImage(filename);
        if (ret->width() == 0)
        {
            cerr << "Error loading image file: " << filename << endl;
            delete ret;
            return NULL;
        }
    }

    return ret;
}

QPixmap *MythContext::LoadScalePixmap(QString filename) 
{ 
    if (filename.left(5) == "myth:")
        return NULL;
              
    QPixmap *ret = new QPixmap();

    int width, height;
    float wmult, hmult;

    GetScreenSettings(width, wmult, height, hmult);

    if (width != 800 || height != 600)
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

QImage *MythContext::CacheRemotePixmap(const QString &url, bool needevents)
{
    QUrl qurl = url;
    if (qurl.host() == "")
        return NULL;

    if (imageCache.contains(url))
    {
        return &(imageCache[url]);
    }

    RemoteFile *rf = new RemoteFile(url, true);

    QByteArray data;
    bool ret = rf->SaveAs(data, needevents);

    delete rf;

    if (ret)
    {
        QImage image(data);
        if (image.width() > 0)
        {
            imageCache[url] = image;
            return &(imageCache[url]);
        }
    }
    
    return NULL;
}

void MythContext::SetSetting(const QString &key, const QString &newValue)
{
    m_settings->SetSetting(key, newValue);
}

void MythContext::SendReceiveStringList(QStringList &strlist)
{
    pthread_mutex_lock(&serverSockLock);
    expectingReply = true;

    WriteStringList(serverSock, strlist);
    ReadStringList(serverSock, strlist);

    while (strlist[0] == "BACKEND_MESSAGE")
    {
        // oops, not for us
        QString message = strlist[1];
        QString extra = strlist[2];

        MythEvent me(message, extra);
        dispatch(me);

        ReadStringList(serverSock, strlist);
    }

    expectingReply = false;
    pthread_mutex_unlock(&serverSockLock);
}

void MythContext::readSocket(void)
{
    if (expectingReply)
        return;

    pthread_mutex_lock(&serverSockLock);

    while (serverSock->bytesAvailable() > 0)
    {
        QStringList strlist;
        ReadStringList(serverSock, strlist);

        QString prefix = strlist[0];
        QString message = strlist[1];
        QString extra = strlist[2];

        if (prefix != "BACKEND_MESSAGE")
        {
            cerr << "Received a: " << prefix << " message from the backend\n";
            cerr << "But I don't know what to do with it.\n";
        }
        else
        {
            MythEvent me(message, extra);
            dispatch(me);
        }
    }

    pthread_mutex_unlock(&serverSockLock);
}

void MythContext::addListener(QObject *obj)
{
    if (listeners.find(obj) == -1)
        listeners.append(obj);
}

void MythContext::removeListener(QObject *obj)
{
    listeners.remove(obj);
}

void MythContext::dispatch(MythEvent &e)
{
    QObject *obj = listeners.first();
    while (obj)
    {
        QApplication::postEvent(obj, new MythEvent(e));
        obj = listeners.next();
    }
}

void MythContext::LCDconnectToHost(QString hostname, unsigned int port)
{
    lcd_device->connectToHost(hostname, port);
}

void MythContext::LCDswitchToTime()
{
    lcd_device->switchToTime();
}

void MythContext::LCDswitchToMusic(QString artist, QString track)
{
    lcd_device->switchToMusic(artist, track);
}

void MythContext::LCDsetLevels(int numb_levels, float *levels)
{
    lcd_device->setLevels(numb_levels, levels);
}

void MythContext::LCDswitchToChannel(QString channum, 
                                     QString title, 
                                     QString subtitle)
{
    lcd_device->switchToChannel(channum, title, subtitle);
}

void MythContext::LCDsetChannelProgress(float percentViewed)
{
    lcd_device->setChannelProgress(percentViewed);
}

void MythContext::LCDswitchToNothing()
{
    lcd_device->switchToNothing();
}

void MythContext::LCDpopMenu(QString menu_choice, QString menu_title)
{
    lcd_device->popMenu(menu_choice, menu_title);
}

void MythContext::LCDdestroy()
{
    lcd_device->shutdown();
    delete lcd_device;
    lcd_device = NULL;
}

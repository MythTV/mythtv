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
#include "oldsettings.h"
#include "themedmenu.h"
#include "util.h"
#include "remotefile.h"
#include "lcddevice.h"

bool print_verbose_messages = false;    

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
        exit(1);
    }

    m_installprefix = PREFIX;
    m_settings = new Settings;
    m_qtThemeSettings = new Settings;

    language = "";
    m_themeloaded = false;

    m_db = QSqlDatabase::addDatabase("QMYSQL3", "MythContext");

    char localhostname[1024];
    if (gethostname(localhostname, 1024))
    {
        cerr << "Error getting local hostname\n";
        exit(0);
    }
    m_localhostname = localhostname;

    if (!LoadSettingsFiles("mysql.txt"))
        cerr << "Unable to read configuration file mysql.txt" << endl;

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

    mainWindow = NULL;
	
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

void MythContext::ConnectToMasterServer(void)
{
    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port = gContext->GetNumSetting("MasterServerPort", 6543);
    ConnectServer(server, port);
}

bool MythContext::ConnectServer(const QString &hostname, int port)
{
    if (serverSock)
        return true;

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
        ConnectToMasterServer();

    QString ret = QString("myth://%1:%2/").arg(serverSock->peerName())
                                          .arg(serverSock->peerPort());
    return ret;
}

QString MythContext::GetFilePrefix(void)
{
    return GetSetting("RecordFilePrefix");
}

bool MythContext::LoadSettingsFiles(const QString &filename)
{
    return m_settings->LoadSettingsFiles(filename, m_installprefix);
}

void MythContext::LoadQtConfig(void)
{
    language = "";

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

    InitializeScreenSettings();
}

void MythContext::GetScreenSettings(int &width, float &wmult, 
                                    int &height, float &hmult)
{
    height = m_screenheight;
    width = m_screenwidth;

    wmult = m_wmult;
    hmult = m_hmult;
}

void MythContext::InitializeScreenSettings(void)
{
    int height = GetNumSetting("GuiHeight", m_height);
    int width = GetNumSetting("GuiWidth", m_width);

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

    m_wmult = width / 800.0;
    m_hmult = height / 600.0;
    m_screenwidth = width;   
    m_screenheight = height;
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
    dbLock.lock();
    if (!m_db->isOpen()) {
        m_db->setDatabaseName(m_settings->GetSetting("DBName"));
        m_db->setUserName(m_settings->GetSetting("DBUserName"));
        m_db->setPassword(m_settings->GetSetting("DBPassword"));
        m_db->setHostName(m_settings->GetSetting("DBHostName"));
        m_db->open();
    }
    dbLock.unlock();
        
    db->setDatabaseName(m_settings->GetSetting("DBName"));
    db->setUserName(m_settings->GetSetting("DBUserName"));
    db->setPassword(m_settings->GetSetting("DBPassword"));
    db->setHostName(m_settings->GetSetting("DBHostName"));
 
    int res = db->open();
    if (!res)
        cerr << "Unable to connect to database!" << endl
             << DBErrorMessage(db->lastError()) << endl;
    return res;
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

    cerr << "Query was:" << endl
         << query.lastQuery() << endl
         << DBErrorMessage(query.lastError()) << endl;
}

QString MythContext::DBErrorMessage(const QSqlError& err)
{
    if (!err.type())
        return "No error type from QSqlError?  Strange...";

    return QString("Driver error was [%1/%2]:\n"
                   "%3\n"
                   "Database error was:\n"
                   "%4\n")
        .arg(err.type())
        .arg(err.number())
        .arg(err.driverText())
        .arg(err.databaseText());
}

bool MythContext::CheckDBVersion(void)
{
    QString DBSchemaVer = GetSetting("DBSchemaVer", "0");

    int dbversion = DBSchemaVer.toInt();

    int appschema = atoi(MYTH_SCHEMA_VERSION);

    if (dbversion < appschema)
    {
        cerr << "Your current database schema version is: " << dbversion
             << "\nbut this application requires version "
             << MYTH_SCHEMA_VERSION << " or higher.\n";
        cerr << "You should update your database with the latest\n"
             << "changes in the 'database' directory. See section 6\n"
             << "of the MythTV documentation for more information.\n\n";

        return false;
    }
    return true;
}

void MythContext::SaveSetting(QString key, int newValue)
{
    QString strValue = QString("%1").arg(newValue);

    SaveSetting(key, strValue);
}

void MythContext::SaveSetting(QString key, QString newValue)
{
    dbLock.lock();

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

    dbLock.unlock();
}

QString MythContext::GetSetting(const QString &key, const QString &defaultval) 
{
    bool found = false;
    QString value;

    dbLock.lock();

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
    dbLock.unlock();

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

    dbLock.lock();

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

    dbLock.unlock();

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
    QColor bgcolor, fgcolor;

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

QImage *MythContext::LoadScaleImage(QString filename)
{
    if (filename.left(5) == "myth:")
        return NULL;

    QString baseDir = m_installprefix + "/share/mythtv/themes/default/";

    QFile checkFile(filename);

    if (!checkFile.exists())
    {
        QFileInfo fi(filename);
        filename = m_themepathname + fi.fileName();
        checkFile.setName(filename);
        if (!checkFile.exists())
            filename = baseDir + fi.fileName();
    }

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

    QString baseDir = m_installprefix + "/share/mythtv/themes/default/";

    QFile checkFile(filename);

    if (!checkFile.exists())
    {
        QFileInfo fi(filename);
        filename = m_themepathname + fi.fileName();
        checkFile.setName(filename);
        if (!checkFile.exists())
            filename = baseDir + fi.fileName();
    }
              
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
    if (!serverSock)
        ConnectToMasterServer();

    serverSockLock.lock();
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
    serverSockLock.unlock();
}

void MythContext::readSocket(void)
{
    if (expectingReply)
        return;

    serverSockLock.lock();

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

    serverSockLock.unlock();
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

QString MythContext::GetLanguage(void)
{
    if (language == QString::null || language == "")
        language = GetSetting("Language").lower();

    return language;
}

void MythContext::SetMainWindow(MythMainWindow *mainwin)
{
    mainWindow = mainwin;
}

MythMainWindow *MythContext::GetMainWindow(void)
{
    return mainWindow;
}


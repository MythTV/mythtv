#include <qapplication.h>
#include <qsqldatabase.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qdir.h>
#include <qpainter.h>
#include <unistd.h>
#include <qsqldatabase.h>
#include <qurl.h>
#include <qnetwork.h>

#include "mythcontext.h"
#include "oldsettings.h"
#include "themedmenu.h"
#include "util.h"
#include "remotefile.h"
#include "lcddevice.h"
#include "dialogbox.h"
#include "mythdialogs.h"

int print_verbose_messages = VB_IMPORTANT | VB_GENERAL;

MythContext::MythContext(const QString &binversion, bool gui, bool lcd)
           : QObject()
{
    qInitNetworkProtocols();

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

    isconnecting = false;
    language = "";
    m_themeloaded = false;
    m_backgroundimage = NULL;

    m_db = QSqlDatabase::addDatabase("QMYSQL3", "MythContext");

    if (!LoadSettingsFiles("mysql.txt"))
        cerr << "Unable to read configuration file mysql.txt" << endl;

    m_localhostname = m_settings->GetSetting("LocalHostName", NULL);
    if(m_localhostname == NULL)
    {
        char localhostname[1024];
        if (gethostname(localhostname, 1024))
        {
            cerr << "Error getting local hostname\n";
            exit(0);
        }
        m_localhostname = localhostname;
    }

    m_xbase = m_ybase = 0;

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

    if (lcd)
        lcd_device = new LCD();
    else
        lcd_device = NULL;

    disablelibrarypopup = false;
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

    if (lcd_device)
        delete lcd_device;
}

bool MythContext::ConnectToMasterServer(void)
{
    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port = gContext->GetNumSetting("MasterServerPort", 6543);
    return ConnectServer(server, port);
}

bool MythContext::ConnectServer(const QString &hostname, int port)
{
    if (serverSock)
        return true;

    isconnecting = true;

    serverSock = new QSocket();

    VERBOSE(VB_GENERAL, QString("Connecting to backend server: %1:%2")
            .arg(hostname).arg(port));

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
            if (m_height && m_width)
            {
                MythPopupBox::showOkPopup(mainWindow, "connection failure",
                                          tr("Connection to the master backend "
                                             "server timed out.  You probably "
                                             "should modify the Master Server "
                                             "setting in the setup program and "
                                             "set the proper IP address "
                                             "there."));
            }
            serverSock->close();
            delete serverSock;
            serverSock = NULL;
            isconnecting = false;
            return false;
        }
    }

    if (serverSock->state() != QSocket::Connected)
    {
        cout << "Could not connect to backend server\n";
        if (m_height && m_width)
        {
            MythPopupBox::showOkPopup(mainWindow, "connection failure",
                                      tr("Could not connect to the master "
                                         "backend server -- is it running?  Is "
                                         "the IP address set for it in the "
                                         "setup program correct?"));
        }
        serverSock->close();
        delete serverSock;
        serverSock = NULL;
        isconnecting = false;
        return false;
    }

    QString str = QString("ANN Playback %1 %2").arg(m_localhostname).arg(true);
    QStringList strlist = str;
    WriteStringList(serverSock, strlist);
    ReadStringList(serverSock, strlist);

    connect(serverSock, SIGNAL(readyRead()), this, SLOT(readSocket()));
    isconnecting = false;

    return true;
}

QString MythContext::GetMasterHostPrefix(void)
{
    QString ret = "";

    if (!serverSock)
        ConnectToMasterServer();

    if (serverSock)
        ret = QString("myth://%1:%2/").arg(serverSock->peerName())
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
    themecachedir = "";

    m_xbase = GetNumSetting("GuiOffsetX", 0);
    m_ybase = GetNumSetting("GuiOffsetY", 0);

    m_width = GetNumSetting("GuiWidth", 0);
    m_height = GetNumSetting("GuiHeight", 0);

    if (m_width <= 0 || m_height <= 0)
    {
        m_height = QApplication::desktop()->height();
        m_width = QApplication::desktop()->width();
    }

    if (m_qtThemeSettings)
        delete m_qtThemeSettings;

    m_qtThemeSettings = new Settings;

    QString themename = GetSetting("Theme");
    QString themedir = FindThemeDir(themename);
    
    m_themepathname = themedir + "/";
    
    themedir += "/qtlook.txt";
    m_qtThemeSettings->ReadSettings(themedir);
    m_themeloaded = false;

    if (m_backgroundimage)
        delete m_backgroundimage;
    m_backgroundimage = NULL;

    InitializeScreenSettings();
}

void MythContext::UpdateImageCache(void)
{
    imageCache.clear();

    ClearOldImageCache();
    CacheThemeImages();
}

void MythContext::ClearOldImageCache(void)
{
    QString cachedirname = QDir::homeDirPath() + "/.mythtv/themecache/";

    themecachedir = cachedirname + GetSetting("Theme") + "." + 
                    QString::number(m_screenwidth) + "." + 
                    QString::number(m_screenheight);

    QDir d(cachedirname);

    if (!d.exists())
        d.mkdir(cachedirname);

    themecachedir += "/";

    d.setPath(themecachedir);
    if (!d.exists())
        d.mkdir(themecachedir);

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
        if (fi->isDir() && !fi->isSymLink())
        {
            if (fi->absFilePath() == themecachedir)
                continue;
            RemoveCacheDir(fi->absFilePath());
        }
    }
}

void MythContext::RemoveCacheDir(const QString &dir)
{
    QString cachedirname = QDir::homeDirPath() + "/.mythtv/themecache/";

    if (!dir.startsWith(cachedirname))
        return;

    cout << "removing stale cache dir: " << dir << endl;

    QDir d(dir);

    if (!d.exists())
        return;

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
        if (fi->isFile() && !fi->isSymLink())
        {
            QFile file(fi->absFilePath());
            file.remove();
        }
    }

    d.rmdir(dir);
}
    
void MythContext::CacheThemeImages(void)
{
    QString baseDir = m_installprefix + "/share/mythtv/themes/default/";

    if (m_screenwidth == 800 && m_screenheight == 600)
        return;

    CacheThemeImagesDirectory(m_themepathname);
    CacheThemeImagesDirectory(baseDir);
}

void MythContext::CacheThemeImagesDirectory(const QString &dir)
{
    QDir d(dir);
    
    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    MythProgressDialog *caching;
    caching = new MythProgressDialog(QObject::tr("Pre-scaling theme images"),
                                     list->count());

    int progress = 0;

    while ((fi = it.current()) != 0)
    {
        caching->setProgress(progress);
        progress++;

        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        if (fi->isDir())
            continue;

        if (fi->extension().lower() != "png" && 
            fi->extension().lower() != "jpg" &&
            fi->extension().lower() != "gif")
            continue;

        QString filename = fi->fileName();
        QFileInfo cacheinfo(themecachedir + filename);

        if (!cacheinfo.exists() ||
            (cacheinfo.lastModified() < fi->lastModified()))
        {
            VERBOSE(VB_FILE, QString("generating cache image for: %1")
                    .arg(fi->absFilePath()));

            QImage *tmpimage = LoadScaleImage(fi->absFilePath(), false);

            if (tmpimage)
            {
                if (!tmpimage->save(themecachedir + filename, "PNG"))
                {
                    VERBOSE(VB_FILE, QString("Couldn't save cache image: %1")
                            .arg(themecachedir + filename));
                }
             
                delete tmpimage;
            }
        }
    }

    caching->Close();
    delete caching;
}


void MythContext::GetScreenSettings(float &wmult, float &hmult)
{
    wmult = m_wmult;
    hmult = m_hmult;
}

void MythContext::GetScreenSettings(int &width, float &wmult, 
                                    int &height, float &hmult)
{
    height = m_screenheight;
    width = m_screenwidth;

    wmult = m_wmult;
    hmult = m_hmult;
}


void MythContext::GetScreenSettings(int &xbase, int &width, float &wmult, 
                                    int &ybase, int &height, float &hmult)
{
    xbase  = m_xbase;
    ybase  = m_ybase;
    
    height = m_screenheight;
    width = m_screenwidth;

    wmult = m_wmult;
    hmult = m_hmult;
}

void MythContext::InitializeScreenSettings()
{
    int x = 0, y = 0, w = 0, h = 0;

#ifndef QWS
    GetMythTVGeometry(qt_xdisplay(), qt_xscreen(), &x, &y, &w, &h);
#endif

    m_xbase = x + GetNumSetting("GuiOffsetX", 0);
    m_ybase = y + GetNumSetting("GuiOffsetY", 0);

    int height = GetNumSetting("GuiHeight", m_height);
    int width = GetNumSetting("GuiWidth", m_width);

    if (w != 0 && h != 0 && height == 0 && width == 0)
    {
        width = w;
        height = h;
    }
    else
    {
        if (height == 0)
            height = m_height;
        if (width == 0)
            width = m_width;
    }
    
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

    bigfontsize = GetNumSetting("QtFontBig", 25);
    mediumfontsize = GetNumSetting("QtFontMedium", 16);
    smallfontsize = GetNumSetting("QtFontSmall", 12);
}

QString MythContext::FindThemeDir(QString themename)
{
    QString testdir = QDir::homeDirPath() + "/.mythtv/themes/" + themename;

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
    // Some explanation is called for.  This exists because the mysql
    // driver does not gracefully handle the situation where a TCP
    // socketconnection is dropped (for example due to a timeout).  If
    // a Unix domain socket connection is lost, the driver
    // transparently reestablishes the connection and we don't even
    // notice.  However, when this happens with a TCP connection, the
    // driver returns an error for the next query to be executed, and
    // THEN reestablishes the connection (so the second query succeeds
    // with no intervention).
    // mdz, 2003/08/11

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
    if (m_themeloaded)
    {
        widget->setPalette(m_palette);
        if (m_backgroundimage->width() > 0)
            widget->setPaletteBackgroundPixmap(*m_backgroundimage);
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
            m_backgroundimage = new QPixmap(*bgpixmap);
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

            m_backgroundimage = new QPixmap(background);
            widget->setPaletteBackgroundPixmap(background);
        }
    }

    m_themeloaded = true;

    if (bgpixmap)
        delete bgpixmap;
}

QImage *MythContext::LoadScaleImage(QString filename, bool fromcache)
{
    if (filename.left(5) == "myth:")
        return NULL;

    QString baseDir = m_installprefix + "/share/mythtv/themes/default/";

    QFile checkFile(filename);
    QFileInfo fi(filename);

    if (themecachedir != "")
    {
        QString cachefilepath = themecachedir + fi.fileName();
        QFile cachecheck(cachefilepath);
        if (cachecheck.exists() && fromcache)
        {
            QImage *ret = new QImage(cachefilepath);
            if (ret)
                return ret;

            delete ret;
        }
    }

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

QPixmap *MythContext::LoadScalePixmap(QString filename, bool fromcache) 
{ 
    if (filename.left(5) == "myth:")
        return NULL;

    QString baseDir = m_installprefix + "/share/mythtv/themes/default/";

    QFile checkFile(filename);
    QFileInfo fi(filename);

    if (themecachedir != "")
    {
        QString cachefilepath = themecachedir + fi.fileName();
        QFile cachecheck(cachefilepath);
        if (cachecheck.exists() && fromcache)
        {
            QPixmap *ret = new QPixmap(cachefilepath);
            if (ret)
                return ret;
        }
    }

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

bool MythContext::SendReceiveStringList(QStringList &strlist)
{
    if (isconnecting)
        return false;

    if (!serverSock)
        ConnectToMasterServer();

    if (!serverSock)
        return false;

    serverSockLock.lock();
    expectingReply = true;

    WriteStringList(serverSock, strlist);
    bool ok = ReadStringList(serverSock, strlist);

    while (ok && strlist[0] == "BACKEND_MESSAGE")
    {
        // oops, not for us
        QString message = strlist[1];
        QString extra = strlist[2];

        MythEvent me(message, extra);
        dispatch(me);

        ok = ReadStringList(serverSock, strlist);
    }

    if (!ok)
    {
        qApp->lock();
        cout << "Connection to backend server lost\n";
        MythPopupBox::showOkPopup(mainWindow, "connection failure",
                                  tr("The connection to the master backend "
                                     "server has gone away for some reason.. "
                                     "Is it running?"));

        serverSock->close();
        delete serverSock;
        serverSock = NULL;
        qApp->unlock();
    }

    expectingReply = false;
    serverSockLock.unlock();

    return ok;
}

void MythContext::readSocket(void)
{
    if (expectingReply)
        return;

    serverSockLock.lock();

    while (serverSock->state() == QSocket::Connected &&
           serverSock->bytesAvailable() > 0)
    {
        QStringList strlist;
        if (!ReadStringList(serverSock, strlist))
            continue;

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
    if (listeners.find(obj) != -1)
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

void MythContext::dispatchNow(MythEvent &e)
{
    QObject *obj = listeners.first();
    while (obj)
    {
        QApplication::sendEvent(obj, &e);
        obj = listeners.next();
    }
}

QFont MythContext::GetBigFont(void)
{
    return QFont("Arial", (int)ceil(GetBigFontSize() * m_hmult), QFont::Bold);
}

QFont MythContext::GetMediumFont(void)
{
    return QFont("Arial", (int)ceil(GetMediumFontSize() * m_hmult), 
                 QFont::Bold);
}

QFont MythContext::GetSmallFont(void)
{
    return QFont("Arial", (int)ceil(GetSmallFontSize() * m_hmult), QFont::Bold);
}

QString MythContext::GetLanguage(void)
{
    if (language == QString::null || language == "")
        language = GetSetting("Language", "EN").lower();

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

bool MythContext::TestPopupVersion(const QString &name, 
                                   const QString &libversion,
                                   const QString &pluginversion)
{
    if (libversion == pluginversion)
        return true;

    QString err = "The " + name + " plugin was compiled against libmyth " +
                  "version: " + pluginversion + ", but the installed " +
                  "libmyth is at version: " + libversion + ".  You probably " +
                  "want to recompile the " + name + " plugin after doing a " +
                  "make distclean.";

    if (gContext->GetMainWindow() && !disablelibrarypopup)
    {    
        DialogBox dbox(gContext->GetMainWindow(), err);
        dbox.AddButton("OK");
        dbox.exec();
    }

    return false;
}


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
#include "remoteencoder.h"

MythContext::MythContext(bool gui)
           : QObject()
{
    m_installprefix = PREFIX;
    m_settings = new Settings;
    m_qtThemeSettings = new Settings;

    m_themeloaded = false;

    pthread_mutex_init(&dbLock, NULL);
    m_db = QSqlDatabase::addDatabase("QMYSQL3", "MythContext");

    LoadSettingsFiles("settings.txt");
    LoadSettingsFiles("mysql.txt");
    LoadSettingsFiles("theme.txt");

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
}

MythContext::~MythContext()
{
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
            exit(0);
        }
    }

    if (serverSock->state() != QSocket::Connected)
    {
        cout << "Could not connect to backend server\n";
        exit(0);
    }

    char localhostname[256];
    if (gethostname(localhostname, 256))
    {
        cerr << "Error getting local hostname\n";
        exit(0);
    }
    m_localhostname = localhostname;

    QString str = QString("ANN Playback %1 %2").arg(m_localhostname).arg(true);
    QStringList strlist = str;
    WriteStringList(serverSock, strlist);
    ReadStringList(serverSock, strlist);

    connect(serverSock, SIGNAL(readyRead()), this, SLOT(readSocket()));

    return true;
}

QString MythContext::GetFilePrefix(void)
{
    return GetSetting("RecordFilePrefix");
}

void MythContext::LoadSettingsFiles(const QString &filename)
{
    m_settings->LoadSettingsFiles(filename, m_installprefix);
}

void MythContext::LoadSettingsDatabase(QSqlDatabase *db)
{
    QString thequery = "SELECT * FROM settings;";
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
        
    db->setDatabaseName(GetSetting("DBName"));
    db->setUserName(GetSetting("DBUserName"));
    db->setPassword(GetSetting("DBPassword"));
    db->setHostName(GetSetting("DBHostName"));
 
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

void MythContext::DBError(QString where, const QSqlQuery& query) {
    cerr << "DB Error (" << where << "):\n"
         << "was:" << endl
         << query.lastQuery() << endl
         << "Driver error was:" << endl
         << query.lastError().driverText() << endl
         << "Database error was:" << endl
         << query.lastError().databaseText() << endl;
}

QString MythContext::GetSetting(const QString &key, const QString &defaultval) 
{
    bool found = false;
    QString value;
    pthread_mutex_lock(&dbLock);
    if (m_db->isOpen()) {

        KickDatabase(m_db);

        QString query = QString("SELECT data FROM settings WHERE value = '%1';").arg(key);
        QSqlQuery result = m_db->exec(query);

        if (result.isActive() && result.numRowsAffected() > 0) {
            result.next();
            value = result.value(0).toString();
            found = true;
        }
    }
    pthread_mutex_unlock(&dbLock);

    if (found)
        return value;
    return m_settings->GetSetting(key, defaultval); 
}

int MythContext::GetNumSetting(const QString &key, int defaultval)
{
    bool found = false;
    int value = defaultval;
    pthread_mutex_lock(&dbLock);
    if (m_db->isOpen()) {

        KickDatabase(m_db);

        QString query = QString("SELECT data FROM settings WHERE value = '%1';").arg(key);
        QSqlQuery result = m_db->exec(query);

        if (result.isActive() && result.numRowsAffected() > 0) {
            result.next();
            value = result.value(0).toString().toInt();
            found = true;
        }
    }
    pthread_mutex_unlock(&dbLock);

    if (found)
        return value;

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
    bool usetheme = GetNumSetting("ThemeQt");
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

            bgpixmap = LoadScalePixmap(pmapname);
            if (bgpixmap)
            {
                QPixmap background(GetNumSetting("GuiWidth", m_width),
                                   GetNumSetting("GuiHeight", m_height));
                QPainter tmp(&background);

                tmp.drawTiledPixmap(0, 0,
                                    GetNumSetting("GuiWidth", m_width),
                                    GetNumSetting("GuiHeight", m_height),
                                    *bgpixmap);
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

        QString message = strlist[0];

        if (message == "BLAH")
        {
        }
        else
        {
            MythEvent me(message);
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


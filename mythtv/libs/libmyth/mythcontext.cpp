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
#include "guidegrid.h"
#include "programinfo.h"
#include "util.h"

using namespace libmyth;

MythContext::MythContext(bool gui)
           : QObject()
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

    QString str = QString("ANN Playback %1").arg(m_localhostname);
    QStringList strlist = str;
    WriteStringList(serverSock, strlist);
    ReadStringList(serverSock, strlist);

    connect(serverSock, SIGNAL(readyRead()), this, SLOT(readSocket()));

    return true;
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
    QString thequery = "SELECT * FROM settings";
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

QString MythContext::RunProgramGuide(QString startchannel, bool thread,
                             void (*embedcb)(void *data, unsigned long wid,
                                             int x, int y, int w, int h),
                                     void *data)
{
    if (thread)
        qApp->lock();
 
    GuideGrid gg(this, startchannel, embedcb, data);

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

void MythContext::KickDatabase(QSqlDatabase *db)
{
    QString query("SELECT NULL");
    for(unsigned int i = 0 ; i < 2 ; ++i, usleep(50000)) {
        QSqlQuery result = db->exec(query);
        if (result.isActive())
            break;
        else
            cout << "KickDatabase: "
                 << result.lastError().databaseText() << endl;
    }
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

void MythContext::SendReceiveStringList(QStringList &strlist)
{
    pthread_mutex_lock(&serverSockLock);
    expectingReply = true;

    WriteStringList(serverSock, strlist);
    ReadStringList(serverSock, strlist);


    expectingReply = false;
    pthread_mutex_unlock(&serverSockLock);
}

vector<ProgramInfo *> *MythContext::GetRecordedList(bool deltype)
{
    QString str = "QUERY_RECORDINGS ";
    if (deltype)
        str += "Delete";
    else
        str += "Play";

    QStringList strlist = str;

    SendReceiveStringList(strlist);

    int numrecordings = strlist[0].toInt();

    vector<ProgramInfo *> *info = new vector<ProgramInfo *>;
    int offset = 1;

    for (int i = 0; i < numrecordings; i++)
    {
        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->FromStringList(strlist, offset);
        info->push_back(pginfo);

        offset += NUMPROGRAMLINES;
    }

    return info;
} 

void MythContext::GetFreeSpace(int &totalspace, int &usedspace)
{
    QStringList strlist = QString("QUERY_FREESPACE");

    SendReceiveStringList(strlist);

    totalspace = strlist[0].toInt();
    usedspace = strlist[1].toInt();
}

void MythContext::DeleteRecording(ProgramInfo *pginfo)
{
    QStringList strlist = QString("DELETE_RECORDING");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}

bool MythContext::GetAllPendingRecordings(list<ProgramInfo *> &recordinglist)
{
    QStringList strlist = QString("QUERY_GETALLPENDING");

    SendReceiveStringList(strlist);

    bool conflicting = strlist[0].toInt();
    int numrecordings = strlist[1].toInt();

    int offset = 2;

    for (int i = 0; i < numrecordings; i++)
    {
        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->FromStringList(strlist, offset);
        recordinglist.push_back(pginfo);

        offset += NUMPROGRAMLINES;
    }

    return conflicting;
}

list<ProgramInfo *> *MythContext::GetConflictList(ProgramInfo *pginfo, 
                                                  bool removenonplaying)
{
    QString cmd = QString("QUERY_GETCONFLICTING %1").arg(removenonplaying);
    QStringList strlist = cmd;
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    int numrecordings = strlist[0].toInt();
    int offset = 1;

    list<ProgramInfo *> *retlist = new list<ProgramInfo *>;

    for (int i = 0; i < numrecordings; i++)
    {
        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->FromStringList(strlist, offset);
        retlist->push_back(pginfo);

        offset += NUMPROGRAMLINES;
    }

    return retlist;
}

int MythContext::RequestRecorder(void)
{
    QStringList strlist = "GET_FREE_RECORDER";

    SendReceiveStringList(strlist);

    int retval = strlist[0].toInt();
    return retval;
}

bool MythContext::RecorderIsRecording(int recorder)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "IS_RECORDING";

    SendReceiveStringList(strlist);

    bool retval = strlist[0].toInt();
    return retval;
}

float MythContext::GetRecorderFrameRate(int recorder)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "GET_FRAMERATE";

    SendReceiveStringList(strlist);

    float retval = strlist[0].toFloat();
    return retval;
}

long long MythContext::GetRecorderFramesWritten(int recorder)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "GET_FRAMES_WRITTEN";

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

long long MythContext::GetRecorderFilePosition(int recorder)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "GET_FILE_POSITION";

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

long long MythContext::GetRecorderFreeSpace(int recorder, 
                                            long long totalreadpos)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "GET_FREE_SPACE";
    encodeLongLong(strlist, totalreadpos);

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

long long MythContext::GetKeyframePosition(int recorder, long long desired)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "GET_KEYFRAME_POS";
    encodeLongLong(strlist, desired);

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

void MythContext::SetupRecorderRingBuffer(int recorder, QString &path,
                                          long long &filesize, 
                                          long long &fillamount,
                                          bool pip)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "SETUP_RING_BUFFER";
    strlist << QString::number((int)pip);

    SendReceiveStringList(strlist);

    path = strlist[0];

    filesize = decodeLongLong(strlist, 1);
    fillamount = decodeLongLong(strlist, 3);
}

void MythContext::SpawnLiveTVRecording(int recorder)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "SPAWN_LIVETV";

    SendReceiveStringList(strlist);
}

void MythContext::StopLiveTVRecording(int recorder)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "STOP_LIVETV";

    SendReceiveStringList(strlist);
}

void MythContext::PauseRecorder(int recorder)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "PAUSE";

    SendReceiveStringList(strlist);
}

void MythContext::ToggleRecorderInputs(int recorder)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "TOGGLE_INPUTS";

    SendReceiveStringList(strlist);
}

void MythContext::RecorderChangeChannel(int recorder, bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "CHANGE_CHANNEL";
    strlist << QString::number((int)direction);    

    SendReceiveStringList(strlist);
}

void MythContext::RecorderSetChannel(int recorder, QString channel)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "SET_CHANNEL";
    strlist << channel;

    SendReceiveStringList(strlist);
}

void MythContext::RecorderChangeContrast(int recorder, bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "CHANGE_CONTRAST";
    strlist << QString::number((int)direction);    

    SendReceiveStringList(strlist);
}

void MythContext::RecorderChangeBrightness(int recorder, bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "CHANGE_BRIGHTNESS";
    strlist << QString::number((int)direction);    

    SendReceiveStringList(strlist);
}

void MythContext::RecorderChangeColour(int recorder, bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "CHANGE_COLOUR";
    strlist << QString::number((int)direction);    

    SendReceiveStringList(strlist);
}

bool MythContext::CheckChannel(int recorder, QString channel)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "CHECK_CHANNEL";
    strlist << channel;

    SendReceiveStringList(strlist);

    bool retval = strlist[0].toInt();
    return retval;
}

void MythContext::GetRecorderChannelInfo(int recorder, QString &title, 
                                         QString &subtitle,
                                         QString &desc, QString &category,
                                         QString &starttime, QString &endtime,
                                         QString &callsign, QString &iconpath,
                                         QString &channelname)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "GET_PROGRAM_INFO";

    SendReceiveStringList(strlist);

    title = strlist[0];
    subtitle = strlist[1];
    desc = strlist[2];
    category = strlist[3];
    starttime = strlist[4];
    endtime = strlist[5];
    callsign = strlist[6];
    iconpath = strlist[7];
    channelname = strlist[8];
}

void MythContext::GetRecorderInputName(int recorder, QString &inputname)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recorder);
    strlist << "GET_INPUT_NAME";

    SendReceiveStringList(strlist);

    inputname = strlist[0];
}

void MythContext::readSocket(void)
{
    if (expectingReply)
        return;

    if (serverSock->bytesAvailable() > 0)
        cout << "ready read\n";

}


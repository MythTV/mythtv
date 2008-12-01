#include <unistd.h>

// QT headers
#include <QFile>
#include <QTextStream>
#include <QApplication>

// MythTV headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/compat.h>
#include <mythtv/mythdirs.h>

// MythWeather headers
#include "weatherScreen.h"
#include "weatherSource.h"

QStringList WeatherSource::probeTypes(Q3Process *proc)
{
    QStringList types;

    proc->addArgument("-t");
    if (!proc->start())
    {
        VERBOSE(VB_IMPORTANT,
                "cannot run script " + proc->arguments().join(" "));
        return QStringList();
    }
    while (proc->isRunning())
    {
        qApp->processEvents();
        usleep(250);
    }

    if (!proc->normalExit() || proc->exitStatus())
    {
        VERBOSE(VB_IMPORTANT, "Error Running Script");
        VERBOSE(VB_IMPORTANT, proc->readStderr());
        return QStringList();
    }
    QString tempStr;

    while (proc->canReadLineStdout())
    {
        tempStr = proc->readLineStdout();
        types << tempStr;
    }

    if (types.size() == 0)
    {
        VERBOSE(VB_IMPORTANT, "invalid output from -t option");
        return QStringList();
    }

    return types;
}

bool WeatherSource::probeTimeouts(Q3Process *proc, uint &updateTimeout,
                                  uint &scriptTimeout)
{
    proc->addArgument("-T");
    updateTimeout = 0;
    scriptTimeout = 0;

    if (!proc->start())
    {
        VERBOSE(VB_IMPORTANT,
                "cannot run script " + proc->arguments().join(" "));
        return false;
    }

    while (proc->isRunning())
    {
        qApp->processEvents();
        usleep(250);
    }

    if (!proc->normalExit() || proc->exitStatus() )
    {
        VERBOSE(VB_IMPORTANT, "Error Running Script");
        VERBOSE(VB_IMPORTANT, proc->readStderr());
        return false;
    }

    if (!proc->canReadLineStdout())
    {
        VERBOSE(VB_IMPORTANT, "Invalid Script Output!");
        return false;
    }

    QStringList temp =
            QStringList::split(',', QString(proc->readLineStdout()));
    if (temp.size() != 2)
    {
        VERBOSE(VB_IMPORTANT, "Invalid Script Output!");
        return false;
    }

    bool isOK = false;
    uint i = temp[0].toUInt(&isOK);
    updateTimeout = isOK ? i * 1000 : DEFAULT_UPDATE_TIMEOUT;

    i = temp[1].toUInt(&isOK);
    scriptTimeout = isOK ? i * 1000 : DEFAULT_SCRIPT_TIMEOUT;
    return true;
}

bool WeatherSource::probeInfo(Q3Process *proc, QString &name, QString &version,
                              QString &author, QString &email)
{
    /*
     * -v -- name,version,author,email
     */
    proc->addArgument("-v");
    if (!proc->start())
    {
        VERBOSE(VB_IMPORTANT,
                "cannot run script " + proc->arguments().join(" "));
        return false;
    }
    while (proc->isRunning())
    {
        qApp->processEvents();
        usleep(250);
    }

    if (!proc->normalExit() || proc->exitStatus())
    {
        VERBOSE(VB_IMPORTANT, "Error Running Script");
        VERBOSE(VB_IMPORTANT, proc->readStderr());
        return false;
    }

    if (!proc->canReadLineStdout())
    {
        VERBOSE(VB_IMPORTANT, "Invalid Script Output!");
        return false;
    }

    QStringList temp = QStringList::split(',', QString(proc->readLineStdout()));
    if (temp.size() != 4)
    {
       VERBOSE(VB_IMPORTANT, "Invalid Script Output!");
       return false;
    }

    name = temp[0];
    version = temp[1];
    author = temp[2];
    email = temp[3];
    return true;
}

/* Basic logic of this behemouth...
 * run script with -v flag, this returns among other things, the version number
 * Search the database using the name (also returned from -v).
 * if it exists, compare versions from -v and db
 * if the same, populate the info struct from db, and we're done
 * if they differ, get the rest of the needed information from the script and
 * update the database, note, it does not overwrite the existing timeout values.
 * if the script is not in the database, we probe it for types and default
 * timeout values, and add it to the database
 */
ScriptInfo *WeatherSource::probeScript(const QFileInfo &fi)
{
    QStringList temp;
    Q3Process *proc;

    if (fi.isReadable() && fi.isExecutable())
        proc = new Q3Process(fi.absFilePath());
    else
        return 0;

    proc->setWorkingDirectory(fi.dir(true));
    ScriptInfo *info = new ScriptInfo;
    info->file = new QFileInfo(fi);
    if (!WeatherSource::probeInfo(proc, info->name, info->version, info->author,
                info->email))
    {
        delete proc;
        delete info->file;
        delete info;
        return 0;
    }

    MSqlQuery db(MSqlQuery::InitCon());
    QString query =
            "SELECT sourceid, source_name, update_timeout, retrieve_timeout, "
            "path, author, version, email, types FROM weathersourcesettings "
            "WHERE hostname = :HOST AND source_name = :NAME;";
    db.prepare(query);
    db.bindValue(":HOST", gContext->GetHostName());
    db.bindValue(":NAME", info->name);
    db.exec();
    // the script exists in the db
    if (db.size() == 1)
    {
        db.next();
        info->id = db.value(0).toInt();
        info->updateTimeout = db.value(2).toUInt() * 1000;
        info->scriptTimeout = db.value(3).toUInt() * 1000;

        // compare versions, if equal... be happy
        QString dbver = db.value(6).toString();
        if (dbver == info->version)
        {
            info->types = QStringList::split(",", db.value(8).toString());
        }
        else
        {
            // versions differ, change db to match script output
            VERBOSE(VB_GENERAL, "New version of " + info->name + " found");
            query = "UPDATE weathersourcesettings SET source_name = :NAME, "
                "path = :PATH, author = :AUTHOR, version = :VERSION, "
                "email = :EMAIL, types = :TYPES WHERE sourceid = :ID";
            db.prepare(query);
            // these info values were populated when getting the version number
            // we leave the timeout values in
            db.bindValue(":NAME", info->name);
            db.bindValue(":PATH", info->file->absFilePath());
            db.bindValue(":AUTHOR", info->author);
            db.bindValue(":VERSION", info->version);
            // run the script to get supported data types
            proc->clearArguments();
            proc->addArgument(fi.absFilePath());
            info->types = WeatherSource::probeTypes(proc);
            db.bindValue(":TYPES", info->types.join(","));
            db.bindValue(":ID", info->id);
            db.bindValue(":EMAIL", info->email);
            if (!db.exec())
            {
                VERBOSE(VB_IMPORTANT, db.lastError().text());
                delete proc;
                delete info->file;
                delete info;
                return 0;
            }
        }
    }
    else if (db.size() == 0)
    {
        // Script is not in db, probe it and insert it into db
        query = "INSERT INTO weathersourcesettings "
                "(hostname, source_name, update_timeout, retrieve_timeout, "
                "path, author, version, email, types) "
                "VALUES (:HOST, :NAME, :UPDATETO, :RETTO, :PATH, :AUTHOR, "
                ":VERSION, :EMAIL, :TYPES);";
        proc->clearArguments();
        proc->addArgument(fi.absFilePath());
        if (!WeatherSource::probeTimeouts(proc, info->updateTimeout,
                                          info->scriptTimeout))
        {
                delete proc;
                delete info->file;
                delete info;
                return 0;
        }
        db.prepare(query);
        db.bindValue(":NAME", info->name);
        db.bindValue(":HOST", gContext->GetHostName());
        db.bindValue(":UPDATETO", QString::number(info->updateTimeout / 1000));
        db.bindValue(":RETTO", QString::number(info->scriptTimeout / 1000));
        db.bindValue(":PATH", info->file->absFilePath());
        db.bindValue(":AUTHOR", info->author);
        db.bindValue(":VERSION", info->version);
        db.bindValue(":EMAIL", info->email);
        proc->clearArguments();
        proc->addArgument(fi.absFilePath());
        info->types = WeatherSource::probeTypes(proc);
        db.bindValue(":TYPES", info->types.join(","));
        if (!db.exec())
        {
            VERBOSE(VB_IMPORTANT, db.lastError().text());
            delete proc;
            delete info->file;
            delete info;
            return 0;
        }
        query = "SELECT sourceid FROM weathersourcesettings "
                "WHERE source_name = :NAME AND hostname = :HOST;";
        // a little annoying, but look at what we just inserted to get the id
        // number, not sure if we really need it, but better safe than sorry.
        db.prepare(query);
        db.bindValue(":HOST", gContext->GetHostName());
        db.bindValue(":NAME", info->name);
        if (!db.exec())
        {
            VERBOSE(VB_IMPORTANT, db.lastError().text());
            delete proc;
            delete info->file;
            delete info;
            return 0;
        }
        db.next();
        info->id = db.value(0).toInt();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Invalid response from database");
        delete proc;
        delete info->file;
        delete info;
        return 0;
    }
    delete proc;
    return info;
}

/*
 * Watch out, we store the parameter as a member variable, don't go deleting it,
 * that wouldn't be good.
 */
WeatherSource::WeatherSource(ScriptInfo *info)
    : m_ready(info ? true : false),    m_inuse(info ? true : false),
      m_info(info),
      m_proc(info ? new Q3Process(info->file->absFilePath()) : NULL),
      m_locale(""),                    m_buffer(""),
      m_units(SI_UNITS),               m_scriptTimer(new QTimer(this)),
      m_updateTimer(new QTimer(this)), m_connectCnt(0)
{
    QDir dir(GetConfDir());
    if (!dir.exists("MythWeather"))
        dir.mkdir("MythWeather");
    dir.cd("MythWeather");
    if (!dir.exists(info->name))
        dir.mkdir(info->name);
    dir.cd(info->name);
    m_dir = dir.absPath();

    connect( m_scriptTimer, SIGNAL(timeout()),
            this, SLOT(scriptTimeout()));

    connect( m_updateTimer, SIGNAL(timeout()),
            this, SLOT(updateTimeout()));

    if (m_proc) {
        m_proc->setWorkingDirectory(QDir(GetShareDir() +
                                         "mythweather/scripts/"));
        connect(this, SIGNAL(killProcess()), m_proc, SLOT(kill()));
    }
}

WeatherSource::WeatherSource(const QString &filename)
    : m_ready(false),                  m_inuse(false),
      m_info(NULL),                    m_proc(NULL),
      m_dir(""),                       m_locale(""),
      m_buffer(""),                    m_units(SI_UNITS), 
      m_scriptTimer(new QTimer(this)), m_updateTimer(new QTimer(this)),
      m_connectCnt(0)
{
    connect( m_scriptTimer, SIGNAL(timeout()),
            this, SLOT(scriptTimeout()));

    connect( m_updateTimer, SIGNAL(timeout()),
            this, SLOT(updateTimeout()));


    const QFileInfo fi(filename);
    m_info = WeatherSource::probeScript(fi);

    if (m_info)
    {
        m_proc = new Q3Process(filename);
        m_proc->setWorkingDirectory(QDir(GetShareDir() +
                                         "mythweather/scripts/"));
        connect(this, SIGNAL(killProcess()),
                m_proc, SLOT(kill()));
        m_ready = true;
    }
    else
        VERBOSE(VB_IMPORTANT, "Error probing script");
}

WeatherSource::~WeatherSource()
{
    delete m_proc;
    delete m_scriptTimer;
    delete m_updateTimer;
}

void WeatherSource::connectScreen(WeatherScreen *ws)
{
    connect(this, SIGNAL(newData(QString, units_t, DataMap)),
            ws, SLOT(newData(QString, units_t, DataMap)));
    ++m_connectCnt;

    if (m_data.size() > 0)
    {
        emit newData(m_locale, m_units, m_data);
    }
}

void WeatherSource::disconnectScreen(WeatherScreen *ws)
{
    disconnect(this, 0, ws, 0);
    --m_connectCnt;
}

QStringList WeatherSource::getLocationList(const QString &str)
{
    QStringList locs;

    m_proc->clearArguments();
    m_proc->setWorkingDirectory(m_info->file->dir(true));
    m_proc->addArgument(m_info->file->absFilePath());
    m_proc->addArgument("-l");
    m_proc->addArgument(str);

    if (m_proc->isRunning())
    {
        VERBOSE(VB_IMPORTANT, "error script already running");
        return QStringList();
    }

    if (!m_proc->start())
    {
        VERBOSE(VB_IMPORTANT, "cannot start script");
        return QStringList();
    }

    while (m_proc->isRunning())
    {
        if (m_proc->canReadLineStdout())
            locs << m_proc->readLineStdout();
        else
            usleep(250);
        qApp->processEvents();
    }

    while (m_proc->canReadLineStdout())
        locs << m_proc->readLineStdout();

    return locs;
}

void WeatherSource::startUpdate()
{
    m_buffer = "";

    MSqlQuery db(MSqlQuery::InitCon());
    VERBOSE(VB_GENERAL, "Starting update of " + m_info->name);
    db.prepare("SELECT updated FROM weathersourcesettings "
               "WHERE sourceid = :ID AND "
               "TIMESTAMPADD(SECOND,update_timeout,updated) > NOW()");
    db.bindValue(":ID", getId());
    db.exec();
    if (db.size() > 0)
    {
        VERBOSE(VB_IMPORTANT, QString("%1 recently updated, skipping.")
                                    .arg(m_info->name));

        QString cachefile = QString("%1/cache_%2").arg(m_dir).arg(m_locale);
        QFile cache(cachefile);
        if (cache.exists() && cache.open( QIODevice::ReadOnly ))
        {
            QTextStream text( &cache );
            m_buffer += text.read();
            cache.close();

            processData();

            if (m_connectCnt)
            {
                emit newData(m_locale, m_units, m_data);
            }
            return;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("No cachefile for %1, forcing "
                                          "update.").arg(m_info->name));
        }
    }

    m_data.clear();
    m_proc->clearArguments();
    m_proc->setWorkingDirectory(m_info->file->dir(true));
    m_proc->addArgument("nice");
    m_proc->addArgument(m_info->file->absFilePath());
    m_proc->addArgument("-u");
    m_proc->addArgument(m_units == SI_UNITS ? "SI" : "ENG");
    if (!m_dir.isEmpty())
    {
        m_proc->addArgument("-d");
        m_proc->addArgument(m_dir);
    }
    m_proc->addArgument(m_locale);

    connect( m_proc, SIGNAL(readyReadStdout()),
            this, SLOT(readFromStdout()));
    connect( m_proc, SIGNAL(processExited()),
            this, SLOT(processExit()));
    if (!m_proc->start())
    {
        VERBOSE(VB_IMPORTANT, "Error running script");
    }
    else
        m_scriptTimer->start(m_info->scriptTimeout);
}

void WeatherSource::updateTimeout()
{
    if (!isRunning())
    {
        startUpdate();
        startUpdateTimer();
    }
}

void WeatherSource::readFromStdout()
{
    m_buffer += m_proc->readStdout();
}

void WeatherSource::processExit()
{
    VERBOSE(VB_GENERAL, m_proc->arguments().join(" ") + " has exited");
    m_proc->disconnect(); // disconnects all signals

    m_scriptTimer->stop();
    if (!m_proc->normalExit())
    {
        VERBOSE(VB_IMPORTANT, "script exit status " + m_proc->exitStatus());
        return;
    }

    QString tempStr = m_proc->readStdout();
    if (!tempStr.isEmpty())
        m_buffer += tempStr;

    QString cachefile = QString("%1/cache_%2").arg(m_dir).arg(m_locale);
    QFile cache(cachefile);
    if (cache.open( QIODevice::WriteOnly ))
    {
        cache.writeBlock( m_buffer, qstrlen(m_buffer) );
        cache.close();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to save data to cachefile: %1")
                                    .arg(cachefile));
    }

    processData();

    MSqlQuery db(MSqlQuery::InitCon());

    db.prepare("UPDATE weathersourcesettings "
               "SET updated = NOW() WHERE sourceid = :ID;");

    db.bindValue(":ID", getId());
    if (!db.exec())
    {
        VERBOSE(VB_IMPORTANT, db.lastError().text());
        return;
    }

    if (m_connectCnt)
    {
        emit newData(m_locale, m_units, m_data);
    }
}

void WeatherSource::processData()
{
    QStringList data = QStringList::split('\n', m_buffer);
    QStringList temp;
    for (int i = 0; i < data.size(); ++i)
    {
        temp = QStringList::split("::", data[i]);
        if (temp.size() > 2)
            VERBOSE(VB_IMPORTANT, "Error parsing script file, ignoring");
        if (temp.size() < 2)
        {
            VERBOSE(VB_IMPORTANT, data[i]);
            VERBOSE(VB_IMPORTANT,
                    "Unrecoverable error parsing script output " + temp.size());
            return; // we don't emit signal
        }

        if (!m_data[temp[0]].isEmpty())
        {
            m_data[temp[0]].append("\n" + temp[1]);
        }
        else
            m_data[temp[0]] = temp[1];
    }
}

void WeatherSource::scriptTimeout()
{
    if (m_proc->isRunning())
    {
        VERBOSE(VB_IMPORTANT,
                "Script timeout exceeded, summarily executing it");
        emit killProcess();
    }
}

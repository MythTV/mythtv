#include <unistd.h>

// QT headers
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTextCodec>                                                          
#include <QApplication>

// MythTV headers
#include <mythcontext.h>
#include <mythdb.h>
#include <compat.h>
#include <mythdirs.h>
#include <mythsystem.h>
#include <exitcodes.h>

// MythWeather headers
#include "weatherScreen.h"
#include "weatherSource.h"

QStringList WeatherSource::ProbeTypes(QString workingDirectory,
                                      QString program)
{
    QStringList arguments("-t");
    const QString loc = QString("WeatherSource::ProbeTypes(%1 %2): ")
        .arg(program).arg(arguments.join(" "));
    QStringList types;

    uint flags = kMSRunShell | kMSStdOut | kMSBuffered | 
                 kMSDontDisableDrawing | kMSDontBlockInputDevs;
    MythSystem ms(program, arguments, flags);
    ms.SetDirectory(workingDirectory);
    ms.Run();
    if (ms.Wait() != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Cannot run script");
        return types;
    }

    QByteArray result = ms.ReadAll();
    QTextStream text(result);

    while (!text.atEnd())
    {
        QString tmp = text.readLine();

        while (tmp.endsWith('\n') || tmp.endsWith('\r'))
            tmp.chop(1);

        if (!tmp.isEmpty())
            types += tmp;
    }

    if (types.empty())
        LOG(VB_GENERAL, LOG_ERR, loc + "Invalid output from -t option");

    return types;
}

bool WeatherSource::ProbeTimeouts(QString  workingDirectory,
                                  QString  program,
                                  uint    &updateTimeout,
                                  uint    &scriptTimeout)
{
    QStringList arguments("-T");
    const QString loc = QString("WeatherSource::ProbeTimeouts(%1 %2): ")
        .arg(program).arg(arguments.join(" "));

    updateTimeout = DEFAULT_UPDATE_TIMEOUT;
    scriptTimeout = DEFAULT_SCRIPT_TIMEOUT;

    uint flags = kMSRunShell | kMSStdOut | kMSBuffered | 
                 kMSDontDisableDrawing | kMSDontBlockInputDevs;
    MythSystem ms(program, arguments, flags);
    ms.SetDirectory(workingDirectory);
    ms.Run();
    if (ms.Wait() != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Cannot run script");
        return false;
    }

    QByteArray result = ms.ReadAll();
    QTextStream text(result);

    QStringList lines;
    while (!text.atEnd())
    {
        QString tmp = text.readLine();

        while (tmp.endsWith('\n') || tmp.endsWith('\r'))
            tmp.chop(1);

        if (!tmp.isEmpty())
            lines << tmp;
    }

    if (lines.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Invalid Script Output! No Lines");
        return false;
    }

    QStringList temp = lines[0].split(',');
    if (temp.size() != 2)
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Invalid Script Output! '%1'").arg(lines[0]));
        return false;
    }

    bool isOK[2];
    uint ut = temp[0].toUInt(&isOK[0]);
    uint st = temp[1].toUInt(&isOK[1]);
    if (!isOK[0] || !isOK[1])
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Invalid Script Output! '%1'").arg(lines[0]));
        return false;
    }

    updateTimeout = ut * 1000;
    scriptTimeout = st;

    return true;
}

bool WeatherSource::ProbeInfo(ScriptInfo &info)
{
    QStringList arguments("-v");

    const QString loc = QString("WeatherSource::ProbeInfo(%1 %2): ")
        .arg(info.program).arg(arguments.join(" "));

    uint flags = kMSRunShell | kMSStdOut | kMSBuffered | 
                 kMSDontDisableDrawing | kMSDontBlockInputDevs;
    MythSystem ms(info.program, arguments, flags);
    ms.SetDirectory(info.path);
    ms.Run();
    if (ms.Wait() != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Cannot run script");
        return false;
    }

    QByteArray result = ms.ReadAll();
    QTextStream text(result);

    QStringList lines;
    while (!text.atEnd())
    {
        QString tmp = text.readLine();

        while (tmp.endsWith('\n') || tmp.endsWith('\r'))
            tmp.chop(1);

        if (!tmp.isEmpty())
            lines << tmp;
    }

    if (lines.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Invalid Script Output! No Lines");
        return false;
    }

    QStringList temp = lines[0].split(',');
    if (temp.size() != 4)
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Invalid Script Output! '%1'").arg(lines[0]));
        return false;
    }

    info.name    = temp[0];
    info.version = temp[1];
    info.author  = temp[2];
    info.email   = temp[3];

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
ScriptInfo *WeatherSource::ProbeScript(const QFileInfo &fi)
{
    QStringList temp;

    if (!fi.isReadable() || !fi.isExecutable())
        return NULL;

    ScriptInfo info;
    info.path = fi.absolutePath();
    info.program = fi.absoluteFilePath();

    if (!WeatherSource::ProbeInfo(info))
        return NULL;

    MSqlQuery db(MSqlQuery::InitCon());
    QString query =
            "SELECT sourceid, source_name, update_timeout, retrieve_timeout, "
            "path, author, version, email, types FROM weathersourcesettings "
            "WHERE hostname = :HOST AND source_name = :NAME;";
    db.prepare(query);
    db.bindValue(":HOST", gCoreContext->GetHostName());
    db.bindValue(":NAME", info.name);

    if (!db.exec())
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid response from database");
        return NULL;
    }

    // the script exists in the db
    if (db.next())
    {
        info.id            = db.value(0).toInt();
        info.updateTimeout = db.value(2).toUInt() * 1000;
        info.scriptTimeout = db.value(3).toUInt();

        // compare versions, if equal... be happy
        QString dbver = db.value(6).toString();
        if (dbver == info.version)
        {
            info.types = db.value(8).toString().split(",");
        }
        else
        {
            // versions differ, change db to match script output
            LOG(VB_GENERAL, LOG_INFO, "New version of " + info.name + " found");
            query = "UPDATE weathersourcesettings SET source_name = :NAME, "
                "path = :PATH, author = :AUTHOR, version = :VERSION, "
                "email = :EMAIL, types = :TYPES WHERE sourceid = :ID";
            db.prepare(query);
            // these info values were populated when getting the version number
            // we leave the timeout values in
            db.bindValue(":NAME", info.name);
            db.bindValue(":PATH", info.program);
            db.bindValue(":AUTHOR", info.author);
            db.bindValue(":VERSION", info.version);

            // run the script to get supported data types
            info.types = WeatherSource::ProbeTypes(info.path, info.program);

            db.bindValue(":TYPES", info.types.join(","));
            db.bindValue(":ID", info.id);
            db.bindValue(":EMAIL", info.email);
            if (!db.exec())
            {
                MythDB::DBError("Updating weather source settings.", db);
                return NULL;
            }
        }
    }
    else
    {
        // Script is not in db, probe it and insert it into db
        query = "INSERT INTO weathersourcesettings "
                "(hostname, source_name, update_timeout, retrieve_timeout, "
                "path, author, version, email, types) "
                "VALUES (:HOST, :NAME, :UPDATETO, :RETTO, :PATH, :AUTHOR, "
                ":VERSION, :EMAIL, :TYPES);";

        if (!WeatherSource::ProbeTimeouts(info.path,
                                          info.program,
                                          info.updateTimeout,
                                          info.scriptTimeout))
        {
            return NULL;
        }
        db.prepare(query);
        db.bindValue(":NAME", info.name);
        db.bindValue(":HOST", gCoreContext->GetHostName());
        db.bindValue(":UPDATETO", QString::number(info.updateTimeout/1000));
        db.bindValue(":RETTO", QString::number(info.scriptTimeout));
        db.bindValue(":PATH", info.program);
        db.bindValue(":AUTHOR", info.author);
        db.bindValue(":VERSION", info.version);
        db.bindValue(":EMAIL", info.email);
        info.types = ProbeTypes(info.path, info.program);
        db.bindValue(":TYPES", info.types.join(","));
        if (!db.exec())
        {
            MythDB::DBError("Inserting weather source", db);
            return NULL;
        }
        query = "SELECT sourceid FROM weathersourcesettings "
                "WHERE source_name = :NAME AND hostname = :HOST;";
        // a little annoying, but look at what we just inserted to get the id
        // number, not sure if we really need it, but better safe than sorry.
        db.prepare(query);
        db.bindValue(":HOST", gCoreContext->GetHostName());
        db.bindValue(":NAME", info.name);
        if (!db.exec())
        {
            MythDB::DBError("Getting weather sourceid", db);
            return NULL;
        }
        else if (!db.next())
        {
            LOG(VB_GENERAL, LOG_ERR, "Error getting weather sourceid");
            return NULL;
        }
        else
        {
            info.id = db.value(0).toInt();
        }
    }

    return new ScriptInfo(info);
}

/*
 * Watch out, we store the parameter as a member variable, don't go deleting it,
 * that wouldn't be good.
 */
WeatherSource::WeatherSource(ScriptInfo *info)
    : m_ready(info ? true : false),    m_inuse(info ? true : false),
      m_info(info),
      m_ms(NULL),
      m_locale(""),
      m_cachefile(""),
      m_units(SI_UNITS),
      m_updateTimer(new QTimer(this)), m_connectCnt(0)
{
    QDir dir(GetConfDir());
    if (!dir.exists("MythWeather"))
        dir.mkdir("MythWeather");
    dir.cd("MythWeather");
    if (!dir.exists(info->name))
        dir.mkdir(info->name);
    dir.cd(info->name);
    m_dir = dir.absolutePath();

    connect( m_updateTimer, SIGNAL(timeout()), this, SLOT(updateTimeout()));
}

WeatherSource::~WeatherSource()
{
    if (m_ms)
    {
        m_ms->Signal(kSignalKill);
        m_ms->Wait(5);
        delete m_ms;
    }
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
    QString program = m_info->program;
    QStringList args;
    args << "-l";
    args << str;

    const QString loc = QString("WeatherSource::getLocationList(%1 %2): ")
        .arg(program).arg(args.join(" "));

    uint flags = kMSRunShell | kMSStdOut | kMSBuffered | 
                 kMSDontDisableDrawing | kMSDontBlockInputDevs;
    MythSystem ms(program, args, flags);
    ms.SetDirectory(m_info->path);
    ms.Run();
    
    if (ms.Wait() != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Cannot run script");
        return QStringList();
    }

    QStringList locs;
    QByteArray result = ms.ReadAll();
    QTextStream text(result);
                                                                                
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");                     
    while (!text.atEnd())
    {
        QString tmp = text.readLine();

        while (tmp.endsWith('\n') || tmp.endsWith('\r'))
            tmp.chop(1);

        if (!tmp.isEmpty())
         {                                                                      
             QString loc_string = codec->toUnicode(tmp.toUtf8());
             locs << loc_string;
         }
    }

    return locs;
}

void WeatherSource::startUpdate(bool forceUpdate)
{
    m_buffer.clear();

    MSqlQuery db(MSqlQuery::InitCon());
    LOG(VB_GENERAL, LOG_INFO, "Starting update of " + m_info->name);

    if (m_ms)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 process exists, skipping.")
            .arg(m_info->name));
        return;
    }

    if (!forceUpdate)
    {
        db.prepare("SELECT updated FROM weathersourcesettings "
                "WHERE sourceid = :ID AND "
                "TIMESTAMPADD(SECOND,update_timeout-15,updated) > NOW()");
        db.bindValue(":ID", getId());
        if (db.exec() && db.size() > 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1 recently updated, skipping.")
                                        .arg(m_info->name));

            if (m_cachefile.isEmpty())
            {
                QString locale_file(m_locale);
                locale_file.replace("/", "-");
                m_cachefile = QString("%1/cache_%2").arg(m_dir).arg(locale_file);
            }
            QFile cache(m_cachefile);
            if (cache.exists() && cache.open( QIODevice::ReadOnly ))
            {
                m_buffer = cache.readAll();
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
                LOG(VB_GENERAL, LOG_WARNING,
                    QString("No cachefile for %1, forcing update.")
                        .arg(m_info->name));
            }
        }
    }

    m_data.clear();
    QString program = "nice";
    QStringList args;
    args << m_info->program;
    args << "-u";
    args << (m_units == SI_UNITS ? "SI" : "ENG");

    if (!m_dir.isEmpty())
    {
        args << "-d";
        args << m_dir;
    }
    args << m_locale;

    uint flags = kMSRunShell | kMSStdOut | kMSBuffered | kMSRunBackground |
                 kMSDontDisableDrawing | kMSDontBlockInputDevs;
    m_ms = new MythSystem(program, args, flags);
    m_ms->SetDirectory(m_info->path);

    connect(m_ms, SIGNAL(finished()),  this, SLOT(processExit()));
    connect(m_ms, SIGNAL(error(uint)), this, SLOT(processExit(uint)));

    m_ms->Run(m_info->scriptTimeout);
}

void WeatherSource::updateTimeout()
{
    startUpdate();
    startUpdateTimer();
}

void WeatherSource::processExit(uint status)
{
    m_ms->disconnect(); // disconnects all signals

    if (status == GENERIC_EXIT_OK)
    {
        m_buffer = m_ms->ReadAll();
    }

    delete m_ms;
    m_ms = NULL;

    if (status != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("script exit status %1").arg(status));
        return;
    }

    if (m_buffer.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Script returned no data");
        return;
    }

    if (m_cachefile.isEmpty())
    {
        QString locale_file(m_locale);
        locale_file.replace("/", "-");
        m_cachefile = QString("%1/cache_%2").arg(m_dir).arg(locale_file);
    }
    QFile cache(m_cachefile);
    if (cache.open( QIODevice::WriteOnly ))
    {
        cache.write(m_buffer);
        cache.close();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unable to save data to cachefile: %1")
                .arg(m_cachefile));
    }

    processData();

    MSqlQuery db(MSqlQuery::InitCon());

    db.prepare("UPDATE weathersourcesettings "
               "SET updated = NOW() WHERE sourceid = :ID;");

    db.bindValue(":ID", getId());
    if (!db.exec())
    {
        MythDB::DBError("Updating weather source's last update time", db);
        return;
    }

    if (m_connectCnt)
    {
        emit newData(m_locale, m_units, m_data);
    }
}

void WeatherSource::processData()
{
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    QString unicode_buffer = codec->toUnicode(m_buffer);
    QStringList data = unicode_buffer.split('\n', QString::SkipEmptyParts);

    m_data.clear();

    for (int i = 0; i < data.size(); ++i)
    {
        QStringList temp = data[i].split("::", QString::SkipEmptyParts);
        if (temp.size() > 2)
            LOG(VB_GENERAL, LOG_ERR, "Error parsing script file, ignoring");
        if (temp.size() < 2)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unrecoverable error parsing script output %1")
                .arg(temp.size()));
            LOG(VB_GENERAL, LOG_ERR, QString("data[%1]: '%2'")
                .arg(i).arg(data[i]));
            return; // we don't emit signal
        }

        if (temp[1] != "---")
        {
            if (!m_data[temp[0]].isEmpty())
            {
                m_data[temp[0]].append("\n" + temp[1]);
            }
            else
                m_data[temp[0]] = temp[1];
        }
    }
}


// C++
#include <unistd.h>

// QT headers
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QTextCodec>
#else
#include <QStringConverter>
#endif

// MythTV headers
#include <libmyth/mythcontext.h>
#include <libmythbase/compat.h>
#include <libmythbase/exitcodes.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythsystemlegacy.h>

// MythWeather headers
#include "weatherScreen.h"
#include "weatherSource.h"

QStringList WeatherSource::ProbeTypes(const QString& workingDirectory,
                                      const QString& program)
{
    QStringList arguments("-t");
    const QString loc = QString("WeatherSource::ProbeTypes(%1 %2): ")
        .arg(program, arguments.join(" "));
    QStringList types;

    uint flags = kMSRunShell | kMSStdOut | 
                 kMSDontDisableDrawing | kMSDontBlockInputDevs;
    MythSystemLegacy ms(program, arguments, flags);
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

bool WeatherSource::ProbeTimeouts(const QString&  workingDirectory,
                                  const QString&  program,
                                  std::chrono::seconds &updateTimeout,
                                  std::chrono::seconds &scriptTimeout)
{
    QStringList arguments("-T");
    const QString loc = QString("WeatherSource::ProbeTimeouts(%1 %2): ")
        .arg(program, arguments.join(" "));

    updateTimeout = DEFAULT_UPDATE_TIMEOUT;
    scriptTimeout = DEFAULT_SCRIPT_TIMEOUT;

    uint flags = kMSRunShell | kMSStdOut | 
                 kMSDontDisableDrawing | kMSDontBlockInputDevs;
    MythSystemLegacy ms(program, arguments, flags);
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

    std::array<bool,2> isOK {};
    uint ut = temp[0].toUInt(isOK.data());
    uint st = temp[1].toUInt(&isOK[1]);
    if (!isOK[0] || !isOK[1])
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Invalid Script Output! '%1'").arg(lines[0]));
        return false;
    }

    updateTimeout = std::chrono::seconds(ut);
    scriptTimeout = std::chrono::seconds(st);

    return true;
}

bool WeatherSource::ProbeInfo(ScriptInfo &info)
{
    QStringList arguments("-v");

    const QString loc = QString("WeatherSource::ProbeInfo(%1 %2): ")
        .arg(info.program, arguments.join(" "));

    uint flags = kMSRunShell | kMSStdOut | 
                 kMSDontDisableDrawing | kMSDontBlockInputDevs;
    MythSystemLegacy ms(info.program, arguments, flags);
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
    if (!fi.isReadable() || !fi.isExecutable())
        return nullptr;

    ScriptInfo info;
    info.path = fi.absolutePath();
    info.program = fi.absoluteFilePath();

    if (!WeatherSource::ProbeInfo(info))
        return nullptr;

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
        return nullptr;
    }

    // the script exists in the db
    if (db.next())
    {
        info.id            = db.value(0).toInt();
        info.updateTimeout = std::chrono::seconds(db.value(2).toUInt());
        info.scriptTimeout = std::chrono::seconds(db.value(3).toUInt());

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
                return nullptr;
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
            return nullptr;
        }
        db.prepare(query);
        db.bindValue(":NAME", info.name);
        db.bindValue(":HOST", gCoreContext->GetHostName());
        db.bindValue(":UPDATETO", QString::number(info.updateTimeout.count()));
        db.bindValue(":RETTO", QString::number(info.scriptTimeout.count()));
        db.bindValue(":PATH", info.program);
        db.bindValue(":AUTHOR", info.author);
        db.bindValue(":VERSION", info.version);
        db.bindValue(":EMAIL", info.email);
        info.types = ProbeTypes(info.path, info.program);
        db.bindValue(":TYPES", info.types.join(","));
        if (!db.exec())
        {
            MythDB::DBError("Inserting weather source", db);
            return nullptr;
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
            return nullptr;
        }
        if (!db.next())
        {
            LOG(VB_GENERAL, LOG_ERR, "Error getting weather sourceid");
            return nullptr;
        }
        info.id = db.value(0).toInt();
    }

    return new ScriptInfo(info);
}

/**
 * Watch out, we store the parameter as a member variable, don't go deleting it,
 * that wouldn't be good.
 *
 * \param info is a required variable.
 */
WeatherSource::WeatherSource(ScriptInfo *info)
    : m_ready(info != nullptr),
      m_inuse(info != nullptr),
      m_info(info),
      m_updateTimer(new QTimer(this))
{
    QDir dir(GetConfDir());
    if (!dir.exists("MythWeather"))
        dir.mkdir("MythWeather");
    dir.cd("MythWeather");
    if (info != nullptr) {
        if (!dir.exists(info->name))
            dir.mkdir(info->name);
        dir.cd(info->name);
    }
    m_dir = dir.absolutePath();

    connect( m_updateTimer, &QTimer::timeout, this, &WeatherSource::updateTimeout);
}

WeatherSource::~WeatherSource()
{
    if (m_ms)
    {
        m_ms->Signal(kSignalKill);
        m_ms->Wait(5s);
        delete m_ms;
    }
    delete m_updateTimer;
}

void WeatherSource::connectScreen(WeatherScreen *ws)
{
    connect(this, &WeatherSource::newData,
            ws, &WeatherScreen::newData);
    ++m_connectCnt;

    if (!m_data.empty())
    {
        emit newData(m_locale, m_units, m_data);
    }
}

void WeatherSource::disconnectScreen(WeatherScreen *ws)
{
    disconnect(this, nullptr, ws, nullptr);
    --m_connectCnt;
}

QStringList WeatherSource::getLocationList(const QString &str)
{
    QString program = m_info->program;
    QStringList args;
    args << "-l";
    args << str;

    const QString loc = QString("WeatherSource::getLocationList(%1 %2): ")
        .arg(program, args.join(" "));

    uint flags = kMSRunShell | kMSStdOut | 
                 kMSDontDisableDrawing | kMSDontBlockInputDevs;
    MythSystemLegacy ms(program, args, flags);
    ms.SetDirectory(m_info->path);
    ms.Run();
    
    if (ms.Wait() != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Cannot run script");
        return {};
    }

    QStringList locs;
    QByteArray result = ms.ReadAll();
    QTextStream text(result);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    text.setCodec("UTF-8");
#else
    text.setEncoding(QStringConverter::Utf8);
#endif
    while (!text.atEnd())
    {
        QString tmp = text.readLine().trimmed();
        if (!tmp.isEmpty())
            locs << tmp;
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
            LOG(VB_GENERAL, LOG_NOTICE, QString("%1 recently updated, skipping.")
                                        .arg(m_info->name));

            if (m_cachefile.isEmpty())
            {
                QString locale_file(m_locale);
                locale_file.replace("/", "-");
                m_cachefile = QString("%1/cache_%2").arg(m_dir, locale_file);
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
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("No cachefile for %1, forcing update.")
                .arg(m_info->name));
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

    uint flags = kMSRunShell | kMSStdOut | kMSRunBackground |
                 kMSDontDisableDrawing | kMSDontBlockInputDevs;
    m_ms = new MythSystemLegacy(program, args, flags);
    m_ms->SetDirectory(m_info->path);

    connect(m_ms, &MythSystemLegacy::finished,
            this, qOverload<>(&WeatherSource::processExit));
    connect(m_ms, &MythSystemLegacy::error,
            this, qOverload<uint>(&WeatherSource::processExit));

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
    m_ms = nullptr;

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
        m_cachefile = QString("%1/cache_%2").arg(m_dir, locale_file);
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

void WeatherSource::processExit(void)
{
    processExit(GENERIC_EXIT_OK);
}

void WeatherSource::processData()
{
    QString unicode_buffer = QString::fromUtf8(m_buffer);
    QStringList data = unicode_buffer.split('\n', Qt::SkipEmptyParts);

    m_data.clear();

    for (int i = 0; i < data.size(); ++i)
    {
        QStringList temp = data[i].split("::", Qt::SkipEmptyParts);
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
            {
                m_data[temp[0]] = temp[1];
            }
        }
    }
}


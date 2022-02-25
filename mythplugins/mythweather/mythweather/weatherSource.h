#ifndef WEATHER_SOURCE_H
#define WEATHER_SOURCE_H

// Qt
#include <QFileInfo>
#include <QObject>
#include <QStringList>
#include <QTimer>

// MythTV
#include <libmythbase/mythsystemlegacy.h>

// MythWeather headers
#include "weatherUtils.h"

class WeatherScreen;

/*
 * Instance independent information about a script
 */
class ScriptInfo
{
  public:
    QString name;
    QString version;
    QString author;
    QString email;
    QStringList types;
    QString program;
    QString path;
    std::chrono::seconds scriptTimeout {DEFAULT_SCRIPT_TIMEOUT};
    std::chrono::seconds updateTimeout {DEFAULT_UPDATE_TIMEOUT};
    int id                     {0};
};

class WeatherSource : public QObject
{
    Q_OBJECT

  public:
    static ScriptInfo *ProbeScript(const QFileInfo &fi);
    static QStringList ProbeTypes(const QString&    workingDirectory,
                                  const QString&    program);
    static bool ProbeTimeouts(const QString&        workingDirectory,
                              const QString&        program,
                              std::chrono::seconds &updateTimeout,
                              std::chrono::seconds &scriptTimeout);
    static bool ProbeInfo(ScriptInfo &scriptInfo);

    explicit WeatherSource(ScriptInfo *info);
    ~WeatherSource() override;

    bool isReady() const { return m_ready; }
    QString getVersion() { return m_info->version; }
    QString getName() { return m_info->name; }
    QString getAuthor() { return m_info->author; }
    QString getEmail() { return m_info->email; }
    units_t getUnits() const { return m_units; }
    void setUnits(units_t units) { m_units = units; }
    QStringList getLocationList(const QString &str);
    void setLocale(const QString &locale) { m_locale = locale; }
    QString getLocale() { return m_locale; }

    void startUpdate(bool forceUpdate = false);

    std::chrono::seconds getScriptTimeout() { return m_info->scriptTimeout; }
    void setScriptTimeout(std::chrono::seconds timeout) { m_info->scriptTimeout = timeout; }

    std::chrono::seconds getUpdateTimeout() { return m_info->updateTimeout; }
    void setUpdateTimeout(std::chrono::seconds timeout) { m_info->updateTimeout = timeout; }

    void startUpdateTimer() { m_updateTimer->start(m_info->updateTimeout); }
    void stopUpdateTimer() { m_updateTimer->stop(); }

    bool inUse() const { return m_inuse; }
    void setInUse(bool inuse) { m_inuse = inuse; }

    int getId() { return m_info->id; }

    void connectScreen(WeatherScreen *ws);
    void disconnectScreen(WeatherScreen *ws);

  signals:
    void newData(QString, units_t,  DataMap);

  private slots:
    void processExit(uint status);
    void processExit();
    void updateTimeout();

  private:
    void processData();

    bool              m_ready {false};
    bool              m_inuse {false};
    ScriptInfo       *m_info  {nullptr};
    MythSystemLegacy *m_ms    {nullptr};
    QString m_dir;
    QString m_locale;
    QString m_cachefile;
    QByteArray m_buffer;
    units_t m_units           {SI_UNITS};
    QTimer *m_updateTimer     {nullptr};
    int     m_connectCnt      {0};
    DataMap m_data;
};

#endif /* WEATHER_SOURCE_H */

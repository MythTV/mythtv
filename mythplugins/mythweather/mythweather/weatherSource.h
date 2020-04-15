#ifndef WEATHER_SOURCE_H
#define WEATHER_SOURCE_H

#include <QStringList>
#include <QObject>
#include <QTimer>
#include <QFileInfo>
#include "mythsystemlegacy.h"

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
    unsigned int scriptTimeout {DEFAULT_SCRIPT_TIMEOUT};
    unsigned int updateTimeout {DEFAULT_UPDATE_TIMEOUT};
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
                              uint          &updateTimeout,
                              uint          &scriptTimeout);
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

    int getScriptTimeout() { return m_info->scriptTimeout; }
    void setScriptTimeout(int timeout) { m_info->scriptTimeout = timeout; }

    int getUpdateTimeout() { return m_info->updateTimeout; }
    void setUpdateTimeout(int timeout) { m_info->updateTimeout = timeout; }

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
    void processExit(uint status = 0);
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

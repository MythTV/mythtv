#ifndef __WEATHER_SOURCE_H__
#define __WEATHER_SOURCE_H__

#include <QStringList>
#include <QObject>
#include <QTimer>
#include <QFileInfo>
#include "mythsystem.h"

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
    unsigned int scriptTimeout;
    unsigned int updateTimeout;
    int id;
};

class WeatherSource : public QObject
{
    Q_OBJECT

  public:
    static ScriptInfo *ProbeScript(const QFileInfo &fi);
    static QStringList ProbeTypes(QString    workingDirectory,
                                  QString    program);
    static bool ProbeTimeouts(QString        workingDirectory,
                              QString        program,
                              uint          &updateTimeout,
                              uint          &scriptTimeout);
    static bool ProbeInfo(ScriptInfo &scriptInfo);

    WeatherSource(ScriptInfo *info);
    ~WeatherSource();

    bool isReady() { return m_ready; }
    QString getVersion() { return m_info->version; }
    QString getName() { return m_info->name; }
    QString getAuthor() { return m_info->author; }
    QString getEmail() { return m_info->email; }
    units_t getUnits() { return m_units; }
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

    bool inUse() { return m_inuse; }
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

    bool m_ready;
    bool m_inuse;
    ScriptInfo *m_info;
    MythSystem *m_ms;
    QString m_dir;
    QString m_locale;
    QString m_cachefile;
    QByteArray m_buffer;
    units_t m_units;
    QTimer *m_updateTimer;
    int m_connectCnt;
    DataMap m_data;
};

#endif

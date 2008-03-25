#ifndef __WEATHER_SOURCE_H__
#define __WEATHER_SOURCE_H__

#include <qstring.h>
#include <qstringlist.h>
#include <qobject.h>
#include <q3process.h>

#include <qtimer.h>

#include "defs.h"

class QFileInfo;
class WeatherScreen;

/*
 * Instance indpendent information about a script
 */
struct ScriptInfo
{
    QString name;
    QString version;
    QString author;
    QString email;
    QStringList types;
    QFileInfo *file;
    unsigned int scriptTimeout;
    unsigned int updateTimeout;
    int id;
};

class WeatherSource : public QObject
{
    Q_OBJECT

  public:
    static ScriptInfo *probeScript(const QFileInfo &fi);
    static QStringList probeTypes(Q3Process *proc);
    static bool probeTimeouts(Q3Process *proc, uint &updateTimeout,
                              uint &scriptTimeout);
    static bool probeInfo(Q3Process *proc, QString &name, QString &version,
                              QString &author, QString &email);

    WeatherSource(ScriptInfo *info);
    WeatherSource(const QString &filename);
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

    void startUpdate();
    bool isRunning() { return m_proc->isRunning(); }

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
    void killProcess();

  private slots:
    void readFromStdout();
    void processExit();
    void scriptTimeout();
    void updateTimeout();

  private:
    void processData();

    bool m_ready;
    bool m_inuse;
    ScriptInfo *m_info;
    Q3Process *m_proc;
    QString m_dir;
    QString m_locale;
    QString m_buffer;
    units_t m_units;
    QTimer *m_scriptTimer;
    QTimer *m_updateTimer;
    int m_connectCnt;
    DataMap m_data;
};

#endif

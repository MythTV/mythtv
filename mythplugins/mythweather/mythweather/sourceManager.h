#ifndef _SOURCEMANAGER_H_
#define _SOURCEMANAGER_H_

// QT headers
#include <qobject.h>
#include <q3intdict.h>
#include <qstringlist.h>
//Added by qt3to4:
#include <Q3PtrList>

// MythWeather headers
#include "weatherUtils.h"
#include "weatherSource.h"

class WeatherScreen;
struct ScriptInfo;

class SourceManager : public QObject
{
    Q_OBJECT

  public:
    SourceManager();
    WeatherSource *needSourceFor(int id, const QString &loc, units_t units);
    QStringList getLocationList(ScriptInfo *si, const QString &str);
    void startTimers();
    void stopTimers();
    void doUpdate();
    bool findPossibleSources(QStringList types, Q3PtrList<ScriptInfo> &sources);
    void clearSources();
    bool findScripts();
    bool findScriptsDB();
    void setupSources();
    bool connectScreen(uint id, WeatherScreen *screen);
    bool disconnectScreen(WeatherScreen *screen);
    ScriptInfo *getSourceByName(const QString &name);

  private slots:
    void timeout(void) {}

  private:
    Q3PtrList<ScriptInfo> m_scripts; //all scripts
    Q3PtrList<WeatherSource> m_sources; //in-use scripts
    Q3IntDict<WeatherSource> m_sourcemap;
    units_t m_units;
    void recurseDirs(QDir dir);
};

#endif

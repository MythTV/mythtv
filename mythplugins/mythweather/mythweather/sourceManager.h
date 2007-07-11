
#ifndef _SOURCEMANAGER_H_
#define _SOURCEMANAGER_H_
#include <qthread.h>
#include <qstringlist.h>
#include <qregexp.h>
#include "weatherSource.h"
#include "defs.h"

class WeatherSource;
class WeatherScreen;

class SourceManager : public QObject {
    Q_OBJECT

    public: 
        SourceManager();
        WeatherSource*  needSourceFor(int, QString, units_t);
        QStringList getLocationList(struct ScriptInfo*, QString);
        void setUnits(units_t);
        void startTimers();
        void stopTimers();
        void doUpdate();
        QString getDataItem(QString);
        bool findPossibleSources(QStringList, QPtrList<struct ScriptInfo>&);
        void clearSources();
        bool findScripts();
        bool findScriptsDB();
        void setupSources();
        void connectScreen(uint, WeatherScreen*);
        void disconnectScreen(WeatherScreen*);
        struct ScriptInfo* getSourceByName(QString);
        
    private slots:
        void timeout();
    private:
        QPtrList<struct ScriptInfo> m_scripts; //all scripts
        QPtrList<WeatherSource> m_sources; //in-use scripts
        QIntDict<WeatherSource> m_sourcemap;
        units_t m_units;
};
#endif


#ifndef CHANNELSETTINGS_H
#define CHANNELSETTINGS_H

#include <qwidget.h>
#include <qsqldatabase.h>

#include <cstdlib>

#include "settings.h"
#include "mythwidgets.h"
#include "mythwizard.h"
#include "mythcontext.h"

class ChannelID: public IntegerSetting, public TransientStorage {
public:
    ChannelID(QString field = "chanid", QString table = "channel"):
      field(field), table(table) {
        setVisible(false);
    };

    QWidget* configWidget(ConfigurationGroup* cg, QWidget* widget,
                          const char* widgetName = 0) {
        (void)cg; (void)widget; (void)widgetName;
        return NULL;
    };

    void load(QSqlDatabase* _db) { db = _db; };
    void save(QSqlDatabase* db) {
        if (intValue() == 0) {
            setValue(findHighest());

            QSqlQuery query;
            
            query = db->exec(QString("SELECT %1 FROM %2 WHERE %3='%4'")
                             .arg(field).arg(table).arg(field).arg(getValue()));

            if (!query.isActive())
                MythContext::DBError("ChannelID::save", query);

            if (query.numRowsAffected())
                return;

            query = db->exec(QString("INSERT INTO %1 SET %2=%3")
                             .arg(table).arg(field).arg(getValue()));

            if (!query.isActive())
                MythContext::DBError("ChannelID::save", query);

            if (query.numRowsAffected() != 1)
                cerr << "ChannelID:Failed to insert into: " << table << endl;
        }
    };

    int findHighest(int floor = 1000)
    {
        int tmpfloor = floor;
        QSqlQuery query;
        
        query = db->exec(QString("SELECT %1 FROM %2")
                         .arg(field).arg(table));
        if (!query.isActive()) {
            MythContext::DBError("finding highest id", query);
            return floor;
        }

        if (query.numRowsAffected() > 0)
            while (query.next())
                if (tmpfloor <= query.value(0).toInt())
                    tmpfloor = query.value(0).toInt() + 1;

        return floor<tmpfloor?tmpfloor:floor;
    };

    const QString& getField(void) const {
        return field;
    };

protected:
    QString field,table;
    QSqlDatabase* db;
};

class CSetting: public SimpleDBStorage {
protected:
    CSetting(const ChannelID& id, QString name):
      SimpleDBStorage("channel", name), id(id) {
        setName(name);
    };

    virtual QString setClause(void);
    virtual QString whereClause(void);

    const ChannelID& id;
};

class DvbSetting: public SimpleDBStorage {
protected:
    DvbSetting(const ChannelID& id, QString name):
      SimpleDBStorage("dvb_channel", name), id(id) {
        setName(name);
    };

    virtual QString setClause(void);
    virtual QString whereClause(void);

    const ChannelID& id;
};

class DvbPidSetting: public LineEditSetting, public DBStorage {
public:
    DvbPidSetting(const ChannelID& id, QString name):
      LineEditSetting(), DBStorage("dvb_pids", name), id(id) {
        setHelpText(QObject::tr("A comma separated list of pids for each type. "
                    "Note that currently MythTV only supports recording and "
                    "not playback of multiple audio or video pids. It does not "                    "support showing teletext or subtitles either, but these "
                    "do not crash the player as audio and video might."));
    };

    void load(QSqlDatabase* db);
    void save(QSqlDatabase* db);
private:
    const ChannelID& id;
};

class ChannelOptionsCommon: public VerticalConfigurationGroup {
public:
    ChannelOptionsCommon(const ChannelID& id);
};

class ChannelOptionsV4L: public VerticalConfigurationGroup {
public:
    ChannelOptionsV4L(const ChannelID& id);
};

class DvbFrequency;
class DvbSymbolrate;
class DvbSatellite;
class DvbPolarity;
class DvbFec;
class DvbModulation;
class DvbInversion;
class DvbBandwidth;
class DvbConstellation;
class DvbCoderateLP;
class DvbCoderateHP;
class DvbTransmissionMode;
class DvbGuardInterval;
class DvbHierarchy;

class ChannelOptionsDVB: public HorizontalConfigurationGroup {
    Q_OBJECT
public:
    ChannelOptionsDVB(const ChannelID& id);

    QString getFrequency();
    QString getSymbolrate();
    QString getPolarity();
    QString getFec();
    QString getModulation();
    QString getInversion();
    
    QString getBandwidth();
    QString getConstellation();
    QString getCodeRateLP();
    QString getCodeRateHP();
    QString getTransmissionMode();
    QString getGuardInterval();
    QString getHierarchy();

protected:
    const ChannelID& id;

private:
    QSqlDatabase* db;

    DvbFrequency* frequency;
    DvbSymbolrate* symbolrate;
    DvbSatellite* satellite;
    DvbPolarity* polarity;
    DvbFec* fec;
    DvbModulation* modulation;
    DvbInversion* inversion;

    DvbBandwidth* bandwidth;
    DvbConstellation* constellation;
    DvbCoderateLP* coderate_lp;
    DvbCoderateHP* coderate_hp;
    DvbTransmissionMode* trans_mode;
    DvbGuardInterval* guard_interval;
    DvbHierarchy* hierarchy;
};

class ChannelOptionsDVBPids: public HorizontalConfigurationGroup {
public:
    ChannelOptionsDVBPids(const ChannelID& id);
};

/**************************************/

class DvbSatelliteConfiguration: public ConfigurationWizard {
public:

};

#endif //CHANNELEDITOR_H

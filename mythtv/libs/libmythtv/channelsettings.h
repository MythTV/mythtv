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

class OnAirGuide;
class XmltvID;

class ChannelOptionsCommon: public VerticalConfigurationGroup {
    Q_OBJECT
public:
    ChannelOptionsCommon(const ChannelID& id);
    void load(QSqlDatabase* _db);
public slots:
    void onAirGuideChanged(bool);
    void sourceChanged(const QString&);

protected:
    OnAirGuide *onairguide;
    XmltvID    *xmltvID;
    QSqlDatabase *db;
};

class ChannelOptionsV4L: public VerticalConfigurationGroup {
public:
    ChannelOptionsV4L(const ChannelID& id);
};

#endif //CHANNELEDITOR_H

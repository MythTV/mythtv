#ifndef CHANNELSETTINGS_H
#define CHANNELSETTINGS_H

#include <qwidget.h>
#include <qsqldatabase.h>

#include <cstdlib>

#include "settings.h"
#include "mythwidgets.h"
#include "mythwizard.h"
#include "mythcontext.h"
#include "mythdbcon.h"

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

    void load() { };
    void save(QString table) 
    {
        if (intValue() == 0) {
            setValue(findHighest());

            MSqlQuery query(MSqlQuery::InitCon());
            
            QString querystr = QString("SELECT %1 FROM %2 WHERE %3='%4'")
                             .arg(field).arg(table).arg(field).arg(getValue());
            query.prepare(querystr);

            if (!query.exec() && !query.isActive())
                MythContext::DBError("ChannelID::save", query);

            if (query.size())
                return;

            querystr = QString("INSERT INTO %1 (%2) VALUES ('%3')")
                             .arg(table).arg(field).arg(getValue());
            query.prepare(querystr);

            if (!query.exec() || !query.isActive())
                MythContext::DBError("ChannelID::save", query);

            if (query.numRowsAffected() != 1)
                cerr << "ChannelID:Failed to insert into: " << table << endl;
        }
    }
    void save() 
    {
        save(table);
    }

    int findHighest(int floor = 1000)
    {
        int tmpfloor = floor;
        MSqlQuery query(MSqlQuery::InitCon());
        
        QString querystr = QString("SELECT %1 FROM %2")
                                .arg(field).arg(table);
        query.prepare(querystr);

        if (!query.exec() || !query.isActive()) 
        {
            MythContext::DBError("finding highest id", query);
            return floor;
        }

        if (query.size() > 0)
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
    void load();
public slots:
    void onAirGuideChanged(bool);
    void sourceChanged(const QString&);

protected:
    OnAirGuide *onairguide;
    XmltvID    *xmltvID;
};

class ChannelOptionsV4L: public VerticalConfigurationGroup {
public:
    ChannelOptionsV4L(const ChannelID& id);
};

#endif //CHANNELEDITOR_H

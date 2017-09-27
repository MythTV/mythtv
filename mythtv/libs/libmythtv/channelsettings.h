#ifndef CHANNELSETTINGS_H
#define CHANNELSETTINGS_H

// ANSI C
#include <cstdlib>

// C++
#include <vector>

// Qt
#include <QString>

// MythTV
#include "mythtvexp.h"
#include "standardsettings.h"
#include "mythwidgets.h"
#include "mythdb.h"
#include "mythlogging.h"

class QWidget;

class ChannelID : public GroupSetting
{
  public:
    ChannelID(QString _field = "chanid", QString _table = "channel") :
        field(_field), table(_table)
    {
        setVisible(false);
    }

    void Save(void)
    {
        if (getValue().toInt() == 0) {
            setValue(findHighest());

            MSqlQuery query(MSqlQuery::InitCon());

            QString querystr = QString("SELECT %1 FROM %2 WHERE %3='%4'")
                             .arg(field).arg(table).arg(field).arg(getValue());
            query.prepare(querystr);

            if (!query.exec() && !query.isActive())
                MythDB::DBError("ChannelID::save", query);

            if (query.size())
                return;

            querystr = QString("INSERT INTO %1 (%2) VALUES ('%3')")
                             .arg(table).arg(field).arg(getValue());
            query.prepare(querystr);

            if (!query.exec() || !query.isActive())
                MythDB::DBError("ChannelID::save", query);

            if (query.numRowsAffected() != 1)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("ChannelID, Error: ") +
                        QString("Failed to insert into: %1").arg(table));
            }
        }
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
            MythDB::DBError("finding highest id", query);
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

class ChannelDBStorage : public SimpleDBStorage
{
  public:
    ChannelDBStorage(StorageUser *_user, const ChannelID &_id, QString _name) :
        SimpleDBStorage(_user, "channel", _name), id(_id) { }

    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;

    const ChannelID& id;
};

class OnAirGuide;
class XmltvID;
class Freqid;

class MTV_PUBLIC ChannelOptionsCommon: public GroupSetting
{
    Q_OBJECT

  public:
    ChannelOptionsCommon(const ChannelID &id,
        uint default_sourceid,  bool add_freqid);

  public slots:
    void onAirGuideChanged(bool);
    void sourceChanged(const QString&);

  protected:
    OnAirGuide *onairguide;
    XmltvID    *xmltvID;
    Freqid     *freqid;
};

class MTV_PUBLIC ChannelOptionsFilters: public GroupSetting
{
  public:
    explicit ChannelOptionsFilters(const ChannelID& id);
};

class MTV_PUBLIC ChannelOptionsV4L: public GroupSetting
{
  public:
    explicit ChannelOptionsV4L(const ChannelID& id);
};

class MTV_PUBLIC ChannelOptionsRawTS: public GroupSetting
{
  public:
    explicit ChannelOptionsRawTS(const ChannelID &id);

    virtual void Load(void);
    virtual void Save(void);
    virtual void Save(QString /*destination*/) { Save(); }

  private:
    const ChannelID &cid;

    std::vector<TransTextEditSetting*> pids;
    std::vector<TransMythUIComboBoxSetting*> sids;
    std::vector<TransMythUICheckBoxSetting*> pcrs;

    static const uint kMaxPIDs = 10;
};

class MTV_PUBLIC ChannelTVFormat :
    public MythUIComboBoxSetting
{
  public:
    explicit ChannelTVFormat(const ChannelID &id);

    static QStringList GetFormats(void);
};

#endif //CHANNELEDITOR_H

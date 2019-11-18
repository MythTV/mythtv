#ifndef CHANNELSETTINGS_H
#define CHANNELSETTINGS_H

// C++
#include <cstdlib>
#include <vector>

// Qt
#include <QString>

// MythTV
#include "mythtvexp.h"
#include "standardsettings.h"
#include "mythdb.h"
#include "mythlogging.h"

class QWidget;

class ChannelID : public GroupSetting
{
  public:
    ChannelID(const QString& field = "chanid",
              const QString& table = "channel") :
        m_field(field), m_table(table)
    {
        setVisible(false);
    }

    void Save(void) override // StandardSetting
    {
        if (getValue().toInt() == 0) {
            setValue(findHighest());

            MSqlQuery query(MSqlQuery::InitCon());

            QString querystr = QString("SELECT %1 FROM %2 WHERE %3='%4'")
                             .arg(m_field).arg(m_table).arg(m_field).arg(getValue());
            query.prepare(querystr);

            if (!query.exec() && !query.isActive())
                MythDB::DBError("ChannelID::save", query);

            if (query.size())
                return;

            querystr = QString("INSERT INTO %1 (%2) VALUES ('%3')")
                             .arg(m_table).arg(m_field).arg(getValue());
            query.prepare(querystr);

            if (!query.exec() || !query.isActive())
                MythDB::DBError("ChannelID::save", query);

            if (query.numRowsAffected() != 1)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("ChannelID, Error: ") +
                        QString("Failed to insert into: %1").arg(m_table));
            }
        }
    }

    int findHighest(int floor = 1000)
    {
        int tmpfloor = floor;
        MSqlQuery query(MSqlQuery::InitCon());

        QString querystr = QString("SELECT %1 FROM %2")
                                .arg(m_field).arg(m_table);
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
        return m_field;
    };

protected:
    QString m_field;
    QString m_table;
};

class ChannelDBStorage : public SimpleDBStorage
{
  public:
    ChannelDBStorage(StorageUser *_user, const ChannelID &_id, const QString& _name) :
        SimpleDBStorage(_user, "channel", _name), m_id(_id) { }

    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage

    const ChannelID& m_id;
};

class OnAirGuide;
class XmltvID;
class Freqid;
class TransportID_CO;
class Frequency_CO;

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
    OnAirGuide     *m_onairguide  {nullptr};
    XmltvID        *m_xmltvID     {nullptr};
    Freqid         *m_freqid      {nullptr};
    TransportID_CO *m_transportid {nullptr};
    Frequency_CO   *m_frequency   {nullptr};
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

    void Load(void) override; // StandardSetting
    void Save(void) override; // StandardSetting
    virtual void Save(QString /*destination*/) { Save(); }

  private:
    const ChannelID &m_cid;

    std::vector<TransTextEditSetting*>       m_pids;
    std::vector<TransMythUIComboBoxSetting*> m_sids;
    std::vector<TransMythUICheckBoxSetting*> m_pcrs;

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

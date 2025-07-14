#ifndef CHANNELSETTINGS_H
#define CHANNELSETTINGS_H

// C++
#include <cstdlib>
#include <utility>
#include <vector>

// Qt
#include <QString>

// MythTV
#include "libmythui/standardsettings.h"
#include "libmythbase/mythdb.h"
#include "mythtvexp.h"

class QWidget;

class MTV_PUBLIC ChannelID : public GroupSetting
{
  public:
    explicit ChannelID(QString  field = "chanid",
              QString  table = "channel") :
        m_field(std::move(field)), m_table(std::move(table))
    {
        setVisible(false);
    }

    void Save() override; // StandardSetting

    int findHighest(int floor = 1000)
    {
        int tmpfloor = floor;
        MSqlQuery query(MSqlQuery::InitCon());

        QString querystr = QString("SELECT %1 FROM %2")
                                .arg(m_field, m_table);
        query.prepare(querystr);

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("finding highest id", query);
            return floor;
        }

        if (query.size() > 0)
        {
            while (query.next())
                if (tmpfloor <= query.value(0).toInt())
                    tmpfloor = query.value(0).toInt() + 1;
        }
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

class IPTVChannelDBStorage : public SimpleDBStorage
{
  public:
    IPTVChannelDBStorage(StorageUser *_user, const ChannelID &_id, const QString& _name) :
        SimpleDBStorage(_user, "iptv_channel", _name), m_id(_id) { }

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
    static void onAirGuideChanged(bool fValue);
    void sourceChanged(const QString &sourceid);

  protected:
    OnAirGuide     *m_onAirGuide  {nullptr};
    XmltvID        *m_xmltvID     {nullptr};
    Freqid         *m_freqId      {nullptr};
    TransportID_CO *m_transportId {nullptr};
    Frequency_CO   *m_frequency   {nullptr};
};

class MTV_PUBLIC ChannelOptionsFilters: public GroupSetting
{
  public:
    explicit ChannelOptionsFilters(const ChannelID& id);
};

class MTV_PUBLIC ChannelOptionsIPTV: public GroupSetting
{
  public:
    explicit ChannelOptionsIPTV(const ChannelID& id);
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
    virtual void Save(const QString& /*destination*/) { Save(); }

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
    ~ChannelTVFormat() override;

    static QStringList GetFormats(void);
};

#endif //CHANNELEDITOR_H

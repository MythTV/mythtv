#ifndef VIDEOSOURCE_H
#define VIDEOSOURCE_H

#include <utility>
#include <vector>

// MythTV headers
#include "libmythbase/mythconfig.h"
#include "libmythui/standardsettings.h"
#include "libmythbase/mthread.h"

#include "mythtvexp.h"

class SignalTimeout;
class ChannelTimeout;
class UseEIT;
class VideoSource;
class CaptureCard;
class VBIDevice;
class CardInput;
class CardID;
class InputName;
class SourceID;
class DeviceTree;
class DiSEqCDevTree;
class DiSEqCDevSettings;
class InputGroup;

static inline bool is_grabber_external(const QString &grabber)
{
    return !(grabber == "eitonly" ||
             grabber == "/bin/true");
}

static inline bool is_grabber_labs([[maybe_unused]] const QString &grabber)
{
    return false;
}

class VideoSourceDBStorage : public SimpleDBStorage
{
  public:
    VideoSourceDBStorage(StorageUser       *_user,
                         const VideoSource &_parent,
                         const QString     &name) :
        SimpleDBStorage(_user, "videosource", name), m_parent(_parent)
    {
    }

  protected:
    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage

    const VideoSource& m_parent;
};

class VideoSourceSelector : public TransMythUIComboBoxSetting
{
    Q_OBJECT

  public:
    VideoSourceSelector(uint    _initial_sourceid,
                        QString _card_types,
                        bool    _must_have_mplexid);

    void Load(void) override; // StandardSetting

    uint GetSourceID(void) const { return getValue().toUInt(); }

  private:
    uint    m_initialSourceId;
    QString m_cardTypes;
    bool    m_mustHaveMplexId;
};

class VideoSourceShow : public GroupSetting
{
    Q_OBJECT

  public:
    explicit VideoSourceShow(uint _initial_sourceid);

    void Load(void) override; // StandardSetting

    uint GetSourceID(void) const { return getValue().toUInt(); }

  private:
    uint    m_initialSourceId;
};

class FreqTableSelector :
    public MythUIComboBoxSetting
{
    Q_OBJECT
public:
    explicit FreqTableSelector(const VideoSource& parent);
    ~FreqTableSelector() override;
protected:
    QString m_freq;
};

class TransFreqTableSelector : public TransMythUIComboBoxSetting
{
  public:
    explicit TransFreqTableSelector(uint sourceid);

    void Load(void) override; // StandardSetting

    void Save(void) override; // StandardSetting
    virtual void Save(const QString& /*destination*/) { Save(); }

    void SetSourceID(uint sourceid);

  private:
    uint    m_sourceId;
    QString m_loadedFreqTable;
};

class XMLTV_generic_config: public GroupSetting
{
    Q_OBJECT

  public:
    XMLTV_generic_config(const VideoSource& _parent, const QString& _grabber,
                         StandardSetting *_setting);

    void Save(void) override; // StandardSetting
    virtual void Save(const QString& /*destination*/) { Save(); }

  public slots:
    void RunConfig(void);

  protected:
    const VideoSource &m_parent;
    QString            m_grabber;
    QStringList        m_grabberArgs;
};

class EITOnly_config: public GroupSetting
{
public:
    EITOnly_config(const VideoSource& _parent, StandardSetting *_setting);

    void Save(void) override; // StandardSetting
    virtual void Save(const QString& /*destination*/) { Save(); }

protected:
    UseEIT *m_useEit {nullptr};
};

class NoGrabber_config: public GroupSetting
{
public:
    explicit NoGrabber_config(const VideoSource& _parent);

    void Save(void) override; // StandardSetting
    virtual void Save(const QString& /*destination*/) { Save(); }

protected:
    UseEIT *m_useEit {nullptr};
};

class IdSetting : public AutoIncrementSetting {
public:
    IdSetting(const QString &table, const QString &setting):
        AutoIncrementSetting(table, setting)
    {
        setVisible(false);
    }

    int intValue() { return getValue().toInt(); }
    void setValue(int value) override // StandardSetting
        { setValue(QString::number(value)); }
    using StandardSetting::setValue;
};

class VideoSource : public GroupSetting {
    Q_OBJECT

  public:
    VideoSource();

    int getSourceID(void) const { return m_id->intValue(); };

    void loadByID(int id);

    static void fillSelections(GroupSetting* setting);
    static void fillSelections(MythUIComboBoxSetting* setting);
    static QString idToName(int id);

    QString getSourceName(void) const { return m_name->getValue(); };

    void Load(void) override // StandardSetting
    {
        GroupSetting::Load();
    }

    void Save(void) override // StandardSetting
    {
        if (m_name)
            GroupSetting::Save();
    }
    bool canDelete(void) override; // GroupSetting
    void deleteEntry(void) override; // GroupSetting

  private:
    class ID: public IdSetting
    {
      public:
        ID() : IdSetting("videosource", "sourceid")
        {
        };
    };

    class Name : public MythUITextEditSetting
    {
      public:
        explicit Name(const VideoSource &parent) :
            MythUITextEditSetting(new VideoSourceDBStorage(this, parent, "name"))
        {
            setLabel(QObject::tr("Video source name"));
        }

        ~Name() override
        {
            delete GetStorage();
        }
    };

private:
    ID   *m_id   {nullptr};
    Name *m_name {nullptr};
};

class CaptureCardDBStorage : public SimpleDBStorage
{
  public:
    CaptureCardDBStorage(StorageUser       *_user,
                         const CaptureCard &_parent,
                         const QString     &_name) :
        SimpleDBStorage(_user, "capturecard", _name), m_parent(_parent)
    {
    }


protected:
    int getCardID(void) const;
    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage
private:
    const CaptureCard& m_parent;
};

class CaptureCardComboBoxSetting : public MythUIComboBoxSetting
{
  public:
    CaptureCardComboBoxSetting(const CaptureCard &parent,
                               bool rw,
                               const QString &setting) :
        MythUIComboBoxSetting(new CaptureCardDBStorage(this, parent, setting),
                              rw)
    {
    }

    ~CaptureCardComboBoxSetting() override
    {
        delete GetStorage();
    }
};

class TunerCardAudioInput : public CaptureCardComboBoxSetting
{
    Q_OBJECT
  public:
    explicit TunerCardAudioInput(const CaptureCard &parent,
                        QString dev  = QString(),
                        QString type = QString());

  public slots:
    int fillSelections(const QString &device);

  private:
    QString m_lastDevice;
    QString m_lastCardType;
};

class EmptyAudioDevice : public MythUITextEditSetting
{
    Q_OBJECT
  public:
    explicit EmptyAudioDevice(const CaptureCard &parent) :
        MythUITextEditSetting(new CaptureCardDBStorage(this, parent,
                                                       "audiodevice"))
    {
        setVisible(false);
    }

    ~EmptyAudioDevice() override
    {
        delete GetStorage();
    }

    void Save(void) override // StandardSetting
    {
        GetStorage()->SetSaveRequired();
        setValue("");
        GetStorage()->Save();
    }
    void Save(const QString& destination)
    {
        GetStorage()->SetSaveRequired();
        setValue("");
        GetStorage()->Save(destination);
    }
};

class EmptyVBIDevice : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    explicit EmptyVBIDevice(const CaptureCard &parent) :
        MythUITextEditSetting(new CaptureCardDBStorage(this, parent, "vbidevice"))
    {
        setVisible(false);
    };

    ~EmptyVBIDevice() override
    {
        delete GetStorage();
    }

    void Save(void) override // StandardSetting
    {
        GetStorage()->SetSaveRequired();
        setValue("");
        GetStorage()->Save();
    }
    void Save(const QString& destination)
    {
        GetStorage()->SetSaveRequired();
        setValue("");
        GetStorage()->Save(destination);
    }
};

class CardType : public CaptureCardComboBoxSetting
{
public:
    explicit CardType(const CaptureCard& parent);
    static void fillSelections(MythUIComboBoxSetting* setting);
};

#if CONFIG_HDHOMERUN

class UseHDHomeRunDevice;
class HDHomeRunDevice
{
  public:
    QString m_deviceId;
    QString m_model;
    QString m_cardIp;
    UseHDHomeRunDevice *m_checkbox {nullptr};
};

using HDHomeRunDeviceList = QMap<QString, HDHomeRunDevice>;

class HDHomeRunDeviceID;
class HDHomeRunConfigurationGroup : public GroupSetting
{
    Q_OBJECT

    friend class HDHomeRunExtra;

  public:
    HDHomeRunConfigurationGroup(CaptureCard &parent, CardType &cardtype);
    void SetDeviceCheckBoxes(const QString& devices);
    QString GetDeviceCheckBoxes(void);

  private:
    void FillDeviceList(void);

  private:
    CaptureCard           &m_parent;
    HDHomeRunDeviceID     *m_deviceId {nullptr};
    HDHomeRunDeviceList    m_deviceList;
};

class HDHomeRunDeviceID : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    HDHomeRunDeviceID(const CaptureCard &parent,
                      HDHomeRunConfigurationGroup &_group);
    ~HDHomeRunDeviceID() override;
    void Load(void) override; // StandardSetting
    void Save(void) override; // StandardSetting

  private:
    HDHomeRunConfigurationGroup &m_group;
};
#endif  // CONFIG_HDHOMERUN

#if CONFIG_SATIP

class SatIPDevice
{
  public:
    QString m_mythDeviceId;
    QString m_deviceId;
    QString m_friendlyName;
    QString m_cardIP;
    QString m_tunerNo;
    QString m_tunerType;
    bool    m_inUse      {false};
    bool    m_discovered {false};
};

using SatIPDeviceList = QMap<QString, SatIPDevice>;

class SatIPDeviceIDList;
class SatIPDeviceID;
class SatIPDeviceAttribute;
class SatIPConfigurationGroup : public GroupSetting
{
    Q_OBJECT

  public:
    SatIPConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  private:
    void FillDeviceList(void);

  private:
    CaptureCard           &m_parent;
    SatIPDeviceIDList     *m_deviceIdList  {nullptr};
    SatIPDeviceID         *m_deviceId      {nullptr};
    SatIPDeviceAttribute  *m_friendlyName  {nullptr};
    SatIPDeviceAttribute  *m_tunerType     {nullptr};
    SatIPDeviceAttribute  *m_tunerIndex    {nullptr};
    SatIPDeviceList        m_deviceList;
};

class SatIPDeviceIDList : public TransMythUIComboBoxSetting
{
    Q_OBJECT

  public:
    SatIPDeviceIDList(SatIPDeviceID *deviceId,
                      SatIPDeviceAttribute *friendlyName,
                      SatIPDeviceAttribute *tunerType,
                      SatIPDeviceAttribute *tunerIndex,
                      SatIPDeviceList *deviceList,
                      const CaptureCard &parent);

    void fillSelections(const QString &current);

    void Load(void) override; // StandardSetting

  public slots:
    void UpdateDevices(const QString& /*v*/);

  signals:
    void NewTuner(const QString&);

  private:
    SatIPDeviceID        *m_deviceId;
    SatIPDeviceAttribute *m_friendlyName;
    SatIPDeviceAttribute *m_tunerType;
    SatIPDeviceAttribute *m_tunerIndex;
    SatIPDeviceList      *m_deviceList;
    const CaptureCard    &m_parent;
};

class SatIPDeviceID : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    explicit SatIPDeviceID(const CaptureCard &parent);
    ~SatIPDeviceID() override;

    void Load(void) override; // StandardSetting

  public slots:
    void SetTuner(const QString& /*tuner*/);

  private:
    const CaptureCard &m_parent;
};

class SatIPDeviceAttribute : public GroupSetting
{
    Q_OBJECT

  public:
    SatIPDeviceAttribute(const QString& label,
                         const QString& helpText);
};
#endif // CONFIG_SATIP

class VBoxDevice
{
  public:
    QString m_mythDeviceId;
    QString m_deviceId;
    QString m_desc;
    QString m_cardIp;
    QString m_tunerNo;
    QString m_tunerType;
    bool    m_inUse      {false};
    bool    m_discovered {false};
};

using VBoxDeviceList = QMap<QString, VBoxDevice>;

class VBoxDeviceIDList;
class VBoxDeviceID;
class VBoxIP;
class VBoxTunerIndex;
class VBoxConfigurationGroup : public GroupSetting
{
    Q_OBJECT

  public:
    VBoxConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  private:
    void FillDeviceList(void);

  private:
    CaptureCard       &m_parent;
    StandardSetting   *m_desc         {nullptr};
    VBoxDeviceIDList  *m_deviceIdList {nullptr};
    VBoxDeviceID      *m_deviceId     {nullptr};
    VBoxIP            *m_cardIp       {nullptr};
    VBoxTunerIndex    *m_cardTuner    {nullptr};
    VBoxDeviceList     m_deviceList;
};

class V4LConfigurationGroup : public GroupSetting
{
    Q_OBJECT

  public:
    V4LConfigurationGroup(CaptureCard &parent, CardType &cardtype, const QString &inputtype);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard          &m_parent;
    GroupSetting         *m_cardInfo {nullptr};
    VBIDevice            *m_vbiDev   {nullptr};
};

class VideoDevice;
class VBIDevice;

class MPEGConfigurationGroup: public GroupSetting
{
   Q_OBJECT

  public:
    MPEGConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard          &m_parent;
    VideoDevice          *m_device    {nullptr};
    VBIDevice            *m_vbiDevice {nullptr};
    GroupSetting         *m_cardInfo  {nullptr};
};

class HDPVRConfigurationGroup: public GroupSetting
{
   Q_OBJECT

  public:
    HDPVRConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard         &m_parent;
    GroupSetting        *m_cardInfo   {nullptr};
    TunerCardAudioInput *m_audioInput {nullptr};
    VBIDevice           *m_vbiDevice  {nullptr};
};

class ASIDevice;

class V4L2encGroup: public GroupSetting
{
    Q_OBJECT

  public:
    V4L2encGroup(CaptureCard& parent, CardType& cardType);

  private:
    CaptureCard          &m_parent;
    GroupSetting         *m_cardInfo {nullptr};
    VideoDevice          *m_device   {nullptr};

    QString               m_driverName;

  protected slots:
    void probeCard(const QString &device);
};

class ASIConfigurationGroup: public GroupSetting
{
   Q_OBJECT

  public:
    ASIConfigurationGroup(CaptureCard &parent, CardType &cardType);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard          &m_parent;
    ASIDevice            *m_device  {nullptr};
    TransTextEditSetting *m_cardInfo {nullptr};
};

class ImportConfigurationGroup: public GroupSetting
{
   Q_OBJECT

  public:
    ImportConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard  &m_parent;
    GroupSetting *m_info {nullptr};
    GroupSetting *m_size {nullptr};
};

class DemoConfigurationGroup: public GroupSetting
{
   Q_OBJECT

  public:
    DemoConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard  &m_parent;
    GroupSetting *m_info {nullptr};
    GroupSetting *m_size {nullptr};
};

#if !defined( _WIN32 )
class ExternalConfigurationGroup: public GroupSetting
{
   Q_OBJECT

  public:
    ExternalConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  public slots:
    void probeApp(const QString & path);

  private:
    CaptureCard  &m_parent;
    GroupSetting *m_info {nullptr};
};
#endif

class DVBCardNum;
class DVBCardName;
class DVBCardType;
class DVBTuningDelay;

class DVBConfigurationGroup : public GroupSetting
{
    Q_OBJECT

    friend class DVBExtra;

  public:
    DVBConfigurationGroup(CaptureCard& a_parent, CardType& cardType);
    ~DVBConfigurationGroup() override;

    void Load(void) override; // StandardSetting

    void Save(void) override; // StandardSetting

  public slots:
    void probeCard(const QString& videodevice);
    void reloadDiseqcTree(const QString &device);

  private:
    CaptureCard                  &m_parent;

    DVBCardNum                   *m_cardNum        {nullptr};
    DVBCardName                  *m_cardName       {nullptr};
    DVBCardType                  *m_cardType       {nullptr};
    SignalTimeout                *m_signalTimeout  {nullptr};
    ChannelTimeout               *m_channelTimeout {nullptr};
    DVBTuningDelay               *m_tuningDelay    {nullptr};
    DiSEqCDevTree                *m_diseqcTree     {nullptr};
    DeviceTree                   *m_diseqcBtn      {nullptr};
};

class FirewireGUID;
class FirewireModel : public CaptureCardComboBoxSetting
{
    Q_OBJECT

  public:
    FirewireModel(const CaptureCard &parent, const FirewireGUID *_guid);

  public slots:
    void SetGUID(const QString &_guid);

  private:
    const FirewireGUID *m_guid {nullptr};
};

class FirewireDesc : public GroupSetting
{
    Q_OBJECT

  public:
    explicit FirewireDesc(const FirewireGUID *_guid) :
        m_guid(_guid) { }

  public slots:
    void SetGUID(const QString &_guid);

  private:
    const FirewireGUID *m_guid {nullptr};
};

class CaptureCardGroup : public GroupSetting
{
    Q_OBJECT
public:
    explicit CaptureCardGroup(CaptureCard& parent);
};

class CaptureCard : public GroupSetting
{
    Q_OBJECT
public:
    explicit CaptureCard(bool use_card_group = true);

    int  getCardID(void) const { return m_id->intValue(); }
    QString GetRawCardType(void) const;

    void loadByID(int id);

    static void fillSelections(GroupSetting* setting);

    void reload(void);

    void Save(void) override; // StandardSetting

    bool canDelete(void) override; // GroupSetting
    void deleteEntry(void) override; // GroupSetting

private:

    class ID: public IdSetting {
    public:
        ID():
            IdSetting("capturecard", "cardid")
        {
        }
    };

    class Hostname : public StandardSetting
    {
      public:
        explicit Hostname(const CaptureCard &parent);
        ~Hostname() override;
        void edit(MythScreenType */*screen*/) override {} // StandardSetting
        void resultEdit(DialogCompletionEvent */*dce*/) override {} // StandardSetting
    };

private:
    ID       *m_id {nullptr};
};

class CardInputDBStorage : public SimpleDBStorage
{
  public:
    CardInputDBStorage(StorageUser     *_user,
                       const CardInput &_parent,
                       const QString&   _name) :
        SimpleDBStorage(_user, "capturecard", _name), m_parent(_parent)
    {
    }

    int getInputID(void) const;

  protected:
    void fillSelections();

    QString GetSetClause(MSqlBindings &bindings) const override; // SimpleDBStorage
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage

  private:
    const CardInput& m_parent;
};

class CaptureCardButton : public ButtonStandardSetting
{
    Q_OBJECT

  public:
     CaptureCardButton(const QString &label, QString value)
         : ButtonStandardSetting(label),
         m_value(std::move(value))
    {
    }

    void edit(MythScreenType *screen) override; // ButtonStandardSetting

  signals:
    void Clicked(const QString &choice);

  private:
    QString m_value;
};


class MTV_PUBLIC CaptureCardEditor : public GroupSetting
{
    Q_OBJECT

  public:
    CaptureCardEditor();

    void Load(void) override; // StandardSetting

    using CCESlot = void (CaptureCardEditor::*)(void);
    using CCESlotConst = void (CaptureCardEditor::*)(void) const;
    void AddSelection(const QString &label, CCESlot slot);
    void AddSelection(const QString &label, CCESlotConst slot);

  public slots:
    void ShowDeleteAllCaptureCardsDialog(void) const;
    void ShowDeleteAllCaptureCardsDialogOnHost(void) const;
    void DeleteAllCaptureCards(bool doDelete);
    void DeleteAllCaptureCardsOnHost(bool doDelete);
    void AddNewCard(void);
};

class MTV_PUBLIC VideoSourceEditor : public GroupSetting
{
    Q_OBJECT

  public:
    VideoSourceEditor();

    static bool cardTypesInclude(int SourceID,
                                 const QString& thecardtype);

    void Load(void) override; // StandardSetting
    using VSESlot = void (VideoSourceEditor::*)(void);
    using VSESlotConst = void (VideoSourceEditor::*)(void) const;
    void AddSelection(const QString &label, VSESlot slot);
    void AddSelection(const QString &label, VSESlotConst slot);

  public slots:
    void NewSource(void);
    void ShowDeleteAllSourcesDialog(void) const;
    void DeleteAllSources(bool doDelete);
};

class MTV_PUBLIC CardInputEditor : public GroupSetting
{
    Q_OBJECT

  public:
    CardInputEditor();

    void Load(void) override; // StandardSetting

  private:
    std::vector<CardInput*>  m_cardInputs;
};

class StartingChannel : public MythUIComboBoxSetting
{
    Q_OBJECT
  public:
    explicit StartingChannel(const CardInput &parent) :
        MythUIComboBoxSetting(new CardInputDBStorage(this, parent, "startchan"),
                              false)
    {
        setLabel(QObject::tr("Starting channel"));
        setHelpText(QObject::tr("This channel is shown when 'Watch TV' is selected on the main menu. "
                                "It is updated on every Live TV channel change. "
                                "When the value is not valid a suitable default will be chosen."));
    }

    ~StartingChannel() override
    {
        delete GetStorage();
    }
    static void fillSelections(void) {;}
  public slots:
    void SetSourceID(const QString &sourceid);
};

class CardInput : public GroupSetting
{
    Q_OBJECT
  public:
    CardInput(const QString & cardtype, const QString & device,
              int cardid);
    ~CardInput() override;

    int getInputID(void) const { return m_id->intValue(); };

    void loadByID(int id);
    void loadByInput(int cardid, const QString& inputname);
    QString getSourceName(void) const;

    void Save(void) override; // StandardSetting

  public slots:
    void CreateNewInputGroup();
    void channelScanner();
    void sourceFetch();
    void SetSourceID(const QString &sourceid);
    void CreateNewInputGroupSlot(const QString &name);

  private:
    class ID: public IdSetting
    {
      public:
        ID() : IdSetting("capturecard", "cardid")
        {
        }
    };

    ID                    *m_id                    {nullptr};
    InputName             *m_inputName             {nullptr};
    SourceID              *m_sourceId              {nullptr};
    StartingChannel       *m_startChan             {nullptr};
    ButtonStandardSetting *m_scan                  {nullptr};
    ButtonStandardSetting *m_srcFetch              {nullptr};
    DiSEqCDevSettings     *m_externalInputSettings {nullptr};
    InputGroup            *m_inputGrp0             {nullptr};
    InputGroup            *m_inputGrp1             {nullptr};
    MythUISpinBoxSetting  *m_instanceCount         {nullptr};
    MythUICheckBoxSetting *m_schedGroup            {nullptr};
};

///
class VBoxDeviceID;
class VBoxTunerIndex;

class VBoxIP : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    VBoxIP();

    void setEnabled(bool e) override; // StandardSetting
    void SetOldValue(const QString &s)
        { m_oldValue = s; };

  signals:
    void NewIP(const QString&);

  public slots:
    void UpdateDevices(const QString &v);

  private:
    QString m_oldValue;
};

class VBoxTunerIndex : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    VBoxTunerIndex();

    void setEnabled(bool e) override; // StandardSetting
    void SetOldValue(const QString &s)
        { m_oldValue = s; };

  signals:
    void NewTuner(const QString&);

  public slots:
    void UpdateDevices(const QString &v);

  private:
    QString m_oldValue;
};

class VBoxDeviceIDList : public TransMythUIComboBoxSetting
{
    Q_OBJECT

  public:
    VBoxDeviceIDList(VBoxDeviceID *deviceid,
                     StandardSetting *desc,
                     VBoxIP *cardip,
                     VBoxTunerIndex *cardtuner,
                     VBoxDeviceList *devicelist,
                     const CaptureCard &parent);

    void fillSelections(const QString &current);

    void Load(void) override; // StandardSetting

  public slots:
    void UpdateDevices(const QString &v);

  private:
    VBoxDeviceID      *m_deviceId;
    StandardSetting   *m_desc;
    VBoxIP            *m_cardIp;
    VBoxTunerIndex    *m_cardTuner;
    VBoxDeviceList    *m_deviceList;
    const CaptureCard &m_parent;

    QString            m_oldValue;
};

class VBoxDeviceID : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    explicit VBoxDeviceID(const CaptureCard &parent);
    ~VBoxDeviceID() override;

    void Load(void) override; // StandardSetting

  public slots:
    void SetIP(const QString &ip);
    void SetTuner(const QString &tuner);
    void SetOverrideDeviceID(const QString &deviceid);

  private:
    QString m_ip;
    QString m_tuner;
    QString m_overrideDeviceId;
};

#if CONFIG_CETON
class CetonSetting : public TransTextEditSetting
{
    Q_OBJECT

  public:
    CetonSetting(QString label, const QString& helptext);
    static void CetonConfigurationGroup(CaptureCard& parent, CardType& cardtype);

  signals:
    void NewValue(const QString&);

  public slots:
    void UpdateDevices(const QString &v);
    void LoadValue(const QString &value);
};

class CetonDeviceID : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    explicit CetonDeviceID(const CaptureCard &parent);
    ~CetonDeviceID() override;

    void Load(void) override; // StandardSetting
    void UpdateValues();

  signals:
    void LoadedIP(const QString&);
    void LoadedCard(const QString&);
    void LoadedTuner(const QString&);

  public slots:
    void SetIP(const QString &ip);
    void SetTuner(const QString &tuner);

  private:
    QString m_ip;
    QString m_card;
    QString m_tuner;
    const CaptureCard &m_parent;
};
#endif // CONFIG_CETON

#endif

#ifndef VIDEOSOURCE_H
#define VIDEOSOURCE_H

#include <vector>
using namespace std;

#include "mthread.h"
#include "standardsettings.h"
#include "datadirect.h"
#include "mythcontext.h"

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
    return !(grabber == "datadirect" ||
             grabber == "eitonly" ||
             grabber == "schedulesdirect1" ||
             grabber == "/bin/true");
}

static inline bool is_grabber_datadirect(const QString &grabber)
{
    return (grabber == "datadirect") || (grabber == "schedulesdirect1");
}

static inline int get_datadirect_provider(const QString &grabber)
{
    if (grabber == "datadirect")
        return DD_ZAP2IT;
    else if (grabber == "schedulesdirect1")
        return DD_SCHEDULES_DIRECT;
    else
        return -1;
}

static inline bool is_grabber_labs(const QString &grabber)
{
    return grabber == "datadirect";
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
    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;

    const VideoSource& m_parent;
};

class VideoSourceSelector : public TransMythUIComboBoxSetting
{
    Q_OBJECT

  public:
    VideoSourceSelector(uint           _initial_sourceid,
                        const QString &_card_types,
                        bool           _must_have_mplexid);

    virtual void Load(void);

    uint GetSourceID(void) const { return getValue().toUInt(); }

  private:
    uint    initial_sourceid;
    QString card_types;
    bool    must_have_mplexid;
};

class FreqTableSelector :
    public MythUIComboBoxSetting
{
    Q_OBJECT
public:
    explicit FreqTableSelector(const VideoSource& parent);
protected:
    QString freq;
};

class TransFreqTableSelector : public TransMythUIComboBoxSetting
{
  public:
    explicit TransFreqTableSelector(uint _sourceid);

    virtual void Load(void);

    virtual void Save(void);
    virtual void Save(QString /*destination*/) { Save(); }

    void SetSourceID(uint _sourceid);

  private:
    uint    sourceid;
    QString loaded_freq_table;
};

class DataDirectLineupSelector :
    public MythUIComboBoxSetting
{
   Q_OBJECT
public:
   explicit DataDirectLineupSelector(const VideoSource& parent) :
       MythUIComboBoxSetting(new VideoSourceDBStorage(this, parent, "lineupid"))
   {
       setLabel(QObject::tr("Data Direct lineup"));
   };

 public slots:
    void fillSelections(const QString& uid, const QString& pwd, int source);
};

class DataDirectButton : public ButtonStandardSetting
{
  public:
    DataDirectButton() : ButtonStandardSetting(QObject::tr("Retrieve Lineups"))
    {
    }
};

class DataDirectUserID;
class DataDirectPassword;

class DataDirect_config: public GroupSetting
{
    Q_OBJECT
  public:
    DataDirect_config(const VideoSource& _parent, int _ddsource, StandardSetting *_setting);

    virtual void Load(void);

    QString getLineupID(void) const { return lineupselector->getValue(); };

  protected slots:
    void fillDataDirectLineupSelector(void);

  protected:
    const VideoSource        &parent;
    DataDirectUserID         *userid;
    DataDirectPassword       *password;
    DataDirectButton         *button;
    DataDirectLineupSelector *lineupselector;
    QString lastloadeduserid;
    QString lastloadedpassword;
    int source;
};

class XMLTV_generic_config: public GroupSetting
{
    Q_OBJECT

  public:
    XMLTV_generic_config(const VideoSource& _parent, QString _grabber,
                         StandardSetting *_setting);

    virtual void Save(void);
    virtual void Save(QString) { Save(); }

  public slots:
    void RunConfig(void);

  protected:
    const VideoSource &parent;
    QString            grabber;
    QStringList        grabberArgs;
};

class EITOnly_config: public GroupSetting
{
public:
    EITOnly_config(const VideoSource& _parent, StandardSetting *_setting);

    virtual void Save();
    virtual void Save(QString) { Save(); }

protected:
    UseEIT *useeit;
};

class NoGrabber_config: public GroupSetting
{
public:
    explicit NoGrabber_config(const VideoSource& _parent);

    virtual void Save(void);
    virtual void Save(QString) { Save(); }

protected:
    UseEIT *useeit;
};

class IdSetting : public AutoIncrementSetting {
public:
    IdSetting(const QString &table, const QString &setting):
        AutoIncrementSetting(table, setting)
    {
        setVisible(false);
    }

    int intValue() { return getValue().toInt(); }
    void setValue(int value) { setValue(QString::number(value)); }
    using StandardSetting::setValue;
};

class VideoSource : public GroupSetting {
    Q_OBJECT

  public:
    VideoSource();

    int getSourceID(void) const { return id->intValue(); };

    void loadByID(int id);

    static void fillSelections(GroupSetting* setting);
    static void fillSelections(MythUIComboBoxSetting* setting);
    static QString idToName(int id);

    QString getSourceName(void) const { return name->getValue(); };

    virtual void Load(void)
    {
        GroupSetting::Load();
    }

    virtual void Save(void)
    {
        if (name)
            GroupSetting::Save();
    }
    virtual bool canDelete(void);
    virtual void deleteEntry(void);

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
    };

private:
    ID   *id;
    Name *name;
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
    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;
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
};

class TunerCardAudioInput : public CaptureCardComboBoxSetting
{
    Q_OBJECT
  public:
    TunerCardAudioInput(const CaptureCard &parent,
                        QString dev  = QString::null,
                        QString type = QString::null);

  public slots:
    int fillSelections(const QString &device);

  private:
    QString last_device;
    QString last_cardtype;
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

    void Save(void)
    {
        GetStorage()->SetSaveRequired();
        setValue("");
        GetStorage()->Save();
    }
    void Save(QString destination)
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

    void Save(void)
    {
        GetStorage()->SetSaveRequired();
        setValue("");
        GetStorage()->Save();
    }
    void Save(QString destination)
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

class HDHomeRunDevice
{
  public:
    QString mythdeviceid;
    QString deviceid;
    QString desc;
    QString cardip;
    QString cardtuner;
    bool    inuse;
    bool    discovered;
};

typedef QMap<QString, HDHomeRunDevice> HDHomeRunDeviceList;

class HDHomeRunDeviceIDList;
class HDHomeRunDeviceID;
class HDHomeRunIP;
class HDHomeRunTunerIndex;
class HDHomeRunConfigurationGroup : public GroupSetting
{
    Q_OBJECT

    friend class HDHomeRunExtra;

  public:
    HDHomeRunConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  private:
    void FillDeviceList(void);
    bool ProbeCard(HDHomeRunDevice&);

  private:
    CaptureCard           &parent;
    StandardSetting       *desc;
    HDHomeRunDeviceIDList *deviceidlist;
    HDHomeRunDeviceID     *deviceid;
    HDHomeRunIP           *cardip;
    HDHomeRunTunerIndex   *cardtuner;
    HDHomeRunDeviceList    devicelist;
};

class VBoxDevice
{
  public:
    QString mythdeviceid;
    QString deviceid;
    QString desc;
    QString cardip;
    QString tunerno;
    QString tunertype;
    bool    inuse;
    bool    discovered;
};

typedef QMap<QString, VBoxDevice> VBoxDeviceList;

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
    CaptureCard       &parent;
    StandardSetting   *desc;
    VBoxDeviceIDList  *deviceidlist;
    VBoxDeviceID      *deviceid;
    VBoxIP            *cardip;
    VBoxTunerIndex    *cardtuner;
    VBoxDeviceList    devicelist;
};

class V4LConfigurationGroup : public GroupSetting
{
    Q_OBJECT

  public:
    V4LConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard       &parent;
    TransTextEditSetting *cardinfo;
    VBIDevice         *vbidev;
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
    CaptureCard       &parent;
    VideoDevice       *device;
    VBIDevice         *vbidevice;
    TransTextEditSetting *cardinfo;
};

class HDPVRConfigurationGroup: public GroupSetting
{
   Q_OBJECT

  public:
    HDPVRConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard         &parent;
    GroupSetting        *cardinfo;
    TunerCardAudioInput *audioinput;
    VBIDevice           *vbidevice;
};

class ASIDevice;

class V4L2encGroup: public GroupSetting
{
    Q_OBJECT

  public:
    V4L2encGroup(CaptureCard& parent, CardType& cardType);

  private:
    CaptureCard          &m_parent;
    TransTextEditSetting *m_cardinfo;
    VideoDevice          *m_device;

    QString m_DriverName;

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
    CaptureCard       &parent;
    ASIDevice         *device;
    TransTextEditSetting *cardinfo;
};

class ImportConfigurationGroup: public GroupSetting
{
   Q_OBJECT

  public:
    ImportConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard       &parent;
    TransTextEditSetting *info;
    TransTextEditSetting *size;
};

class DemoConfigurationGroup: public GroupSetting
{
   Q_OBJECT

  public:
    DemoConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard       &parent;
    TransTextEditSetting *info;
    TransTextEditSetting *size;
};

#if !defined( USING_MINGW ) && !defined( _MSC_VER )
class ExternalConfigurationGroup: public GroupSetting
{
   Q_OBJECT

  public:
    ExternalConfigurationGroup(CaptureCard &parent, CardType &cardtype);

  public slots:
    void probeApp(const QString & path);

  private:
    CaptureCard       &parent;
    TransTextEditSetting *info;
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
    ~DVBConfigurationGroup();

    virtual void Load(void);

    virtual void Save(void);

  public slots:
    void probeCard(const QString& cardNumber);
    void reloadDiseqcTree(const QString &device);

  private:
    CaptureCard                  &parent;

    DVBCardNum                   *cardnum;
    DVBCardName                  *cardname;
    DVBCardType                  *cardtype;
    SignalTimeout                *signal_timeout;
    ChannelTimeout               *channel_timeout;
#if 0
    TransButtonSetting           *buttonAnalog;
#endif
    DVBTuningDelay               *tuning_delay;
    DiSEqCDevTree                *diseqc_tree;
    DeviceTree                   *diseqc_btn;
};

class FirewireGUID;
class FirewireModel : public CaptureCardComboBoxSetting
{
    Q_OBJECT

  public:
    FirewireModel(const CaptureCard &parent, const FirewireGUID*);

  public slots:
    void SetGUID(const QString&);

  private:
    const FirewireGUID *guid;
};

class FirewireDesc : public GroupSetting
{
    Q_OBJECT

  public:
    explicit FirewireDesc(const FirewireGUID *_guid) :
        GroupSetting(), guid(_guid) { }

  public slots:
    void SetGUID(const QString&);

  private:
    const FirewireGUID *guid;
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

    int  getCardID(void) const { return id->intValue(); }
    QString GetRawCardType(void) const;

    void loadByID(int id);

    static void fillSelections(GroupSetting* setting);

    void reload(void);

    virtual void Save(void);

    virtual bool canDelete(void);
    virtual void deleteEntry(void);

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
        explicit Hostname(const CaptureCard &parent) :
            StandardSetting(new CaptureCardDBStorage(this, parent, "hostname"))
        {
            setVisible(false);
            setValue(gCoreContext->GetHostName());
        }
        void edit(MythScreenType *) {}
        void resultEdit(DialogCompletionEvent *) {}
    };

private:
    ID       *id;
};

class CardInputDBStorage : public SimpleDBStorage
{
  public:
    CardInputDBStorage(StorageUser     *_user,
                       const CardInput &_parent,
                       QString          _name) :
        SimpleDBStorage(_user, "capturecard", _name), m_parent(_parent)
    {
    }

    int getInputID(void) const;

  protected:
    void fillSelections();

    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;

  private:
    const CardInput& m_parent;
};

class CaptureCardButton : public ButtonStandardSetting
{
    Q_OBJECT

  public:
     CaptureCardButton(const QString &label, const QString &value)
         : ButtonStandardSetting(label),
         m_value(value)
    {
    }

    virtual void edit(MythScreenType *screen);

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

    virtual void Load(void);

    void AddSelection(const QString &label, const char *slot);

  public slots:
    void ShowDeleteAllCaptureCardsDialog(void);
    void ShowDeleteAllCaptureCardsDialogOnHost(void);
    void DeleteAllCaptureCards(bool);
    void DeleteAllCaptureCardsOnHost(bool);
    void AddNewCard(void);
};

class MTV_PUBLIC VideoSourceEditor : public GroupSetting
{
    Q_OBJECT

  public:
    VideoSourceEditor();

    bool cardTypesInclude(const int& SourceID,
                          const QString& thecardtype);

    virtual void Load(void);
    void AddSelection(const QString &label, const char *slot);

  public slots:
    void NewSource(void);
    void ShowDeleteAllSourcesDialog(void);
    void DeleteAllSources(bool);
};

class MTV_PUBLIC CardInputEditor : public GroupSetting
{
    Q_OBJECT

  public:
    CardInputEditor();

    virtual void Load(void);

  private:
    vector<CardInput*>  cardinputs;
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
        setHelpText(QObject::tr("Starting Live TV channel.") + " " +
                    QObject::tr("This is updated on every successful "
                                "channel change."));
    }
    void fillSelections(void) {;}
  public slots:
    void SetSourceID(const QString &sourceid);
};

class CardInput : public GroupSetting
{
    Q_OBJECT
  public:
    CardInput(const QString & cardtype, const QString & device,
              bool is_new_input, int cardid);
    ~CardInput();

    int getInputID(void) const { return id->intValue(); };

    void loadByID(int id);
    void loadByInput(int cardid, QString input);
    QString getSourceName(void) const;

    virtual void Save(void);

  public slots:
    void CreateNewInputGroup();
    void channelScanner();
    void sourceFetch();
    void SetSourceID(const QString &sourceid);
    void UpdateSchedGroup(const QString &value);
    void CreateNewInputGroupSlot(const QString &name);

  private:
    class ID: public IdSetting
    {
      public:
        ID() : IdSetting("capturecard", "cardid")
        {
        }
    };

    ID              *id;
    InputName       *inputname;
    SourceID        *sourceid;
    StartingChannel *startchan;
    ButtonStandardSetting *scan;
    ButtonStandardSetting *srcfetch;
    DiSEqCDevSettings  *externalInputSettings;
    InputGroup         *inputgrp0;
    InputGroup         *inputgrp1;
    MythUISpinBoxSetting  *instancecount;
    MythUICheckBoxSetting *schedgroup;
};

class HDHomeRunDeviceID;
class HDHomeRunTunerIndex;

class HDHomeRunIP : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    HDHomeRunIP();

    virtual void setEnabled(bool e);
    void SetOldValue(const QString &s)
        { _oldValue = s; _oldValue.detach(); };

  signals:
    void NewIP(const QString&);

  public slots:
    void UpdateDevices(const QString&);

  private:
    QString _oldValue;
};

class HDHomeRunTunerIndex : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    HDHomeRunTunerIndex();

    virtual void setEnabled(bool e);
    void SetOldValue(const QString &s)
        { _oldValue = s; _oldValue.detach(); };

  signals:
    void NewTuner(const QString&);

  public slots:
    void UpdateDevices(const QString&);

  private:
    QString _oldValue;
};


class HDHomeRunDeviceIDList : public TransMythUIComboBoxSetting
{
    Q_OBJECT

  public:
    HDHomeRunDeviceIDList(HDHomeRunDeviceID *deviceid,
                          StandardSetting *desc,
                          HDHomeRunIP       *cardip,
                          HDHomeRunTunerIndex *cardtuner,
                          HDHomeRunDeviceList *devicelist,
                          const CaptureCard &parent);

    void fillSelections(const QString &current);

    virtual void Load(void);

  public slots:
    void UpdateDevices(const QString&);

  private:
    HDHomeRunDeviceID   *_deviceid;
    StandardSetting     *_desc;
    HDHomeRunIP         *_cardip;
    HDHomeRunTunerIndex *_cardtuner;
    HDHomeRunDeviceList *_devicelist;
    const CaptureCard &m_parent;

    QString              _oldValue;
};

class HDHomeRunDeviceID : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    explicit HDHomeRunDeviceID(const CaptureCard &parent);

    virtual void Load(void);

  public slots:
    void SetIP(const QString&);
    void SetTuner(const QString&);
    void SetOverrideDeviceID(const QString&);

  private:
    QString _ip;
    QString _tuner;
    QString _overridedeviceid;
};

///
class VBoxDeviceID;
class VBoxTunerIndex;

class VBoxIP : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    VBoxIP();

    virtual void setEnabled(bool e);
    void SetOldValue(const QString &s)
        { _oldValue = s; _oldValue.detach(); };

  signals:
    void NewIP(const QString&);

  public slots:
    void UpdateDevices(const QString&);

  private:
    QString _oldValue;
};

class VBoxTunerIndex : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    VBoxTunerIndex();

    virtual void setEnabled(bool e);
    void SetOldValue(const QString &s)
        { _oldValue = s; _oldValue.detach(); };

  signals:
    void NewTuner(const QString&);

  public slots:
    void UpdateDevices(const QString&);

  private:
    QString _oldValue;
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

    virtual void Load(void);

  public slots:
    void UpdateDevices(const QString&);

  private:
    VBoxDeviceID      *_deviceid;
    StandardSetting *_desc;
    VBoxIP            *_cardip;
    VBoxTunerIndex    *_cardtuner;
    VBoxDeviceList    *_devicelist;
    const CaptureCard &m_parent;

    QString            _oldValue;
};

class VBoxDeviceID : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    explicit VBoxDeviceID(const CaptureCard &parent);

    virtual void Load(void);

  public slots:
    void SetIP(const QString&);
    void SetTuner(const QString&);
    void SetOverrideDeviceID(const QString&);

  private:
    QString _ip;
    QString _tuner;
    QString _overridedeviceid;
};

class CetonSetting : public TransTextEditSetting
{
    Q_OBJECT

  public:
    CetonSetting(const char* label, const char* helptext);

  signals:
    void NewValue(const QString&);

  public slots:
    void UpdateDevices(const QString&);
    void LoadValue(const QString&);
};

class CetonDeviceID : public MythUITextEditSetting
{
    Q_OBJECT

  public:
    explicit CetonDeviceID(const CaptureCard &parent);

    virtual void Load(void);
    void UpdateValues();

  signals:
    void LoadedIP(const QString&);
    void LoadedCard(const QString&);
    void LoadedTuner(const QString&);

  public slots:
    void SetIP(const QString&);
    void SetTuner(const QString&);

  private:
    QString _ip;
    QString _card;
    QString _tuner;
    const CaptureCard &_parent;
};

#endif

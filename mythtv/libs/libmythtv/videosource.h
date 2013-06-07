#ifndef VIDEOSOURCE_H
#define VIDEOSOURCE_H

#include <vector>
using namespace std;

#include "mthread.h"
#include "settings.h"
#include "datadirect.h"

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
  protected:
    VideoSourceDBStorage(StorageUser       *_user,
                         const VideoSource &_parent,
                         const QString     &name) :
        SimpleDBStorage(_user, "videosource", name), m_parent(_parent)
    {
    }

    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;

    const VideoSource& m_parent;
};

class VideoSourceSelector : public ComboBoxSetting, public TransientStorage
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
    public ComboBoxSetting, public VideoSourceDBStorage
{
    Q_OBJECT
public:
    FreqTableSelector(const VideoSource& parent);
protected:
    QString freq;
};

class TransFreqTableSelector : public ComboBoxSetting, public TransientStorage
{
  public:
    TransFreqTableSelector(uint _sourceid);

    virtual void Load(void);

    virtual void Save(void);
    virtual void Save(QString /*destination*/) { Save(); }

    void SetSourceID(uint _sourceid);

  private:
    uint    sourceid;
    QString loaded_freq_table;
};

class DataDirectLineupSelector :
    public ComboBoxSetting, public VideoSourceDBStorage
{
   Q_OBJECT
public:
   DataDirectLineupSelector(const VideoSource& parent) :
       ComboBoxSetting(this), VideoSourceDBStorage(this, parent, "lineupid")
   {
       setLabel(QObject::tr("Data Direct lineup"));
   };

 public slots:
    void fillSelections(const QString& uid, const QString& pwd, int source);
};

class DataDirectButton : public TransButtonSetting
{
  public:
    DataDirectButton() { setLabel(QObject::tr("Retrieve Lineups")); }
};

class DataDirectUserID;
class DataDirectPassword;

class DataDirect_config: public VerticalConfigurationGroup
{
    Q_OBJECT
  public:
    DataDirect_config(const VideoSource& _parent, int _ddsource); 

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

class XMLTV_generic_config: public VerticalConfigurationGroup 
{
    Q_OBJECT

  public:
    XMLTV_generic_config(const VideoSource& _parent, QString _grabber);

    virtual void Save(void);
    virtual void Save(QString) { Save(); }

  public slots:
    void RunConfig(void);

  protected:
    const VideoSource &parent;
    QString            grabber;
    QStringList        grabberArgs;
};

class EITOnly_config: public VerticalConfigurationGroup
{
public:
    EITOnly_config(const VideoSource& _parent);

    virtual void Save();
    virtual void Save(QString) { Save(); }

protected:
    UseEIT *useeit;
};

class NoGrabber_config: public VerticalConfigurationGroup
{
public:
    NoGrabber_config(const VideoSource& _parent);

    virtual void Save(void);
    virtual void Save(QString) { Save(); }

protected:
    UseEIT *useeit;
};


class XMLTVGrabber;
class XMLTVConfig : public TriggeredConfigurationGroup
{
    Q_OBJECT

  public:
    XMLTVConfig(const VideoSource &aparent);

    virtual void Load(void);
    virtual void Save(void);
    virtual void Save(QString /*destination*/) { Save(); }

  private:
    const VideoSource &parent;
    XMLTVGrabber      *grabber;

    void LoadXMLTVGrabbers(QStringList name_list, QStringList prog_list);
};

class VideoSource : public ConfigurationWizard {
public:
    VideoSource();

    int getSourceID(void) const { return id->intValue(); };

    void loadByID(int id);

    static void fillSelections(SelectSetting* setting);
    static QString idToName(int id);

    QString getSourceName(void) const { return name->getValue(); };

    virtual void Save(void)
    {
        if (name)
            ConfigurationWizard::Save();
    }

    virtual void Save(QString destination)
    {
        if (name)
            ConfigurationWizard::Save(destination);
    }

  private:
    class ID: public AutoIncrementDBSetting
    {
      public:
        ID() : AutoIncrementDBSetting("videosource", "sourceid")
        {
            setName("VideoSourceName");
            setVisible(false);
        };
    };

    class Name : public LineEditSetting, public VideoSourceDBStorage
    {
      public:
        Name(const VideoSource &parent) :
            LineEditSetting(this), VideoSourceDBStorage(this, parent, "name")
        {
            setLabel(QObject::tr("Video source name"));
        }
    };

private:
    ID   *id;
    Name *name;
    XMLTVConfig *xmltv;
};

class CaptureCardDBStorage : public SimpleDBStorage
{
  protected:
    CaptureCardDBStorage(StorageUser       *_user,
                         const CaptureCard &_parent,
                         const QString     &_name) :
        SimpleDBStorage(_user, "capturecard", _name), m_parent(_parent)
    {
    }

    int getCardID(void) const;

protected:
    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;
private:
    const CaptureCard& m_parent;
};

class TunerCardAudioInput : public ComboBoxSetting, public CaptureCardDBStorage
{
    Q_OBJECT
  public:
    TunerCardAudioInput(const CaptureCard &parent,
                        QString dev  = QString::null,
                        QString type = QString::null);

  public slots:
    void fillSelections(const QString &device);

  private:
    QString last_device;
    QString last_cardtype;
};

class EmptyAudioDevice : public LineEditSetting, public CaptureCardDBStorage
{
    Q_OBJECT
  public:
    EmptyAudioDevice(const CaptureCard &parent) :
        LineEditSetting(this),
        CaptureCardDBStorage(this, parent, "audiodevice")
    {
        setVisible(false);
    }

    void Save(void)
    {
        SetSaveRequired();
        settingValue = "";
        SimpleDBStorage::Save();
    }
    void Save(QString destination)
    {
        SetSaveRequired();
        settingValue = "";
        SimpleDBStorage::Save(destination);
    }
};

class EmptyVBIDevice : public LineEditSetting, public CaptureCardDBStorage
{
    Q_OBJECT

  public:
    EmptyVBIDevice(const CaptureCard &parent) :
        LineEditSetting(this),
        CaptureCardDBStorage(this, parent, "vbidevice")
    {
        setVisible(false);
    };

    void Save(void)
    {
        SetSaveRequired();
        settingValue = "";
        SimpleDBStorage::Save();
    }
    void Save(QString destination)
    {
        SetSaveRequired();
        settingValue = "";
        SimpleDBStorage::Save(destination);
    }
};

class CardType : public ComboBoxSetting, public CaptureCardDBStorage
{
public:
    CardType(const CaptureCard& parent);
    static void fillSelections(SelectSetting* setting);
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
class HDHomeRunConfigurationGroup : public VerticalConfigurationGroup
{
    Q_OBJECT

    friend class HDHomeRunExtra;

  public:
    HDHomeRunConfigurationGroup(CaptureCard &parent);

  public slots:
    void HDHomeRunExtraPanel(void);

  private:
    void FillDeviceList(void);
    bool ProbeCard(HDHomeRunDevice&);

  private:
    CaptureCard           &parent;
    TransLabelSetting     *desc;
    HDHomeRunDeviceIDList *deviceidlist;
    HDHomeRunDeviceID     *deviceid;
    HDHomeRunIP           *cardip;
    HDHomeRunTunerIndex   *cardtuner;
    HDHomeRunDeviceList    devicelist;
};


class CetonDeviceID;
class CetonSetting;
class CetonConfigurationGroup : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    CetonConfigurationGroup(CaptureCard &parent);

  private:
    CaptureCard         &parent;
    TransLabelSetting   *desc;
    CetonDeviceID       *deviceid;
    CetonSetting        *ip;
    CetonSetting        *tuner;
};



class V4LConfigurationGroup : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    V4LConfigurationGroup(CaptureCard &parent);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard       &parent;
    TransLabelSetting *cardinfo;
    VBIDevice         *vbidev;
};

class VideoDevice;
class VBIDevice;

class MPEGConfigurationGroup: public VerticalConfigurationGroup
{
   Q_OBJECT

  public:
    MPEGConfigurationGroup(CaptureCard &parent);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard       &parent;
    VideoDevice       *device;
    VBIDevice         *vbidevice;
    TransLabelSetting *cardinfo;
};

class HDPVRConfigurationGroup: public VerticalConfigurationGroup
{
   Q_OBJECT

  public:
    HDPVRConfigurationGroup(CaptureCard &parent);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard         &parent;
    TransLabelSetting   *cardinfo;
    TunerCardAudioInput *audioinput;
};

class InstanceCount;
class ASIDevice;

class ASIConfigurationGroup: public VerticalConfigurationGroup
{
   Q_OBJECT

  public:
    ASIConfigurationGroup(CaptureCard &parent);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard       &parent;
    ASIDevice         *device;
    TransLabelSetting *cardinfo;
    InstanceCount     *instances;
};

class InstanceCount;

class ImportConfigurationGroup: public VerticalConfigurationGroup
{
   Q_OBJECT

  public:
    ImportConfigurationGroup(CaptureCard &parent);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard       &parent;
    TransLabelSetting *info;
    TransLabelSetting *size;
};

class DemoConfigurationGroup: public VerticalConfigurationGroup
{
   Q_OBJECT

  public:
    DemoConfigurationGroup(CaptureCard &parent);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard       &parent;
    TransLabelSetting *info;
    TransLabelSetting *size;
};

class DVBCardNum;
class DVBCardName;
class DVBCardType;
class DVBTuningDelay;

class DVBConfigurationGroup : public VerticalConfigurationGroup
{
    Q_OBJECT

    friend class DVBExtra;

  public:
    DVBConfigurationGroup(CaptureCard& a_parent);
    ~DVBConfigurationGroup();

    virtual void Load(void);

    virtual void Save(void);
    virtual void Save(QString /*destination*/) { Save(); }
    
  public slots:
    void probeCard(const QString& cardNumber);
    void DiSEqCPanel(void);
    void DVBExtraPanel(void);

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
    TransButtonSetting           *diseqc_btn;
};

class FirewireGUID;
class FirewireModel : public ComboBoxSetting, public CaptureCardDBStorage
{
    Q_OBJECT

  public:
    FirewireModel(const CaptureCard &parent, const FirewireGUID*);

  public slots:
    void SetGUID(const QString&);

  private:
    const FirewireGUID *guid;
};

class FirewireDesc : public TransLabelSetting
{
    Q_OBJECT

  public:
    FirewireDesc(const FirewireGUID *_guid) :
        TransLabelSetting(), guid(_guid) { }

  public slots:
    void SetGUID(const QString&);

  private:
    const FirewireGUID *guid;
};

class CaptureCardGroup : public TriggeredConfigurationGroup
{
    Q_OBJECT
public:
    CaptureCardGroup(CaptureCard& parent);

protected slots:
    virtual void triggerChanged(const QString& value);
};

class CaptureCard : public QObject, public ConfigurationWizard
{
    Q_OBJECT
public:
    CaptureCard(bool use_card_group = true);

    int  getCardID(void) const { return id->intValue(); }
    QString GetRawCardType(void) const;

    void loadByID(int id);

    static void fillSelections(SelectSetting* setting);

    void reload(void);

    virtual void Save(void);
    virtual void Save(QString /*destination*/) { Save(); }

    uint GetInstanceCount(void) const { return instance_count; }

public slots:
    void SetInstanceCount(uint cnt) { instance_count = cnt; }
    // this is needed to connect valueChanged() signal from legacy settings
    void SetInstanceCount(int cnt)  { instance_count = (uint)cnt; }

private:

    class ID: public AutoIncrementDBSetting {
    public:
        ID():
            AutoIncrementDBSetting("capturecard", "cardid") {
            setVisible(false);
            setName("ID");
        };
    };

    class Hostname : public HostnameSetting, public CaptureCardDBStorage
    {
      public:
        Hostname(const CaptureCard &parent) :
            HostnameSetting(this),
            CaptureCardDBStorage(this, parent, "hostname") { }
    };

private:
    ID       *id;
    uint      instance_count;
};

class CardInputDBStorage : public SimpleDBStorage
{
  protected:
    CardInputDBStorage(StorageUser     *_user,
                       const CardInput &_parent,
                       QString          _name) :
        SimpleDBStorage(_user, "cardinput", _name), m_parent(_parent)
    {
    }

    int getInputID(void) const;

    void fillSelections();

protected:
    virtual QString GetSetClause(MSqlBindings &bindings) const;
    virtual QString GetWhereClause(MSqlBindings &bindings) const;
private:
    const CardInput& m_parent;
};

class MTV_PUBLIC CaptureCardEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    CaptureCardEditor();

    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);
    virtual DialogCode exec(void);
    virtual DialogCode exec(bool /*saveOnExec*/, bool /*doLoad*/)
        { return exec(); }
    virtual void Load(void);

    virtual void Save(void) { }
    virtual void Save(QString destination) { }

  public slots:
    void menu(void);
    void edit(void);
    void del(void);

  private:
    ListBoxSetting *listbox;
};

class MTV_PUBLIC VideoSourceEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    VideoSourceEditor();

    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);

    bool cardTypesInclude(const int& SourceID, 
                          const QString& thecardtype);

    virtual DialogCode exec(void);
    virtual DialogCode exec(bool /*saveOnExec*/, bool /*doLoad*/)
        { return exec(); }
    virtual void Load(void);
    virtual void Save(void) { }
    virtual void Save(QString /*destination*/) { }

  public slots:
    void menu(void);
    void edit(void);
    void del(void);

  private:
    ListBoxSetting *listbox;
};

class MTV_PUBLIC CardInputEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    CardInputEditor();

    virtual DialogCode exec(void);
    virtual DialogCode exec(bool /*saveOnExec*/, bool /*doLoad*/)
        { return exec(); }
    virtual void Load(void);
    virtual void Save(void) { }
    virtual void Save(QString /*destination*/) { }

  private:
    vector<CardInput*>  cardinputs;
    ListBoxSetting     *listbox;
};

class StartingChannel : public ComboBoxSetting, public CardInputDBStorage
{
    Q_OBJECT
  public:
    StartingChannel(const CardInput &parent) :
        ComboBoxSetting(this, false, 1),
        CardInputDBStorage(this, parent, "startchan")
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

class CardInput : public QObject, public ConfigurationWizard
{
    Q_OBJECT
  public:
    CardInput(bool is_dtv_card, bool is_dvb_card,
              bool is_new_input, int cardid);
    ~CardInput();

    int getInputID(void) const { return id->intValue(); };

    void loadByID(int id);
    void loadByInput(int cardid, QString input);
    QString getSourceName(void) const;

    virtual void Save(void);
    virtual void Save(QString /*destination*/) { Save(); }

  public slots:
    void CreateNewInputGroup();
    void channelScanner();
    void sourceFetch();
    void SetSourceID(const QString &sourceid);

  private:
    class ID: public AutoIncrementDBSetting
    {
      public:
        ID() : AutoIncrementDBSetting("cardinput", "cardid")
        {
            setVisible(false);
            setName("CardInputID");
        }
    };

    ID              *id;
    CardID          *cardid;
    InputName       *inputname;
    SourceID        *sourceid;
    StartingChannel *startchan;
    TransButtonSetting *scan;
    TransButtonSetting *srcfetch;
    DiSEqCDevSettings  *externalInputSettings;
    InputGroup         *inputgrp0;
    InputGroup         *inputgrp1;
};

class HDHomeRunDeviceID;
class HDHomeRunTunerIndex;

class HDHomeRunIP : public TransLineEditSetting
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

class HDHomeRunTunerIndex : public TransLineEditSetting
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


class HDHomeRunDeviceIDList : public TransComboBoxSetting
{
    Q_OBJECT

  public:
    HDHomeRunDeviceIDList(HDHomeRunDeviceID *deviceid,
                          TransLabelSetting *desc,
                          HDHomeRunIP       *cardip,
                          HDHomeRunTunerIndex *cardtuner,
                          HDHomeRunDeviceList *devicelist);

    void fillSelections(const QString &current);

    virtual void Load(void);

  public slots:
    void UpdateDevices(const QString&);

  private:
    HDHomeRunDeviceID   *_deviceid;
    TransLabelSetting   *_desc;
    HDHomeRunIP         *_cardip;
    HDHomeRunTunerIndex *_cardtuner;
    HDHomeRunDeviceList *_devicelist;

    QString              _oldValue;
};

class HDHomeRunDeviceID : public LabelSetting, public CaptureCardDBStorage
{
    Q_OBJECT

  public:
    HDHomeRunDeviceID(const CaptureCard &parent);

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

class CetonSetting : public TransLineEditSetting
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

class CetonDeviceID : public LabelSetting, public CaptureCardDBStorage
{
    Q_OBJECT

  public:
    CetonDeviceID(const CaptureCard &parent);

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
};

#endif

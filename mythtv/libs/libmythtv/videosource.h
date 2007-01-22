#ifndef VIDEOSOURCE_H
#define VIDEOSOURCE_H

#include <vector>
using namespace std;

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
class ChildID;
class InputName;
class SourceID;
class DiSEqCDevTree;
class DiSEqCDevSettings;

class VideoSourceDBStorage : public SimpleDBStorage
{
  protected:
    VideoSourceDBStorage(Setting *_setting,
                         const VideoSource &_parent,
                         QString name) :
        SimpleDBStorage(_setting, "videosource", name), parent(_parent)
    {
        setting->setName(name);
    }

    virtual QString setClause(MSqlBindings& bindings);
    virtual QString whereClause(MSqlBindings& bindings);

    const VideoSource& parent;
};

class VideoSourceSelector : public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT

  public:
    VideoSourceSelector(uint           _initial_sourceid,
                        const QString &_card_types,
                        bool           _must_have_mplexid);

    virtual void load(void);

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

    virtual void load(void);
    virtual void save(void);

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
       setLabel(QObject::tr("Data Direct Lineup"));
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
    DataDirect_config(const VideoSource& _parent, int _source = DD_ZAP2IT); 

    virtual void load(void);

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
public:
    XMLTV_generic_config(const VideoSource& _parent, QString _grabber);

    virtual void save();
    virtual void save(QString) { save(); }

protected:
    const VideoSource& parent;
    QString grabber;
};

class EITOnly_config: public VerticalConfigurationGroup
{
public:
    EITOnly_config(const VideoSource& _parent);
    virtual void save();
    virtual void save(QString) { save(); }

protected:
    UseEIT *useeit;
};

class NoGrabber_config: public VerticalConfigurationGroup
{
public:
    NoGrabber_config(const VideoSource& _parent);

    virtual void save();
    virtual void save(QString) { save(); }

protected:
    UseEIT *useeit;
};

class XMLTVConfig : public TriggeredConfigurationGroup
{
public:
    XMLTVConfig(const VideoSource& parent);
};

class VideoSource : public ConfigurationWizard {
public:
    VideoSource();
        
    int getSourceID(void) const { return id->intValue(); };

    void loadByID(int id);

    static void fillSelections(SelectSetting* setting);
    static QString idToName(int id);

    QString getSourceName(void) const { return name->getValue(); };

    virtual void save(void)
    {
        if (name)
            ConfigurationWizard::save();
    }

    virtual void save(QString destination)
    {
        if (name)
            ConfigurationWizard::save(destination);
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
};

class CaptureCardDBStorage : public SimpleDBStorage
{
  protected:
    CaptureCardDBStorage(Setting           *_setting,
                         const CaptureCard &_parent,
                         QString            _name) :
        SimpleDBStorage(_setting, "capturecard", _name), parent(_parent)
    {
        setting->setName(_name);
    }

    int getCardID(void) const;

protected:
    virtual QString setClause(MSqlBindings& bindings);
    virtual QString whereClause(MSqlBindings& bindings);
private:
    const CaptureCard& parent;
};

class TunerCardInput : public ComboBoxSetting, public CaptureCardDBStorage
{
    Q_OBJECT
  public:
    TunerCardInput(const CaptureCard &parent,
                   QString dev  = QString::null,
                   QString type = QString::null);

  public slots:
    void fillSelections(const QString &device);

  private:
    QString last_device;
    QString last_cardtype;
    int     last_diseqct;
};

class SingleCardInput : public TunerCardInput
{
    Q_OBJECT

  public:
    SingleCardInput(const CaptureCard &parent) : TunerCardInput(parent)
    {
        setLabel(QObject::tr("Default Input"));
        addSelection("MPEG2TS");
        setVisible(false);
    }

  public slots:
    void fillSelections(const QString&)
    {
        clearSelections();
        addSelection("MPEG2TS");
    }
};

class DVBAudioDevice : public LineEditSetting, public CaptureCardDBStorage
{
    Q_OBJECT
  public:
    DVBAudioDevice(const CaptureCard &parent) :
        LineEditSetting(this),
        CaptureCardDBStorage(this, parent, "audiodevice")
    {
        setVisible(false);
    }

    void save() {
        changed = true;
        settingValue = "";
        SimpleDBStorage::save();
    };
    void save(QString destination) {
        changed = true;
        settingValue = "";
        SimpleDBStorage::save(destination);
    };
};

class DVBVbiDevice : public LineEditSetting, public CaptureCardDBStorage
{
    Q_OBJECT

  public:
    DVBVbiDevice(const CaptureCard &parent) :
        LineEditSetting(this),
        CaptureCardDBStorage(this, parent, "vbidevice")
    {
        setVisible(false);
    };
    void save() {
        changed = true;
        settingValue = "";
        SimpleDBStorage::save();
    };
    void save(QString destination) {
        changed = true;
        settingValue = "";
        SimpleDBStorage::save(destination);
    };
};

class CardType : public ComboBoxSetting, public CaptureCardDBStorage
{
public:
    CardType(const CaptureCard& parent);
    static void fillSelections(SelectSetting* setting);
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
    TunerCardInput    *input;
};

class MPEGConfigurationGroup: public VerticalConfigurationGroup
{
   Q_OBJECT

  public:
    MPEGConfigurationGroup(CaptureCard &parent);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard       &parent;
    TransLabelSetting *cardinfo;
    TunerCardInput    *input;
};

class pcHDTVConfigurationGroup: public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    pcHDTVConfigurationGroup(CaptureCard& a_parent);

  public slots:
    void probeCard(const QString &device);

  private:
    CaptureCard       &parent;
    TransLabelSetting *cardinfo;
    TunerCardInput    *input;
};

class DVBInput;
class DVBCardName;
class DVBCardType;
class DVBTuningDelay;

class DVBConfigurationGroup: public VerticalConfigurationGroup {
    Q_OBJECT
public:
    DVBConfigurationGroup(CaptureCard& a_parent);
    ~DVBConfigurationGroup();

    virtual void load(void);
    virtual void save(void);
    
public slots:
    void probeCard(const QString& cardNumber);
    void DiSEqCPanel(void);
    
private:
    CaptureCard        &parent;

    DVBInput           *defaultinput;
    DVBCardName        *cardname;
    DVBCardType        *cardtype;
    SignalTimeout      *signal_timeout;
    ChannelTimeout     *channel_timeout;
    TransButtonSetting *buttonAnalog;
    DVBTuningDelay     *tuning_delay;
    DiSEqCDevTree      *diseqc_tree;
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

    void loadByID(int id);
    void setParentID(int id);

    static void fillSelections(SelectSetting* setting);
    static void fillSelections(SelectSetting* setting, bool no_children);

    void reload(void);

public slots:
    void analogPanel();
    void recorderOptionsPanel();

private:

    class ID: public AutoIncrementDBSetting {
    public:
        ID():
            AutoIncrementDBSetting("capturecard", "cardid") {
            setVisible(false);
            setName("ID");
        };
    };

    class ParentID : public LabelSetting, public CaptureCardDBStorage
    {
      public:
        ParentID(const CaptureCard &parent) :
            LabelSetting(this), CaptureCardDBStorage(this, parent, "parentid")
        {
            setValue("0");
            setVisible(false);
        }
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
    ParentID *parentid;
};

class CardInputDBStorage : public SimpleDBStorage
{
  protected:
    CardInputDBStorage(Setting         *_setting,
                       const CardInput &_parent,
                       QString          _name) :
        SimpleDBStorage(_setting, "cardinput", _name), parent(_parent)
    {
        _setting->setName(_name);
    }

    int getInputID(void) const;

    void fillSelections();

protected:
    virtual QString setClause(MSqlBindings& bindings);
    virtual QString whereClause(MSqlBindings& bindings);
private:
    const CardInput& parent;
};

class MPUBLIC CaptureCardEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    CaptureCardEditor();

    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);
    virtual int exec();
    virtual void load();
    virtual void save() { };

  public slots:
    void menu(void);
    void edit(void);
    void del(void);

  private:
    ListBoxSetting *listbox;
};

class MPUBLIC VideoSourceEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    VideoSourceEditor();

    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetName=0);

    bool cardTypesInclude(const int& SourceID, 
                          const QString& thecardtype);

    virtual int exec();
    virtual void load();
    virtual void save() { };

  public slots:
    void menu(void);
    void edit(void);
    void del(void);

  private:
    ListBoxSetting *listbox;
};

class MPUBLIC CardInputEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT

  public:
    CardInputEditor();

    virtual int exec();
    virtual void load();
    virtual void save() { };

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
        setHelpText(QObject::tr("Starting LiveTV channel.") + " " +
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

    void SetChildCardID(uint);

    virtual void save();
    virtual void save(QString /*destination*/) { save(); }

  public slots:
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
    ChildID         *childid;
    InputName       *inputname;
    SourceID        *sourceid;
    StartingChannel *startchan;
    TransButtonSetting *scan;
    TransButtonSetting *srcfetch;
    DiSEqCDevSettings  *externalInputSettings;
};

#endif

#ifndef VIDEOSOURCE_H
#define VIDEOSOURCE_H

#include "libmyth/settings.h"
#include <qregexp.h>
#include <vector>
#include <qdir.h>
#include <qstringlist.h>

class VideoSource;
class VSSetting: public SimpleDBStorage {
protected:
    VSSetting(const VideoSource& _parent, QString name):
        SimpleDBStorage("videosource", name),
        parent(_parent) {
        setName(name);
    };

    virtual QString setClause(void);
    virtual QString whereClause(void);

    const VideoSource& parent;
};


class XMLTVGrabber: public ComboBoxSetting, public VSSetting {
public:
    XMLTVGrabber(const VideoSource& parent): VSSetting(parent, "xmltvgrabber") {
        setLabel("XMLTV listings grabber");
    };
};

class UserID: public LineEditSetting, public VSSetting {
public:
    UserID(const VideoSource& parent): VSSetting(parent, "userid") {
        setLabel("Gist.com user id");
    };
};

class PostalCode: public LineEditSetting, public TransientStorage {
public: PostalCode() { setLabel("ZIP/postal code"); };
};

class RegionSelector: public ComboBoxSetting, public TransientStorage {
    Q_OBJECT
public:
    RegionSelector() {
        setLabel("Region");
        fillSelections();
    };

public slots:
    void fillSelections();
};

class ProviderSelector: public ComboBoxSetting, public TransientStorage {
    Q_OBJECT
public:
    ProviderSelector(const QString& _grabber) :
        grabber(_grabber) { setLabel("Provider"); };

public slots:
    void fillSelections(const QString& location);

protected:
    QString grabber;
};

class XMLTV_na_config: public VerticalConfigurationGroup {
    Q_OBJECT
public:
    XMLTV_na_config(const VideoSource& _parent): parent(_parent) {
        setLabel("tv_grab_na configuration");
        postalcode = new PostalCode();
        addChild(postalcode);

        provider = new ProviderSelector("tv_grab_na");
        addChild(provider);

        connect(postalcode, SIGNAL(valueChanged(const QString&)),
                this, SLOT(fillProviderSelections(const QString&)));
    };

    virtual void save(QSqlDatabase* db);

protected slots:
     void fillProviderSelections(const QString& maybePostalCode) {
         if (QRegExp("\\d{5}").exactMatch(maybePostalCode) ||
             QRegExp("[a-z]\\d[a-z]\\d[a-z]\\d", false).exactMatch(maybePostalCode))
                 provider->fillSelections(maybePostalCode);
     }

protected:
    const VideoSource& parent;
    PostalCode* postalcode;
    ProviderSelector* provider;
};

class XMLTV_uk_config: public VerticalConfigurationGroup {
public:
    XMLTV_uk_config(const VideoSource& _parent): parent(_parent) {
        setLabel("tv_grab_uk configuration");
        region = new RegionSelector();
        addChild(region);

        provider = new ProviderSelector("tv_grab_uk");
        addChild(provider);

        connect(region, SIGNAL(valueChanged(const QString&)),
                provider, SLOT(fillSelections(const QString&)));
    };

    virtual void save(QSqlDatabase* db);

protected:
    const VideoSource& parent;
    RegionSelector* region;
    ProviderSelector* provider;
};

class Grabber_gist_config: public VerticalConfigurationGroup {
public:
    Grabber_gist_config(const VideoSource& _parent): parent(_parent) {
        setLabel("gist.com listings configuration");
        addChild(new UserID(parent));
    };

protected:
    const VideoSource& parent;
};

class XMLTV_generic_config: public LabelSetting {
public:
    XMLTV_generic_config(const VideoSource& _parent, QString _grabber):
        parent(_parent),
        grabber(_grabber) {
        setLabel(grabber);
        setValue("Configuration will run in the terminal window");
    };

    virtual void load(QSqlDatabase* db) { (void)db; };
    virtual void save(QSqlDatabase* db);

protected:
    const VideoSource& parent;
    QString grabber;
};

class XMLTVConfig: public VerticalConfigurationGroup, public TriggeredConfigurationGroup {
public:
    XMLTVConfig(const VideoSource& parent) {
        XMLTVGrabber* grabber = new XMLTVGrabber(parent);
        addChild(grabber);
        setTrigger(grabber);

        // only save settings for the selected grabber
        setSaveAll(false);

        addTarget("tv_grab_na", new XMLTV_na_config(parent));
        grabber->addSelection("North America (xmltv)", "tv_grab_na");

        //addTarget("gist", new Grabber_gist_config(parent));
        //grabber->addSelection("North America (gist)", "gist");

        addTarget("tv_grab_de", new XMLTV_generic_config(parent, "tv_grab_de"));
        grabber->addSelection("Germany/Austria", "tv_grab_de");

        addTarget("tv_grab_sn", new XMLTV_generic_config(parent, "tv_grab_sn"));
        grabber->addSelection("Sweden/Norway","tv_grab_sn");

        addTarget("tv_grab_uk", new XMLTV_uk_config(parent));
        grabber->addSelection("United Kingdom","tv_grab_uk");

        addTarget("tv_grab_uk_rt", new XMLTV_generic_config(parent, "tv_grab_uk_rt"));
        grabber->addSelection("United Kingdom (alternative)","tv_grab_uk_rt");

        addTarget("tv_grab_nz", new XMLTV_generic_config(parent, "tv_grab_nz"));
        grabber->addSelection("New Zealand", "tv_grab_nz");

        addTarget("tv_grab_fi", new XMLTV_generic_config(parent, "tv_grab_fi"));
        grabber->addSelection("Finland", "tv_grab_fi");

        addTarget("tv_grab_es", new XMLTV_generic_config(parent, "tv_grab_es"));
        grabber->addSelection("Spain", "tv_grab_es");

        addTarget("tv_grab_nl", new XMLTV_generic_config(parent, "tv_grab_nl"));
        grabber->addSelection("Holland", "tv_grab_nl");
    };
};

class VideoSource: public ConfigurationWizard {
public:
    VideoSource() {
        // must be first
        addChild(id = new ID());

        ConfigurationGroup *group = new VerticalConfigurationGroup(false);
        group->setLabel("Video source setup");
        group->addChild(name = new Name(*this));
        group->addChild(new XMLTVConfig(*this));
        addChild(group);
    };
        
    int getSourceID(void) const { return id->intValue(); };

    void loadByID(QSqlDatabase* db, int id);

    static void fillSelections(QSqlDatabase* db, SelectSetting* setting);
    static QString idToName(QSqlDatabase* db, int id);

    QString getSourceName(void) const { return name->getValue(); };
        

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("videosource", "sourceid") {
            setName("VideoSourceName");
            setVisible(false);
        };
        virtual QWidget* configWidget(ConfigurationGroup *cg, 
                                      QWidget* parent,
                                      const char* widgetName = 0) {
            (void)cg; (void)parent; (void)widgetName;
            return NULL;
        };
    };
    class Name: virtual public VSSetting,
                virtual public LineEditSetting {
    public:
        Name(const VideoSource& parent):
            VSSetting(parent, "name") {
            setLabel("Video source name");
        };
    };

private:
    ID* id;
    Name* name;
};

class CaptureCard;
class CCSetting: virtual public Setting, public SimpleDBStorage {
protected:
    CCSetting(const CaptureCard& _parent, QString name):
        SimpleDBStorage("capturecard", name),
        parent(_parent) {
        setName(name);
    };

    int getCardID(void) const;

protected:
    virtual QString setClause(void);
    virtual QString whereClause(void);
private:
    const CaptureCard& parent;
};

class VideoDevice: public PathSetting, public CCSetting {
public:
    VideoDevice(const CaptureCard& parent):
        PathSetting(true),
        CCSetting(parent, "videodevice") {
        setLabel("Video device");
        QDir dev("/dev", "video*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/v4l");
        fillSelectionsFromDir(dev);
    };

    static QStringList probeInputs(QString device);
};

class VbiDevice: public PathSetting, public CCSetting {
public:
    VbiDevice(const CaptureCard& parent):
        PathSetting(true),
        CCSetting(parent, "vbidevice") {
        setLabel("VBI device");
        QDir dev("/dev", "vbi*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/v4l");
        fillSelectionsFromDir(dev);
    };
};

class AudioDevice: public PathSetting, public CCSetting {
public:
    AudioDevice(const CaptureCard& parent):
        PathSetting(true),
        CCSetting(parent, "audiodevice") {
        setLabel("Audio device");
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        dev.setPath("/dev/sound");
        fillSelectionsFromDir(dev);
        addSelection("(None)", "/dev/null");
    };
};

class AudioRateLimit: public ComboBoxSetting, public CCSetting {
public:
    AudioRateLimit(const CaptureCard& parent):
        CCSetting(parent, "audioratelimit") {
        setLabel("Audio sampling rate limit");
        addSelection("(None)", "0");
        addSelection("32000");
        addSelection("44100");
        addSelection("48000");
    };
};

class TunerCardInput: public ComboBoxSetting, public CCSetting {
    Q_OBJECT
public:
    TunerCardInput(const CaptureCard& parent):
        CCSetting(parent, "defaultinput") {
        setLabel("Default input");
    };

public slots:
    void fillSelections(const QString& device);
};

class CardType: public ComboBoxSetting, public CCSetting {
public:
    CardType(const CaptureCard& parent);
};

class CaptureCard: public ConfigurationWizard {
public:
    CaptureCard() {
        VideoDevice* device;
        TunerCardInput* input;

        // must be first
        addChild(id = new ID());

        ConfigurationGroup *devices = new VerticalConfigurationGroup(false);
        devices->setLabel("Capture card");
        devices->addChild(device = new VideoDevice(*this));
        devices->addChild(new VbiDevice(*this));
        devices->addChild(new AudioDevice(*this));
        devices->addChild(new AudioRateLimit(*this));
        devices->addChild(input = new TunerCardInput(*this));
        devices->addChild(new CardType(*this));
        addChild(devices);

        addChild(new Hostname(*this));

        connect(device, SIGNAL(valueChanged(const QString&)),
                input, SLOT(fillSelections(const QString&)));
        input->fillSelections(device->getValue());
    };

    int getCardID(void) const {
        return id->intValue();
    };

    void loadByID(QSqlDatabase* db, int id);

    static void fillSelections(QSqlDatabase* db, SelectSetting* setting);

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("capturecard", "cardid") {
            setVisible(false);
            setName("ID");
        };
    };

    class Hostname: public HostnameSetting, public CCSetting {
    public:
        Hostname(const CaptureCard& parent): CCSetting(parent, "hostname") {};
    };

private:
    ID* id;
};

class CardInput;
class CISetting: virtual public Setting, public SimpleDBStorage {
protected:
    CISetting(const CardInput& _parent, QString name):
        SimpleDBStorage("cardinput", name),
        parent(_parent) {
        setName(name);
    };

    int getInputID(void) const;

    void fillSelections(QSqlDatabase* db);

protected:
    virtual QString setClause(void);
    virtual QString whereClause(void);
private:
    const CardInput& parent;
};

class CardID: public SelectLabelSetting, public CISetting {
public:
    CardID(const CardInput& parent):
        CISetting(parent, "cardid") {
        setLabel("Capture device");
    };

    virtual void load(QSqlDatabase* db) {
        fillSelections(db);
        CISetting::load(db);
    };

    void fillSelections(QSqlDatabase* db) {
        CaptureCard::fillSelections(db, this);
    };
};


class SourceID: public ComboBoxSetting, public CISetting {
public:
    SourceID(const CardInput& parent):
        CISetting(parent, "sourceid") {
        setLabel("Video source");
        addSelection("(None)", "0");
    };

    virtual void load(QSqlDatabase* db) {
        fillSelections(db);
        CISetting::load(db);
    };

    void fillSelections(QSqlDatabase* db) {
        clearSelections();
        addSelection("(None)", "0");
        VideoSource::fillSelections(db, this);
    };
};

class InputName: public LabelSetting, public CISetting {
public:
    InputName(const CardInput& parent):
        CISetting(parent, "inputname") {
        setLabel("Input");
    };
};

class ExternalChannelCommand: public LineEditSetting, public CISetting {
public:
    ExternalChannelCommand(const CardInput& parent):
        CISetting(parent,"externalcommand") {
        setLabel("External channel change command");
        setValue("");
        setHelpText("If specified, this command will be run to change the "
                    "channel for inputs which do not have a tuner.  The "
                    "first argument will be the channel number.");
    };
};

class PresetTuner: public LineEditSetting, public CISetting {
public:
    PresetTuner(const CardInput& parent):
        CISetting(parent, "tunechan") {
        setLabel("Preset tuner to channel");
        setValue("");
        setHelpText("If specified, the tuner will change to this channel "
                    "when the input is selected.  This is only useful if you "
                    "use your tuner input with an external channel changer.");
    };
};

class StartingChannel: public LineEditSetting, public CISetting {
public:
    StartingChannel(const CardInput& parent):
        CISetting(parent, "startchan") {
        setLabel("Starting channel");
        setValue("3");
        setHelpText("LiveTV will change to the above channel when the "
                    "input is first selected.");
    };
};

class CardInput: public ConfigurationWizard {
public:
    CardInput() {
        addChild(id = new ID());

        ConfigurationGroup *group = new VerticalConfigurationGroup(false);
        group->setLabel("Connect source to input");
        group->addChild(cardid = new CardID(*this));
        group->addChild(inputname = new InputName(*this));
        group->addChild(sourceid = new SourceID(*this));
        group->addChild(new ExternalChannelCommand(*this));
        group->addChild(new PresetTuner(*this));
        group->addChild(new StartingChannel(*this));
        addChild(group);
    };

    int getInputID(void) const { return id->intValue(); };

    void loadByID(QSqlDatabase* db, int id);
    void loadByInput(QSqlDatabase* db, int cardid, QString input);
    QString getSourceName(void) const { return sourceid->getSelectionLabel(); };

    virtual void save(QSqlDatabase* db);

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("cardinput", "cardid") {
            setVisible(false);
            setName("CardInputID");
        };
        virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                      const char* widgetName = 0) {
            (void)cg; (void)parent; (void)widgetName;
            return NULL;
        };
    };

private:
    ID* id;
    CardID* cardid;
    InputName* inputname;
    SourceID* sourceid;
    QSqlDatabase* db;
};

class CaptureCardEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    CaptureCardEditor(QSqlDatabase* _db):
        db(_db) {
        setLabel("Capture cards");
    };

    virtual int exec(QSqlDatabase* db);
    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db) { (void)db; };

protected slots:
    void edit(int id) {
        CaptureCard cc;

        if (id != 0)
            cc.loadByID(db,id);

        cc.exec(db);
    };

protected:
    QSqlDatabase* db;
};

class VideoSourceEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    VideoSourceEditor(QSqlDatabase* _db):
        db(_db) {
        setLabel("Video sources");
    };

    virtual int exec(QSqlDatabase* db);
    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db) { (void)db; };

protected slots:
    void edit(int id) {
        VideoSource vs;

        if (id != 0)
            vs.loadByID(db,id);

        vs.exec(db);
    };

protected:
    QSqlDatabase* db;
};

class CardInputEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    CardInputEditor(QSqlDatabase* _db):
        db(_db) {
        setLabel("Input connections");
    };
    virtual ~CardInputEditor();

    virtual int exec(QSqlDatabase* db);
    virtual void load(QSqlDatabase* db);
    virtual void save(QSqlDatabase* db) { (void)db; };

protected slots:
    void edit(int id) {
        cardinputs[id]->exec(db);
    };

protected:
    vector<CardInput*> cardinputs;
    QSqlDatabase* db;
};

#endif

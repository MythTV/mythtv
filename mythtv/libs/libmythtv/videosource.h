#ifndef VIDEOSOURCE_H
#define VIDEOSOURCE_H

#include "libmyth/settings.h"
#include <qlistview.h>
#include <qdialog.h>
#include <qregexp.h>
#include <vector>

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
             QRegExp("[a-z]\\d[a-z]\\d[a-z]\\d").exactMatch(maybePostalCode))
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
        grabber->addSelection("North America", "tv_grab_na");

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
    };
};

class VideoSource: public VerticalConfigurationGroup, public ConfigurationDialog {
public:
    VideoSource() {
        // must be first
        addChild(id = new ID());
        addChild(name = new Name(*this));
        addChild(new XMLTVConfig(*this));
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
        addSelection("/dev/video");
        addSelection("/dev/video0");
        addSelection("/dev/video1");
        addSelection("/dev/video2");
        addSelection("/dev/v4l/video0");
        addSelection("/dev/v4l/video1");
        addSelection("/dev/v4l/video2");
    };
};

class VbiDevice: public PathSetting, public CCSetting {
public:
    VbiDevice(const CaptureCard& parent):
        PathSetting(true),
        CCSetting(parent, "vbidevice") {
        setLabel("VBI device");
        addSelection("/dev/vbi");
        addSelection("/dev/vbi0");
        addSelection("/dev/vbi1");
        addSelection("/dev/vbi2");
        addSelection("/dev/v4l/vbi0");
        addSelection("/dev/v4l/vbi1");
        addSelection("/dev/v4l/vbi2");
    };
};

class AudioDevice: public PathSetting, public CCSetting {
public:
    AudioDevice(const CaptureCard& parent):
        PathSetting(true),
        CCSetting(parent, "audiodevice") {
        setLabel("Audio device");
        addSelection("/dev/dsp");
        addSelection("/dev/dsp1");
        addSelection("/dev/dsp2");
        addSelection("/dev/dsp3");
        addSelection("/dev/sound/dsp");
        addSelection("/dev/sound/dsp1");
        addSelection("/dev/sound/dsp2");
        addSelection("/dev/sound/dsp3");
    };
};

// unused
class CardType: public ComboBoxSetting, public CCSetting {
public:
    CardType(const CaptureCard& parent):
        CCSetting(parent, "cardtype") {
        setLabel("Card type");
        addSelection("V4L");
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

// XXX, this should find available inputs in a reasonable way
class TunerCardInput: public ComboBoxSetting, public CCSetting {
public:
    TunerCardInput(const CaptureCard& parent):
        CCSetting(parent, "defaultinput") {
        setLabel("Default input");
        addSelection("Television");
        addSelection("Composite1");
        addSelection("Composite3");
        addSelection("S-Video");
    };
};

class CaptureCard: public VerticalConfigurationGroup, public ConfigurationDialog {
public:
    CaptureCard() {
        setLabel("Capture card");
        // must be first
        addChild(id = new ID());
        addChild(new VideoDevice(*this));
        addChild(new VbiDevice(*this));
        addChild(new AudioDevice(*this));
        addChild(new AudioRateLimit(*this));
        addChild(new TunerCardInput(*this));
        addChild(new Hostname(*this));
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
                    "first argument will be the channel number");
    };
};

class CardInput: public VerticalConfigurationGroup, public ConfigurationDialog {
public:
    CardInput() {
        setLabel("Connect source to input");
        addChild(id = new ID());
        addChild(cardid = new CardID(*this));
        addChild(inputname = new InputName(*this));
        addChild(sourceid = new SourceID(*this));
        addChild(new ExternalChannelCommand(*this));
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

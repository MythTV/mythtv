#ifndef VIDEOSOURCE_H
#define VIDEOSOURCE_H

#include "libmyth/settings.h"
#include "libmyth/mythcontext.h"
#include <qlistview.h>
#include <qdialog.h>
#include <vector>

class VideoSource;
class VSSetting: public SimpleDBStorage {
protected:
    VSSetting(const VideoSource& _parent, QString table, QString name):
        SimpleDBStorage(table, name),
        parent(_parent) {
        setName(name);
    };

    virtual QString setClause(void);
    virtual QString whereClause(void);

    const VideoSource& parent;
};

class VideoSource: public VerticalConfigurationGroup {
public:
    VideoSource() {
        // must be first
        addChild(id = new ID());
        addChild(new Name(*this));
    };
        
    int getSourceID(void) const { return id->intValue(); };

    static void fillSelections(QSqlDatabase* db, StringSelectSetting* setting);

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("videosource", "sourceid") {
            setName("VideoSourceName");
            setVisible(false);
        };
        virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0) {
            (void)parent; (void)widgetName;
            return NULL;
        };
    };
    class Name: virtual public VSSetting,
                virtual public LineEditSetting {
    public:
        Name(const VideoSource& parent):
            VSSetting(parent, "videosource", "name") {
            setLabel("Video source name");
        };
    };
private:
    ID* id;
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
        addSelection("/dev/v4l/video0");
        addSelection("/dev/v4l/video1");
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
        addSelection("/dev/sound/dsp");
    };
};

class CardType: public ComboBoxSetting, public CCSetting {
public:
    CardType(const CaptureCard& parent):
        CCSetting(parent, "cardtype") {
        setLabel("Card type");
        addSelection("V4L");
    };
};

class AudioRateLimit: public SpinBoxSetting, public CCSetting {
public:
    AudioRateLimit(const CaptureCard& parent):
        SpinBoxSetting(0, 48000, 100),
        CCSetting(parent, "audioratelimit") {
        setLabel("Audio sampling rate limit");
        setValue(0);
    };
};

class CaptureCard: public VerticalConfigurationGroup {
public:
    CaptureCard() {
        setLabel("Capture card");
        // must be first
        addChild(id = new ID());
        addChild(new VideoDevice(*this));
        addChild(new AudioDevice(*this));
        addChild(new AudioRateLimit(*this));
    };

    int getCardID(void) const {
        return id->intValue();
    };

    static void fillSelections(QSqlDatabase* db, StringSelectSetting* setting);

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("capturecard", "cardid") {
            setVisible(false);
            setName("ID");
        };
        virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0) {
            (void)parent; (void)widgetName;
            return NULL;
        };
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

class CardID: public ComboBoxSetting, public CISetting {
public:
    CardID(const CardInput& parent):
        CISetting(parent, "cardid") {
        setLabel("Capture device");
    };

    void fillSelections(QSqlDatabase* db) {
        CaptureCard::fillSelections(db, this);
    };
};


class VideoInputSelector: public TriggeredConfigurationGroup,
                          public VerticalConfigurationGroup {
    Q_OBJECT
public:
    VideoInputSelector(const CardInput& _parent):
        parent(_parent) {
        setLabel("Input");
        addChild(cardid = new CardID(parent));
        setTrigger(cardid);

        connect(cardid, SIGNAL(selectionAdded(const QString&,
                                              QString)),
                this, SLOT(probeInputs(const QString&,
                                       QString)));
    };

    void fillSelections(QSqlDatabase* db);

protected slots:
    void probeInputs(const QString& videoDevice, QString cardID);

private:
    CardID* cardid;
    const CardInput& parent;
};

class SourceID: public ComboBoxSetting, public CISetting {
public:
    SourceID(const CardInput& parent):
        CISetting(parent, "sourceid") {
        setLabel("Video source");
    };

    void fillSelections(QSqlDatabase* db) {
        VideoSource::fillSelections(db, this);
    };
};

class InputName: public ComboBoxSetting, public CISetting {
public:
    InputName(const CardInput& parent):
        CISetting(parent, "inputname") {
    };
};

class CardInput: public VerticalConfigurationGroup {
public:
    CardInput() {
        setLabel("Associate input with source");
        addChild(id = new ID());
        addChild(selector = new VideoInputSelector(*this));
        addChild(sourceid = new SourceID(*this));
    };

    void fillSelections(QSqlDatabase* db);

    int getInputID(void) const { return id->intValue(); };

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("cardinput", "cardid") {
            setVisible(false);
            setName("CardInputID");
        };
        virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0) {
            (void)parent; (void)widgetName;
            return NULL;
        };
    };

private:
    ID* id;
    VideoInputSelector* selector;
    SourceID* sourceid;
    QSqlDatabase* db;
};

class CaptureCardDialog: public ConfigurationWizard {
public:
    CaptureCardDialog(MythContext *context) : ConfigurationWizard(context),
                                              ConfigurationDialog(context) {
        CaptureCard* cc = new CaptureCard();
        cc->setLabel("Configure new capture card");
        addChild(cc);
    };
};

class VideoSourceDialog: public ConfigurationWizard {
public:
    VideoSourceDialog(MythContext *context) : ConfigurationWizard(context),
                                              ConfigurationDialog(context) {
        VideoSource* vs = new VideoSource();
        vs->setLabel("Configure new video source");
        addChild(vs);
    };
};

class CardInputDialog: public ConfigurationWizard {
public:
    CardInputDialog(MythContext *context) : ConfigurationWizard(context),
                                            ConfigurationDialog(context) {
        CardInput* ci = new CardInput();
        ci->setLabel("Configure capture card input");
        addChild(ci);
    };
};

#endif

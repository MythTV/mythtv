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

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("videosource", "sourceid") {
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

class CaptureCard: public VerticalConfigurationGroup {
public:
    CaptureCard() {
        // must be first
        addChild(id = new ID());
    };

    int getCardID(void) const {
        return id->intValue();
    };

private:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("capturecard", "cardid") {
            setVisible(false);
        };
        virtual QWidget* configWidget(QWidget* parent, const char* widgetName = 0) {
            (void)parent; (void)widgetName;
            return NULL;
        };
    };

    class CCSetting: virtual public Setting, public SimpleDBStorage {
    protected:
        CCSetting(const CaptureCard& _parent, QString name):
            SimpleDBStorage("capturecard", name),
            parent(_parent) {
            setName(name);
        };

        int getCardID(void) const { return parent.getCardID(); };

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
            CCSetting(parent, "VideoDevice") {
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
            CCSetting(parent, "AudioDevice") {
            addSelection("/dev/dsp");
            addSelection("/dev/dsp1");
            addSelection("/dev/sound/dsp");
        };
    };

private:
    ID* id;
};

#endif

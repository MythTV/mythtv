#ifndef RECORDINGPROFILE_H
#define RECORDINGPROFILE_H

#include "mythtvexp.h"
#include "standardsettings.h"
#include "mythdbcon.h"

const QString availProfiles[] =
      {"Default", "Live TV", "High Quality", "Low Quality", "" };

class RecordingProfile;
class VideoCompressionSettings;
class AudioCompressionSettings;
class V4L2util;

// A parameter associated with the profile itself
class RecordingProfileStorage : public SimpleDBStorage
{
  public:
    RecordingProfileStorage(StandardSetting *_setting,
                            const RecordingProfile &parentProfile,
                            QString name) :
        SimpleDBStorage(_setting, "recordingprofiles", name),
        m_parent(parentProfile)
    {
    }

  protected:
    virtual QString GetWhereClause(MSqlBindings &bindings) const;

    const RecordingProfile &m_parent;
};

class ImageSize;
class TranscodeResize;
class TranscodeLossless;
class TranscodeFilters;

class MTV_PUBLIC RecordingProfile : public GroupSetting
{
  Q_OBJECT
  protected:
    class ID : public AutoIncrementSetting {
      public:
        ID():
            AutoIncrementSetting("recordingprofiles", "id") {
            setVisible(false);
        };
    };

    class Name: public MythUITextEditSetting
    {
      public:
        explicit Name(const RecordingProfile &parent):
            MythUITextEditSetting(
                new RecordingProfileStorage(this, parent, "name"))
        {
            setEnabled(false);
            setLabel(QObject::tr("Profile name"));
            setName("name");
        }

      // -=>TODO: Qt4 can't have nested classes with slots/signals
      //          Is this slot even used????
      //public slots:
      public:
        virtual void setValue(const QString &newValue)
        {
            bool editable = (newValue != "Default") && (newValue != "Live TV");
            setEnabled(editable);

            MythUITextEditSetting::setValue(newValue);
        }
        using StandardSetting::setValue;
    };

  public:
    // initializers
    explicit RecordingProfile(QString profName = QString());
    virtual ~RecordingProfile(void);
    virtual void loadByID(int id);
    virtual bool loadByType(const QString &name, const QString &cardtype,
                            const QString &videodev);
    virtual bool loadByGroup(const QString &name, const QString &group);
    virtual void CompleteLoad(int profileId, const QString &type,
                              const QString &name);
    virtual bool canDelete(void);
    virtual void deleteEntry(void);

    // sets
    void setCodecTypes();
    void setName(const QString& newName)
        { name->setValue(newName); }

    // gets
    const ImageSize& getImageSize(void) const { return *imageSize;       }
    int     getProfileNum(void)         const { return id->getValue().toInt(); }
    QString getName(void)               const { return name->getValue(); }
    QString groupType(void)             const;

    // static functions
    static QString getName(int id);

    // Hardcoded DB group values
    typedef enum RecProfileGroups
    {
        AllGroups            =  0,
        SoftwareEncoderGroup =  1,
        HardwareMPEG2Group   =  2,
        HardwareMJPEGGroup   =  3,
        HardwareHDTVGroup    =  4,
        DVBGroup             =  5,
        TranscoderGroup      =  6,
        FireWireGroup        =  7,
        USBMPEG4Group        =  8,
        FreeboxGroup         = 12,
        HDHomeRunGroup       = 13,
        CRCIGroup            = 14,
        ASIGroup             = 15,
        OCURGroup            = 16,
        CetonGroup           = 17
    } RecProfileGroup;

    static QMap<int, QString> GetProfiles(RecProfileGroup group = AllGroups);
    static QMap<int, QString> GetTranscodingProfiles();
    static void fillSelections(GroupSetting* setting,
                               int group, bool foldautodetect = false);

    // constants
    static const uint TranscoderAutodetect = 0; ///< sentinel value

  private slots:
    void ResizeTranscode(const QString &val);
    void SetLosslessTranscode(const QString &val);
    void FiltersChanged(const QString &val);

  private:
    ID                       *id;
    Name                     *name;
    ImageSize                *imageSize;
    TranscodeResize          *tr_resize;
    TranscodeLossless        *tr_lossless;
    TranscodeFilters         *tr_filters;
    VideoCompressionSettings *videoSettings;
    AudioCompressionSettings *audioSettings;
    QString                   profileName;
    bool                      isEncoder;

    V4L2util                 *v4l2util;
};

class RecordingProfileEditor :
    public GroupSetting
{
    Q_OBJECT

  public:
    RecordingProfileEditor(int id, QString profName);
    virtual ~RecordingProfileEditor() {}

    virtual void Load(void);

  public slots:
    void ShowNewProfileDialog();
    void CreateNewProfile(QString);

  protected:
    int             group;
    QString         labelName;
};

#endif

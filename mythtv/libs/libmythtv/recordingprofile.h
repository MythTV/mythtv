#ifndef RECORDINGPROFILE_H
#define RECORDINGPROFILE_H

#include <array>

#include "libmythui/standardsettings.h"
#include "libmythbase/mythdbcon.h"
#include "mythtvexp.h"

const std::array<QString,4> kAvailProfiles
      {"Default", "Live TV", "High Quality", "Low Quality" };

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
                            const QString& name) :
        SimpleDBStorage(_setting, "recordingprofiles", name),
        m_parent(parentProfile)
    {
    }

  protected:
    QString GetWhereClause(MSqlBindings &bindings) const override; // SimpleDBStorage

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
            setReadOnly(true);
            setLabel(QObject::tr("Profile name"));
            setName("name");
        }

        ~Name() override
        {
            delete GetStorage();
        }

      // -=>TODO: Qt4 can't have nested classes with slots/signals
      //          Is this slot even used????
      //public slots:
      public:
        void setValue(const QString &newValue) override // StandardSetting
        {
            bool readonly = (newValue == "Default") || (newValue == "Live TV");
            setReadOnly(readonly);

            MythUITextEditSetting::setValue(newValue);
        }
        using StandardSetting::setValue;
    };

  public:
    // initializers
    explicit RecordingProfile(const QString& profName = QString());
    ~RecordingProfile(void) override;
    virtual void loadByID(int id);
    virtual bool loadByType(const QString &name, const QString &cardtype,
                            const QString &videodev);
    virtual bool loadByGroup(const QString &name, const QString &group);
    virtual void CompleteLoad(int profileId, const QString &type,
                              const QString &name);
    bool canDelete(void) override; // GroupSetting
    void deleteEntry(void) override; // GroupSetting

    // sets
    void setCodecTypes();
    void setName(const QString& newName) override // StandardSetting
        { m_name->setValue(newName); }

    // gets
    const ImageSize& getImageSize(void) const { return *m_imageSize;       }
    int     getProfileNum(void)         const { return m_id->getValue().toInt(); }
    QString getName(void)               const { return m_name->getValue(); }
    QString groupType(void)             const;

    // static functions
    static QString getName(int id);

    // Hardcoded DB group values
    enum RecProfileGroup : std::uint8_t
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
    };

    static QMap<int, QString> GetProfiles(RecProfileGroup group = AllGroups);
    static QMap<int, QString> GetTranscodingProfiles();
    static void fillSelections(GroupSetting* setting,
                               int group, bool foldautodetect = false);

    // constants
    static const uint kTranscoderAutodetect = 0; ///< sentinel value

  private slots:
    void ResizeTranscode(const QString &val);
    void SetLosslessTranscode(const QString &val);
    void FiltersChanged(const QString &val);

  private:
    ID                       *m_id            {nullptr};
    Name                     *m_name          {nullptr};
    ImageSize                *m_imageSize     {nullptr};
    TranscodeResize          *m_trResize      {nullptr};
    TranscodeLossless        *m_trLossless    {nullptr};
    TranscodeFilters         *m_trFilters     {nullptr};
    VideoCompressionSettings *m_videoSettings {nullptr};
    AudioCompressionSettings *m_audioSettings {nullptr};
    QString                   m_profileName;
    bool                      m_isEncoder     {true};

    V4L2util                 *m_v4l2util      {nullptr};
};

class RecordingProfileEditor :
    public GroupSetting
{
    Q_OBJECT

  public:
    RecordingProfileEditor(int id, QString profName);
    ~RecordingProfileEditor() override = default;

    void Load(void) override; // StandardSetting

  public slots:
    void ShowNewProfileDialog() const;
    void CreateNewProfile(const QString &profName);

  protected:
    int             m_group;
    QString         m_labelName;
};

#endif

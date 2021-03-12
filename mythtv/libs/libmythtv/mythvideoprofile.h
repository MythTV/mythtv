#ifndef VIDEO_DISPLAY_PROFILE_H_
#define VIDEO_DISPLAY_PROFILE_H_

// Qt
#include <QStringList>
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
#include <QMutex>
#else
#include <QRecursiveMutex>
#endif
#include <QSize>
#include <QMap>

// MythTV
#include "mythtvexp.h"
#include "mythcontext.h"

// Std
#include <vector>

#define DEINT_QUALITY_NONE   QString("none")
#define DEINT_QUALITY_LOW    QString("low")
#define DEINT_QUALITY_MEDIUM QString("medium")
#define DEINT_QUALITY_HIGH   QString("high")
#define DEINT_QUALITY_SHADER QString("shader")
#define DEINT_QUALITY_DRIVER QString("driver")
#define UPSCALE_DEFAULT QString("bilinear")
#define UPSCALE_HQ1     QString("bicubic")

#define COND_WIDTH    "cond_width"
#define COND_HEIGHT   "cond_height"
#define COND_RATE     "cond_framerate"
#define COND_CODECS   "cond_codecs"
#define PREF_DEC      "pref_decoder"
#define PREF_CPUS     "pref_max_cpus"
#define PREF_LOOP     "pref_skiploop"
#define PREF_RENDER   "pref_videorenderer"
#define PREF_DEINT1X  "pref_deint0"
#define PREF_DEINT2X  "pref_deint1"
#define PREF_PRIORITY "pref_priority"
#define PREF_UPSCALE  "pref_upscale"

#define VIDEO_MAX_CPUS (16U)

struct RenderOptions
{
    QStringList               *renderers;
    QMap<QString,QStringList> *safe_renderers;
    QMap<QString,QStringList> *render_group;
    QMap<QString,uint>        *priorities;
    QStringList               *decoders;
    QMap<QString,QStringList> *equiv_decoders;
};

class MTV_PUBLIC MythVideoProfileItem
{
  public:
    MythVideoProfileItem() = default;
   ~MythVideoProfileItem() = default;

    // Sets
    void    Clear();
    void    SetProfileID(uint Id);
    void    Set(const QString &Value, const QString &Data);

    // Gets
    uint    GetProfileID() const;
    QString Get(const QString &Value) const;
    uint    GetPriority() const;
    QMap<QString,QString> GetAll() const;

    // Other
    bool CheckRange(const QString& Key, float Value, bool *Ok = nullptr) const;
    bool CheckRange(const QString& Key, int Value, bool *Ok = nullptr) const;
    bool CheckRange(const QString& Key, float FValue, int IValue, bool IsFloat, bool *Ok = nullptr) const;
    bool IsMatch(QSize Size, float Framerate, const QString &CodecName,
                 const QStringList &DisallowedDecoders = QStringList()) const;
    auto IsValid() const;
    bool operator<(const MythVideoProfileItem &Other) const;
    QString toString() const;

  private:
    uint       m_profileid        { 0 };
    QMap<QString,QString> m_pref  { };
};

class MTV_PUBLIC MythVideoProfile : public QObject
{
    Q_OBJECT

  signals:
    void DeinterlacersChanged(const QString& Single, const QString& Double);
    void UpscalerChanged(const QString& Upscaler);

  public:
    MythVideoProfile();
   ~MythVideoProfile() override = default;

    void    SetInput(QSize Size, float Framerate = 0, const QString &CodecName = QString(),
                     const QStringList &DisallowedDecoders = QStringList());
    void    SetOutput(float Framerate);
    float   GetOutput() const;
    void    SetVideoRenderer(const QString &VideoRenderer);
    QString GetDecoder() const;
    bool    IsDecoderCompatible(const QString &Decoder) const;
    uint    GetMaxCPUs() const ;
    bool    IsSkipLoopEnabled() const;
    QString GetVideoRenderer() const;
    QString toString() const;
    QString GetSingleRatePreferences() const;
    QString GetDoubleRatePreferences() const;
    QString GetUpscaler() const;

    // Statics
    static void        InitStatics(bool Reinit = false);
    static const QList<QPair<QString,QString> >& GetDeinterlacers();
    static QStringList GetDecoders();
    static QStringList GetDecoderNames();
    static std::vector<std::pair<QString,QString>> GetUpscalers();
    static QString     GetDecoderName(const QString &Decoder);
    static QString     GetDecoderHelp(const QString &Decoder = QString());
    static QString     GetVideoRendererName(const QString &Renderer);
    static QString     GetDefaultProfileName(const QString &HostName);
    static void        SetDefaultProfileName(const QString &ProfileName, const QString &HostName);
    static uint        GetProfileGroupID(const QString &ProfileName, const QString &HostName);
    static QStringList GetProfiles(const QString &HostName);
    static bool        DeleteProfileGroup(const QString &GroupName, const QString &HostName);
    static uint        CreateProfileGroup(const QString &ProfileName, const QString &HostName);
    static void        CreateProfile(uint GroupId, uint Priority,
                                     const QString& Width, const QString& Height,
                                     const QString& Codecs, const QString& Decoder,
                                     uint MaxCpus, bool SkipLoop, const QString& VideoRenderer,
                                     const QString& Deint1, const QString& Deint2,
                                     const QString& Upscale = UPSCALE_DEFAULT);
    static void        CreateProfiles(const QString &HostName);
    static QStringList GetVideoRenderers(const QString &Decoder);
    static QString     GetVideoRendererHelp(const QString &Renderer);
    static QString     GetPreferredVideoRenderer(const QString &Decoder);
    static QStringList GetFilteredRenderers(const QString &Decoder, const QStringList &Renderers);
    static QString     GetBestVideoRenderer(const QStringList &Renderers);
    static std::vector<MythVideoProfileItem> LoadDB(uint GroupId);
    static bool        DeleteDB(uint GroupId, const std::vector<MythVideoProfileItem>& Items);
    static bool        SaveDB(uint GroupId, std::vector<MythVideoProfileItem>& Items);

  private:
    std::vector<MythVideoProfileItem>::const_iterator
            FindMatch(QSize Size, float Framerate, const QString &CodecName,
                      const QStringList& DisallowedDecoders = QStringList());
    void    LoadBestPreferences(QSize Size, float Framerate, const QString &CodecName,
                                const QStringList &DisallowedDecoders = QStringList());
    QString GetPreference(const QString &Key) const;
    void    SetPreference(const QString &Key, const QString &Value);

  private:
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    mutable QMutex        m_lock                { QMutex::Recursive };
#else
    mutable QRecursiveMutex m_lock;
#endif
    QSize                 m_lastSize            { 0, 0 };
    float                 m_lastRate            { 0.0F };
    QString               m_lastCodecName       { };
    QMap<QString,QString> m_currentPreferences  { };
    std::vector<MythVideoProfileItem> m_allowedPreferences { };

#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    static inline QMutex                    kSafeLock = QMutex(QMutex::Recursive);
#else
    static inline QRecursiveMutex           kSafeLock;
#endif
    static inline bool                      kSafeInitialized = false;
    static inline QMap<QString,QStringList> kSafeRenderer = {};
    static inline QMap<QString,QStringList> kSafeRendererGroup = {};
    static inline QMap<QString,QStringList> kSafeEquivDec = {};
    static inline QStringList               kSafeCustom = {};
    static inline QMap<QString,uint>        kSafeRendererPriority = {};
    static inline QMap<QString,QString>     kDecName = {};
    static inline QMap<QString,QString>     kRendName = {};
    static inline QStringList               kSafeDecoders = {};
};

#endif

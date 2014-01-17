// -*- Mode: c++ -*-

#ifndef _VIDEO_DISPLAY_PROFILE_H_
#define _VIDEO_DISPLAY_PROFILE_H_

#include <vector>
using namespace std;

#include <QStringList>
#include <QMutex>
#include <QSize>
#include <QMap>

#include "mythtvexp.h"
#include "mythcontext.h"

typedef QMap<QString,QString>     pref_map_t;
typedef QMap<QString,QStringList> safe_map_t;
typedef QStringList               safe_list_t;
typedef QMap<QString,uint>        priority_map_t;

struct render_opts
{
    safe_list_t    *renderers;
    safe_map_t     *safe_renderers;
    safe_map_t     *deints;
    safe_map_t     *osds;
    safe_map_t     *render_group;
    priority_map_t *priorities;
    safe_list_t    *decoders;
    safe_map_t     *equiv_decoders;
};

class ProfileItem
{
  public:
    ProfileItem() : profileid(0) {}
    ~ProfileItem() {}

    void    Clear(void) { pref.clear(); }

    // Sets
    void    SetProfileID(uint id) { profileid = id; }
    void    Set(const QString &value, const QString &data)
        { pref[value] = data; }


    // Gets
    uint    GetProfileID(void) const { return profileid; }

    QString Get(const QString &value) const
    {
        pref_map_t::const_iterator it = pref.find(value);
        if (it != pref.end())
            return *it;
        return QString::null;
    }

    uint GetPriority(void) const
    {
        QString tmp = Get("pref_priority");
        return (tmp.isEmpty()) ? 0 : tmp.toUInt();
    }

    pref_map_t GetAll(void) const { return pref; }

    // Other
    bool    IsMatch(const QSize &size, float rate) const;
    bool    IsValid(QString *reason = NULL) const;

    bool    operator<(const ProfileItem &other) const;

    QString toString(void) const;

  private:
    uint       profileid;
    pref_map_t pref;
};
typedef vector<ProfileItem>       item_list_t;

class MTV_PUBLIC VideoDisplayProfile
{
  public:
    VideoDisplayProfile();
    ~VideoDisplayProfile();

    void SetInput(const QSize &size);
    void SetOutput(float framerate);
    float GetOutput(void) const { return last_rate; }

    void SetVideoRenderer(const QString &video_renderer);
    bool CheckVideoRendererGroup(const QString &renderer);

    QString GetDecoder(void) const
        { return GetPreference("pref_decoder"); }
    bool    IsDecoderCompatible(const QString &decoder);

    uint GetMaxCPUs(void) const
        { return GetPreference("pref_max_cpus").toUInt(); }

    bool IsSkipLoopEnabled(void) const
        { return GetPreference("pref_skiploop").toInt(); }     

    QString GetVideoRenderer(void) const
        { return GetPreference("pref_videorenderer"); }

    QString GetOSDRenderer(void) const
        { return GetPreference("pref_osdrenderer"); }
    bool IsOSDFadeEnabled(void) const
        { return GetPreference("pref_osdfade").toInt(); }

    QString GetDeinterlacer(void) const
        { return GetPreference("pref_deint0"); }
    QString GetFallbackDeinterlacer(void) const
        { return GetPreference("pref_deint1"); }

    QString GetFilters(void) const
        { return GetPreference("pref_filters"); }

    QString GetFilteredDeint(const QString &override);

    QString toString(void) const;

    static QStringList GetDecoders(void);
    static QStringList GetDecoderNames(void);
    static QString     GetDecoderName(const QString &decoder);
    static QString     GetDecoderHelp(QString decoder = QString::null);

    static QString     GetDefaultProfileName(const QString &hostname);
    static void        SetDefaultProfileName(const QString &profilename,
                                             const QString &hostname);
    static uint        GetProfileGroupID(const QString &profilename,
                                         const QString &hostname);
    static QStringList GetProfiles(const QString &hostname);

    static bool        DeleteProfileGroup(const QString &groupname,
                                          const QString &hostname);
    static uint        CreateProfileGroup(const QString &groupname,
                                          const QString &hostname);

    static void        CreateProfile(
        uint grpid, uint priority,
        QString cmp0, uint width0, uint height0,
        QString cmp1, uint width1, uint height1,
        QString decoder, uint max_cpus, bool skiploop, QString videorenderer,
        QString osdrenderer, bool osdfade,
        QString deint0, QString deint1, QString filters);

    static void        DeleteProfiles(const QString &hostname);
    static void        CreateProfiles(const QString &hostname);
    static void        CreateNewProfiles(const QString &hostname);
    static void        CreateVDPAUProfiles(const QString &hostname);
    static void        CreateVDAProfiles(const QString &hostname);
    static void        CreateOpenGLProfiles(const QString &hostname);
    static void        CreateVAAPIProfiles(const QString &hostname);

    static QStringList GetVideoRenderers(const QString &decoder);
    static QString     GetVideoRendererHelp(const QString &renderer);
    static QString     GetPreferredVideoRenderer(const QString &decoder);
    static QStringList GetDeinterlacers(const QString &video_renderer);
    static QString     GetDeinterlacerName(const QString &short_name);
    static QString     GetDeinterlacerHelp(const QString &deint);
    static QStringList GetOSDs(const QString &video_renderer);
    static QString     GetOSDHelp(const QString &osd);
    static bool        IsFilterAllowed( const QString &video_renderer);

    static QStringList GetFilteredRenderers(const QString     &decoder,
                                            const QStringList &renderers);
    static QString     GetBestVideoRenderer(const QStringList &renderers);

    static item_list_t LoadDB(uint groupid);
    static bool DeleteDB(uint groupid, const item_list_t&);
    static bool SaveDB(uint groupid, item_list_t&);

    QString GetActualVideoRenderer(void) const
        { QString tmp = last_video_renderer; tmp.detach(); return tmp; }

  private:
    item_list_t::const_iterator FindMatch(const QSize &size, float framerate);
    void LoadBestPreferences(const QSize &size, float framerate);

    QString GetPreference(const QString &key) const;
    void    SetPreference(const QString &key, const QString &value);

    static void init_statics(void);

  private:
    mutable QMutex      lock;
    QSize               last_size;
    float               last_rate;
    QString             last_video_renderer;
    pref_map_t          pref;
    item_list_t         all_pref;

    static QMutex       safe_lock;
    static bool         safe_initialized;
    static safe_map_t   safe_renderer;
    static safe_map_t   safe_renderer_group;
    static safe_map_t   safe_deint;
    static safe_map_t   safe_osd;
    static safe_map_t   safe_equiv_dec;
    static safe_list_t  safe_custom;
    static priority_map_t safe_renderer_priority;
    static pref_map_t   dec_name;
    static safe_list_t  safe_decoders;
};

#endif // _VIDEO_DISPLAY_PROFILE_H_

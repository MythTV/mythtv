// -*- Mode: c++ -*-

#include "videodisplayprofile.h"
#include "mythcontext.h"
#include "mythdbcon.h"

bool ProfileItem::IsMatch(const QSize &size, float rate) const
{
    (void) rate; // we don't use the video output rate yet

    bool    match = true;
    QString cmp   = QString::null;

    for (uint i = 0; (i < 1000) && match; i++)
    {
        cmp = Get(QString("pref_cmp%1").arg(i));
        if (cmp.isEmpty())
            break;

        QStringList clist = QStringList::split(" ", cmp);
        if (clist.size() != 3)
            break;

        int width  = clist[1].toInt();
        int height = clist[2].toInt();
        cmp = clist[0];

        if (cmp == "==")
            match &= (size.width() == width) && (size.height() == height);
        else if (cmp == "!=")
            match &= (size.width() != width) && (size.height() != height);
        else if (cmp == "<=")
            match &= (size.width() <= width) && (size.height() <= height);
        else if (cmp == "<")
            match &= (size.width() <  width) && (size.height() <  height);
        else if (cmp == ">=")
            match &= (size.width() >= width) && (size.height() >= height);
        else if (cmp == ">")
            match &= (size.width() >  width) && (size.height() >  height);
        else
            match = false;
    }

    return match;
}

static QString toCommaList(const QStringList &list)
{
    QString ret = "";
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
        ret += *it + ",";

    if (ret.length())
        return ret.left(ret.length()-1);

    return "";
}

bool ProfileItem::IsValid(QString *reason) const
{
    QString     decoder   = Get("pref_decoder");
    QString     renderer  = Get("pref_videorenderer");
    if (decoder.isEmpty() || renderer.isEmpty())
    {
        if (reason)
            *reason = "Need a decoder and renderer";

        return false;
    }

    QStringList decoders  = VideoDisplayProfile::GetDecoders();
    if (!decoders.contains(decoder))
    {
        if (reason)
        {
            *reason = QString("decoder %1 is not supported (supported: %2)")
                .arg(decoder).arg(toCommaList(decoders));
        }

        return false;
    }

    QStringList renderers = VideoDisplayProfile::GetVideoRenderers(decoder);
    if (!renderers.contains(renderer))
    {
        if (reason)
        {
            *reason = QString("renderer %1 is not supported "
                       "w/decoder %2 (supported: %3)")
                .arg(renderer).arg(decoder).arg(toCommaList(renderers));
        }

        return false;
    }

    QStringList deints    = VideoDisplayProfile::GetDeinterlacers(renderer);
    QString     deint0    = Get("pref_deint0");
    QString     deint1    = Get("pref_deint1");
    if (!deint0.isEmpty() && !deints.contains(deint0))
    {
        if (reason)
        {
            *reason = QString("deinterlacer %1 is not supported "
                              "w/renderer %2 (supported: %3)")
                .arg(deint0).arg(renderer).arg(toCommaList(deints));
        }

        return false;
    }

    if (!deint1.isEmpty() &&
        (!deints.contains(deint1) ||
         deint1.contains("bobdeint") ||
         deint1.contains("doublerate")))
    {
        if (reason)
        {
            if (deint1.contains("bobdeint") || deint1.contains("doublerate"))
                deints.remove(deint1);

            *reason = QString("deinterlacer %1 is not supported w/renderer %2 "
                              "as second deinterlacer (supported: %3)")
                .arg(deint1).arg(renderer).arg(toCommaList(deints));
        }

        return false;
    }

    QStringList osds      = VideoDisplayProfile::GetOSDs(renderer);
    QString     osd       = Get("pref_osdrenderer");
    if (!osds.contains(osd))
    {
        if (reason)
        {
            *reason = QString("OSD Renderer %1 is not supported "
                              "w/renderer %2 (supported: %3)")
                .arg(osd).arg(renderer).arg(toCommaList(osds));
        }

        return false;
    }

    QString     filter    = Get("pref_filters");
    if (!filter.isEmpty() && !VideoDisplayProfile::IsFilterAllowed(renderer))
    {
        if (reason)
        {
            *reason = QString("Filter %1 is not supported w/renderer %2")
                .arg(filter).arg(renderer);
        }

        return false;
    }

    if (reason)
        *reason = QString::null;

    return true;
}

bool ProfileItem::operator< (const ProfileItem &other) const
{
    return GetPriority() < other.GetPriority();
}

QString ProfileItem::toString(void) const
{
    QString cmp0      = Get("pref_cmp0");
    QString cmp1      = Get("pref_cmp1");
    QString decoder   = Get("pref_decoder");
    QString renderer  = Get("pref_videorenderer");
    QString osd       = Get("pref_osdrenderer");
    QString deint0    = Get("pref_deint0");
    QString deint1    = Get("pref_deint1");
    QString filter    = Get("pref_filters");
    bool    osdfade   = Get("pref_osdfade").toInt();

    return QString("cmp(%1%2) dec(%3) rend(%4) osd(%5) osdfade(%6) "
                   "deint(%7,%8) filt(%9)")
        .arg(cmp0).arg(QString(cmp1.isEmpty() ? "" : ",") + cmp1)
        .arg(decoder).arg(renderer)
        .arg(osd).arg((osdfade) ? "enabled" : "disabled")
        .arg(deint0).arg(deint1).arg(filter);
}

//////////////////////////////////////////////////////////////////////////////

#define LOC     QString("VDP: ")
#define LOC_ERR QString("VDP, Error: ")

QMutex      VideoDisplayProfile::safe_lock(true);
bool        VideoDisplayProfile::safe_initialized = false;
safe_map_t  VideoDisplayProfile::safe_renderer;
safe_map_t  VideoDisplayProfile::safe_deint;
safe_map_t  VideoDisplayProfile::safe_osd;
safe_map_t  VideoDisplayProfile::safe_equiv_dec;
safe_list_t VideoDisplayProfile::safe_custom;
priority_map_t VideoDisplayProfile::safe_renderer_priority;

VideoDisplayProfile::VideoDisplayProfile()
    : lock(true), last_size(0,0), last_rate(0.0f),
      last_video_renderer(QString::null)
{
    QMutexLocker locker(&safe_lock);
    init_statics();

    QString hostname    = gContext->GetHostName();
    QString cur_profile = GetDefaultProfileName(hostname);
    uint    groupid     = GetProfileGroupID(cur_profile, hostname);

    item_list_t items = LoadDB(groupid);
    item_list_t::const_iterator it;
    for (it = items.begin(); it != items.end(); it++)
    {
        QString err;
        if (!(*it).IsValid(&err))
        {
            VERBOSE(VB_PLAYBACK, LOC + "Rejecting: " + (*it).toString() +
                    "\n\t\t\t" + err);

            continue;
        }
        VERBOSE(VB_PLAYBACK, LOC + "Accepting: " + (*it).toString());
        all_pref.push_back(*it);
    }

    SetInput(QSize(2048, 2048));
    SetOutput(60.0f);
}

VideoDisplayProfile::~VideoDisplayProfile()
{
}

void VideoDisplayProfile::SetInput(const QSize &size)
{
    QMutexLocker locker(&lock);
    if (size != last_size)
    {
        last_size = size;
        LoadBestPreferences(last_size, last_rate);
    }
}

void VideoDisplayProfile::SetOutput(float framerate)
{
    QMutexLocker locker(&lock);
    if (framerate != last_rate)
    {
        last_rate = framerate;
        LoadBestPreferences(last_size, last_rate);
    }
}

void VideoDisplayProfile::SetVideoRenderer(const QString &video_renderer)
{
    QMutexLocker locker(&lock);

    VERBOSE(VB_PLAYBACK, LOC +
            QString("SetVideoRenderer(%1)").arg(video_renderer));

    last_video_renderer = QDeepCopy<QString>(video_renderer);

    if (video_renderer == GetVideoRenderer())
    {
        VERBOSE(VB_PLAYBACK, LOC +
                QString("SetVideoRender(%1) == GetVideoRenderer()")
                .arg(video_renderer));
        return; // already made preferences safe...
    }

    // Make preferences safe...

    VERBOSE(VB_PLAYBACK, LOC + "Old preferences: " + toString());

    SetPreference("pref_videorenderer", video_renderer);

    QStringList osds = GetOSDs(video_renderer);
    if (!osds.contains(GetOSDRenderer()))
        SetPreference("pref_osdrenderer", osds[0]);

    QStringList deints = GetDeinterlacers(video_renderer);
    if (!deints.contains(GetDeinterlacer()))
        SetPreference("pref_deint0", deints[0]);
    if (!deints.contains(GetFallbackDeinterlacer()))
        SetPreference("pref_deint1", deints[0]);
    if (GetFallbackDeinterlacer().contains("bobdeint") ||
        GetFallbackDeinterlacer().contains("doublerate"))
    {
        SetPreference("pref_deint1", deints[1]);
    }

    SetPreference("pref_filters", "");

    VERBOSE(VB_PLAYBACK, LOC + "New preferences: " + toString());
}

bool VideoDisplayProfile::IsDecoderCompatible(const QString &decoder)
{
    const QString dec = GetDecoder();
    if (dec == decoder)
        return true;

    QMutexLocker locker(&safe_lock);
    return (safe_equiv_dec[dec].contains(decoder));        
}

QString VideoDisplayProfile::GetFilteredDeint(const QString &override)
{
    QString renderer = GetActualVideoRenderer();
    QString deint    = GetDeinterlacer();

    QMutexLocker locker(&lock);

    if (!override.isEmpty() && GetDeinterlacers(renderer).contains(override))
        deint = override;

    VERBOSE(VB_PLAYBACK, LOC + QString("GetFilteredDeint(%1) : %2 -> '%3'")
            .arg(override).arg(renderer).arg(deint));

    return QDeepCopy<QString>(deint);
}

QString VideoDisplayProfile::GetPreference(const QString &key) const
{
    QMutexLocker locker(&lock);

    if (key.isEmpty())
        return QString::null;

    pref_map_t::const_iterator it = pref.find(key);
    if (it == pref.end())
        return QString::null;

    return QDeepCopy<QString>(*it);
}

void VideoDisplayProfile::SetPreference(
    const QString &key, const QString &value)
{
    QMutexLocker locker(&lock);

    if (!key.isEmpty())
        pref[key] = QDeepCopy<QString>(value);
}

item_list_t::const_iterator VideoDisplayProfile::FindMatch(
    const QSize &size, float rate)
{
    item_list_t::const_iterator it = all_pref.begin();
    for (; it != all_pref.end(); ++it)
    {
        if ((*it).IsMatch(size, rate))
            return it;
    }

    return all_pref.end();
}

void VideoDisplayProfile::LoadBestPreferences(const QSize &size,
                                              float framerate)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("LoadBestPreferences(%1x%2, %3)")
            .arg(size.width()).arg(size.height()).arg(framerate));

    pref.clear();
    item_list_t::const_iterator it = FindMatch(size, framerate);
    if (it != all_pref.end())
        pref = (*it).GetAll();
}

////////////////////////////////////////////////////////////////////////////
// static methods

item_list_t VideoDisplayProfile::LoadDB(uint groupid)
{
    ProfileItem tmp;
    item_list_t list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT profileid, value, data "
        "FROM displayprofiles "
        "WHERE profilegroupid = :GROUPID "
        "ORDER BY profileid");
    query.bindValue(":GROUPID", groupid);
    if (!query.exec())
    {
        MythContext::DBError("loaddb 1", query);
        return list;
    }

    uint profileid = 0;
    while (query.next())
    {
        if (query.value(0).toUInt() != profileid)
        {
            if (profileid)
            {
                tmp.SetProfileID(profileid);
                list.push_back(tmp);
            }
            tmp.Clear();
            profileid = query.value(0).toUInt();
        }
        tmp.Set(query.value(1).toString(), query.value(2).toString());
    }
    if (profileid)
    {
        tmp.SetProfileID(profileid);
        list.push_back(tmp);
    }

    sort(list.begin(), list.end());
    return list;
}

bool VideoDisplayProfile::DeleteDB(uint groupid, const item_list_t &items)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "DELETE FROM displayprofiles "
        "WHERE profilegroupid = :GROUPID   AND "
        "      profileid      = :PROFILEID");

    bool ok = true;
    item_list_t::const_iterator it = items.begin();
    for (; it != items.end(); ++it)
    {
        if (!(*it).GetProfileID())
            continue;

        query.bindValue(":GROUPID",   groupid);
        query.bindValue(":PROFILEID", (*it).GetProfileID());
        if (!query.exec())
        {
            MythContext::DBError("vdp::deletedb", query);
            ok = false;
        }
    }

    return ok;
}

bool VideoDisplayProfile::SaveDB(uint groupid, item_list_t &items)
{
    MSqlQuery update(MSqlQuery::InitCon());
    update.prepare(
        "UPDATE displayprofiles "
        "SET data = :DATA "
        "WHERE profilegroupid = :GROUPID   AND "
        "      profileid      = :PROFILEID AND "
        "      value          = :VALUE");

    MSqlQuery insert(MSqlQuery::InitCon());

    bool ok = true;
    item_list_t::iterator it = items.begin();
    for (; it != items.end(); ++it)
    {
        pref_map_t list = (*it).GetAll();
        if (list.begin() == list.end())
            continue;

        pref_map_t::const_iterator lit = list.begin();

        if (!(*it).GetProfileID())
        {
            // create new profileid
            if (!insert.exec("SELECT MAX(profileid) FROM displayprofiles"))
            {
                MythContext::DBError("save_profile 1", insert);
                ok = false;
                continue;
            }
            else if (insert.next())
            {
                (*it).SetProfileID(insert.value(0).toUInt() + 1);
            }

            insert.prepare(
                "INSERT INTO displayprofiles "
                " ( profilegroupid,  profileid,  value,  data) "
                "VALUES "
                " (:GROUPID,        :PROFILEID, :VALUE, :DATA) ");

            for (; lit != list.end(); ++lit)
            {
                if ((*lit).isEmpty())
                    continue;

                insert.bindValue(":GROUPID",   groupid);
                insert.bindValue(":PROFILEID", (*it).GetProfileID());
                insert.bindValue(":VALUE",     lit.key());
                insert.bindValue(":DATA",      (*lit));
                if (!insert.exec())
                {
                    MythContext::DBError("save_profile 2", insert);
                    ok = false;
                    continue;
                }
            }
            continue;
        }

        for (; lit != list.end(); ++lit)
        {
            update.bindValue(":GROUPID",   groupid);
            update.bindValue(":PROFILEID", (*it).GetProfileID());
            update.bindValue(":VALUE",     lit.key());
            update.bindValue(":DATA",      (*lit));
            if (!update.exec())
            {
                MythContext::DBError("save_profile 3", update);
                ok = false;
                continue;
            }
        }
    }

    return ok;
}

QStringList VideoDisplayProfile::GetDecoders(void)
{
    QStringList list;

    list += "ffmpeg";
    list += "libmpeg2";
    list += "xvmc";
    list += "xvmc-vld";
    list += "macaccel";
    list += "ivtv";

    return list;
}

QStringList VideoDisplayProfile::GetDecoderNames(void)
{
    QStringList list;

    list += QObject::tr("Standard");
    list += QObject::tr("libmpeg2");
    list += QObject::tr("Standard XvMC");
    list += QObject::tr("VIA XvMC");
    list += QObject::tr("Mac hardware acceleration");
    list += QObject::tr("PVR-350 decoder");

    return list;
}

QString VideoDisplayProfile::GetDecoderHelp(void)
{
    return
        QObject::tr("Decoder to use to play back MPEG2 video.") + " " +
        QObject::tr("Standard will use ffmpeg library.") + " " +
        QObject::tr("libmpeg2 will use mpeg2 library; "
                    "this is faster on some AMD processors.")
#ifdef USING_XVMC
        + " " +
        QObject::tr("Standard XvMC will use XvMC API 1.0 to "
                    "play back video; this is fast, but does not "
                    "work well with HDTV sized frames.")
#endif // USING_XVMC

#ifdef USING_XVMC_VLD
        + " " +
        QObject::tr("VIA XvMC will use the VIA VLD XvMC extension.")
#endif // USING_XVMC_VLD

#ifdef USING_DVDV
        + " " +
        QObject::tr("Mac hardware will try to use the graphics "
                    "processor - this may hang or crash your Mac!")
#endif // USING_DVDV
#ifdef USING_IVTV
        + " " +
        QObject::tr("MythTV can use the PVR-350's TV out and MPEG "
                    "decoder for high quality playback.  This requires that "
                    "the ivtv-fb kernel module is also loaded and configured "
                    "properly.")
#endif // USING_IVTV
        ;
}

QString VideoDisplayProfile::GetDeinterlacerName(const QString short_name)
{
    if ("none" == short_name)
        return QObject::tr("None");
    else if ("linearblend" == short_name)
        return QObject::tr("Linear blend");
    else if ("kerneldeint" == short_name)
        return QObject::tr("Kernel");
    else if ("bobdeint" == short_name)
        return QObject::tr("Bob (2x)");
    else if ("onefield" == short_name)
        return QObject::tr("One field");
    else if ("opengllinearblend" == short_name)
        return QObject::tr("Linear blend (HW)");
    else if ("openglkerneldeint" == short_name)
        return QObject::tr("Kernel (HW)");
    else if ("openglbobdeint" == short_name)
        return QObject::tr("Bob (2x, HW)");
    else if ("openglonefield" == short_name)
        return QObject::tr("One field (HW)");
    else if ("opengldoublerateonefield" == short_name)
        return QObject::tr("One Field (2x, HW)");
    else if ("opengldoubleratekerneldeint" == short_name)
        return QObject::tr("Kernel (2x, HW)");
    else if ("opengldoubleratelinearblend" == short_name)
        return QObject::tr("Linear blend (2x, HW)");
    else if ("opengldoubleratefieldorder" == short_name)
        return QObject::tr("Interlaced (2x, Hw)");
    return "";
}

QStringList VideoDisplayProfile::GetProfiles(const QString &hostname)
{
    QStringList list;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT name "
        "FROM displayprofilegroups "
        "WHERE hostname = :HOST ");
    query.bindValue(":HOST", hostname);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("get_profiles", query);
    else
    {
        while (query.next())
            list += query.value(0).toString();
    }
    return list;
}

QString VideoDisplayProfile::GetDefaultProfileName(const QString &hostname)
{
    QString tmp =
        gContext->GetSettingOnHost("DefaultVideoPlaybackProfile", hostname);

    QStringList profiles = GetProfiles(hostname);

    tmp = (profiles.contains(tmp)) ? tmp : QString::null;

    if (tmp.isEmpty())
    {
        if (profiles.size())
            tmp = profiles[0];

        tmp = (profiles.contains("CPU+")) ? "CPU+" : tmp;

        if (!tmp.isEmpty())
        {
            gContext->SaveSettingOnHost(
                "DefaultVideoPlaybackProfile", hostname, tmp);
        }
    }

    return tmp;
}

void VideoDisplayProfile::SetDefaultProfileName(
    const QString &profilename, const QString &hostname)
{
    gContext->SaveSettingOnHost(
        "DefaultVideoPlaybackProfile", hostname, profilename);
}

uint VideoDisplayProfile::GetProfileGroupID(const QString &profilename,
                                            const QString &hostname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT profilegroupid "
        "FROM displayprofilegroups "
        "WHERE name     = :NAME AND "
        "      hostname = :HOST ");
    query.bindValue(":NAME", profilename);
    query.bindValue(":HOST", hostname);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("get_profile_group_id", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}

void VideoDisplayProfile::DeleteProfiles(const QString &hostname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());
    query.prepare(
        "SELECT profilegroupid "
        "FROM displayprofilegroups "
        "WHERE hostname = :HOST ");
    query.bindValue(":HOST", hostname);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("delete_profiles 1", query);
    else
    {
        while (query.next())
        {
            query2.prepare("DELETE FROM displayprofiles "
                           "WHERE profilegroupid = :PROFID");
            query2.bindValue(":PROFID", query.value(0).toUInt());
            if (!query2.exec())
                MythContext::DBError("delete_profiles 2", query2);
        }
    }
    query.prepare("DELETE FROM displayprofilegroups WHERE hostname = :HOST");
    query.bindValue(":HOST", hostname);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("delete_profiles 3", query);
}

//displayprofilegroups pk(name, hostname), uk(profilegroupid)
//displayprofiles      k(profilegroupid), k(profileid), value, data

void VideoDisplayProfile::CreateProfile(
    uint groupid, uint priority,
    QString cmp0, uint width0, uint height0,
    QString cmp1, uint width1, uint height1,
    QString decoder, QString videorenderer,
    QString osdrenderer, bool osdfade,
    QString deint0, QString deint1, QString filters)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (cmp0.isEmpty() && cmp1.isEmpty())
        return;

    // create new profileid
    uint profileid = 1;
    if (!query.exec("SELECT MAX(profileid) FROM displayprofiles"))
        MythContext::DBError("create_profile 1", query);
    else if (query.next())
        profileid = query.value(0).toUInt() + 1;

    query.prepare(
        "INSERT INTO displayprofiles "
        "VALUES (:GRPID, :PROFID, 'pref_priority', :PRIORITY)");
    query.bindValue(":GRPID",    groupid);
    query.bindValue(":PROFID",   profileid);
    query.bindValue(":PRIORITY", priority);
    if (!query.exec())
        MythContext::DBError("create_profile 2", query);

    QStringList queryValue;
    QStringList queryData;

    if (!cmp0.isEmpty())
    {
        queryValue += "pref_cmp0";
        queryData  += QString("%1 %2 %3").arg(cmp0).arg(width0).arg(height0);
    }

    if (!cmp1.isEmpty())
    {
        queryValue += QString("pref_cmp%1").arg(cmp0.isEmpty() ? 0 : 1);
        queryData  += QString("%1 %2 %3").arg(cmp1).arg(width1).arg(height1);
    }

    queryValue += "pref_decoder";
    queryData  += decoder;

    queryValue += "pref_videorenderer";
    queryData  += videorenderer;

    queryValue += "pref_osdrenderer";
    queryData  += osdrenderer;

    queryValue += "pref_osdfade";
    queryData  += (osdfade) ? "1" : "0";

    queryValue += "pref_deint0";
    queryData  += deint0;

    queryValue += "pref_deint1";
    queryData  += deint1;

    queryValue += "pref_filters";
    queryData  += filters;

    QStringList::const_iterator itV = queryValue.begin();
    QStringList::const_iterator itD = queryData.begin();
    for (; itV != queryValue.end() && itD != queryData.end(); ++itV,++itD)
    {
        query.prepare(
            "INSERT INTO displayprofiles "
            "VALUES (:GRPID, :PROFID, :VALUE, :DATA)");
        query.bindValue(":GRPID",  groupid);
        query.bindValue(":PROFID", profileid);
        query.bindValue(":VALUE",  *itV);
        query.bindValue(":DATA",   *itD);
        if (!query.exec())
            MythContext::DBError("create_profile 3", query);
    }
}

uint VideoDisplayProfile::CreateProfileGroup(
    const QString &profilename, const QString &hostname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "INSERT INTO displayprofilegroups (name, hostname) "
        "VALUES (:NAME,:HOST)");

    query.bindValue(":NAME", profilename);
    query.bindValue(":HOST", hostname);

    if (!query.exec())
    {
        MythContext::DBError("create_profile_group", query);
        return 0;
    }

    return GetProfileGroupID(profilename, hostname);
}

bool VideoDisplayProfile::DeleteProfileGroup(
    const QString &groupname, const QString &hostname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "DELETE FROM displayprofilegroups "
        "WHERE name     = :NAME AND "
        "      hostname = :HOST");

    query.bindValue(":NAME", groupname);
    query.bindValue(":HOST", hostname);

    if (!query.exec())
    {
        MythContext::DBError("delete_profile_group", query);
        return false;
    }

    return true;
}

void VideoDisplayProfile::CreateProfiles(const QString &hostname)
{
    DeleteProfiles(hostname);

    uint groupid = CreateProfileGroup("CPU++", hostname);
    CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                  "ffmpeg", "xv-blit", "softblend", true,
                  "bobdeint", "linearblend", "");
    CreateProfile(groupid, 2, ">", 0, 0, "", 0, 0,
                  "ffmpeg", "quartz-blit", "softblend", true,
                  "linearblend", "linearblend", "");

    groupid = CreateProfileGroup("CPU+", hostname);
    CreateProfile(groupid, 1, "<=", 720, 576, ">", 0, 0,
                  "ffmpeg", "xv-blit", "softblend", true,
                  "bobdeint", "linearblend", "");
    CreateProfile(groupid, 2, "<=", 1280, 720, ">", 720, 576,
                  "xvmc", "xvmc-blit", "opengl", true,
                  "bobdeint", "onefield", "");
    CreateProfile(groupid, 3, "<=", 1280, 720, ">", 720, 576,
                  "libmpeg2", "xv-blit", "softblend", true,
                  "bobdeint", "onefield", "");
    CreateProfile(groupid, 4, ">", 0, 0, "", 0, 0,
                  "xvmc", "xvmc-blit", "ia44blend", false,
                  "bobdeint", "onefield", "");
    CreateProfile(groupid, 5, ">", 0, 0, "", 0, 0,
                  "libmpeg2", "xv-blit", "chromakey", false,
                  "bobdeint", "onefield", "");

    groupid = CreateProfileGroup("CPU--", hostname);
    CreateProfile(groupid, 1, "<=", 720, 576, ">", 0, 0,
                  "ivtv", "ivtv", "ivtv", true,
                  "none", "none", "");
    CreateProfile(groupid, 2, "<=", 720, 576, ">", 0, 0,
                  "xvmc", "xvmc-blit", "ia44blend", false,
                  "bobdeint", "onefield", "");
    CreateProfile(groupid, 3, "<=", 1280, 720, ">", 720, 576,
                  "xvmc", "xvmc-blit", "ia44blend", false,
                  "bobdeint", "onefield", "");
    CreateProfile(groupid, 4, ">", 0, 0, "", 0, 0,
                  "xvmc", "xvmc-blit", "ia44blend", false,
                  "bobdeint", "onefield", "");
    CreateProfile(groupid, 5, ">", 0, 0, "", 0, 0,
                  "libmpeg2", "xv-blit", "chromakey", false,
                  "none", "none", "");
}

QStringList VideoDisplayProfile::GetVideoRenderers(const QString &decoder)
{
    QMutexLocker locker(&safe_lock);
    init_statics();

    safe_map_t::const_iterator it = safe_renderer.find(decoder);
    QStringList tmp;
    if (it != safe_renderer.end())
        tmp = QDeepCopy<QStringList>(*it);

    return tmp;
}

QString VideoDisplayProfile::GetPreferredVideoRenderer(const QString &decoder)
{
    return GetBestVideoRenderer(GetVideoRenderers(decoder));
}

QStringList VideoDisplayProfile::GetDeinterlacers(
    const QString &video_renderer)
{
    QMutexLocker locker(&safe_lock);
    init_statics();

    safe_map_t::const_iterator it = safe_deint.find(video_renderer);
    QStringList tmp;
    if (it != safe_deint.end())
        tmp = QDeepCopy<QStringList>(*it);

    return tmp;
}

QStringList VideoDisplayProfile::GetOSDs(const QString &video_renderer)
{
    QMutexLocker locker(&safe_lock);
    init_statics();

    safe_map_t::const_iterator it = safe_osd.find(video_renderer);
    QStringList tmp;
    if (it != safe_osd.end())
        tmp = QDeepCopy<QStringList>(*it);

    return tmp;
}

bool VideoDisplayProfile::IsFilterAllowed(const QString &video_renderer)
{
    QMutexLocker locker(&safe_lock);
    init_statics();
    return safe_custom.contains(video_renderer);
}

QStringList VideoDisplayProfile::GetFilteredRenderers(
    const QString &decoder, const QStringList &renderers)
{
    const QStringList dec_list = GetVideoRenderers(decoder);
    QStringList new_list;

    QStringList::const_iterator it = dec_list.begin();
    for (; it != dec_list.end(); ++it)
    {
        if (renderers.contains(*it))
            new_list.push_back(*it); // deep copy not needed
    }

    return new_list;
}

QString VideoDisplayProfile::GetBestVideoRenderer(const QStringList &renderers)
{
    QMutexLocker locker(&safe_lock);
    init_statics();

    uint    top_priority = 0;
    QString top_renderer = QString::null;

    QStringList::const_iterator it = renderers.begin();
    for (; it != renderers.end(); ++it)
    {
        priority_map_t::const_iterator p = safe_renderer_priority.find(*it);
        if ((p != safe_renderer_priority.end()) && (*p >= top_priority))
        {
            top_priority = *p;
            top_renderer = *it;
        }
    }

    return QDeepCopy<QString>(top_renderer);
}

QString VideoDisplayProfile::toString(void) const
{
    QString renderer  = GetPreference("pref_videorenderer");
    QString osd       = GetPreference("pref_osdrenderer");
    QString deint0    = GetPreference("pref_deint0");
    QString deint1    = GetPreference("pref_deint1");
    QString filter    = GetPreference("pref_filters");
    return QString("rend(%4) osd(%5) deint(%6,%7) filt(%8)")
        .arg(renderer).arg(osd).arg(deint0).arg(deint1).arg(filter);
}

/*
// Decoders
"dummy"
"nuppel"
"ffmpeg"
"libmpeg2"
"xvmc"
"xvmc-vld"
"macaccel"
"ivtv"

// Video Renderers
"null"
"xlib"
"xshm"
"xv-blit"
"xvmc-blit"
"xvmc-opengl"
"directfb"
"directx"
"quartz-blit"
"quartz-accel"
"ivtv"
"opengl"

// OSD Renderers
"chromakey"
"softblend"
"ia44blend"
"ivtv"
"opengl"
"opengl2"
"opengl3"

// deinterlacers
"none"
"onefield"
"bobdeint"
"linearblend"
"kerneldeint"
"opengllinearblend"
"openglonefield"
"openglbobdeint"
"opengldoubleratelinearblend"
"opengldoublerateonefield"
"opengldoubleratekerneldeint"
"opengldoubleratefieldorder"
*/

void VideoDisplayProfile::init_statics(void)
{
    if (safe_initialized)
        return;

    safe_initialized = true;

    safe_custom += "null";
    safe_custom += "xlib";
    safe_custom += "xshm";
    safe_custom += "directfb";
    safe_custom += "directx";
    safe_custom += "quartz-blit";
    safe_custom += "xv-blit";
    safe_custom += "opengl";

    safe_list_t::const_iterator it;
    for (it = safe_custom.begin(); it != safe_custom.end(); ++it)
    {
        safe_deint[*it] += "linearblend";
        safe_deint[*it] += "kerneldeint";
        safe_deint[*it] += "none";
        safe_osd[*it]   += "softblend";
    }

    QStringList tmp;
    tmp += "xv-blit";
    tmp += "xvmc-blit";
    tmp += "xvmc-opengl";

    QStringList::const_iterator it2;
    for (it2 = tmp.begin(); it2 != tmp.end(); ++it2)
    {
        safe_deint[*it2] += "bobdeint";
        safe_deint[*it2] += "onefield";
        safe_deint[*it2] += "none";
        safe_osd[*it2]   += "chromakey";
    }

    safe_deint["opengl"] += "opengllinearblend";
    safe_deint["opengl"] += "openglonefield";
    safe_deint["opengl"] += "openglkerneldeint";

    safe_deint["opengl"] += "bobdeint";
    safe_deint["opengl"] += "openglbobdeint";
    safe_deint["opengl"] += "opengldoubleratelinearblend";
    safe_deint["opengl"] += "opengldoublerateonefield";
    safe_deint["opengl"] += "opengldoubleratekerneldeint";
    safe_deint["opengl"] += "opengldoubleratefieldorder";

    safe_osd["xv-blit"]     += "softblend";
    safe_osd["xvmc-blit"]   += "chromakey";
    safe_osd["xvmc-blit"]   += "ia44blend";
    safe_osd["xvmc-opengl"] += "opengl";
    safe_osd["ivtv"]        += "ivtv";
    safe_osd["opengl"]      += "opengl2";
    safe_osd["quartz-accel"]+= "opengl3";

    // These video renderers do not support deinterlacing in MythTV
    safe_deint["quartz-accel"] += "none";
    safe_deint["ivtv"]         += "none";

    tmp.clear();
    tmp += "dummy";
    tmp += "nuppel";
    tmp += "ffmpeg";
    tmp += "libmpeg2";
    for (it2 = tmp.begin(); it2 != tmp.end(); ++it2)
    {
        safe_renderer[*it2] += "null";
        safe_renderer[*it2] += "xlib";
        safe_renderer[*it2] += "xshm";
        safe_renderer[*it2] += "directfb";
        safe_renderer[*it2] += "directx";
        safe_renderer[*it2] += "quartz-blit";
        safe_renderer[*it2] += "xv-blit";
        safe_renderer[*it2] += "opengl";
    }

    safe_renderer["dummy"]    += "xvmc-blit";
    safe_renderer["xvmc"]     += "xvmc-blit";
    safe_renderer["xvmc-vld"] += "xvmc-blit";
    safe_renderer["dummy"]    += "xvmc-opengl";
    safe_renderer["xvmc"]     += "xvmc-opengl";

    safe_renderer["dummy"]    += "quartz-accel";
    safe_renderer["macaccel"] += "quartz-accel";
    safe_renderer["ivtv"]     += "ivtv";

    safe_renderer_priority["null"]         =  10;
    safe_renderer_priority["xlib"]         =  20;
    safe_renderer_priority["xshm"]         =  30;
    safe_renderer_priority["xv-blit"]      =  90;
    safe_renderer_priority["xvmc-blit"]    = 110;
    safe_renderer_priority["xvmc-opengl"]  = 100;
    safe_renderer_priority["directfb"]     =  60;
    safe_renderer_priority["directx"]      =  50;
    safe_renderer_priority["quartz-blit"]  =  70;
    safe_renderer_priority["quartz-accel"] =  80;
    safe_renderer_priority["ivtv"]         =  40;

    safe_equiv_dec["ffmpeg"]   += "nuppel";
    safe_equiv_dec["libmpeg2"] += "nuppel";
    safe_equiv_dec["libmpeg2"] += "ffmpeg";

    safe_equiv_dec["ffmpeg"]   += "dummy";
    safe_equiv_dec["libmpeg2"] += "dummy";
    safe_equiv_dec["xvmc"]     += "dummy";
    safe_equiv_dec["xvmc-vld"] += "dummy";
    safe_equiv_dec["macaccel"] += "dummy";
    safe_equiv_dec["ivtv"]     += "dummy";
}

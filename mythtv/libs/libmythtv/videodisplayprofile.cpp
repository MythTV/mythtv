// -*- Mode: c++ -*-
#include <algorithm>
#include <utility>
using namespace std;

#include "videodisplayprofile.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "videooutbase.h"
#include "avformatdecoder.h"
#include "mythcodeccontext.h"


// options are NNN NNN-MMM 0-MMM NNN-99999 >NNN >=NNN <MMM <=MMM or blank
// If string is blank then assumes a match.
// If value is 0 or negative assume a match (i.e. value unknown assumes a match)
// float values must be no more than 3 decimals.

bool ProfileItem::checkRange(const QString& key, float fvalue, bool *ok) const
{
    return checkRange(key, fvalue, 0, true, ok);
}

bool ProfileItem::checkRange(const QString& key, int ivalue, bool *ok) const
{
    return checkRange(key, 0.0, ivalue, false, ok);
}

bool ProfileItem::checkRange(const QString& key,
    float fvalue, int ivalue, bool isFloat, bool *ok) const
{
    bool match = true;
    bool isOK = true;
    if (isFloat)
        ivalue = int(fvalue * 1000.0F);
    QString cmp = Get(key);
    if (!cmp.isEmpty())
    {
        cmp.replace(QLatin1String(" "),QLatin1String(""));
        QStringList expr = cmp.split("&");
        for (int ix = 0; ix < expr.size(); ++ix)
        {
            if (expr[ix].isEmpty())
            {
                isOK = false;
                continue;
            }
            if (ivalue > 0)
            {
                QRegularExpression regex("^([0-9.]*)([^0-9.]*)([0-9.]*)$");
                QRegularExpressionMatch rmatch = regex.match(expr[ix]);

                int value1 = 0;
                int value2 = 0;
                QString oper;
                QString capture1 = rmatch.captured(1);
                QString capture3;
                if (!capture1.isEmpty())
                {
                    if (isFloat)
                    {
                        int dec=capture1.indexOf('.');
                        if (dec > -1 && (capture1.length()-dec) > 4)
                            isOK = false;
                        if (isOK)
                        {
                            double double1 = capture1.toDouble(&isOK);
                            if (double1 > 2000000.0 || double1 < 0.0)
                                isOK = false;
                            value1 = int(double1 * 1000.0);
                        }
                    }
                    else
                        value1 = capture1.toInt(&isOK);
                }
                if (isOK)
                {
                    oper = rmatch.captured(2);
                    capture3 = rmatch.captured(3);
                    if (!capture3.isEmpty())
                    {
                        if (isFloat)
                        {
                            int dec=capture3.indexOf('.');
                            if (dec > -1 && (capture3.length()-dec) > 4)
                                isOK = false;
                            if (isOK)
                            {
                                double double1 = capture3.toDouble(&isOK);
                                if (double1 > 2000000.0 || double1 < 0.0)
                                    isOK = false;
                                value2 = int(double1 * 1000.0);
                            }
                        }
                        else
                            value2 = capture3.toInt(&isOK);
                    }
                }
                if (isOK)
                {
                    // Invalid string
                    if (value1 == 0 && value2 == 0 && oper.isEmpty())
                        isOK=false;
                }
                if (isOK)
                {
                    // Case NNN
                    if (value1 != 0 && oper.isEmpty() && value2 == 0)
                    {
                        value2 = value1;
                        oper = "-";
                    }
                    // NNN-MMM 0-MMM NNN-99999 NNN- -MMM
                    else if (oper == "-")
                    {
                        // NNN- or -NNN
                        if (capture1.isEmpty() || capture3.isEmpty())
                            isOK = false;
                        // NNN-MMM
                        if (value2 < value1)
                            isOK = false;
                    }
                    else if (capture1.isEmpty())
                    {
                        // Other operators == > < >= <=
                        // Convert to a range
                        if (oper == "==")
                            value1 = value2;
                        else if (oper == ">")
                        {
                            value1 = value2 + 1;
                            value2 = 99999999;
                        }
                        else if (oper == ">=")
                        {
                            value1 = value2;
                            value2 = 99999999;
                        }
                        else if (oper == "<")
                            value2 = value2 - 1;
                        else if (oper == "<=")
                            ;
                        else isOK = false;
                        oper = "-";
                    }
                }
                if (isOK)
                {
                    if (oper == "-")
                        match = match && (ivalue >= value1 && ivalue <= value2);
                    else isOK = false;
                }
            }
        }
    }
    if (ok != nullptr)
        *ok = isOK;
    if (!isOK)
        match=false;
    return match;
}

bool ProfileItem::IsMatch(const QSize &size,
    float framerate, const QString &codecName) const
{
    bool    match = true;

    QString cmp;

    // cond_width, cond_height, cond_codecs, cond_framerate.
    // These replace old settings pref_cmp0 and pref_cmp1
    match &= checkRange("cond_width",size.width());
    match &= checkRange("cond_height",size.height());
    match &= checkRange("cond_framerate",framerate);
    // codec
    cmp = Get(QString("cond_codecs"));
    if (!cmp.isEmpty())
    {
        QStringList clist = cmp.split(" ", QString::SkipEmptyParts);
        if (!clist.empty())
            match &= clist.contains(codecName,Qt::CaseInsensitive);
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

    bool isOK = true;
    checkRange("cond_width",1,&isOK);
    if (!isOK)
    {
        if (reason)
            *reason = QString("Invalid width condition");
        return false;
    }
    checkRange("cond_height",1,&isOK);
    if (!isOK)
    {
        if (reason)
            *reason = QString("Invalid height condition");
        return false;
    }
    checkRange("cond_framerate",1.0F,&isOK);
    if (!isOK)
    {
        if (reason)
            *reason = QString("Invalid framerate condition");
        return false;
    }

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
    QStringList decoderdeints  = MythCodecContext::GetDeinterlacers(decoder);
    deints.append(decoderdeints);
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
         deint1.contains("doublerate") ||
         deint1.contains("doubleprocess")))
    {
        if (reason)
        {
            if (deint1.contains("bobdeint") ||
                deint1.contains("doublerate") ||
                deint1.contains("doubleprocess"))
                deints.removeAll(deint1);

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
        *reason = QString();

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
    QString width     = Get("cond_width");
    QString height    = Get("cond_height");
    QString framerate = Get("cond_framerate");
    QString codecs    = Get("cond_codecs");
    QString decoder   = Get("pref_decoder");
    uint    max_cpus  = Get("pref_max_cpus").toUInt();
    bool    skiploop  = Get("pref_skiploop").toInt() != 0;
    QString renderer  = Get("pref_videorenderer");
    QString osd       = Get("pref_osdrenderer");
    QString deint0    = Get("pref_deint0");
    QString deint1    = Get("pref_deint1");
    QString filter    = Get("pref_filters");
    bool    osdfade   = Get("pref_osdfade").toInt() != 0;

    QString cond = QString("w(%1) h(%2) framerate(%3) codecs(%4)")
        .arg(width).arg(height).arg(framerate).arg(codecs);
    QString str =  QString("cmp(%1%2) %7 dec(%3) cpus(%4) skiploop(%5) rend(%6) ")
        .arg(cmp0).arg(QString(cmp1.isEmpty() ? "" : ",") + cmp1)
        .arg(decoder).arg(max_cpus).arg((skiploop) ? "enabled" : "disabled").arg(renderer)
        .arg(cond);
    str += QString("osd(%1) osdfade(%2) deint(%3,%4) filt(%5)")
        .arg(osd).arg((osdfade) ? "enabled" : "disabled")
        .arg(deint0).arg(deint1).arg(filter);

    return str;
}

//////////////////////////////////////////////////////////////////////////////

#define LOC     QString("VDP: ")

QMutex         VideoDisplayProfile::s_safe_lock(QMutex::Recursive);
bool           VideoDisplayProfile::s_safe_initialized = false;
safe_map_t     VideoDisplayProfile::s_safe_renderer;
safe_map_t     VideoDisplayProfile::s_safe_renderer_group;
safe_map_t     VideoDisplayProfile::s_safe_deint;
safe_map_t     VideoDisplayProfile::s_safe_osd;
safe_map_t     VideoDisplayProfile::s_safe_equiv_dec;
safe_list_t    VideoDisplayProfile::s_safe_custom;
priority_map_t VideoDisplayProfile::s_safe_renderer_priority;
pref_map_t     VideoDisplayProfile::s_dec_name;
safe_list_t    VideoDisplayProfile::s_safe_decoders;

VideoDisplayProfile::VideoDisplayProfile()
    : lock(QMutex::Recursive), last_size(0,0), last_rate(0.0F)
{
    QMutexLocker locker(&s_safe_lock);
    init_statics();

    QString hostname    = gCoreContext->GetHostName();
    QString cur_profile = GetDefaultProfileName(hostname);
    uint    groupid     = GetProfileGroupID(cur_profile, hostname);

    item_list_t items = LoadDB(groupid);
    item_list_t::const_iterator it;
    for (it = items.begin(); it != items.end(); ++it)
    {
        QString err;
        if (!(*it).IsValid(&err))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Rejecting: " + (*it).toString() +
                    "\n\t\t\t" + err);

            continue;
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Accepting: " + (*it).toString());
        all_pref.push_back(*it);
    }
}

void VideoDisplayProfile::SetInput(const QSize &size,
    float framerate, const QString &codecName)
{
    QMutexLocker locker(&lock);
    bool change = false;

    if (size != last_size)
    {
        last_size = size;
        change = true;
    }
    if (framerate > 0.0F && framerate != last_rate)
    {
        last_rate = framerate;
        change = true;
    }
    if (!codecName.isEmpty() && codecName != last_codecName)
    {
        last_codecName = codecName;
        change = true;
    }
    if (change)
        LoadBestPreferences(last_size, last_rate, last_codecName);
}

void VideoDisplayProfile::SetOutput(float framerate)
{
    QMutexLocker locker(&lock);
    if (framerate != last_rate)
    {
        last_rate = framerate;
        LoadBestPreferences(last_size, last_rate, last_codecName);
    }
}

void VideoDisplayProfile::SetVideoRenderer(const QString &video_renderer)
{
    QMutexLocker locker(&lock);

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("SetVideoRenderer(%1)").arg(video_renderer));

    last_video_renderer = video_renderer;
    if (video_renderer == GetVideoRenderer())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("SetVideoRender(%1) == GetVideoRenderer()")
                .arg(video_renderer));
        return; // already made preferences safe...
    }

    // Make preferences safe...

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Old preferences: " + toString());

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
        GetFallbackDeinterlacer().contains("doublerate") ||
        GetFallbackDeinterlacer().contains("doubleprocess"))
    {
        SetPreference("pref_deint1", deints[1]);
    }

    SetPreference("pref_filters", "");

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "New preferences: " + toString());
}

bool VideoDisplayProfile::CheckVideoRendererGroup(const QString &renderer)
{
    if (last_video_renderer == renderer ||
        last_video_renderer == "null")
        return true;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Preferred video renderer: %1 (current: %2)")
                .arg(renderer).arg(last_video_renderer));

    safe_map_t::const_iterator it = s_safe_renderer_group.begin();
    for (; it != s_safe_renderer_group.end(); ++it)
        if (it->contains(last_video_renderer) &&
            it->contains(renderer))
            return true;
    return false;
}

bool VideoDisplayProfile::IsDecoderCompatible(const QString &decoder)
{
    const QString dec = GetDecoder();
    if (dec == decoder)
        return true;

    QMutexLocker locker(&s_safe_lock);
    return (s_safe_equiv_dec[dec].contains(decoder));
}

QString VideoDisplayProfile::GetFilteredDeint(const QString &override)
{
    QString renderer = GetActualVideoRenderer();
    QString deint    = GetDeinterlacer();

    QMutexLocker locker(&lock);

    if (!override.isEmpty() && GetDeinterlacers(renderer).contains(override))
        deint = override;

    LOG(VB_PLAYBACK, LOG_INFO,
        LOC + QString("GetFilteredDeint(%1) : %2 -> '%3'")
            .arg(override).arg(renderer).arg(deint));

    return deint;
}

QString VideoDisplayProfile::GetPreference(const QString &key) const
{
    QMutexLocker locker(&lock);

    if (key.isEmpty())
        return QString();

    pref_map_t::const_iterator it = pref.find(key);
    if (it == pref.end())
        return QString();

    return *it;
}

void VideoDisplayProfile::SetPreference(
    const QString &key, const QString &value)
{
    QMutexLocker locker(&lock);

    if (!key.isEmpty())
    {
        pref[key] = value;
    }
}

item_list_t::const_iterator VideoDisplayProfile::FindMatch
    (const QSize &size, float framerate, const QString &codecName)
{
    item_list_t::const_iterator it = all_pref.begin();
    for (; it != all_pref.end(); ++it)
    {
        if ((*it).IsMatch(size, framerate, codecName))
            return it;
    }

    return all_pref.end();
}

void VideoDisplayProfile::LoadBestPreferences
    (const QSize &size, float framerate, const QString &codecName)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("LoadBestPreferences(%1x%2, %3, %4)")
            .arg(size.width()).arg(size.height()).arg(framerate,0,'f',3).arg(codecName));

    pref.clear();
    item_list_t::const_iterator it = FindMatch(size, framerate, codecName);
    if (it != all_pref.end())
        pref = (*it).GetAll();

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("LoadBestPreferences Result "
            "prio:%1, w:%2, h:%3, fps:%4,"
            " codecs:%5, decoder:%6, renderer:%7, deint:%8")
            .arg(GetPreference("pref_priority")).arg(GetPreference("cond_width"))
            .arg(GetPreference("cond_height")).arg(GetPreference("cond_framerate"))
            .arg(GetPreference("cond_codecs")).arg(GetPreference("pref_decoder"))
            .arg(GetPreference("pref_videorenderer")).arg(GetPreference("pref_deint0"))
            );
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
        MythDB::DBError("loaddb 1", query);
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
                QString error;
                bool valid = tmp.IsValid(&error);
                if (valid)
                    list.push_back(tmp);
                else
                    LOG(VB_PLAYBACK, LOG_NOTICE, LOC +
                        QString("Ignoring profile item %1 (%2)")
                            .arg(profileid).arg(error));
            }
            tmp.Clear();
            profileid = query.value(0).toUInt();
        }
        tmp.Set(query.value(1).toString(), query.value(2).toString());
    }
    if (profileid)
    {
        tmp.SetProfileID(profileid);
        QString error;
        bool valid = tmp.IsValid(&error);
        if (valid)
            list.push_back(tmp);
        else
            LOG(VB_PLAYBACK, LOG_NOTICE, LOC +
                QString("Ignoring profile item %1 (%2)")
                .arg(profileid).arg(error));
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
            MythDB::DBError("vdp::deletedb", query);
            ok = false;
        }
    }

    return ok;
}

bool VideoDisplayProfile::SaveDB(uint groupid, item_list_t &items)
{
    MSqlQuery query(MSqlQuery::InitCon());

    MSqlQuery update(MSqlQuery::InitCon());
    update.prepare(
        "UPDATE displayprofiles "
        "SET data = :DATA "
        "WHERE profilegroupid = :GROUPID   AND "
        "      profileid      = :PROFILEID AND "
        "      value          = :VALUE");

    MSqlQuery insert(MSqlQuery::InitCon());
    insert.prepare(
        "INSERT INTO displayprofiles "
        " ( profilegroupid,  profileid,  value,  data) "
        "VALUES "
        " (:GROUPID,        :PROFILEID, :VALUE, :DATA) ");

    MSqlQuery sqldelete(MSqlQuery::InitCon());
    sqldelete.prepare(
        "DELETE FROM displayprofiles "
        "WHERE profilegroupid = :GROUPID   AND "
        "      profileid      = :PROFILEID AND "
        "      value          = :VALUE");

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
            if (!query.exec("SELECT MAX(profileid) FROM displayprofiles"))
            {
                MythDB::DBError("save_profile 1", query);
                ok = false;
                continue;
            }
            if (query.next())
            {
                (*it).SetProfileID(query.value(0).toUInt() + 1);
            }

            for (; lit != list.end(); ++lit)
            {
                if ((*lit).isEmpty())
                    continue;

                insert.bindValue(":GROUPID",   groupid);
                insert.bindValue(":PROFILEID", (*it).GetProfileID());
                insert.bindValue(":VALUE",     lit.key());
                insert.bindValue(":DATA", ((*lit).isNull()) ? "" : (*lit));
                if (!insert.exec())
                {
                    MythDB::DBError("save_profile 2", insert);
                    ok = false;
                    continue;
                }
            }
            continue;
        }

        for (; lit != list.end(); ++lit)
        {
            query.prepare(
                "SELECT count(*) "
                "FROM displayprofiles "
                "WHERE  profilegroupid = :GROUPID AND "
                "       profileid      = :PROFILEID AND "
                "       value          = :VALUE");
            query.bindValue(":GROUPID",   groupid);
            query.bindValue(":PROFILEID", (*it).GetProfileID());
            query.bindValue(":VALUE",     lit.key());

            if (!query.exec())
            {
                MythDB::DBError("save_profile 3", query);
                ok = false;
                continue;
            }
            if (query.next() && (1 == query.value(0).toUInt()))
            {
                if (lit->isEmpty())
                {
                    sqldelete.bindValue(":GROUPID",   groupid);
                    sqldelete.bindValue(":PROFILEID", (*it).GetProfileID());
                    sqldelete.bindValue(":VALUE",     lit.key());
                    if (!sqldelete.exec())
                    {
                        MythDB::DBError("save_profile 5a", sqldelete);
                        ok = false;
                        continue;
                    }
                }
                else
                {
                    update.bindValue(":GROUPID",   groupid);
                    update.bindValue(":PROFILEID", (*it).GetProfileID());
                    update.bindValue(":VALUE",     lit.key());
                    update.bindValue(":DATA", ((*lit).isNull()) ? "" : (*lit));
                    if (!update.exec())
                    {
                        MythDB::DBError("save_profile 5b", update);
                        ok = false;
                        continue;
                    }
                }
            }
            else
            {
                insert.bindValue(":GROUPID",   groupid);
                insert.bindValue(":PROFILEID", (*it).GetProfileID());
                insert.bindValue(":VALUE",     lit.key());
                insert.bindValue(":DATA", ((*lit).isNull()) ? "" : (*lit));
                if (!insert.exec())
                {
                    MythDB::DBError("save_profile 4", insert);
                    ok = false;
                    continue;
                }
            }
        }
    }

    return ok;
}

QStringList VideoDisplayProfile::GetDecoders(void)
{
    init_statics();
    return s_safe_decoders;
}

QStringList VideoDisplayProfile::GetDecoderNames(void)
{
    init_statics();
    QStringList list;

    const QStringList decs = GetDecoders();
    QStringList::const_iterator it = decs.begin();
    for (; it != decs.end(); ++it)
        list += GetDecoderName(*it);

    return list;
}

QString VideoDisplayProfile::GetDecoderName(const QString &decoder)
{
    if (decoder.isEmpty())
        return "";

    QMutexLocker locker(&s_safe_lock);
    if (s_dec_name.empty())
    {
        s_dec_name["ffmpeg"]   = QObject::tr("Standard");
        s_dec_name["macaccel"] = QObject::tr("Mac hardware acceleration");
        s_dec_name["vdpau"]    = QObject::tr("NVidia VDPAU acceleration");
        s_dec_name["vaapi"]    = QObject::tr("VAAPI acceleration");
        s_dec_name["dxva2"]    = QObject::tr("Windows hardware acceleration");
        s_dec_name["vda"]      = QObject::tr("Mac VDA hardware acceleration");
        s_dec_name["mediacodec"] = QObject::tr("Android MediaCodec decoder");
        s_dec_name["vaapi2"]   = QObject::tr("VAAPI2 acceleration");
        s_dec_name["nvdec"]    = QObject::tr("NVidia NVDEC acceleration");
    }

    QString ret = decoder;
    pref_map_t::const_iterator it = s_dec_name.find(decoder);
    if (it != s_dec_name.end())
        ret = *it;
    return ret;
}


QString VideoDisplayProfile::GetDecoderHelp(const QString& decoder)
{
    QString msg = QObject::tr("Processing method used to decode video.");

    if (decoder.isEmpty())
        return msg;

    msg += "\n";

    if (decoder == "ffmpeg")
        msg += QObject::tr("Standard will use ffmpeg library.");

    if (decoder == "macaccel")
        msg += QObject::tr(
            "Mac hardware will try to use the graphics "
            "processor - this may hang or crash your Mac!");

    if (decoder == "vdpau")
        msg += QObject::tr(
            "VDPAU will attempt to use the graphics hardware to "
            "accelerate video decoding and playback.");

    if (decoder == "dxva2")
        msg += QObject::tr(
            "DXVA2 will use the graphics hardware to "
            "accelerate video decoding and playback "
            "(requires Windows Vista or later).");

    if (decoder == "vaapi")
        msg += QObject::tr(
            "VAAPI will attempt to use the graphics hardware to "
            "accelerate video decoding. REQUIRES OPENGL PAINTER.");

    if (decoder == "vda")
        msg += QObject::tr(
            "VDA will attempt to use the graphics hardware to "
            "accelerate video decoding. "
            "(H264 only, requires Mac OS 10.6.3)");

    if (decoder == "openmax")
        msg += QObject::tr(
            "Openmax will use the graphics hardware to "
            "accelerate video decoding on Raspberry Pi. ");

    if (decoder == "mediacodec")
        msg += QObject::tr(
            "Mediacodec will use the graphics hardware to "
            "accelerate video decoding on Android. ");

    if (decoder == "vaapi2")
        msg += QObject::tr(
            "VAAPI2 is a new implementation of VAAPI to will use the graphics hardware to "
            "accelerate video decoding on Intel CPUs. ");

    if (decoder == "nvdec")
        msg += QObject::tr(
            "Nvdec uses the new NVDEC API to "
            "accelerate video decoding on NVidia Graphics Adapters. ");

    return msg;
}

QString VideoDisplayProfile::GetDeinterlacerName(const QString &short_name)
{
    if ("none" == short_name)
        return QObject::tr("None");
    if ("linearblend" == short_name)
        return QObject::tr("Linear blend");
    if ("kerneldeint" == short_name)
        return QObject::tr("Kernel");
    if ("kerneldoubleprocessdeint" == short_name)
        return QObject::tr("Kernel (2x)");
    if ("greedyhdeint" == short_name)
        return QObject::tr("Greedy HighMotion");
    if ("greedyhdoubleprocessdeint" == short_name)
        return QObject::tr("Greedy HighMotion (2x)");
    if ("yadifdeint" == short_name)
        return QObject::tr("Yadif");
    if ("yadifdoubleprocessdeint" == short_name)
        return QObject::tr("Yadif (2x)");
    if ("bobdeint" == short_name)
        return QObject::tr("Bob (2x)");
    if ("onefield" == short_name)
        return QObject::tr("One field");
    if ("fieldorderdoubleprocessdeint" == short_name)
        return QObject::tr("Interlaced (2x)");
    if ("opengllinearblend" == short_name)
        return QObject::tr("Linear blend (HW-GL)");
    if ("openglkerneldeint" == short_name)
        return QObject::tr("Kernel (HW-GL)");
    if ("openglbobdeint" == short_name)
        return QObject::tr("Bob (2x, HW-GL)");
    if ("openglonefield" == short_name)
        return QObject::tr("One field (HW-GL)");
    if ("opengldoubleratekerneldeint" == short_name)
        return QObject::tr("Kernel (2x, HW-GL)");
    if ("opengldoubleratelinearblend" == short_name)
        return QObject::tr("Linear blend (2x, HW-GL)");
    if ("opengldoubleratefieldorder" == short_name)
        return QObject::tr("Interlaced (2x, HW-GL)");
    if ("vdpauonefield" == short_name)
        return QObject::tr("One Field (1x, HW)");
    if ("vdpaubobdeint" == short_name)
        return QObject::tr("Bob (2x, HW)");
    if ("vdpaubasic" == short_name)
        return QObject::tr("Temporal (1x, HW)");
    if ("vdpaubasicdoublerate" == short_name)
        return QObject::tr("Temporal (2x, HW)");
    if ("vdpauadvanced" == short_name)
        return QObject::tr("Advanced (1x, HW)");
    if ("vdpauadvanceddoublerate" == short_name)
        return QObject::tr("Advanced (2x, HW)");
    if ("vaapionefield" == short_name)
        return QObject::tr("One Field (1x, HW)");
    if ("vaapibobdeint" == short_name)
        return QObject::tr("Bob (2x, HW)");
#ifdef USING_OPENMAX
    if ("openmaxadvanced" == short_name)
        return QObject::tr("Advanced (HW)");
    if ("openmaxfast" == short_name)
        return QObject::tr("Fast (HW)");
    if ("openmaxlinedouble" == short_name)
        return QObject::tr("Line double (HW)");
#endif // def USING_OPENMAX
#ifdef USING_VAAPI2
    if ("vaapi2default" == short_name)
        return QObject::tr("Advanced (HW-VA)");
    if ("vaapi2bob" == short_name)
        return QObject::tr("Bob (HW-VA)");
    if ("vaapi2weave" == short_name)
        return QObject::tr("Weave (HW-VA)");
    if ("vaapi2motion_adaptive" == short_name)
        return QObject::tr("Motion Adaptive (HW-VA)");
    if ("vaapi2motion_compensated" == short_name)
        return QObject::tr("Motion Compensated (HW-VA)");
    if ("vaapi2doubleratedefault" == short_name)
        return QObject::tr("Advanced (2x, HW-VA)");
    if ("vaapi2doubleratebob" == short_name)
        return QObject::tr("Bob (2x, HW-VA)");
    if ("vaapi2doublerateweave" == short_name)
        return QObject::tr("Weave (2x, HW-VA)");
    if ("vaapi2doubleratemotion_adaptive" == short_name)
        return QObject::tr("Motion Adaptive (2x, HW-VA)");
    if ("vaapi2doubleratemotion_compensated" == short_name)
        return QObject::tr("Motion Compensated (2x, HW-VA)");
#endif
#ifdef USING_NVDEC
    if ("nvdecweave" == short_name)
        return QObject::tr("Weave (HW-NV)");
    if ("nvdecbob" == short_name)
        return QObject::tr("Bob (HW-NV)");
    if ("nvdecadaptive" == short_name)
        return QObject::tr("Adaptive (HW-NV)");
    if ("nvdecdoublerateweave" == short_name)
        return QObject::tr("Weave (2x, HW-NV)");
    if ("nvdecdoubleratebob" == short_name)
        return QObject::tr("Bob (2x, HW-NV)");
    if ("nvdecdoublerateadaptive" == short_name)
        return QObject::tr("Adaptive (2x, HW-NV)");
#endif // USING_NVDEC

    return "";
}

QStringList VideoDisplayProfile::GetProfiles(const QString &hostname)
{
    init_statics();
    QStringList list;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT name "
        "FROM displayprofilegroups "
        "WHERE hostname = :HOST ");
    query.bindValue(":HOST", hostname);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("get_profiles", query);
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
        gCoreContext->GetSettingOnHost("DefaultVideoPlaybackProfile", hostname);

    QStringList profiles = GetProfiles(hostname);

    tmp = (profiles.contains(tmp)) ? tmp : QString();

    if (tmp.isEmpty())
    {
        if (!profiles.empty())
            tmp = profiles[0];

        tmp = (profiles.contains("Normal")) ? "Normal" : tmp;

        if (!tmp.isEmpty())
        {
            gCoreContext->SaveSettingOnHost(
                "DefaultVideoPlaybackProfile", tmp, hostname);
        }
    }

    return tmp;
}

void VideoDisplayProfile::SetDefaultProfileName(
    const QString &profilename, const QString &hostname)
{
    gCoreContext->SaveSettingOnHost(
        "DefaultVideoPlaybackProfile", profilename, hostname);
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
        MythDB::DBError("get_profile_group_id", query);
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
        MythDB::DBError("delete_profiles 1", query);
    else
    {
        while (query.next())
        {
            query2.prepare("DELETE FROM displayprofiles "
                           "WHERE profilegroupid = :PROFID");
            query2.bindValue(":PROFID", query.value(0).toUInt());
            if (!query2.exec())
                MythDB::DBError("delete_profiles 2", query2);
        }
    }
    query.prepare("DELETE FROM displayprofilegroups WHERE hostname = :HOST");
    query.bindValue(":HOST", hostname);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("delete_profiles 3", query);
}

//displayprofilegroups pk(name, hostname), uk(profilegroupid)
//displayprofiles      k(profilegroupid), k(profileid), value, data

// Old style
void VideoDisplayProfile::CreateProfile(
    uint groupid, uint priority,
    const QString& cmp0, uint width0, uint height0,
    const QString& cmp1, uint width1, uint height1,
    const QString& decoder, uint max_cpus, bool skiploop, const QString& videorenderer,
    const QString& osdrenderer, bool osdfade,
    const QString& deint0, const QString& deint1, const QString& filters)
{
    QString width;
    QString height;
    if (!cmp0.isEmpty()
         && ! (cmp0 == ">" && width0 == 0 && height0 == 0))
    {
        width.append(QString("%1%2").arg(cmp0).arg(width0));
        height.append(QString("%1%2").arg(cmp0).arg(height0));
        if (!cmp1.isEmpty())
        {
            width.append("&");
            height.append("&");
        }
    }
    if (!cmp1.isEmpty()
         && ! (cmp1 == ">" && width1 == 0 && height1 == 0))
    {
        width.append(QString("%1%2").arg(cmp1).arg(width1));
        height.append(QString("%1%2").arg(cmp1).arg(height1));
    }
    CreateProfile(
        groupid, priority,
        width, height, QString(),
        decoder, max_cpus, skiploop, videorenderer,
        osdrenderer, osdfade,
        deint0, deint1, filters);
}

// New Style
void VideoDisplayProfile::CreateProfile(
    uint groupid, uint priority,
    const QString& width, const QString& height, const QString& codecs,
    const QString& decoder, uint max_cpus, bool skiploop, const QString& videorenderer,
    const QString& osdrenderer, bool osdfade,
    const QString& deint0, const QString& deint1, const QString& filters)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // create new profileid
    uint profileid = 1;
    if (!query.exec("SELECT MAX(profileid) FROM displayprofiles"))
        MythDB::DBError("create_profile 1", query);
    else if (query.next())
        profileid = query.value(0).toUInt() + 1;

    query.prepare(
        "INSERT INTO displayprofiles "
        "VALUES (:GRPID, :PROFID, 'pref_priority', :PRIORITY)");
    query.bindValue(":GRPID",    groupid);
    query.bindValue(":PROFID",   profileid);
    query.bindValue(":PRIORITY", priority);
    if (!query.exec())
        MythDB::DBError("create_profile 2", query);

    QStringList queryValue;
    QStringList queryData;

    queryValue += "cond_width";
    queryData  += width;

    queryValue += "cond_height";
    queryData  += height;

    queryValue += "cond_codecs";
    queryData  += codecs;

    queryValue += "pref_decoder";
    queryData  += decoder;

    queryValue += "pref_max_cpus";
    queryData  += QString::number(max_cpus);

    queryValue += "pref_skiploop";
    queryData  += (skiploop) ? "1" : "0";

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
        if (itD->isEmpty())
            continue;
        query.prepare(
            "INSERT INTO displayprofiles "
            "VALUES (:GRPID, :PROFID, :VALUE, :DATA)");
        query.bindValue(":GRPID",  groupid);
        query.bindValue(":PROFID", profileid);
        query.bindValue(":VALUE",  *itV);
        query.bindValue(":DATA",   *itD);
        if (!query.exec())
            MythDB::DBError("create_profile 3", query);
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
        MythDB::DBError("create_profile_group", query);
        return 0;
    }

    return GetProfileGroupID(profilename, hostname);
}

bool VideoDisplayProfile::DeleteProfileGroup(
    const QString &groupname, const QString &hostname)
{
    bool ok = true;
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());

    query.prepare(
        "SELECT profilegroupid "
        "FROM displayprofilegroups "
        "WHERE name     = :NAME AND "
        "      hostname = :HOST ");

    query.bindValue(":NAME", groupname);
    query.bindValue(":HOST", hostname);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("delete_profile_group 1", query);
        ok = false;
    }
    else
    {
        while (query.next())
        {
            query2.prepare("DELETE FROM displayprofiles "
                           "WHERE profilegroupid = :PROFID");
            query2.bindValue(":PROFID", query.value(0).toUInt());
            if (!query2.exec())
            {
                MythDB::DBError("delete_profile_group 2", query2);
                ok = false;
            }
        }
    }

    query.prepare(
        "DELETE FROM displayprofilegroups "
        "WHERE name     = :NAME AND "
        "      hostname = :HOST");

    query.bindValue(":NAME", groupname);
    query.bindValue(":HOST", hostname);

    if (!query.exec())
    {
        MythDB::DBError("delete_profile_group 3", query);
        ok = false;
    }

    return ok;
}

void VideoDisplayProfile::CreateProfiles(const QString &hostname)
{
    QStringList profiles = GetProfiles(hostname);
    uint groupid;

#ifdef USING_XV
    if (!profiles.contains("High Quality")) {
        (void) QObject::tr("High Quality", "Sample: high quality");
        groupid = CreateProfileGroup("High Quality", hostname);
        CreateProfile(groupid, 1, ">=", 1920, 1080, "", 0, 0,
                      "ffmpeg", 2, true, "xv-blit", "softblend", true,
                      "linearblend", "linearblend", "");
        CreateProfile(groupid, 2, ">", 0, 0, "", 0, 0,
                      "ffmpeg", 1, true, "xv-blit", "softblend", true,
                      "yadifdoubleprocessdeint", "yadifdeint", "");
    }

    if (!profiles.contains("Normal")) {
        (void) QObject::tr("Normal", "Sample: average quality");
        groupid = CreateProfileGroup("Normal", hostname);
        CreateProfile(groupid, 1, ">=", 1280, 720, "", 0, 0,
                      "ffmpeg", 1, true, "xv-blit", "softblend", false,
                      "linearblend", "linearblend", "");
        CreateProfile(groupid, 2, ">", 0, 0, "", 0, 0,
                      "ffmpeg", 1, true, "xv-blit", "softblend", true,
                      "greedyhdoubleprocessdeint", "kerneldeint", "");
    }

    if (!profiles.contains("Slim")) {
        (void) QObject::tr("Slim", "Sample: low CPU usage");
        groupid = CreateProfileGroup("Slim", hostname);
        CreateProfile(groupid, 1, ">=", 1280, 720, "", 0, 0,
                      "ffmpeg", 1, true, "xv-blit", "softblend", false,
                      "onefield", "onefield", "");
        CreateProfile(groupid, 2, ">", 0, 0, "", 0, 0,
                      "ffmpeg", 1, true, "xv-blit", "softblend", false,
                      "linearblend", "linearblend", "");
    }
#endif

#ifdef USING_VDPAU
    if (!profiles.contains("VDPAU High Quality")) {
        (void) QObject::tr("VDPAU High Quality", "Sample: VDPAU high quality");
        groupid = CreateProfileGroup("VDPAU High Quality", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "vdpau", 1, true, "vdpau", "vdpau", true,
                      "vdpauadvanceddoublerate", "vdpauadvanced",
                      "vdpaucolorspace=auto");
    }

    if (!profiles.contains("VDPAU Normal")) {
        (void) QObject::tr("VDPAU Normal", "Sample: VDPAU average quality");
        groupid = CreateProfileGroup("VDPAU Normal", hostname);
        CreateProfile(groupid, 1, ">=", 0, 720, "", 0, 0,
                      "vdpau", 1, true, "vdpau", "vdpau", true,
                      "vdpaubasicdoublerate", "vdpaubasic",
                      "vdpaucolorspace=auto");
        CreateProfile(groupid, 2, ">", 0, 0, "", 0, 0,
                      "vdpau", 1, true, "vdpau", "vdpau", true,
                      "vdpauadvanceddoublerate", "vdpauadvanced",
                      "vdpaucolorspace=auto");
    }

    if (!profiles.contains("VDPAU Slim")) {
        (void) QObject::tr("VDPAU Slim", "Sample: VDPAU low power GPU");
        groupid = CreateProfileGroup("VDPAU Slim", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "vdpau", 1, true, "vdpau", "vdpau", true,
                      "vdpaubobdeint", "vdpauonefield",
                      "vdpauskipchroma,vdpaucolorspace=auto");
    }
#endif

#if defined(Q_OS_MACX)
    if (!profiles.contains("VDA High Quality")) {
        (void) QObject::tr("VDA High Quality", "Sample: VDA high quality");
        groupid = CreateProfileGroup("VDA High Quality", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "vda", 2, true, "opengl", "opengl2", true,
                      "greedyhdoubleprocessdeint", "greedyhdeint",
                      "");
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "ffmpeg", 2, true, "opengl", "opengl2", true,
                      "greedyhdoubleprocessdeint", "greedyhdeint",
                      "");
    }

    if (!profiles.contains("VDA Normal")) {
        (void) QObject::tr("VDA Normal", "Sample: VDA average quality");
        groupid = CreateProfileGroup("VDA Normal", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "vda", 2, true, "opengl", "opengl2", true,
                      "opengldoubleratekerneldeint", "openglkerneldeint",
                      "");
        CreateProfile(groupid, 2, ">", 0, 0, "", 0, 0,
                      "ffmpeg", 2, true, "opengl", "opengl2", true,
                      "opengldoubleratekerneldeint", "openglkerneldeint",
                      "");
    }

    if (!profiles.contains("VDA Slim")) {
        (void) QObject::tr("VDA Slim", "Sample: VDA low power GPU");
        groupid = CreateProfileGroup("VDA Slim", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "vda", 2, true, "opengl", "opengl2", true,
                      "opengldoubleratelinearblend", "opengllinearblend",
                      "");
        CreateProfile(groupid, 2, ">", 0, 0, "", 0, 0,
                      "ffmpeg", 2, true, "opengl", "opengl2", true,
                      "opengldoubleratelinearblend", "opengllinearblend",
                      "");
    }
#endif

#ifdef USING_OPENGL_VIDEO
    if (!profiles.contains("OpenGL High Quality")) {
        (void) QObject::tr("OpenGL High Quality",
                           "Sample: OpenGL high quality");
        groupid = CreateProfileGroup("OpenGL High Quality", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "ffmpeg", 2, true, "opengl", "opengl2", true,
                      "greedyhdoubleprocessdeint", "greedyhdeint",
                      "");
    }

    if (!profiles.contains("OpenGL Normal")) {
        (void) QObject::tr("OpenGL Normal", "Sample: OpenGL average quality");
        groupid = CreateProfileGroup("OpenGL Normal", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "ffmpeg", 2, true, "opengl", "opengl2", true,
                      "opengldoubleratekerneldeint", "openglkerneldeint",
                      "");
    }

    if (!profiles.contains("OpenGL Slim")) {
        (void) QObject::tr("OpenGL Slim", "Sample: OpenGL low power GPU");
        groupid = CreateProfileGroup("OpenGL Slim", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "ffmpeg", 1, true, "opengl", "opengl2", true,
                      "opengldoubleratelinearblend", "opengllinearblend",
                      "");
    }
#endif

#ifdef USING_GLVAAPI
    if (!profiles.contains("VAAPI Normal")) {
        (void) QObject::tr("VAAPI Normal", "Sample: VAAPI average quality");
        groupid = CreateProfileGroup("VAAPI Normal", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "vaapi", 2, true, "openglvaapi", "opengl2", true,
                      "vaapibobdeint", "vaapionefield",
                      "");
        CreateProfile(groupid, 2, ">", 0, 0, "", 0, 0,
                      "ffmpeg", 2, true, "opengl", "opengl2", true,
                      "opengldoubleratekerneldeint", "openglkerneldeint",
                      "");
    }
#endif

#ifdef USING_OPENMAX
#ifdef USING_OPENGLES
    if (!profiles.contains("OpenMAX High Quality")) {
        (void) QObject::tr("OpenMAX High Quality",
                           "Sample: OpenMAX High Quality");
        groupid = CreateProfileGroup("OpenMAX High Quality", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "openmax", 4, true, "openmax", "opengl", true,
                      "openmaxadvanced", "onefield",
                      "");
    }
#endif

    if (!profiles.contains("OpenMAX Normal")) {
        (void) QObject::tr("OpenMAX Normal", "Sample: OpenMAX Normal");
        groupid = CreateProfileGroup("OpenMAX Normal", hostname);
        CreateProfile(groupid, 1, ">", 0, 0, "", 0, 0,
                      "openmax", 4, true, "openmax", "softblend", false,
                      "openmaxadvanced", "onefield",
                      "");
    }
#endif

#ifdef USING_MEDIACODEC
    if (!profiles.contains("MediaCodec Normal")) {
        (void) QObject::tr("MediaCodec Normal",
                           "Sample: MediaCodec Normal");
        groupid = CreateProfileGroup("MediaCodec Normal", hostname);
        CreateProfile(groupid, 1, "", "", "",
                      "mediacodec", 4, true, "opengl",
                      "opengl2", true,
                      "opengldoubleratelinearblend", "opengllinearblend",
                      "");
    }
#endif

#if defined(USING_VAAPI2) && defined(USING_OPENGL_VIDEO)
    if (!profiles.contains("VAAPI2 Normal")) {
        (void) QObject::tr("VAAPI2 Normal",
                           "Sample: VAAPI2 Normal");
        groupid = CreateProfileGroup("VAAPI2 Normal", hostname);
        CreateProfile(groupid, 1, "", "", "",
                      "vaapi2", 4, true, "opengl",
                      "opengl2", true,
                      "vaapi2doubleratedefault", "vaapi2default",
                      "");
    }
#endif

#if defined(USING_NVDEC) && defined(USING_OPENGL_VIDEO)
    if (!profiles.contains("NVDEC Normal")) {
        (void) QObject::tr("NVDEC Normal",
                           "Sample: NVDEC Normal");
        groupid = CreateProfileGroup("NVDEC Normal", hostname);
        CreateProfile(groupid, 1, "", "", "",
                      "nvdec", 4, true, "opengl",
                      "opengl2", true,
                      "nvdecdoublerateadaptive", "nvdecadaptive",
                      "");
    }
#endif

}

QStringList VideoDisplayProfile::GetVideoRenderers(const QString &decoder)
{
    QMutexLocker locker(&s_safe_lock);
    init_statics();

    safe_map_t::const_iterator it = s_safe_renderer.find(decoder);
    QStringList tmp;
    if (it != s_safe_renderer.end())
        tmp = *it;
    return tmp;
}

QString VideoDisplayProfile::GetVideoRendererHelp(const QString &renderer)
{
    QString msg = QObject::tr("Video rendering method");

    if (renderer.isEmpty())
        return msg;

    if ((renderer == "null") || (renderer == "nullvaapi") ||
        (renderer == "nullvdpau"))
        msg = QObject::tr(
            "Render video offscreen. Used internally.");

    if (renderer == "xlib")
        msg = QObject::tr(
            "Use X11 pixel copy to render video. This is not recommended if "
            "any other option is available. The video will not be scaled to "
            "fit the screen. This will work with all X11 servers, local "
            "and remote.");

    if (renderer == "xshm")
        msg = QObject::tr(
            "Use X11 shared memory pixel transfer to render video. This is "
            "only recommended over the X11 pixel copy renderer. The video "
            "will not be scaled to fit the screen. This works with most "
            "local X11 servers.");

    if (renderer == "xv-blit")
        msg = QObject::tr(
            "This is the standard video renderer for X11 systems. It uses "
            "XVideo hardware assist for scaling, color conversion. If the "
            "hardware offers picture controls the renderer supports them.");

    if (renderer == "direct3d")
        msg = QObject::tr(
            "Windows video renderer based on Direct3D. Requires "
            "video card compatible with Direct3D 9. This is the preferred "
            "renderer for current Windows systems.");

    if (renderer == "opengl")
    {
        msg = QObject::tr(
            "Video is converted to an intermediate format by the CPU (UYVY) "
            "before OpenGL is used for color conversion, scaling, picture controls"
            " and optionally deinterlacing. Processing is balanced between the CPU"
            "and GPU.");
    }

    if (renderer == "opengl-lite")
        msg = QObject::tr(
            "OpenGL is used for scaling and color conversion. "
            "It uses faster OpenGL functionality when available but at the "
            "expense of picture controls and GPU based deinterlacing.");

    if (renderer == "opengl-yv12")
    {
        msg = QObject::tr(
            "OpenGL is used for all color conversion, scaling, picture "
            "controls and optionally deinterlacing. CPU load is low but a more "
            "powerful GPU is needed for deinterlacing.");
    }

    if (renderer == "opengl-hquyv")
    {
        msg = QObject::tr(
            "This renderer uses a higher quality CPU conversion for interlaced "
            "content before using OpenGL for color conversion, scaling, picture"
            " controls and optionally deinterlacing. CPU load is higher "
            "particularly on embedded systems.");
    }

    if (renderer == "opengl-rgba")
    {
        msg = QObject::tr(
            "All video processing is performed by the CPU. OpenGL is used "
            "for display only. Does not support picture controls or GPU "
            "deinterlacing. Requires a significantly faster CPU.");
    }

    if (renderer == "vdpau")
    {
        msg = QObject::tr(
            "This is the only video renderer for NVidia VDPAU decoding.");
    }

    if (renderer == "openglvaapi")
    {
        msg = QObject::tr(
             "This video renderer uses VAAPI for video decoding and "
             "OpenGL for scaling and color conversion.");
    }

    return msg;
}

QString VideoDisplayProfile::GetPreferredVideoRenderer(const QString &decoder)
{
    return GetBestVideoRenderer(GetVideoRenderers(decoder));
}

QStringList VideoDisplayProfile::GetDeinterlacers(
    const QString &video_renderer)
{
    QMutexLocker locker(&s_safe_lock);
    init_statics();

    safe_map_t::const_iterator it = s_safe_deint.find(video_renderer);
    QStringList tmp;
    if (it != s_safe_deint.end())
        tmp = *it;
    return tmp;
}

QString VideoDisplayProfile::GetDeinterlacerHelp(const QString &deint)
{
    if (deint.isEmpty())
        return "";

    QString msg = "";

    QString kDoubleRateMsg =
        QObject::tr(
            "This deinterlacer requires the display to be capable "
            "of twice the frame rate as the source video.");

    QString kNoneMsg =
        QObject::tr("Perform no deinterlacing.") + " " +
        QObject::tr(
            "Use this with an interlaced display whose "
            "resolution exactly matches the video size. "
            "This is incompatible with MythTV zoom modes.");

    QString kOneFieldMsg = QObject::tr(
        "Shows only one of the two fields in the frame. "
        "This looks good when displaying a high motion "
        "1080i video on a 720p display.");

    QString kBobMsg = QObject::tr(
        "Shows one field of the frame followed by the "
        "other field displaced vertically.") + " " +
        kDoubleRateMsg;

    QString kLinearBlendMsg = QObject::tr(
        "Blends the odd and even fields linearly into one frame.");

    QString kKernelMsg = QObject::tr(
        "This filter disables deinterlacing when the two fields are "
        "similar, and performs linear deinterlacing otherwise.");

    QString kUsingGPU = QObject::tr("(Hardware Accelerated)");

    QString kUsingVA = QObject::tr("(VAAPI Hardware Accelerated)");

    QString kUsingNV = QObject::tr("(NVDEC Hardware Accelerated)");

    QString kUsingGL = QObject::tr("(OpenGL Hardware Accelerated)");

    QString kGreedyHMsg = QObject::tr(
        "This deinterlacer uses several fields to reduce motion blur. "
        "It has increased CPU requirements.");

    QString kYadifMsg = QObject::tr(
        "This deinterlacer uses several fields to reduce motion blur. "
        "It has increased CPU requirements.");

    QString kFieldOrderMsg = QObject::tr(
        "This deinterlacer attempts to synchronize with interlaced displays "
        "whose size and refresh rate exactly match the video source. "
        "It has low CPU requirements.");

    QString kBasicMsg = QObject::tr(
        "This deinterlacer uses several fields to reduce motion blur. ");

    QString kAdvMsg = QObject::tr(
        "This deinterlacer uses multiple fields to reduce motion blur "
        "and smooth edges. ");

    QString kMostAdvMsg = QObject::tr(
        "Use the most advanced hardware deinterlacing algorithm available. ");

    QString kWeaveMsg = QObject::tr(
        "Use the weave deinterlacing algorithm. ");

    QString kMAMsg = QObject::tr(
        "Use the motion adaptive deinterlacing algorithm. ");

    QString kMCMsg = QObject::tr(
        "Use the motion compensated deinterlacing algorithm. ");

    if (deint == "none")
        msg = kNoneMsg;
    else if (deint == "onefield")
        msg = kOneFieldMsg;
    else if (deint == "bobdeint")
        msg = kBobMsg;
    else if (deint == "linearblend")
        msg = kLinearBlendMsg;
    else if (deint == "kerneldeint")
        msg = kKernelMsg;
    else if (deint == "kerneldoubleprocessdeint")
        msg = kKernelMsg + " " + kDoubleRateMsg;
    else if (deint == "openglonefield")
        msg = kOneFieldMsg + " " + kUsingGL;
    else if (deint == "openglbobdeint")
        msg = kBobMsg + " " + kUsingGL;
    else if (deint == "opengllinearblend")
        msg = kLinearBlendMsg + " " + kUsingGL;
    else if (deint == "openglkerneldeint")
        msg = kKernelMsg + " " + kUsingGL;
    else if (deint == "opengldoubleratelinearblend")
        msg = kLinearBlendMsg + " " +  kDoubleRateMsg + " " + kUsingGL;
    else if (deint == "opengldoubleratekerneldeint")
        msg = kKernelMsg + " " +  kDoubleRateMsg + " " + kUsingGL;
    else if (deint == "opengldoubleratefieldorder")
        msg = kFieldOrderMsg + " " +  kDoubleRateMsg  + " " + kUsingGL;
    else if (deint == "greedyhdeint")
        msg = kGreedyHMsg;
    else if (deint == "greedyhdoubleprocessdeint")
        msg = kGreedyHMsg + " " +  kDoubleRateMsg;
    else if (deint == "yadifdeint")
        msg = kYadifMsg;
    else if (deint == "yadifdoubleprocessdeint")
        msg = kYadifMsg + " " +  kDoubleRateMsg;
    else if (deint == "fieldorderdoubleprocessdeint")
        msg = kFieldOrderMsg + " " +  kDoubleRateMsg;
    else if (deint == "vdpauonefield")
        msg = kOneFieldMsg + " " + kUsingGPU;
    else if (deint == "vdpaubobdeint")
        msg = kBobMsg + " " + kUsingGPU;
    else if (deint == "vdpaubasic")
        msg = kBasicMsg + " " + kUsingGPU;
    else if (deint == "vdpauadvanced")
        msg = kAdvMsg + " " + kUsingGPU;
    else if (deint == "vdpaubasicdoublerate")
        msg = kBasicMsg + " " +  kDoubleRateMsg + " " + kUsingGPU;
    else if (deint == "vdpauadvanceddoublerate")
        msg = kAdvMsg + " " +  kDoubleRateMsg + " " + kUsingGPU;
    else if (deint == "vaapionefield")
        msg = kOneFieldMsg + " " + kUsingGPU;
    else if (deint == "vaapibobdeint")
        msg = kBobMsg + " " + kUsingGPU;

    else if (deint == "vaapi2default")
        msg = kMostAdvMsg + " " +  kUsingVA;
    else if (deint == "vaapi2bob")
        msg = kBobMsg + " " +  kUsingVA;
    else if (deint == "vaapi2weave")
        msg = kWeaveMsg + " " +  kUsingVA;
    else if (deint == "vaapi2motion_adaptive")
        msg = kMAMsg + " " +  kUsingVA;
    else if (deint == "vaapi2motion_compensated")
        msg = kMCMsg + " " +  kUsingVA;
    else if (deint == "vaapi2doubleratedefault")
        msg = kMostAdvMsg + " " +  kDoubleRateMsg + " " + kUsingVA;
    else if (deint == "vaapi2doubleratebob")
        msg = kBobMsg + " " +  kDoubleRateMsg + " " + kUsingVA;
    else if (deint == "vaapi2doublerateweave")
        msg = kWeaveMsg + " " +  kDoubleRateMsg + " " + kUsingVA;
    else if (deint == "vaapi2doubleratemotion_adaptive")
        msg = kMAMsg + " " +  kDoubleRateMsg + " " + kUsingVA;
    else if (deint == "vaapi2doubleratemotion_compensated")
        msg = kMCMsg + " " +  kDoubleRateMsg + " " + kUsingVA;

    else if (deint == "nvdecweave")
        msg = kWeaveMsg + " " +  kUsingNV;
    else if (deint == "nvdecbob")
        msg = kBobMsg + " " +  kUsingNV;
    else if (deint == "nvdecadaptive")
        msg = kMAMsg + " " +  kUsingNV;
    else if (deint == "nvdecdoublerateweave")
        msg = kWeaveMsg + " " +  kDoubleRateMsg + " " +  kUsingNV;
    else if (deint == "nvdecdoubleratebob")
        msg = kBobMsg + " " +  kDoubleRateMsg + " " +  kUsingNV;
    else if (deint == "nvdecdoublerateadaptive")
        msg = kMAMsg + " " +  kDoubleRateMsg + " " +  kUsingNV;
    else
        msg = QObject::tr("'%1' has not been documented yet.").arg(deint);

    return msg;
}

QStringList VideoDisplayProfile::GetOSDs(const QString &video_renderer)
{
    QMutexLocker locker(&s_safe_lock);
    init_statics();

    safe_map_t::const_iterator it = s_safe_osd.find(video_renderer);
    QStringList tmp;
    if (it != s_safe_osd.end())
        tmp = *it;
    return tmp;
}

QString VideoDisplayProfile::GetOSDHelp(const QString &osd)
{

    QString msg = QObject::tr("OSD rendering method");

    if (osd.isEmpty())
        return msg;

    if (osd == "chromakey")
        msg = QObject::tr(
            "Render the OSD using the XVideo chromakey feature."
            "This renderer does not alpha blend but is the fastest "
            "OSD renderer for XVideo.") + "\n" +
            QObject::tr(
                "Note: nVidia hardware after the 5xxx series does not "
                "have XVideo chromakey support.");


    if (osd == "softblend")
    {
        msg = QObject::tr(
            "Software OSD rendering uses your CPU to alpha blend the OSD.");
    }

    if (osd.contains("opengl"))
    {
        msg = QObject::tr(
            "Uses OpenGL to alpha blend the OSD onto the video.");
    }

    if (osd =="threaded")
    {
        msg = QObject::tr(
            "Uses OpenGL in a separate thread to overlay the OSD onto the video.");
    }

#ifdef USING_OPENMAX
    if (osd.contains("openmax"))
    {
        msg = QObject::tr(
            "Uses OpenMAX to alpha blend the OSD onto the video.");
    }
#endif

    return msg;
}

bool VideoDisplayProfile::IsFilterAllowed(const QString &video_renderer)
{
    QMutexLocker locker(&s_safe_lock);
    init_statics();
    return s_safe_custom.contains(video_renderer);
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
    QMutexLocker locker(&s_safe_lock);
    init_statics();

    uint    top_priority = 0;
    QString top_renderer;

    QStringList::const_iterator it = renderers.begin();
    for (; it != renderers.end(); ++it)
    {
        priority_map_t::const_iterator p = s_safe_renderer_priority.find(*it);
        if ((p != s_safe_renderer_priority.end()) && (*p >= top_priority))
        {
            top_priority = *p;
            top_renderer = *it;
        }
    }

    return top_renderer;
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

void VideoDisplayProfile::init_statics(void)
{
    if (s_safe_initialized)
        return;

    s_safe_initialized = true;

    render_opts options {};
    options.renderers      = &s_safe_custom;
    options.safe_renderers = &s_safe_renderer;
    options.deints         = &s_safe_deint;
    options.osds           = &s_safe_osd;
    options.render_group   = &s_safe_renderer_group;
    options.priorities     = &s_safe_renderer_priority;
    options.decoders       = &s_safe_decoders;
    options.equiv_decoders = &s_safe_equiv_dec;

    // N.B. assumes NuppelDecoder and DummyDecoder always present
    AvFormatDecoder::GetDecoders(options);
    VideoOutput::GetRenderOptions(options);

    foreach(QString decoder, s_safe_decoders)
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("decoder<->render support: %1%2")
                .arg(decoder, -12).arg(GetVideoRenderers(decoder).join(" ")));
}

// MythTV
#include "videodisplayprofile.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "videooutbase.h"
#include "avformatdecoder.h"

// Std
#include <algorithm>
#include <utility>

void ProfileItem::Clear(void)
{
    m_pref.clear();
}

void ProfileItem::SetProfileID(uint Id)
{
    m_profileid = Id;
}

void ProfileItem::Set(const QString &Value, const QString &Data)
{
    m_pref[Value] = Data;
}

uint ProfileItem::GetProfileID() const
{
    return m_profileid;
}

QMap<QString,QString> ProfileItem::GetAll(void) const
{
    return m_pref;
}

QString ProfileItem::Get(const QString &Value) const
{
    QMap<QString,QString>::const_iterator it = m_pref.find(Value);
    if (it != m_pref.end())
        return *it;
    return QString();
}

uint ProfileItem::GetPriority(void) const
{
    QString tmp = Get("pref_priority");
    return tmp.isEmpty() ? 0 : tmp.toUInt();
}

// options are NNN NNN-MMM 0-MMM NNN-99999 >NNN >=NNN <MMM <=MMM or blank
// If string is blank then assumes a match.
// If value is 0 or negative assume a match (i.e. value unknown assumes a match)
// float values must be no more than 3 decimals.
bool ProfileItem::CheckRange(QString Key, float Value, bool *Ok) const
{
    return CheckRange(std::move(Key), Value, 0, true, Ok);
}

bool ProfileItem::CheckRange(QString Key, int Value, bool *Ok) const
{
    return CheckRange(std::move(Key), 0.0, Value, false, Ok);
}

bool ProfileItem::CheckRange(const QString& Key,
    float FValue, int IValue, bool IsFloat, bool *Ok) const
{
    bool match = true;
    bool isOK = true;
    if (IsFloat)
        IValue = int(FValue * 1000.0F);
    QString cmp = Get(Key);
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
            if (IValue > 0)
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
                    if (IsFloat)
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
                        if (IsFloat)
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
                        match = match && (IValue >= value1 && IValue <= value2);
                    else isOK = false;
                }
            }
        }
    }
    if (Ok != nullptr)
        *Ok = isOK;
    if (!isOK)
        match=false;
    return match;
}

bool ProfileItem::IsMatch(const QSize &Size,
    float Framerate, const QString &CodecName) const
{
    bool    match = true;

    QString cmp;

    // cond_width, cond_height, cond_codecs, cond_framerate.
    // These replace old settings pref_cmp0 and pref_cmp1
    match &= CheckRange("cond_width",Size.width());
    match &= CheckRange("cond_height",Size.height());
    match &= CheckRange("cond_framerate",Framerate);
    // codec
    cmp = Get(QString("cond_codecs"));
    if (!cmp.isEmpty())
    {
        QStringList clist = cmp.split(" ", QString::SkipEmptyParts);
        if (!clist.empty())
            match &= clist.contains(CodecName,Qt::CaseInsensitive);
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

bool ProfileItem::IsValid(QString *Reason) const
{

    bool isOK = true;
    CheckRange("cond_width",1,&isOK);
    if (!isOK)
    {
        if (Reason)
            *Reason = QString("Invalid width condition");
        return false;
    }
    CheckRange("cond_height",1,&isOK);
    if (!isOK)
    {
        if (Reason)
            *Reason = QString("Invalid height condition");
        return false;
    }
    CheckRange("cond_framerate",1.0F,&isOK);
    if (!isOK)
    {
        if (Reason)
            *Reason = QString("Invalid framerate condition");
        return false;
    }

    QString     decoder   = Get("pref_decoder");
    QString     renderer  = Get("pref_videorenderer");
    if (decoder.isEmpty() || renderer.isEmpty())
    {
        if (Reason)
            *Reason = "Need a decoder and renderer";

        return false;
    }

    QStringList decoders  = VideoDisplayProfile::GetDecoders();
    if (!decoders.contains(decoder))
    {
        if (Reason)
        {
            *Reason = QString("decoder %1 is not supported (supported: %2)")
                .arg(decoder).arg(toCommaList(decoders));
        }

        return false;
    }

    QStringList renderers = VideoDisplayProfile::GetVideoRenderers(decoder);
    if (!renderers.contains(renderer))
    {
        if (Reason)
        {
            *Reason = QString("renderer %1 is not supported "
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
        if (Reason)
        {
            *Reason = QString("deinterlacer %1 is not supported "
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
        if (Reason)
        {
            if (deint1.contains("bobdeint") ||
                deint1.contains("doublerate") ||
                deint1.contains("doubleprocess"))
                deints.removeAll(deint1);

            *Reason = QString("deinterlacer %1 is not supported w/renderer %2 "
                              "as second deinterlacer (supported: %3)")
                .arg(deint1).arg(renderer).arg(toCommaList(deints));
        }

        return false;
    }

    QString     filter    = Get("pref_filters");
    if (!filter.isEmpty() && !VideoDisplayProfile::IsFilterAllowed(renderer))
    {
        if (Reason)
        {
            *Reason = QString("Filter %1 is not supported w/renderer %2")
                .arg(filter).arg(renderer);
        }

        return false;
    }

    if (Reason)
        *Reason = QString();

    return true;
}

bool ProfileItem::operator< (const ProfileItem &Other) const
{
    return GetPriority() < Other.GetPriority();
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
    QString deint0    = Get("pref_deint0");
    QString deint1    = Get("pref_deint1");
    QString filter    = Get("pref_filters");

    QString cond = QString("w(%1) h(%2) framerate(%3) codecs(%4)")
        .arg(width).arg(height).arg(framerate).arg(codecs);
    QString str =  QString("cmp(%1%2) %7 dec(%3) cpus(%4) skiploop(%5) rend(%6) ")
        .arg(cmp0).arg(QString(cmp1.isEmpty() ? "" : ",") + cmp1)
        .arg(decoder).arg(max_cpus).arg((skiploop) ? "enabled" : "disabled").arg(renderer)
        .arg(cond);
    str += QString("deint(%1,%2) filt(%3)").arg(deint0).arg(deint1).arg(filter);

    return str;
}

//////////////////////////////////////////////////////////////////////////////

#define LOC     QString("VDP: ")

QMutex                    VideoDisplayProfile::s_safe_lock(QMutex::Recursive);
bool                      VideoDisplayProfile::s_safe_initialized = false;
QMap<QString,QStringList> VideoDisplayProfile::s_safe_renderer;
QMap<QString,QStringList> VideoDisplayProfile::s_safe_renderer_group;
QMap<QString,QStringList> VideoDisplayProfile::s_safe_deint;
QMap<QString,QStringList> VideoDisplayProfile::s_safe_equiv_dec;
QStringList               VideoDisplayProfile::s_safe_custom;
QMap<QString,uint>        VideoDisplayProfile::s_safe_renderer_priority;
QMap<QString,QString>     VideoDisplayProfile::s_dec_name;
QStringList               VideoDisplayProfile::s_safe_decoders;

VideoDisplayProfile::VideoDisplayProfile()
{
    QMutexLocker locker(&s_safe_lock);
    InitStatics();

    QString hostname    = gCoreContext->GetHostName();
    QString cur_profile = GetDefaultProfileName(hostname);
    uint    groupid     = GetProfileGroupID(cur_profile, hostname);

    vector<ProfileItem> items = LoadDB(groupid);
    vector<ProfileItem>::const_iterator it;
    for (it = items.begin(); it != items.end(); ++it)
    {
        QString err;
        if (!(*it).IsValid(&err))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Rejecting: " + (*it).toString() + "\n\t\t\t" + err);
            continue;
        }

        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Accepting: " + (*it).toString());
        m_allowedPreferences.push_back(*it);
    }
}

void VideoDisplayProfile::SetInput(const QSize &Size, float Framerate, const QString &CodecName)
{
    QMutexLocker locker(&m_lock);
    bool change = false;

    if (Size != m_lastSize)
    {
        m_lastSize = Size;
        change = true;
    }
    if (Framerate > 0.0F && !qFuzzyCompare(Framerate + 1.0f, m_lastRate + 1.0f))
    {
        m_lastRate = Framerate;
        change = true;
    }
    if (!CodecName.isEmpty() && CodecName != m_lastCodecName)
    {
        m_lastCodecName = CodecName;
        change = true;
    }
    if (change)
        LoadBestPreferences(m_lastSize, m_lastRate, m_lastCodecName);
}

void VideoDisplayProfile::SetOutput(float Framerate)
{
    QMutexLocker locker(&m_lock);
    if (!qFuzzyCompare(Framerate + 1.0f, m_lastRate + 1.0f))
    {
        m_lastRate = Framerate;
        LoadBestPreferences(m_lastSize, m_lastRate, m_lastCodecName);
    }
}

float VideoDisplayProfile::GetOutput(void) const
{
    return m_lastRate;
}

QString VideoDisplayProfile::GetDecoder(void) const
{
    return GetPreference("pref_decoder");
}

uint VideoDisplayProfile::GetMaxCPUs(void) const
{
    return GetPreference("pref_max_cpus").toUInt();
}

bool VideoDisplayProfile::IsSkipLoopEnabled(void) const
{
    return GetPreference("pref_skiploop").toInt();
}

QString VideoDisplayProfile::GetVideoRenderer(void) const
{
    return GetPreference("pref_videorenderer");
}

QString VideoDisplayProfile::GetDeinterlacer(void) const
{
    return GetPreference("pref_deint0");
}

QString VideoDisplayProfile::GetFallbackDeinterlacer(void) const
{
    return GetPreference("pref_deint1");
}

QString VideoDisplayProfile::GetActualVideoRenderer(void) const
{
    return m_lastVideoRenderer;
}

QString VideoDisplayProfile::GetFilters(void) const
{
    return GetPreference("pref_filters");
}

void VideoDisplayProfile::SetVideoRenderer(const QString &VideoRenderer)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("SetVideoRenderer(%1)").arg(VideoRenderer));

    m_lastVideoRenderer = VideoRenderer;
    if (VideoRenderer == GetVideoRenderer())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("SetVideoRender(%1) == GetVideoRenderer()").arg(VideoRenderer));
        return; // already made preferences safe...
    }

    // Make preferences safe...
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Old preferences: " + toString());

    SetPreference("pref_videorenderer", VideoRenderer);

    QStringList deints = GetDeinterlacers(VideoRenderer);
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

bool VideoDisplayProfile::CheckVideoRendererGroup(const QString &Renderer)
{
    if (m_lastVideoRenderer == Renderer || m_lastVideoRenderer == "null")
        return true;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Preferred video renderer: %1 (current: %2)")
                .arg(Renderer).arg(m_lastVideoRenderer));

    QMap<QString,QStringList>::const_iterator it = s_safe_renderer_group.begin();
    for (; it != s_safe_renderer_group.end(); ++it)
        if (it->contains(m_lastVideoRenderer) && it->contains(Renderer))
            return true;
    return false;
}

bool VideoDisplayProfile::IsDecoderCompatible(const QString &Decoder)
{
    const QString dec = GetDecoder();
    if (dec == Decoder)
        return true;

    QMutexLocker locker(&s_safe_lock);
    return (s_safe_equiv_dec[dec].contains(Decoder));
}

QString VideoDisplayProfile::GetFilteredDeint(const QString &Override)
{
    QString renderer = GetActualVideoRenderer();
    QString deint    = GetDeinterlacer();

    QMutexLocker locker(&m_lock);

    if (!Override.isEmpty() && GetDeinterlacers(renderer).contains(Override))
        deint = Override;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("GetFilteredDeint(%1) : %2 -> '%3'")
            .arg(Override).arg(renderer).arg(deint));

    return deint;
}

QString VideoDisplayProfile::GetPreference(const QString &Key) const
{
    QMutexLocker locker(&m_lock);

    if (Key.isEmpty())
        return QString();

    QMap<QString,QString>::const_iterator it = m_currentPreferences.find(Key);
    if (it == m_currentPreferences.end())
        return QString();

    return *it;
}

void VideoDisplayProfile::SetPreference(const QString &Key, const QString &Value)
{
    QMutexLocker locker(&m_lock);

    if (!Key.isEmpty())
        m_currentPreferences[Key] = Value;
}

vector<ProfileItem>::const_iterator VideoDisplayProfile::FindMatch
    (const QSize &Size, float Framerate, const QString &CodecName)
{
    vector<ProfileItem>::const_iterator it = m_allowedPreferences.begin();
    for (; it != m_allowedPreferences.end(); ++it)
        if ((*it).IsMatch(Size, Framerate, CodecName))
            return it;
    return m_allowedPreferences.end();
}

void VideoDisplayProfile::LoadBestPreferences
    (const QSize &Size, float Framerate, const QString &CodecName)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("LoadBestPreferences(%1x%2, %3, %4)")
        .arg(Size.width()).arg(Size.height())
        .arg(static_cast<double>(Framerate), 0, 'f', 3).arg(CodecName));

    m_currentPreferences.clear();
    vector<ProfileItem>::const_iterator it = FindMatch(Size, Framerate, CodecName);
    if (it != m_allowedPreferences.end())
        m_currentPreferences = (*it).GetAll();

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("LoadBestPreferences Result "
            "prio:%1, w:%2, h:%3, fps:%4,"
            " codecs:%5, decoder:%6, renderer:%7, deint:%8")
            .arg(GetPreference("pref_priority")).arg(GetPreference("cond_width"))
            .arg(GetPreference("cond_height")).arg(GetPreference("cond_framerate"))
            .arg(GetPreference("cond_codecs")).arg(GetPreference("pref_decoder"))
            .arg(GetPreference("pref_videorenderer")).arg(GetPreference("pref_deint0"))
            );
}

vector<ProfileItem> VideoDisplayProfile::LoadDB(uint GroupId)
{
    ProfileItem tmp;
    vector<ProfileItem> list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT profileid, value, data "
        "FROM displayprofiles "
        "WHERE profilegroupid = :GROUPID "
        "ORDER BY profileid");
    query.bindValue(":GROUPID", GroupId);
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

bool VideoDisplayProfile::DeleteDB(uint GroupId, const vector<ProfileItem> &Items)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "DELETE FROM displayprofiles "
        "WHERE profilegroupid = :GROUPID   AND "
        "      profileid      = :PROFILEID");

    bool ok = true;
    vector<ProfileItem>::const_iterator it = Items.begin();
    for (; it != Items.end(); ++it)
    {
        if (!(*it).GetProfileID())
            continue;

        query.bindValue(":GROUPID",   GroupId);
        query.bindValue(":PROFILEID", (*it).GetProfileID());
        if (!query.exec())
        {
            MythDB::DBError("vdp::deletedb", query);
            ok = false;
        }
    }

    return ok;
}

bool VideoDisplayProfile::SaveDB(uint GroupId, vector<ProfileItem> &Items)
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
    vector<ProfileItem>::iterator it = Items.begin();
    for (; it != Items.end(); ++it)
    {
        QMap<QString,QString> list = (*it).GetAll();
        if (list.begin() == list.end())
            continue;

        QMap<QString,QString>::const_iterator lit = list.begin();

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

                insert.bindValue(":GROUPID",   GroupId);
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
            query.bindValue(":GROUPID",   GroupId);
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
                    sqldelete.bindValue(":GROUPID",   GroupId);
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
                    update.bindValue(":GROUPID",   GroupId);
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
                insert.bindValue(":GROUPID",   GroupId);
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
    InitStatics();
    return s_safe_decoders;
}

QStringList VideoDisplayProfile::GetDecoderNames(void)
{
    InitStatics();
    QStringList list;

    const QStringList decs = GetDecoders();
    QStringList::const_iterator it = decs.begin();
    for (; it != decs.end(); ++it)
        list += GetDecoderName(*it);

    return list;
}

QString VideoDisplayProfile::GetDecoderName(const QString &Decoder)
{
    if (Decoder.isEmpty())
        return "";

    QMutexLocker locker(&s_safe_lock);
    if (s_dec_name.empty())
    {
        s_dec_name["ffmpeg"]         = QObject::tr("Standard");
        s_dec_name["vdpau"]          = QObject::tr("NVIDIA VDPAU acceleration");
        s_dec_name["vdpau-dec"]      = QObject::tr("NVIDIA VDPAU acceleration (decode only)");
        s_dec_name["vaapi"]          = QObject::tr("VAAPI acceleration");
        s_dec_name["vaapi-dec"]      = QObject::tr("VAAPI acceleration (decode only)");
        s_dec_name["dxva2"]          = QObject::tr("Windows hardware acceleration");
        s_dec_name["mediacodec"]     = QObject::tr("Android MediaCodec acceleration");
        s_dec_name["mediacodec-dec"] = QObject::tr("Android MediaCodec acceleration (decode only)");
        s_dec_name["nvdec"]          = QObject::tr("NVIDIA NVDEC acceleration");
        s_dec_name["nvdec-dec"]      = QObject::tr("NVIDIA NVDEC acceleration (decode only)");
    }

    QString ret = Decoder;
    QMap<QString,QString>::const_iterator it = s_dec_name.find(Decoder);
    if (it != s_dec_name.end())
        ret = *it;
    return ret;
}


QString VideoDisplayProfile::GetDecoderHelp(const QString& Decoder)
{
    QString msg = QObject::tr("Processing method used to decode video.");

    if (Decoder.isEmpty())
        return msg;

    msg += "\n";

    if (Decoder == "ffmpeg")
        msg += QObject::tr("Standard will use ffmpeg library.");

    if (Decoder == "vdpau")
        msg += QObject::tr(
            "VDPAU will attempt to use the graphics hardware to "
            "accelerate video decoding and playback.");

    if (Decoder == "vaapi")
        msg += QObject::tr(
            "VAAPI will attempt to use the graphics hardware to "
            "accelerate video decoding and playback.");

    if (Decoder == "vaapi-dec")
        msg += QObject::tr(
            "Will attempt to use the graphics hardware to "
            "accelerate video decoding only.");

    if (Decoder == "dxva2")
        msg += QObject::tr(
            "DXVA2 will use the graphics hardware to "
            "accelerate video decoding and playback. ");

    if (Decoder == "mediacodec-dec")
        msg += QObject::tr(
            "Mediacodec will use the graphics hardware to "
            "accelerate video decoding on Android. ");

    if (Decoder == "nvdec-dec")
        msg += QObject::tr(
            "Nvdec uses the new NVDEC API to "
            "accelerate video decoding on NVIDIA Graphics Adapters. ");

    return msg;
}

QString VideoDisplayProfile::GetDeinterlacerName(const QString &ShortName)
{
    if ("none" == ShortName)
        return QObject::tr("None");
    if ("linearblend" == ShortName)
        return QObject::tr("Linear blend");
    if ("kerneldeint" == ShortName)
        return QObject::tr("Kernel");
    if ("kerneldoubleprocessdeint" == ShortName)
        return QObject::tr("Kernel (2x)");
    if ("greedyhdeint" == ShortName)
        return QObject::tr("Greedy HighMotion");
    if ("greedyhdoubleprocessdeint" == ShortName)
        return QObject::tr("Greedy HighMotion (2x)");
    if ("yadifdeint" == ShortName)
        return QObject::tr("Yadif");
    if ("yadifdoubleprocessdeint" == ShortName)
        return QObject::tr("Yadif (2x)");
    if ("bobdeint" == ShortName)
        return QObject::tr("Bob (2x)");
    if ("onefield" == ShortName)
        return QObject::tr("One field");
    if ("fieldorderdoubleprocessdeint" == ShortName)
        return QObject::tr("Interlaced (2x)");
    if ("opengllinearblend" == ShortName)
        return QObject::tr("Linear blend (HW-GL)");
    if ("openglkerneldeint" == ShortName)
        return QObject::tr("Kernel (HW-GL)");
    if ("openglbobdeint" == ShortName)
        return QObject::tr("Bob (2x, HW-GL)");
    if ("openglonefield" == ShortName)
        return QObject::tr("One field (HW-GL)");
    if ("opengldoubleratekerneldeint" == ShortName)
        return QObject::tr("Kernel (2x, HW-GL)");
    if ("opengldoubleratelinearblend" == ShortName)
        return QObject::tr("Linear blend (2x, HW-GL)");
    if ("opengldoubleratefieldorder" == ShortName)
        return QObject::tr("Interlaced (2x, HW-GL)");
    if ("vdpauonefield" == ShortName)
        return QObject::tr("One Field (1x, HW)");
    if ("vdpaubobdeint" == ShortName)
        return QObject::tr("Bob (2x, HW)");
    if ("vdpaubasic" == ShortName)
        return QObject::tr("Temporal (1x, HW)");
    if ("vdpaubasicdoublerate" == ShortName)
        return QObject::tr("Temporal (2x, HW)");
    if ("vdpauadvanced" == ShortName)
        return QObject::tr("Advanced (1x, HW)");
    if ("vdpauadvanceddoublerate" == ShortName)
        return QObject::tr("Advanced (2x, HW)");
#ifdef USING_VAAPI
    if ("vaapi2default" == ShortName)
        return QObject::tr("Advanced (HW-VA)");
    if ("vaapi2bob" == ShortName)
        return QObject::tr("Bob (HW-VA)");
    if ("vaapi2weave" == ShortName)
        return QObject::tr("Weave (HW-VA)");
    if ("vaapi2motion_adaptive" == ShortName)
        return QObject::tr("Motion Adaptive (HW-VA)");
    if ("vaapi2motion_compensated" == ShortName)
        return QObject::tr("Motion Compensated (HW-VA)");
    if ("vaapi2doubleratedefault" == ShortName)
        return QObject::tr("Advanced (2x, HW-VA)");
    if ("vaapi2doubleratebob" == ShortName)
        return QObject::tr("Bob (2x, HW-VA)");
    if ("vaapi2doublerateweave" == ShortName)
        return QObject::tr("Weave (2x, HW-VA)");
    if ("vaapi2doubleratemotion_adaptive" == ShortName)
        return QObject::tr("Motion Adaptive (2x, HW-VA)");
    if ("vaapi2doubleratemotion_compensated" == ShortName)
        return QObject::tr("Motion Compensated (2x, HW-VA)");
#endif
#ifdef USING_NVDEC
    if ("nvdecweave" == ShortName)
        return QObject::tr("Weave (HW-NV)");
    if ("nvdecbob" == ShortName)
        return QObject::tr("Bob (HW-NV)");
    if ("nvdecadaptive" == ShortName)
        return QObject::tr("Adaptive (HW-NV)");
    if ("nvdecdoublerateweave" == ShortName)
        return QObject::tr("Weave (2x, HW-NV)");
    if ("nvdecdoubleratebob" == ShortName)
        return QObject::tr("Bob (2x, HW-NV)");
    if ("nvdecdoublerateadaptive" == ShortName)
        return QObject::tr("Adaptive (2x, HW-NV)");
#endif // USING_NVDEC

    return "";
}

QStringList VideoDisplayProfile::GetProfiles(const QString &HostName)
{
    InitStatics();
    QStringList list;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT name "
        "FROM displayprofilegroups "
        "WHERE hostname = :HOST ");
    query.bindValue(":HOST", HostName);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("get_profiles", query);
    else
    {
        while (query.next())
            list += query.value(0).toString();
    }
    return list;
}

QString VideoDisplayProfile::GetDefaultProfileName(const QString &HostName)
{
    QString tmp =
        gCoreContext->GetSettingOnHost("DefaultVideoPlaybackProfile", HostName);

    QStringList profiles = GetProfiles(HostName);

    tmp = (profiles.contains(tmp)) ? tmp : QString();

    if (tmp.isEmpty())
    {
        if (!profiles.empty())
            tmp = profiles[0];

        tmp = (profiles.contains("Normal")) ? "Normal" : tmp;

        if (!tmp.isEmpty())
        {
            gCoreContext->SaveSettingOnHost(
                "DefaultVideoPlaybackProfile", tmp, HostName);
        }
    }

    return tmp;
}

void VideoDisplayProfile::SetDefaultProfileName(const QString &ProfileName, const QString &HostName)
{
    gCoreContext->SaveSettingOnHost("DefaultVideoPlaybackProfile", ProfileName, HostName);
}

uint VideoDisplayProfile::GetProfileGroupID(const QString &ProfileName,
                                            const QString &HostName)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT profilegroupid "
        "FROM displayprofilegroups "
        "WHERE name     = :NAME AND "
        "      hostname = :HOST ");
    query.bindValue(":NAME", ProfileName);
    query.bindValue(":HOST", HostName);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("get_profile_group_id", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}

void VideoDisplayProfile::DeleteProfiles(const QString &HostName)
{
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());
    query.prepare(
        "SELECT profilegroupid "
        "FROM displayprofilegroups "
        "WHERE hostname = :HOST ");
    query.bindValue(":HOST", HostName);
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
    query.bindValue(":HOST", HostName);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("delete_profiles 3", query);
}

void VideoDisplayProfile::CreateProfile(uint GroupId, uint Priority,
    const QString& Width, const QString& Height, const QString& Codecs,
    const QString& Decoder, uint MaxCpus, bool SkipLoop, const QString& VideoRenderer,
    const QString& Deint1, const QString& Deint2, const QString& Filters)
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
    query.bindValue(":GRPID",    GroupId);
    query.bindValue(":PROFID",   profileid);
    query.bindValue(":PRIORITY", Priority);
    if (!query.exec())
        MythDB::DBError("create_profile 2", query);

    QStringList queryValue;
    QStringList queryData;

    queryValue += "cond_width";
    queryData  += Width;

    queryValue += "cond_height";
    queryData  += Height;

    queryValue += "cond_codecs";
    queryData  += Codecs;

    queryValue += "pref_decoder";
    queryData  += Decoder;

    queryValue += "pref_max_cpus";
    queryData  += QString::number(MaxCpus);

    queryValue += "pref_skiploop";
    queryData  += (SkipLoop) ? "1" : "0";

    queryValue += "pref_videorenderer";
    queryData  += VideoRenderer;

    queryValue += "pref_deint0";
    queryData  += Deint1;

    queryValue += "pref_deint1";
    queryData  += Deint2;

    queryValue += "pref_filters";
    queryData  += Filters;

    QStringList::const_iterator itV = queryValue.begin();
    QStringList::const_iterator itD = queryData.begin();
    for (; itV != queryValue.end() && itD != queryData.end(); ++itV,++itD)
    {
        if (itD->isEmpty())
            continue;
        query.prepare(
            "INSERT INTO displayprofiles "
            "VALUES (:GRPID, :PROFID, :VALUE, :DATA)");
        query.bindValue(":GRPID",  GroupId);
        query.bindValue(":PROFID", profileid);
        query.bindValue(":VALUE",  *itV);
        query.bindValue(":DATA",   *itD);
        if (!query.exec())
            MythDB::DBError("create_profile 3", query);
    }
}

uint VideoDisplayProfile::CreateProfileGroup(const QString &ProfileName, const QString &HostName)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "INSERT INTO displayprofilegroups (name, hostname) "
        "VALUES (:NAME,:HOST)");

    query.bindValue(":NAME", ProfileName);
    query.bindValue(":HOST", HostName);

    if (!query.exec())
    {
        MythDB::DBError("create_profile_group", query);
        return 0;
    }

    return GetProfileGroupID(ProfileName, HostName);
}

bool VideoDisplayProfile::DeleteProfileGroup(const QString &GroupName, const QString &HostName)
{
    bool ok = true;
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());

    query.prepare(
        "SELECT profilegroupid "
        "FROM displayprofilegroups "
        "WHERE name     = :NAME AND "
        "      hostname = :HOST ");

    query.bindValue(":NAME", GroupName);
    query.bindValue(":HOST", HostName);

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

    query.bindValue(":NAME", GroupName);
    query.bindValue(":HOST", HostName);

    if (!query.exec())
    {
        MythDB::DBError("delete_profile_group 3", query);
        ok = false;
    }

    return ok;
}

void VideoDisplayProfile::CreateProfiles(const QString &HostName)
{
    QStringList profiles = GetProfiles(HostName);
    uint groupid;

#ifdef USING_OPENGL_VIDEO
    if (!profiles.contains("OpenGL High Quality")) {
        (void) QObject::tr("OpenGL High Quality",
                           "Sample: OpenGL high quality");
        groupid = CreateProfileGroup("OpenGL High Quality", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl",
                      "greedyhdoubleprocessdeint", "greedyhdeint", "");
    }

    if (!profiles.contains("OpenGL Normal")) {
        (void) QObject::tr("OpenGL Normal", "Sample: OpenGL average quality");
        groupid = CreateProfileGroup("OpenGL Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl",
                      "opengldoubleratekerneldeint", "openglkerneldeint", "");
    }

    if (!profiles.contains("OpenGL Slim")) {
        (void) QObject::tr("OpenGL Slim", "Sample: OpenGL low power GPU");
        groupid = CreateProfileGroup("OpenGL Slim", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 1, true, "opengl",
                      "opengldoubleratelinearblend", "opengllinearblend", "");
    }
#endif

#ifdef USING_VAAPI
    if (!profiles.contains("VAAPI Normal")) {
        (void) QObject::tr("VAAPI Normal", "Sample: VAAPI average quality");
        groupid = CreateProfileGroup("VAAPI Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "vaapi", 2, true, "opengl",
                      "none", "none", "");
        CreateProfile(groupid, 2, "", "", "",
                      "ffmpeg", 2, true, "opengl",
                      "opengldoubleratekerneldeint", "openglkerneldeint", "");
    }
#endif

#ifdef USING_MEDIACODEC
    if (!profiles.contains("MediaCodec Normal")) {
        (void) QObject::tr("MediaCodec Normal",
                           "Sample: MediaCodec Normal");
        groupid = CreateProfileGroup("MediaCodec Normal", hostname);
        CreateProfile(groupid, 1, "", "", "",
                      "mediacodec-dec", 4, true, "opengl",
                      "opengldoubleratelinearblend", "opengllinearblend", "");
    }
#endif

#if defined(USING_VAAPI) && defined(USING_OPENGL_VIDEO)
    if (!profiles.contains("VAAPI2 Normal")) {
        (void) QObject::tr("VAAPI2 Normal",
                           "Sample: VAAPI2 Normal");
        groupid = CreateProfileGroup("VAAPI2 Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "vaapi-dec", 4, true, "opengl",
                      "vaapi2doubleratedefault", "vaapi2default", "");
    }
#endif

#if defined(USING_NVDEC) && defined(USING_OPENGL_VIDEO)
    if (!profiles.contains("NVDEC Normal")) {
        (void) QObject::tr("NVDEC Normal",
                           "Sample: NVDEC Normal");
        groupid = CreateProfileGroup("NVDEC Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "nvdec", 4, true, "opengl",
                      "nvdecdoublerateadaptive", "nvdecadaptive", "");
    }
#endif

}

QStringList VideoDisplayProfile::GetVideoRenderers(const QString &Decoder)
{
    QMutexLocker locker(&s_safe_lock);
    InitStatics();

    QMap<QString,QStringList>::const_iterator it = s_safe_renderer.find(Decoder);
    QStringList tmp;
    if (it != s_safe_renderer.end())
        tmp = *it;
    return tmp;
}

QString VideoDisplayProfile::GetVideoRendererHelp(const QString &Renderer)
{
    QString msg = QObject::tr("Video rendering method");

    if (Renderer.isEmpty())
        return msg;

    if (Renderer == "null")
        msg = QObject::tr(
            "Render video offscreen. Used internally.");

    if (Renderer == "direct3d")
        msg = QObject::tr(
            "Windows video renderer based on Direct3D. Requires "
            "video card compatible with Direct3D 9. This is the preferred "
            "renderer for current Windows systems.");

    if (Renderer == "opengl")
    {
        msg = QObject::tr(
            "Video is converted to an intermediate format by the CPU (YUV2) "
            "before OpenGL is used for color conversion, scaling, picture controls"
            " and optionally deinterlacing. Processing is balanced between the CPU "
            "and GPU.");
    }

    if (Renderer == "opengl-yv12")
    {
        msg = QObject::tr(
            "OpenGL is used for all color conversion, scaling, picture "
            "controls and optionally deinterlacing. CPU load is low but a more "
            "powerful GPU is needed for deinterlacing.");
    }

    if (Renderer == "opengl-hw")
    {
        msg = QObject::tr(
             "This video renderer is used by hardware decoders to display "
             "frames using OpenGL.");
    }

    return msg;
}

QString VideoDisplayProfile::GetPreferredVideoRenderer(const QString &Decoder)
{
    return GetBestVideoRenderer(GetVideoRenderers(Decoder));
}

QStringList VideoDisplayProfile::GetDeinterlacers(const QString &VideoRenderer)
{
    QMutexLocker locker(&s_safe_lock);
    InitStatics();

    QMap<QString,QStringList>::const_iterator it = s_safe_deint.find(VideoRenderer);
    QStringList tmp;
    if (it != s_safe_deint.end())
        tmp = *it;
    return tmp;
}

QString VideoDisplayProfile::GetDeinterlacerHelp(const QString &Deinterlacer)
{
    if (Deinterlacer.isEmpty())
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

    if (Deinterlacer == "none")
        msg = kNoneMsg;
    else if (Deinterlacer == "onefield")
        msg = kOneFieldMsg;
    else if (Deinterlacer == "bobdeint")
        msg = kBobMsg;
    else if (Deinterlacer == "linearblend")
        msg = kLinearBlendMsg;
    else if (Deinterlacer == "kerneldeint")
        msg = kKernelMsg;
    else if (Deinterlacer == "kerneldoubleprocessdeint")
        msg = kKernelMsg + " " + kDoubleRateMsg;
    else if (Deinterlacer == "openglonefield")
        msg = kOneFieldMsg + " " + kUsingGL;
    else if (Deinterlacer == "openglbobdeint")
        msg = kBobMsg + " " + kUsingGL;
    else if (Deinterlacer == "opengllinearblend")
        msg = kLinearBlendMsg + " " + kUsingGL;
    else if (Deinterlacer == "openglkerneldeint")
        msg = kKernelMsg + " " + kUsingGL;
    else if (Deinterlacer == "opengldoubleratelinearblend")
        msg = kLinearBlendMsg + " " +  kDoubleRateMsg + " " + kUsingGL;
    else if (Deinterlacer == "opengldoubleratekerneldeint")
        msg = kKernelMsg + " " +  kDoubleRateMsg + " " + kUsingGL;
    else if (Deinterlacer == "opengldoubleratefieldorder")
        msg = kFieldOrderMsg + " " +  kDoubleRateMsg  + " " + kUsingGL;
    else if (Deinterlacer == "greedyhdeint")
        msg = kGreedyHMsg;
    else if (Deinterlacer == "greedyhdoubleprocessdeint")
        msg = kGreedyHMsg + " " +  kDoubleRateMsg;
    else if (Deinterlacer == "yadifdeint")
        msg = kYadifMsg;
    else if (Deinterlacer == "yadifdoubleprocessdeint")
        msg = kYadifMsg + " " +  kDoubleRateMsg;
    else if (Deinterlacer == "fieldorderdoubleprocessdeint")
        msg = kFieldOrderMsg + " " +  kDoubleRateMsg;
    else if (Deinterlacer == "vdpauonefield")
        msg = kOneFieldMsg + " " + kUsingGPU;
    else if (Deinterlacer == "vdpaubobdeint")
        msg = kBobMsg + " " + kUsingGPU;
    else if (Deinterlacer == "vdpaubasic")
        msg = kBasicMsg + " " + kUsingGPU;
    else if (Deinterlacer == "vdpauadvanced")
        msg = kAdvMsg + " " + kUsingGPU;
    else if (Deinterlacer == "vdpaubasicdoublerate")
        msg = kBasicMsg + " " +  kDoubleRateMsg + " " + kUsingGPU;
    else if (Deinterlacer == "vdpauadvanceddoublerate")
        msg = kAdvMsg + " " +  kDoubleRateMsg + " " + kUsingGPU;
    else if (Deinterlacer == "vaapionefield")
        msg = kOneFieldMsg + " " + kUsingGPU;
    else if (Deinterlacer == "vaapibobdeint")
        msg = kBobMsg + " " + kUsingGPU;

    else if (Deinterlacer == "vaapi2default")
        msg = kMostAdvMsg + " " +  kUsingVA;
    else if (Deinterlacer == "vaapi2bob")
        msg = kBobMsg + " " +  kUsingVA;
    else if (Deinterlacer == "vaapi2weave")
        msg = kWeaveMsg + " " +  kUsingVA;
    else if (Deinterlacer == "vaapi2motion_adaptive")
        msg = kMAMsg + " " +  kUsingVA;
    else if (Deinterlacer == "vaapi2motion_compensated")
        msg = kMCMsg + " " +  kUsingVA;
    else if (Deinterlacer == "vaapi2doubleratedefault")
        msg = kMostAdvMsg + " " +  kDoubleRateMsg + " " + kUsingVA;
    else if (Deinterlacer == "vaapi2doubleratebob")
        msg = kBobMsg + " " +  kDoubleRateMsg + " " + kUsingVA;
    else if (Deinterlacer == "vaapi2doublerateweave")
        msg = kWeaveMsg + " " +  kDoubleRateMsg + " " + kUsingVA;
    else if (Deinterlacer == "vaapi2doubleratemotion_adaptive")
        msg = kMAMsg + " " +  kDoubleRateMsg + " " + kUsingVA;
    else if (Deinterlacer == "vaapi2doubleratemotion_compensated")
        msg = kMCMsg + " " +  kDoubleRateMsg + " " + kUsingVA;

    else if (Deinterlacer == "nvdecweave")
        msg = kWeaveMsg + " " +  kUsingNV;
    else if (Deinterlacer == "nvdecbob")
        msg = kBobMsg + " " +  kUsingNV;
    else if (Deinterlacer == "nvdecadaptive")
        msg = kMAMsg + " " +  kUsingNV;
    else if (Deinterlacer == "nvdecdoublerateweave")
        msg = kWeaveMsg + " " +  kDoubleRateMsg + " " +  kUsingNV;
    else if (Deinterlacer == "nvdecdoubleratebob")
        msg = kBobMsg + " " +  kDoubleRateMsg + " " +  kUsingNV;
    else if (Deinterlacer == "nvdecdoublerateadaptive")
        msg = kMAMsg + " " +  kDoubleRateMsg + " " +  kUsingNV;
    else
        msg = QObject::tr("'%1' has not been documented yet.").arg(Deinterlacer);

    return msg;
}

bool VideoDisplayProfile::IsFilterAllowed(const QString &VideoRenderer)
{
    QMutexLocker locker(&s_safe_lock);
    InitStatics();
    return s_safe_custom.contains(VideoRenderer);
}

QStringList VideoDisplayProfile::GetFilteredRenderers(const QString &Decoder, const QStringList &Renderers)
{
    const QStringList dec_list = GetVideoRenderers(Decoder);
    QStringList new_list;

    QStringList::const_iterator it = dec_list.begin();
    for (; it != dec_list.end(); ++it)
        if (Renderers.contains(*it))
            new_list.push_back(*it);

    return new_list;
}

QString VideoDisplayProfile::GetBestVideoRenderer(const QStringList &Renderers)
{
    QMutexLocker locker(&s_safe_lock);
    InitStatics();

    uint    top_priority = 0;
    QString top_renderer;

    QStringList::const_iterator it = Renderers.begin();
    for (; it != Renderers.end(); ++it)
    {
        QMap<QString,uint>::const_iterator p = s_safe_renderer_priority.find(*it);
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

void VideoDisplayProfile::InitStatics(bool Reinit /*= false*/)
{
    if (Reinit)
    {
        s_safe_custom.clear();
        s_safe_renderer.clear();
        s_safe_deint.clear();
        s_safe_renderer_group.clear();
        s_safe_renderer_priority.clear();
        s_safe_decoders.clear();
        s_safe_equiv_dec.clear();
    }
    else if (s_safe_initialized)
    {
        return;
    }
    s_safe_initialized = true;

    RenderOptions options;
    options.renderers      = &s_safe_custom;
    options.safe_renderers = &s_safe_renderer;
    options.deints         = &s_safe_deint;
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

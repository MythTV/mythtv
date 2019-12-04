// MythTV
#include "videodisplayprofile.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "mythvideoout.h"
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
bool ProfileItem::CheckRange(const QString &Key, float Value, bool *Ok) const
{
    return CheckRange(Key, Value, 0, true, Ok);
}

bool ProfileItem::CheckRange(const QString &Key, int Value, bool *Ok) const
{
    return CheckRange(Key, 0.0, Value, false, Ok);
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
            *Reason = QString("decoder %1 is not available").arg(decoder);
        return false;
    }

    QStringList renderers = VideoDisplayProfile::GetVideoRenderers(decoder);
    if (!renderers.contains(renderer))
    {
        if (Reason)
            *Reason = QString("renderer %1 is not supported with decoder %2")
                .arg(renderer).arg(decoder);
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

    QString cond = QString("w(%1) h(%2) framerate(%3) codecs(%4)")
        .arg(width).arg(height).arg(framerate).arg(codecs);
    QString str =  QString("cmp(%1%2) %7 dec(%3) cpus(%4) skiploop(%5) rend(%6) ")
        .arg(cmp0).arg(QString(cmp1.isEmpty() ? "" : ",") + cmp1)
        .arg(decoder).arg(max_cpus).arg((skiploop) ? "enabled" : "disabled").arg(renderer)
        .arg(cond);
    str += QString("deint(%1,%2)").arg(deint0).arg(deint1);

    return str;
}

//////////////////////////////////////////////////////////////////////////////

#define LOC     QString("VDP: ")

QMutex                    VideoDisplayProfile::s_safe_lock(QMutex::Recursive);
bool                      VideoDisplayProfile::s_safe_initialized = false;
QMap<QString,QStringList> VideoDisplayProfile::s_safe_renderer;
QMap<QString,QStringList> VideoDisplayProfile::s_safe_renderer_group;
QMap<QString,QStringList> VideoDisplayProfile::s_safe_equiv_dec;
QStringList               VideoDisplayProfile::s_safe_custom;
QMap<QString,uint>        VideoDisplayProfile::s_safe_renderer_priority;
QMap<QString,QString>     VideoDisplayProfile::s_dec_name;
QMap<QString,QString>     VideoDisplayProfile::s_rend_name;
QStringList               VideoDisplayProfile::s_safe_decoders;
QList<QPair<QString,QString> > VideoDisplayProfile::s_deinterlacer_options;

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
    if (Framerate > 0.0F && !qFuzzyCompare(Framerate + 1.0F, m_lastRate + 1.0F))
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
    if (!qFuzzyCompare(Framerate + 1.0F, m_lastRate + 1.0F))
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

QString VideoDisplayProfile::GetSingleRatePreferences(void) const
{
    return GetPreference("pref_deint0");
}

QString VideoDisplayProfile::GetDoubleRatePreferences(void) const
{
    return GetPreference("pref_deint1");
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

QString VideoDisplayProfile::GetActualVideoRenderer(void) const
{
    return m_lastVideoRenderer;
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
    auto it = FindMatch(Size, Framerate, CodecName);
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
                    LOG(VB_PLAYBACK, LOG_NOTICE, LOC + QString("Ignoring profile %1 (%2)")
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
            LOG(VB_PLAYBACK, LOG_NOTICE, LOC + QString("Ignoring profile %1 (%2)")
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
    auto it = Items.cbegin();
    for (; it != Items.cend(); ++it)
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
    auto it = Items.begin();
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
        s_dec_name["vtb"]            = QObject::tr("VideoToolbox acceleration");
        s_dec_name["vtb-dec"]        = QObject::tr("VideoToolbox acceleration (decode only)");
        s_dec_name["v4l2"]           = QObject::tr("V4L2 acceleration");
        s_dec_name["v4l2-dec"]       = QObject::tr("V4L2 acceleration (decode only)");
        s_dec_name["mmal"]           = QObject::tr("MMAL acceleration");
        s_dec_name["mmal-dec"]       = QObject::tr("MMAL acceleration (decode only)");
        s_dec_name["drmprime"]       = QObject::tr("DRM PRIME acceleration");
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
        msg += QObject::tr("Standard will use the FFmpeg library for software decoding.");

    if (Decoder.startsWith("vdpau"))
        msg += QObject::tr(
            "VDPAU will attempt to use the graphics hardware to "
            "accelerate video decoding.");

    if (Decoder.startsWith("vaapi"))
        msg += QObject::tr(
            "VAAPI will attempt to use the graphics hardware to "
            "accelerate video decoding and playback.");

    if (Decoder.startsWith("dxva2"))
        msg += QObject::tr(
            "DXVA2 will use the graphics hardware to "
            "accelerate video decoding and playback. ");

    if (Decoder.startsWith("mediacodec"))
        msg += QObject::tr(
            "Mediacodec will use Android graphics hardware to "
            "accelerate video decoding and playback. ");

    if (Decoder.startsWith("nvdec"))
        msg += QObject::tr(
            "Nvdec uses the NVDEC API to "
            "accelerate video decoding and playback with NVIDIA Graphics Adapters. ");

    if (Decoder.startsWith("vtb"))
        msg += QObject::tr(
            "The VideoToolbox library is used to accelerate video decoding. ");

    if (Decoder.startsWith("mmal"))
        msg += QObject::tr(
            "MMAL is used to accelerated video decoding (Raspberry Pi only). ");

    if (Decoder == "v4l2")
        msg += "Highly experimental: ";

    if (Decoder.startsWith("v4l2"))
        msg += QObject::tr(
            "Video4Linux codecs are used to accelerate video decoding on "
            "supported platforms. ");

    if (Decoder == "drmprime")
        msg += QObject::tr(
            "DRM-PRIME decoders are used to accelerate video decoding on "
            "supported platforms. ");

    if (Decoder.endsWith("-dec"))
        msg += QObject::tr("The decoder will transfer frames back to system memory "
                           "which will significantly reduce performance but may allow "
                           "other functionality to be used (such as automatic "
                           "letterbox detection). ");
    return msg;
}

QString VideoDisplayProfile::GetVideoRendererName(const QString &Renderer)
{
    QMutexLocker locker(&s_safe_lock);
    if (s_rend_name.empty())
    {
        s_rend_name["opengl"]         = QObject::tr("OpenGL");
        s_rend_name["opengl-yv12"]    = QObject::tr("OpenGL YV12");
        s_rend_name["opengl-hw"]      = QObject::tr("OpenGL Hardware");
    }

    QString ret = Renderer;
    QMap<QString,QString>::const_iterator it = s_rend_name.find(Renderer);
    if (it != s_rend_name.end())
        ret = *it;
    return ret;
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

void VideoDisplayProfile::CreateProfile(uint GroupId, uint Priority,
    const QString& Width, const QString& Height, const QString& Codecs,
    const QString& Decoder, uint MaxCpus, bool SkipLoop, const QString& VideoRenderer,
    const QString& Deint1, const QString& Deint2)
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

#ifdef USING_OPENGL
    if (!profiles.contains("OpenGL High Quality"))
    {
        (void) QObject::tr("OpenGL High Quality",
                           "Sample: OpenGL high quality");
        groupid = CreateProfileGroup("OpenGL High Quality", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:high", "shader:high");
    }

    if (!profiles.contains("OpenGL Normal"))
    {
        (void) QObject::tr("OpenGL Normal", "Sample: OpenGL medium quality");
        groupid = CreateProfileGroup("OpenGL Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:medium", "shader:medium");
    }

    if (!profiles.contains("OpenGL Slim"))
    {
        (void) QObject::tr("OpenGL Slim", "Sample: OpenGL low power GPU");
        groupid = CreateProfileGroup("OpenGL Slim", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 1, true, "opengl",
                      "medium", "medium");
    }
#endif

#ifdef USING_VAAPI
    if (!profiles.contains("VAAPI Normal"))
    {
        (void) QObject::tr("VAAPI Normal", "Sample: VAAPI average quality");
        groupid = CreateProfileGroup("VAAPI Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "vaapi", 2, true, "opengl-hw",
                      "shader:driver:high", "shader:driver:high");
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 1, true, "opengl-yv12",
                      "shader:high", "shader:high");
    }
#endif

#ifdef USING_VDPAU
    if (!profiles.contains("VDPAU Normal"))
    {
        (void) QObject::tr("VDPAU Normal", "Sample: VDPAU medium quality");
        groupid = CreateProfileGroup("VDPAU Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "vdpau", 1, true, "opengl-hw",
                      "driver:medium", "driver:medium");
    }
#endif

#ifdef USING_MEDIACODEC
    if (!profiles.contains("MediaCodec Normal"))
    {
        (void) QObject::tr("MediaCodec Normal",
                           "Sample: MediaCodec Normal");
        groupid = CreateProfileGroup("MediaCodec Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "mediacodec-dec", 4, true, "opengl-yv12",
                      "shader:driver:medium", "shader:driver:medium");
    }
#endif

#if defined(USING_NVDEC) && defined(USING_OPENGL)
    if (!profiles.contains("NVDEC Normal"))
    {
        (void) QObject::tr("NVDEC Normal", "Sample: NVDEC Normal");
        groupid = CreateProfileGroup("NVDEC Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "nvdec", 1, true, "opengl-hw",
                      "shader:driver:high", "shader:driver:high");
    }
#endif

#if defined(USING_VTB) && defined(USING_OPENGL)
    if (!profiles.contains("VideoToolBox Normal")) {
        (void) QObject::tr("VideoToolBox Normal", "Sample: VideoToolBox Normal");
        groupid = CreateProfileGroup("VideoToolBox Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "vtb", 1, true, "opengl-hw",
                      "shader:driver:medium", "shader:driver:medium");
    }
#endif

#if defined(USING_MMAL) && defined(USING_OPENGL)
    if (!profiles.contains("MMAL"))
    {
        (void) QObject::tr("MMAL", "Sample: MMAL");
        groupid = CreateProfileGroup("MMAL", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "mmal", 1, true, "opengl-hw",
                      "shader:driver:medium", "shader:driver:medium");
    }
#endif

#if defined(USING_V4L2)
    if (!profiles.contains("V4L2 Codecs"))
    {
        (void) QObject::tr("V4L2 Codecs", "Sample: V4L2");
        groupid = CreateProfileGroup("V4L2 Codecs", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "v4l2-dec", 1, true, "opengl-yv12",
                      "shader:driver:medium", "shader:driver:medium");
        CreateProfile(groupid, 2, "", "", "",
                      "v4l2", 1, true, "opengl-hw",
                      "shader:driver:medium", "shader:driver:medium");
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
            "controls and optionally deinterlacing. CPU load is low but a slightly more "
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
    return QString("rend(%4) osd(%5) deint(%6,%7) filt(%8)")
        .arg(renderer).arg(osd).arg(deint0).arg(deint1);
}

QList<QPair<QString,QString> > VideoDisplayProfile::GetDeinterlacers(void)
{
    InitStatics();
    return s_deinterlacer_options;
}

void VideoDisplayProfile::InitStatics(bool Reinit /*= false*/)
{
    if (Reinit)
    {
        s_safe_custom.clear();
        s_safe_renderer.clear();
        s_safe_renderer_group.clear();
        s_safe_renderer_priority.clear();
        s_safe_decoders.clear();
        s_safe_equiv_dec.clear();
        s_deinterlacer_options.clear();
    }
    else if (s_safe_initialized)
    {
        return;
    }
    s_safe_initialized = true;

    RenderOptions options {};
    options.renderers      = &s_safe_custom;
    options.safe_renderers = &s_safe_renderer;
    options.render_group   = &s_safe_renderer_group;
    options.priorities     = &s_safe_renderer_priority;
    options.decoders       = &s_safe_decoders;
    options.equiv_decoders = &s_safe_equiv_dec;

    // N.B. assumes NuppelDecoder and DummyDecoder always present
    AvFormatDecoder::GetDecoders(options);
    MythVideoOutput::GetRenderOptions(options);

    foreach(QString decoder, s_safe_decoders)
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("decoder<->render support: %1%2")
                .arg(decoder, -12).arg(GetVideoRenderers(decoder).join(" ")));

    s_deinterlacer_options.append(QPair<QString,QString>(DEINT_QUALITY_NONE,   QObject::tr("None")));
    s_deinterlacer_options.append(QPair<QString,QString>(DEINT_QUALITY_LOW,    QObject::tr("Low quality")));
    s_deinterlacer_options.append(QPair<QString,QString>(DEINT_QUALITY_MEDIUM, QObject::tr("Medium quality")));
    s_deinterlacer_options.append(QPair<QString,QString>(DEINT_QUALITY_HIGH,   QObject::tr("High quality")));
}

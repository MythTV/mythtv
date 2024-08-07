// Std
#include <algorithm>
#include <utility>

// Qt
#include <QRegularExpression>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"

#include "decoders/mythcodeccontext.h"
#include "mythvideoout.h"
#include "mythvideoprofile.h"

void MythVideoProfileItem::Clear()
{
    m_pref.clear();
}

void MythVideoProfileItem::SetProfileID(uint Id)
{
    m_profileid = Id;
}

void MythVideoProfileItem::Set(const QString &Value, const QString &Data)
{
    m_pref[Value] = Data;
}

uint MythVideoProfileItem::GetProfileID() const
{
    return m_profileid;
}

QMap<QString,QString> MythVideoProfileItem::GetAll() const
{
    return m_pref;
}

QString MythVideoProfileItem::Get(const QString &Value) const
{
    QMap<QString,QString>::const_iterator it = m_pref.find(Value);
    if (it != m_pref.end())
        return *it;
    return {};
}

uint MythVideoProfileItem::GetPriority() const
{
    QString tmp = Get(PREF_PRIORITY);
    return tmp.isEmpty() ? 0 : tmp.toUInt();
}

// options are NNN NNN-MMM 0-MMM NNN-99999 >NNN >=NNN <MMM <=MMM or blank
// If string is blank then assumes a match.
// If value is 0 or negative assume a match (i.e. value unknown assumes a match)
// float values must be no more than 3 decimals.
bool MythVideoProfileItem::CheckRange(const QString &Key, float Value, bool *Ok) const
{
    return CheckRange(Key, Value, 0, true, Ok);
}

bool MythVideoProfileItem::CheckRange(const QString &Key, int Value, bool *Ok) const
{
    return CheckRange(Key, 0.0, Value, false, Ok);
}

bool MythVideoProfileItem::CheckRange(const QString& Key,
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
        QStringList exprList = cmp.split("&");
        for (const QString& expr : std::as_const(exprList))
        {
            if (expr.isEmpty())
            {
                isOK = false;
                continue;
            }
            if (IValue > 0)
            {
                static const QRegularExpression regex("^([0-9.]*)([^0-9.]*)([0-9.]*)$");
                QRegularExpressionMatch rmatch = regex.match(expr);

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
                    {
                        value1 = capture1.toInt(&isOK);
                    }
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
                        {
                            value2 = capture3.toInt(&isOK);
                        }
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
                        {
                            value2 = value2 - 1;
                        }
                        else if (oper == "<=")
                        {
                            ;
                        }
                        else
                        {
                            isOK = false;
                        }
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

bool MythVideoProfileItem::IsMatch(QSize Size, float Framerate, const QString &CodecName,
                                   const QStringList &DisallowedDecoders) const
{
    bool match = true;

    // cond_width, cond_height, cond_codecs, cond_framerate.
    // These replace old settings pref_cmp0 and pref_cmp1
    match &= CheckRange(COND_WIDTH,  Size.width());
    match &= CheckRange(COND_HEIGHT, Size.height());
    match &= CheckRange(COND_RATE,   Framerate);
    // codec
    QString cmp = Get(QString(COND_CODECS));
    if (!cmp.isEmpty())
    {
        QStringList clist = cmp.split(" ", Qt::SkipEmptyParts);
        if (!clist.empty())
            match &= clist.contains(CodecName,Qt::CaseInsensitive);
    }

    QString decoder = Get(PREF_DEC);
    if (DisallowedDecoders.contains(decoder))
        match = false;
    return match;
}

auto MythVideoProfileItem::IsValid() const
{
    using result = std::tuple<bool,QString>;
    bool ok = true;
    if (CheckRange(COND_WIDTH, 1, &ok); !ok)
        return result { false, "Invalid width condition" };
    if (CheckRange(COND_HEIGHT, 1, &ok); !ok)
        return result { false, "Invalid height condition" };
    if (CheckRange(COND_RATE, 1.0F, &ok); !ok)
        return result { false, "Invalid framerate condition" };

    QString decoder  = Get(PREF_DEC);
    QString renderer = Get(PREF_RENDER);
    if (decoder.isEmpty() || renderer.isEmpty())
        return result { false, "Need a decoder and renderer" };
    if (auto decoders = MythVideoProfile::GetDecoders(); !decoders.contains(decoder))
        return result { false, QString("decoder %1 is not available").arg(decoder) };
    if (auto renderers = MythVideoProfile::GetVideoRenderers(decoder); !renderers.contains(renderer))
        return result { false, QString("renderer %1 is not supported with decoder %2") .arg(renderer, decoder) };

    return result { true, {}};
}

bool MythVideoProfileItem::operator< (const MythVideoProfileItem &Other) const
{
    return GetPriority() < Other.GetPriority();
}

QString MythVideoProfileItem::toString() const
{
    QString cmp0      = Get("pref_cmp0");
    QString cmp1      = Get("pref_cmp1");
    QString width     = Get(COND_WIDTH);
    QString height    = Get(COND_HEIGHT);
    QString framerate = Get(COND_RATE);
    QString codecs    = Get(COND_CODECS);
    QString decoder   = Get(PREF_DEC);
    uint    max_cpus  = Get(PREF_CPUS).toUInt();
    bool    skiploop  = Get(PREF_LOOP).toInt() != 0;
    QString renderer  = Get(PREF_RENDER);
    QString deint0    = Get(PREF_DEINT1X);
    QString deint1    = Get(PREF_DEINT2X);
    QString upscale   = Get(PREF_UPSCALE);

    QString cond = QString("w(%1) h(%2) framerate(%3) codecs(%4)")
        .arg(width, height, framerate, codecs);
    QString str =  QString("cmp(%1%2) %7 dec(%3) cpus(%4) skiploop(%5) rend(%6) ")
        .arg(cmp0, QString(cmp1.isEmpty() ? "" : ",") + cmp1,
             decoder, QString::number(max_cpus), (skiploop) ? "enabled" : "disabled",
             renderer, cond);
    str += QString("deint(%1,%2) upscale(%3)").arg(deint0, deint1, upscale);
    return str;
}

#define LOC QString("VideoProfile: ")

MythVideoProfile::MythVideoProfile()
{
    QMutexLocker locker(&kSafeLock);
    InitStatics();

    QString hostname    = gCoreContext->GetHostName();
    QString cur_profile = GetDefaultProfileName(hostname);
    uint    groupid     = GetProfileGroupID(cur_profile, hostname);

    std::vector<MythVideoProfileItem> items = LoadDB(groupid);
    for (const auto & item : items)
    {
        if (auto [valid, error] = item.IsValid(); !valid)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Rejecting: " + item.toString() + "\n\t\t\t" + error);
            continue;
        }

        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Accepting: " + item.toString());
        m_allowedPreferences.push_back(item);
    }
}

void MythVideoProfile::SetInput(QSize Size, float Framerate, const QString &CodecName,
                                const QStringList &DisallowedDecoders)
{
    QMutexLocker locker(&m_lock);
    bool change = !DisallowedDecoders.isEmpty();

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
        LoadBestPreferences(m_lastSize, m_lastRate, m_lastCodecName, DisallowedDecoders);
}

void MythVideoProfile::SetOutput(float Framerate)
{
    QMutexLocker locker(&m_lock);
    if (!qFuzzyCompare(Framerate + 1.0F, m_lastRate + 1.0F))
    {
        m_lastRate = Framerate;
        LoadBestPreferences(m_lastSize, m_lastRate, m_lastCodecName);
    }
}

float MythVideoProfile::GetOutput() const
{
    return m_lastRate;
}

QString MythVideoProfile::GetDecoder() const
{
    return GetPreference(PREF_DEC);
}

QString MythVideoProfile::GetSingleRatePreferences() const
{
    return GetPreference(PREF_DEINT1X);
}

QString MythVideoProfile::GetDoubleRatePreferences() const
{
    return GetPreference(PREF_DEINT2X);
}

QString MythVideoProfile::GetUpscaler() const
{
    return GetPreference(PREF_UPSCALE);
}

uint MythVideoProfile::GetMaxCPUs() const
{
    return std::clamp(GetPreference(PREF_CPUS).toUInt(), 1U, VIDEO_MAX_CPUS);
}

bool MythVideoProfile::IsSkipLoopEnabled() const
{
    return GetPreference(PREF_LOOP).toInt() != 0;
}

QString MythVideoProfile::GetVideoRenderer() const
{
    return GetPreference(PREF_RENDER);
}

void MythVideoProfile::SetVideoRenderer(const QString &VideoRenderer)
{
    QMutexLocker locker(&m_lock);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SetVideoRenderer: '%1'").arg(VideoRenderer));
    if (VideoRenderer == GetVideoRenderer())
        return;

    // Make preferences safe...
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Old preferences: " + toString());
    SetPreference(PREF_RENDER, VideoRenderer);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "New preferences: " + toString());
}

bool MythVideoProfile::IsDecoderCompatible(const QString &Decoder) const
{
    const QString dec = GetDecoder();
    if (dec == Decoder)
        return true;

    QMutexLocker locker(&kSafeLock);
    return (kSafeEquivDec[dec].contains(Decoder));
}

QString MythVideoProfile::GetPreference(const QString &Key) const
{
    QMutexLocker locker(&m_lock);

    if (Key.isEmpty())
        return {};

    QMap<QString,QString>::const_iterator it = m_currentPreferences.find(Key);
    if (it == m_currentPreferences.end())
        return {};

    return *it;
}

void MythVideoProfile::SetPreference(const QString &Key, const QString &Value)
{
    QMutexLocker locker(&m_lock);
    if (!Key.isEmpty())
        m_currentPreferences[Key] = Value;
}

std::vector<MythVideoProfileItem>::const_iterator MythVideoProfile::FindMatch
    (const QSize Size, float Framerate, const QString &CodecName, const QStringList& DisallowedDecoders)
{
    for (auto it = m_allowedPreferences.cbegin(); it != m_allowedPreferences.cend(); ++it)
        if ((*it).IsMatch(Size, Framerate, CodecName, DisallowedDecoders))
            return it;
    return m_allowedPreferences.end();
}

void MythVideoProfile::LoadBestPreferences(const QSize Size, float Framerate,
                                           const QString &CodecName,
                                           const QStringList &DisallowedDecoders)
{
    auto olddeint1x = GetPreference(PREF_DEINT1X);
    auto olddeint2x = GetPreference(PREF_DEINT2X);
    auto oldupscale = GetPreference(PREF_UPSCALE);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("LoadBestPreferences(%1x%2, %3, %4)")
        .arg(Size.width()).arg(Size.height())
        .arg(static_cast<double>(Framerate), 0, 'f', 3).arg(CodecName));

    m_currentPreferences.clear();
    auto it = FindMatch(Size, Framerate, CodecName, DisallowedDecoders);
    if (it != m_allowedPreferences.end())
    {
        m_currentPreferences = (*it).GetAll();
    }
    else
    {
        int threads = std::clamp(QThread::idealThreadCount(), 1, 4);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "No useable profile. Using defaults.");
        SetPreference(PREF_DEC,    "ffmpeg");
        SetPreference(PREF_CPUS,   QString::number(threads));
        SetPreference(PREF_RENDER, "opengl-yv12");
        SetPreference(PREF_DEINT1X, DEINT_QUALITY_LOW);
        SetPreference(PREF_DEINT2X, DEINT_QUALITY_LOW);
        SetPreference(PREF_UPSCALE, UPSCALE_DEFAULT);
    }

    if (auto upscale = GetPreference(PREF_UPSCALE); upscale.isEmpty())
        SetPreference(PREF_UPSCALE, UPSCALE_DEFAULT);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("LoadBestPreferences result: "
            "priority:%1 width:%2 height:%3 fps:%4 codecs:%5")
            .arg(GetPreference(PREF_PRIORITY), GetPreference(COND_WIDTH),
                 GetPreference(COND_HEIGHT),   GetPreference(COND_RATE),
                 GetPreference(COND_CODECS)));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("decoder:%1 renderer:%2 deint0:%3 deint1:%4 cpus:%5 upscale:%6")
            .arg(GetPreference(PREF_DEC),     GetPreference(PREF_RENDER),
                 GetPreference(PREF_DEINT1X), GetPreference(PREF_DEINT2X),
                 GetPreference(PREF_CPUS),    GetPreference(PREF_UPSCALE)));

    // Signal any changes
    if (auto upscale = GetPreference(PREF_UPSCALE); oldupscale != upscale)
        emit UpscalerChanged(upscale);

    auto deint1x = GetPreference(PREF_DEINT1X);
    auto deint2x = GetPreference(PREF_DEINT2X);
    if ((deint1x != olddeint1x) || (deint2x != olddeint2x))
        emit DeinterlacersChanged(deint1x, deint2x);
}

std::vector<MythVideoProfileItem> MythVideoProfile::LoadDB(uint GroupId)
{
    MythVideoProfileItem tmp;
    std::vector<MythVideoProfileItem> list;

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
                auto [valid, error] = tmp.IsValid();
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
        auto [valid, error] =  tmp.IsValid();
        if (valid)
            list.push_back(tmp);
        else
            LOG(VB_PLAYBACK, LOG_NOTICE, LOC + QString("Ignoring profile %1 (%2)")
                .arg(profileid).arg(error));
    }

    sort(list.begin(), list.end());
    return list;
}

bool MythVideoProfile::DeleteDB(uint GroupId, const std::vector<MythVideoProfileItem> &Items)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "DELETE FROM displayprofiles "
        "WHERE profilegroupid = :GROUPID   AND "
        "      profileid      = :PROFILEID");

    bool ok = true;
    for (const auto & item : Items)
    {
        if (!item.GetProfileID())
            continue;

        query.bindValue(":GROUPID",   GroupId);
        query.bindValue(":PROFILEID", item.GetProfileID());
        if (!query.exec())
        {
            MythDB::DBError("vdp::deletedb", query);
            ok = false;
        }
    }

    return ok;
}

bool MythVideoProfile::SaveDB(uint GroupId, std::vector<MythVideoProfileItem> &Items)
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
    for (auto & item : Items)
    {
        QMap<QString,QString> list = item.GetAll();
        if (list.begin() == list.end())
            continue;

        QMap<QString,QString>::const_iterator lit = list.cbegin();

        if (!item.GetProfileID())
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
                item.SetProfileID(query.value(0).toUInt() + 1);
            }

            for (; lit != list.cend(); ++lit)
            {
                if ((*lit).isEmpty())
                    continue;

                insert.bindValue(":GROUPID",   GroupId);
                insert.bindValue(":PROFILEID", item.GetProfileID());
                insert.bindValue(":VALUE",     lit.key());
                insert.bindValueNoNull(":DATA", *lit);
                if (!insert.exec())
                {
                    MythDB::DBError("save_profile 2", insert);
                    ok = false;
                    continue;
                }
            }
            continue;
        }

        for (; lit != list.cend(); ++lit)
        {
            query.prepare(
                "SELECT count(*) "
                "FROM displayprofiles "
                "WHERE  profilegroupid = :GROUPID AND "
                "       profileid      = :PROFILEID AND "
                "       value          = :VALUE");
            query.bindValue(":GROUPID",   GroupId);
            query.bindValue(":PROFILEID", item.GetProfileID());
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
                    sqldelete.bindValue(":PROFILEID", item.GetProfileID());
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
                    update.bindValue(":PROFILEID", item.GetProfileID());
                    update.bindValue(":VALUE",     lit.key());
                    update.bindValueNoNull(":DATA", *lit);
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
                insert.bindValue(":PROFILEID", item.GetProfileID());
                insert.bindValue(":VALUE",     lit.key());
                insert.bindValueNoNull(":DATA", *lit);
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

QStringList MythVideoProfile::GetDecoders()
{
    QMutexLocker locker(&kSafeLock);
    InitStatics();
    return kSafeDecoders;
}

QStringList MythVideoProfile::GetDecoderNames()
{
    QMutexLocker locker(&kSafeLock);
    InitStatics();
    return std::accumulate(kSafeDecoders.cbegin(), kSafeDecoders.cend(), QStringList{},
        [](QStringList Res, const QString& Dec) { return Res << GetDecoderName(Dec); });
}

std::vector<std::pair<QString, QString> > MythVideoProfile::GetUpscalers()
{
    static std::vector<std::pair<QString,QString>> s_upscalers =
    {
        { tr("Default (Bilinear)"), UPSCALE_DEFAULT },
        { tr("Bicubic"), UPSCALE_HQ1 }
    };
    return s_upscalers;
}

QString MythVideoProfile::GetDecoderName(const QString &Decoder)
{
    if (Decoder.isEmpty())
        return "";

    QMutexLocker locker(&kSafeLock);
    if (kDecName.empty())
    {
        kDecName["ffmpeg"]         = tr("Standard");
        kDecName["vdpau"]          = tr("VDPAU acceleration");
        kDecName["vdpau-dec"]      = tr("VDPAU acceleration (decode only)");
        kDecName["vaapi"]          = tr("VAAPI acceleration");
        kDecName["vaapi-dec"]      = tr("VAAPI acceleration (decode only)");
        kDecName["dxva2"]          = tr("Windows hardware acceleration");
        kDecName["mediacodec"]     = tr("Android MediaCodec acceleration");
        kDecName["mediacodec-dec"] = tr("Android MediaCodec acceleration (decode only)");
        kDecName["nvdec"]          = tr("NVIDIA NVDEC acceleration");
        kDecName["nvdec-dec"]      = tr("NVIDIA NVDEC acceleration (decode only)");
        kDecName["vtb"]            = tr("VideoToolbox acceleration");
        kDecName["vtb-dec"]        = tr("VideoToolbox acceleration (decode only)");
        kDecName["v4l2"]           = tr("V4L2 acceleration");
        kDecName["v4l2-dec"]       = tr("V4L2 acceleration (decode only)");
        kDecName["mmal"]           = tr("MMAL acceleration");
        kDecName["mmal-dec"]       = tr("MMAL acceleration (decode only)");
        kDecName["drmprime"]       = tr("DRM PRIME acceleration");
    }

    QString ret = Decoder;
    QMap<QString,QString>::const_iterator it = kDecName.constFind(Decoder);
    if (it != kDecName.constEnd())
        ret = *it;
    return ret;
}


QString MythVideoProfile::GetDecoderHelp(const QString& Decoder)
{
    QString msg = tr("Processing method used to decode video.");

    if (Decoder.isEmpty())
        return msg;

    msg += "\n";

    if (Decoder == "ffmpeg")
        msg += tr("Standard will use the FFmpeg library for software decoding.");

    if (Decoder.startsWith("vdpau"))
    {
        msg += tr(
            "VDPAU will attempt to use the graphics hardware to "
            "accelerate video decoding.");
    }

    if (Decoder.startsWith("vaapi"))
    {
        msg += tr(
            "VAAPI will attempt to use the graphics hardware to "
            "accelerate video decoding and playback.");
    }

    if (Decoder.startsWith("dxva2"))
    {
        msg += tr(
            "DXVA2 will use the graphics hardware to "
            "accelerate video decoding and playback. ");
    }

    if (Decoder.startsWith("mediacodec"))
    {
        msg += tr(
            "Mediacodec will use Android graphics hardware to "
            "accelerate video decoding and playback. ");
    }

    if (Decoder.startsWith("nvdec"))
    {
        msg += tr(
            "Nvdec uses the NVDEC API to "
            "accelerate video decoding and playback with NVIDIA Graphics Adapters. ");
    }

    if (Decoder.startsWith("vtb"))
        msg += tr(
            "The VideoToolbox library is used to accelerate video decoding. ");

    if (Decoder.startsWith("mmal"))
        msg += tr(
            "MMAL is used to accelerated video decoding (Raspberry Pi only). ");

    if (Decoder == "v4l2")
        msg += "Highly experimental: ";

    if (Decoder.startsWith("v4l2"))
    {
        msg += tr(
            "Video4Linux codecs are used to accelerate video decoding on "
            "supported platforms. ");
    }

    if (Decoder == "drmprime")
    {
        msg += tr(
            "DRM-PRIME decoders are used to accelerate video decoding on "
            "supported platforms. ");
    }

    if (Decoder.endsWith("-dec"))
    {
        msg += tr("The decoder will transfer frames back to system memory "
                           "which will significantly reduce performance but may allow "
                           "other functionality to be used (such as automatic "
                           "letterbox detection). ");
    }
    return msg;
}

QString MythVideoProfile::GetVideoRendererName(const QString &Renderer)
{
    QMutexLocker locker(&kSafeLock);
    if (kRendName.empty())
    {
        kRendName["opengl"]      = tr("OpenGL");
        kRendName["opengl-yv12"] = tr("OpenGL YV12");
        kRendName["opengl-hw"]   = tr("OpenGL Hardware");
        kRendName["vulkan"]      = tr("Vulkan");
    }

    QString ret = Renderer;
    QMap<QString,QString>::const_iterator it = kRendName.constFind(Renderer);
    if (it != kRendName.constEnd())
        ret = *it;
    return ret;
}

QStringList MythVideoProfile::GetProfiles(const QString &HostName)
{
    InitStatics();
    QStringList list;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM displayprofilegroups WHERE hostname = :HOST ");
    query.bindValue(":HOST", HostName);
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("get_profiles", query);
    }
    else
    {
        while (query.next())
            list += query.value(0).toString();
    }
    return list;
}

QString MythVideoProfile::GetDefaultProfileName(const QString &HostName)
{
    auto tmp = gCoreContext->GetSettingOnHost("DefaultVideoPlaybackProfile", HostName);
    QStringList profiles = GetProfiles(HostName);
    tmp = (profiles.contains(tmp)) ? tmp : QString();

    if (tmp.isEmpty())
    {
        if (!profiles.empty())
            tmp = profiles[0];

        tmp = (profiles.contains("Normal")) ? "Normal" : tmp;
        if (!tmp.isEmpty())
            gCoreContext->SaveSettingOnHost("DefaultVideoPlaybackProfile", tmp, HostName);
    }

    return tmp;
}

void MythVideoProfile::SetDefaultProfileName(const QString &ProfileName, const QString &HostName)
{
    gCoreContext->SaveSettingOnHost("DefaultVideoPlaybackProfile", ProfileName, HostName);
}

uint MythVideoProfile::GetProfileGroupID(const QString &ProfileName,
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

void MythVideoProfile::CreateProfile(uint GroupId, uint Priority,
    const QString& Width, const QString& Height, const QString& Codecs,
    const QString& Decoder, uint MaxCpus, bool SkipLoop, const QString& VideoRenderer,
    const QString& Deint1, const QString& Deint2, const QString &Upscale)
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

    queryValue += COND_WIDTH;
    queryData  += Width;

    queryValue += COND_HEIGHT;
    queryData  += Height;

    queryValue += COND_CODECS;
    queryData  += Codecs;

    queryValue += PREF_DEC;
    queryData  += Decoder;

    queryValue += PREF_CPUS;
    queryData  += QString::number(MaxCpus);

    queryValue += PREF_LOOP;
    queryData  += (SkipLoop) ? "1" : "0";

    queryValue += PREF_RENDER;
    queryData  += VideoRenderer;

    queryValue += PREF_DEINT1X;
    queryData  += Deint1;

    queryValue += PREF_DEINT2X;
    queryData  += Deint2;

    queryValue += PREF_UPSCALE;
    queryData  += Upscale;

    QStringList::const_iterator itV = queryValue.cbegin();
    QStringList::const_iterator itD = queryData.cbegin();
    for (; itV != queryValue.cend() && itD != queryData.cend(); ++itV,++itD)
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

uint MythVideoProfile::CreateProfileGroup(const QString &ProfileName, const QString &HostName)
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

bool MythVideoProfile::DeleteProfileGroup(const QString &GroupName, const QString &HostName)
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

void MythVideoProfile::CreateProfiles(const QString &HostName)
{
    QStringList profiles = GetProfiles(HostName);

#ifdef USING_OPENGL
    if (!profiles.contains("OpenGL High Quality"))
    {
        (void)tr("OpenGL High Quality",
                           "Sample: OpenGL high quality");
        uint groupid = CreateProfileGroup("OpenGL High Quality", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:high", "shader:high");
    }

    if (!profiles.contains("OpenGL Normal"))
    {
        (void)tr("OpenGL Normal", "Sample: OpenGL medium quality");
        uint groupid = CreateProfileGroup("OpenGL Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:medium", "shader:medium");
    }

    if (!profiles.contains("OpenGL Slim"))
    {
        (void)tr("OpenGL Slim", "Sample: OpenGL low power GPU");
        uint groupid = CreateProfileGroup("OpenGL Slim", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 1, true, "opengl",
                      "medium", "medium");
    }
#endif

#ifdef USING_VAAPI
    if (!profiles.contains("VAAPI Normal"))
    {
        (void)tr("VAAPI Normal", "Sample: VAAPI average quality");
        uint groupid = CreateProfileGroup("VAAPI Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "vaapi", 2, true, "opengl-hw",
                      "shader:driver:high", "shader:driver:high");
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:medium", "shader:medium");
    }
#endif

#ifdef USING_VDPAU
    if (!profiles.contains("VDPAU Normal"))
    {
        (void)tr("VDPAU Normal", "Sample: VDPAU medium quality");
        uint groupid = CreateProfileGroup("VDPAU Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "vdpau", 1, true, "opengl-hw",
                      "driver:medium", "driver:medium");
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:medium", "shader:medium");
    }
#endif

#ifdef USING_MEDIACODEC
    if (!profiles.contains("MediaCodec Normal"))
    {
        (void)tr("MediaCodec Normal",
                           "Sample: MediaCodec Normal");
        uint groupid = CreateProfileGroup("MediaCodec Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "mediacodec-dec", 4, true, "opengl-yv12",
                      "shader:driver:medium", "shader:driver:medium");
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:medium", "shader:medium");

    }
#endif

#if defined(USING_NVDEC) && defined(USING_OPENGL)
    if (!profiles.contains("NVDEC Normal"))
    {
        (void)tr("NVDEC Normal", "Sample: NVDEC Normal");
        uint groupid = CreateProfileGroup("NVDEC Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "nvdec", 1, true, "opengl-hw",
                      "shader:driver:high", "shader:driver:high");
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:high", "shader:high");
    }
#endif

#if defined(USING_VTB) && defined(USING_OPENGL)
    if (!profiles.contains("VideoToolBox Normal")) {
        (void)tr("VideoToolBox Normal", "Sample: VideoToolBox Normal");
        uint groupid = CreateProfileGroup("VideoToolBox Normal", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "vtb", 1, true, "opengl-hw",
                      "shader:driver:medium", "shader:driver:medium");
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:medium", "shader:medium");
    }
#endif

#if defined(USING_MMAL) && defined(USING_OPENGL)
    if (!profiles.contains("MMAL"))
    {
        (void)tr("MMAL", "Sample: MMAL");
        uint groupid = CreateProfileGroup("MMAL", HostName);
        CreateProfile(groupid, 1, "", "", "",
                      "mmal", 1, true, "opengl-hw",
                      "shader:driver:medium", "shader:driver:medium");
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:medium", "shader:medium");
    }
#endif

#if defined(USING_V4L2)
    if (!profiles.contains("V4L2 Codecs"))
    {
        (void)tr("V4L2 Codecs", "Sample: V4L2");
        uint groupid = CreateProfileGroup("V4L2 Codecs", HostName);
        CreateProfile(groupid, 2, "", "", "",
                      "v4l2", 1, true, "opengl-hw",
                      "shader:driver:medium", "shader:driver:medium");
        CreateProfile(groupid, 1, "", "", "",
                      "ffmpeg", 2, true, "opengl-yv12",
                      "shader:medium", "shader:medium");
    }
#endif
}

QStringList MythVideoProfile::GetVideoRenderers(const QString &Decoder)
{
    QMutexLocker locker(&kSafeLock);
    InitStatics();

    QMap<QString,QStringList>::const_iterator it = kSafeRenderer.constFind(Decoder);
    if (it != kSafeRenderer.constEnd())
        return *it;
    return {};
}

QString MythVideoProfile::GetVideoRendererHelp(const QString &Renderer)
{
    if (Renderer == "null")
        return tr("Render video offscreen. Used internally.");

    if (Renderer == "direct3d")
    {
        return tr("Windows video renderer based on Direct3D. Requires "
                  "video card compatible with Direct3D 9. This is the preferred "
                  "renderer for current Windows systems.");
    }

    if (Renderer == "opengl")
    {
        return tr("Video is converted to an intermediate format by the CPU (YUV2) "
                  "before OpenGL is used for color conversion, scaling, picture controls"
                  " and optionally deinterlacing. Processing is balanced between the CPU "
                  "and GPU.");
    }

    if (Renderer == "opengl-yv12")
    {
        return tr("OpenGL is used for all color conversion, scaling, picture "
                  "controls and optionally deinterlacing. CPU load is low but a slightly more "
                  "powerful GPU is needed for deinterlacing.");
    }

    if (Renderer == "opengl-hw")
        return tr("This video renderer is used by hardware decoders to display frames using OpenGL.");

    return tr("Video rendering method");
}

QString MythVideoProfile::GetPreferredVideoRenderer(const QString &Decoder)
{
    return GetBestVideoRenderer(GetVideoRenderers(Decoder));
}

QStringList MythVideoProfile::GetFilteredRenderers(const QString &Decoder, const QStringList &Renderers)
{
    const QStringList safe = GetVideoRenderers(Decoder);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Safe renderers for '%1': %2").arg(Decoder, safe.join(",")));

    QStringList filtered;
    for (const auto& dec : std::as_const(safe))
        if (Renderers.contains(dec))
            filtered.push_back(dec);

    return filtered;
}

QString MythVideoProfile::GetBestVideoRenderer(const QStringList &Renderers)
{
    QMutexLocker locker(&kSafeLock);
    InitStatics();

    uint    toppriority = 0;
    QString toprenderer;
    for (const auto& renderer : std::as_const(Renderers))
    {
        QMap<QString,uint>::const_iterator it = kSafeRendererPriority.constFind(renderer);
        if ((it != kSafeRendererPriority.constEnd()) && (*it >= toppriority))
        {
            toppriority = *it;
            toprenderer = renderer;
        }
    }
    return toprenderer;
}

QString MythVideoProfile::toString() const
{
    auto renderer = GetPreference(PREF_RENDER);
    auto deint0   = GetPreference(PREF_DEINT1X);
    auto deint1   = GetPreference(PREF_DEINT2X);
    auto cpus     = GetPreference(PREF_CPUS);
    auto upscale  = GetPreference(PREF_UPSCALE);
    return QString("rend:%1 deint:%2/%3 CPUs: %4 Upscale: %5")
        .arg(renderer, deint0, deint1, cpus, upscale);
}

const QList<QPair<QString, QString> >& MythVideoProfile::GetDeinterlacers()
{
    static const QList<QPair<QString,QString> > s_deinterlacerOptions =
    {
        { DEINT_QUALITY_NONE,   tr("None") },
        { DEINT_QUALITY_LOW,    tr("Low quality") },
        { DEINT_QUALITY_MEDIUM, tr("Medium quality") },
        { DEINT_QUALITY_HIGH,   tr("High quality") }
    };

    return s_deinterlacerOptions;
}

void MythVideoProfile::InitStatics(bool Reinit /*= false*/)
{
    QMutexLocker locker(&kSafeLock);

    if (!gCoreContext->IsUIThread())
    {
        if (!kSafeInitialized)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot initialise video profiles from this thread");
        return;
    }

    if (!HasMythMainWindow())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No window!");
        return;
    }

    if (Reinit)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Resetting decoder/render support");
        kSafeCustom.clear();
        kSafeRenderer.clear();
        kSafeRendererGroup.clear();
        kSafeRendererPriority.clear();
        kSafeDecoders.clear();
        kSafeEquivDec.clear();
    }
    else if (kSafeInitialized)
    {
        return;
    }
    kSafeInitialized = true;

    RenderOptions options {};
    options.renderers      = &kSafeCustom;
    options.safe_renderers = &kSafeRenderer;
    options.render_group   = &kSafeRendererGroup;
    options.priorities     = &kSafeRendererPriority;
    options.decoders       = &kSafeDecoders;
    options.equiv_decoders = &kSafeEquivDec;

    auto * render = GetMythMainWindow()->GetRenderDevice();

    // N.B. assumes DummyDecoder always present
    MythCodecContext::GetDecoders(options, Reinit);
    MythVideoOutput::GetRenderOptions(options, render);

    auto interops = MythInteropGPU::GetTypes(render);
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Available GPU interops: %1")
        .arg(MythInteropGPU::TypesToString(interops)));

    for (const QString& decoder : std::as_const(kSafeDecoders))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Decoder/render support: %1%2")
            .arg(decoder, -12).arg(GetVideoRenderers(decoder).join(" ")));
    }
}

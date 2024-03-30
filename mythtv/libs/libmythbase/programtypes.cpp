// -*- Mode: c++ -*-

// Posix headers
#include <deque>

// Qt headers
#include <QDateTime>
#include <QMutex>
#include <QObject>

// MythTV headers
#include "libmythbase/mythdate.h"

#include "programtypes.h"

const QString kPlayerInUseID           { QStringLiteral("player") };
const QString kPIPPlayerInUseID        { QStringLiteral("pipplayer") };
const QString kPBPPlayerInUseID        { QStringLiteral("pbpplayer") };
const QString kImportRecorderInUseID   { QStringLiteral("import_recorder") };
const QString kRecorderInUseID         { QStringLiteral("recorder") };
const QString kFileTransferInUseID     { QStringLiteral("filetransfer") };
const QString kTruncatingDeleteInUseID { QStringLiteral("truncatingdelete") };
const QString kFlaggerInUseID          { QStringLiteral("flagger") };
const QString kTranscoderInUseID       { QStringLiteral("transcoder") };
const QString kPreviewGeneratorInUseID { QStringLiteral("preview_generator") };
const QString kJobQueueInUseID         { QStringLiteral("jobqueue") };
const QString kCCExtractorInUseID      { QStringLiteral("ccextractor") };

QString toString(MarkTypes type)
{
    switch (type)
    {
        case MARK_INVALID:      return "INVALID";
        case MARK_ALL:          return "ALL";
        case MARK_UNSET:        return "UNSET";
        case MARK_TMP_CUT_END:  return "TMP_CUT_END";
        case MARK_TMP_CUT_START:return "TMP_CUT_START";
        case MARK_UPDATED_CUT:  return "UPDATED_CUT";
        case MARK_PLACEHOLDER:  return "PLACEHOLDER";
        case MARK_CUT_END:      return "CUT_END";
        case MARK_CUT_START:    return "CUT_START";
        case MARK_BOOKMARK:     return "BOOKMARK";
        case MARK_BLANK_FRAME:  return "BLANK_FRAME";
        case MARK_COMM_START:   return "COMM_START";
        case MARK_COMM_END:     return "COMM_END";
        case MARK_GOP_START:    return "GOP_START";
        case MARK_KEYFRAME:     return "KEYFRAME";
        case MARK_SCENE_CHANGE: return "SCENE_CHANGE";
        case MARK_GOP_BYFRAME:  return "GOP_BYFRAME";
        case MARK_ASPECT_1_1:   return "ASPECT_1_1 (depreciated)";
        case MARK_ASPECT_4_3:   return "ASPECT_4_3";
        case MARK_ASPECT_16_9:  return "ASPECT_16_9";
        case MARK_ASPECT_2_21_1:return "ASPECT_2_21_1";
        case MARK_ASPECT_CUSTOM:return "ASPECT_CUSTOM";
        case MARK_SCAN_PROGRESSIVE: return "PROGRESSIVE";
        case MARK_VIDEO_WIDTH:  return "VIDEO_WIDTH";
        case MARK_VIDEO_HEIGHT: return "VIDEO_HEIGHT";
        case MARK_VIDEO_RATE:   return "VIDEO_RATE";
        case MARK_DURATION_MS:  return "DURATION_MS";
        case MARK_TOTAL_FRAMES: return "TOTAL_FRAMES";
        case MARK_UTIL_PROGSTART: return "UTIL_PROGSTART";
        case MARK_UTIL_LASTPLAYPOS: return "UTIL_LASTPLAYPOS";
    }

    return "unknown";
}

MarkTypes markTypeFromString(const QString & str)
{
    auto Im = MarkTypeStrings.find(str);
    if (Im == MarkTypeStrings.end())
        return MARK_INVALID;
    return (*Im).second;
}

QString toString(AvailableStatusType status)
{
    switch (status)
    {
        case asAvailable:       return "Available";
        case asNotYetAvailable: return "NotYetAvailable";
        case asPendingDelete:   return "PendingDelete";
        case asFileNotFound:    return "FileNotFound";
        case asZeroByte:        return "ZeroByte";
        case asDeleted:         return "Deleted";
    }
    return QString("Unknown(%1)").arg((int)status);
}


QString SkipTypeToString(int flags)
{
    if (COMM_DETECT_COMMFREE == flags)
        return QObject::tr("Commercial Free");
    if (COMM_DETECT_UNINIT == flags)
        return QObject::tr("Use Global Setting");

    QChar chr = '0';
    QString ret = QString("0x%1").arg(flags,3,16,chr);
    bool blank = (COMM_DETECT_BLANK & flags) != 0;
    bool scene = (COMM_DETECT_SCENE & flags) != 0;
    bool logo  = (COMM_DETECT_LOGO  & flags) != 0;
    bool exp   = (COMM_DETECT_2     & flags) != 0;
    bool prePst= (COMM_DETECT_PREPOSTROLL & flags) != 0;

    if (blank && scene && logo)
        ret = QObject::tr("All Available Methods");
    else if (blank && scene && !logo)
        ret = QObject::tr("Blank Frame + Scene Change");
    else if (blank && !scene && logo)
        ret = QObject::tr("Blank Frame + Logo Detection");
    else if (!blank && scene && logo)
        ret = QObject::tr("Scene Change + Logo Detection");
    else if (blank && !scene && !logo)
        ret = QObject::tr("Blank Frame Detection");
    else if (!blank && scene && !logo)
        ret = QObject::tr("Scene Change Detection");
    else if (!blank && !scene && logo)
        ret = QObject::tr("Logo Detection");

    if (exp)
        ret = QObject::tr("Experimental") + ": " + ret;
    else if(prePst)
        ret = QObject::tr("Pre & Post Roll") + ": " + ret;

    return ret;
}

std::deque<int> GetPreferredSkipTypeCombinations(void)
{
    std::deque<int> tmp;
    tmp.push_back(COMM_DETECT_BLANK | COMM_DETECT_SCENE | COMM_DETECT_LOGO);
    tmp.push_back(COMM_DETECT_BLANK);
    tmp.push_back(COMM_DETECT_BLANK | COMM_DETECT_SCENE);
    tmp.push_back(COMM_DETECT_SCENE);
    tmp.push_back(COMM_DETECT_LOGO);
    tmp.push_back(COMM_DETECT_2 | COMM_DETECT_BLANK | COMM_DETECT_LOGO);
    tmp.push_back(COMM_DETECT_PREPOSTROLL | COMM_DETECT_BLANK |
                  COMM_DETECT_SCENE);
    return tmp;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

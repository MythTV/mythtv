#include "playbackstate.h"

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programtypes.h"
#include "libmythmetadata/videometadata.h"

PlaybackState::PlaybackState()
{
    m_alwaysShowWatchedProgress = gCoreContext->GetBoolSetting("AlwaysShowWatchedProgress", false);
}

void PlaybackState::Initialize()
{
    LOG(VB_GENERAL, LOG_INFO, "Query playback state for all videos from database");

    m_fileMarkup.clear();

    QueryData();

    LOG(VB_GENERAL, LOG_INFO, QString("Collected playbackstate for %1 videos").arg(m_fileMarkup.count()));
}

void PlaybackState::Update(const QString &filename)
{
    if (!filename.isEmpty())
    {
        QueryData(filename);
    }
}

void PlaybackState::QueryData(const QString &filterFilename)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (filterFilename.isEmpty())
    {
        query.prepare("SELECT filename, type, mark, `offset` "
                    "FROM filemarkup "
                    "WHERE type IN (:BOOKMARK, :FRAMES, :PLAYPOS) "
                    "ORDER BY filename");
    }
    else
    {
        query.prepare("SELECT filename, type, mark, `offset` "
                    "FROM filemarkup "
                    "WHERE type IN (:BOOKMARK, :FRAMES, :PLAYPOS) "
                    "AND filename = :FILENAME "
                    "ORDER BY filename");
        query.bindValue(":FILENAME", filterFilename);
    }
    query.bindValue(":BOOKMARK", MARK_BOOKMARK);
    query.bindValue(":FRAMES", MARK_TOTAL_FRAMES);
    query.bindValue(":PLAYPOS", MARK_UTIL_LASTPLAYPOS);

    if (!query.exec())
    {
        MythDB::DBError("PlaybackState", query);
        return;
    }

    QString lastFilename;
    Markup currMarks;
    while (query.next())
    {
        const QString filename = query.value(0).toString();
        if (!lastFilename.isEmpty() && filename != lastFilename)
        {
            m_fileMarkup[lastFilename] = currMarks;
            currMarks = Markup();
        }
        switch(query.value(1).toInt())
        {
            case MARK_BOOKMARK:
                currMarks.bookmarkPos = query.value(2).toLongLong();
                break;
            case MARK_TOTAL_FRAMES:
                currMarks.totalFrames = query.value(3).toLongLong();
                break;
            case MARK_UTIL_LASTPLAYPOS:
                currMarks.lastPlayPos = query.value(2).toLongLong();
                break;
        }
        lastFilename = filename;
    }
    m_fileMarkup[lastFilename] = currMarks;
}

bool PlaybackState::HasBookmark(const QString &filename) const
{
    auto it = m_fileMarkup.constFind(filename);
    if (it == m_fileMarkup.constEnd())
    {
        return false;
    }
    return it.value().bookmarkPos > 0;
}

uint64_t PlaybackState::GetLastPlayPos(const QString &filename) const
{
    auto it = m_fileMarkup.constFind(filename);
    if (it == m_fileMarkup.constEnd())
    {
        return 0;
    }
    return it.value().lastPlayPos;
}

uint PlaybackState::GetWatchedPercent(const QString &filename) const
{
    auto it = m_fileMarkup.constFind(filename);
    if (it == m_fileMarkup.constEnd())
    {
        return 0;
    }
    const auto pos = it.value().lastPlayPos;
    const auto total = it.value().totalFrames;
    if (0 == total)
    {
        return 0;
    }
    return std::clamp(100 * pos / total, (uint64_t)0, (uint64_t)100);
}

bool PlaybackState::AlwaysShowWatchedProgress() const
{
    return m_alwaysShowWatchedProgress;
}

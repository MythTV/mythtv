#include "BDRingBuffer.h"
#include "mythbdplayer.h"

#define LOC     QString("BDPlayer: ")
#define LOC_ERR QString("BDPlayer error: ")

MythBDPlayer::MythBDPlayer(bool muted) : NuppelVideoPlayer(muted)
{
}

int MythBDPlayer::GetNumChapters(void)
{
    if (player_ctx->buffer->BD() && player_ctx->buffer->BD()->IsOpen())
        return player_ctx->buffer->BD()->GetNumChapters();
    return 0;
}

int MythBDPlayer::GetCurrentChapter(void)
{
    uint total = GetNumChapters();
    if (!total)
        return 0;

    for (int i = (total - 1); i > -1 ; i--)
    {
        uint64_t frame = player_ctx->buffer->BD()->GetChapterStartFrame(i);
        if (framesPlayed >= frame)
        {
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("GetCurrentChapter(selected chapter %1 framenum %2)")
                            .arg(i + 1).arg(frame));
            return i + 1;
        }
    }
    return 0;
}

int64_t MythBDPlayer::GetChapter(int chapter)
{
    uint total = GetNumChapters();
    if (!total)
        return -1;

    return (int64_t)player_ctx->buffer->BD()->GetChapterStartFrame(chapter-1);
}

void MythBDPlayer::GetChapterTimes(QList<long long> &times)
{
    uint total = GetNumChapters();
    if (!total)
        return;

    for (uint i = 0; i < total; i++)
        times.push_back(player_ctx->buffer->BD()->GetChapterStartTime(i));
}

#ifndef BREAKMAP_H
#define BREAKMAP_H

// MythTV headers
#include "tv.h"
#include "programtypes.h"
#include "playercontext.h"

// Qt headers
#include <QMutex>
#include <QMap>
#include <QCoreApplication>


#include <compat.h>
#include <stdint.h>

class NuppelVideoPlayer;

class CommBreakMap
{
    Q_DECLARE_TR_FUNCTIONS(CommBreakMap)

  public:
    CommBreakMap(void);

    bool HasMap(void) const { return hascommbreaktable; }

    CommSkipMode GetAutoCommercialSkip(void) const;
    void SetAutoCommercialSkip(CommSkipMode autoskip, uint64_t framesPlayed);

    int  GetSkipCommercials(void) const { return skipcommercials; }
    void SkipCommercials(int direction);

    void ResetLastSkip(void);
    void SetTracker(uint64_t framesPlayed);
    void GetMap(frm_dir_map_t &map) const;
    void SetMap(const frm_dir_map_t &newMap, uint64_t framesPlayed);
    void LoadMap(PlayerContext *player_ctx, uint64_t framesPlayed);

    bool IsInCommBreak(uint64_t frameNumber) const;
    bool AutoCommercialSkip(uint64_t &jumpToFrame, uint64_t framesPlayed,
                            double video_frame_rate, uint64_t totalFrames,
                            QString &comm_msg);
    bool DoSkipCommercials(uint64_t &jumpToFrame, uint64_t framesPlayed,
                           double video_frame_rate, uint64_t totalFrames,
                           QString &comm_msg);

  private:
    void MergeShortCommercials(double video_frame_rate);

    mutable QMutex commBreakMapLock;
    int            skipcommercials;
    CommSkipMode   autocommercialskip;
    int            commrewindamount;
    int            commnotifyamount;
    int            lastCommSkipDirection;
    time_t         lastCommSkipTime;
    uint64_t       lastCommSkipStart;
    time_t         lastSkipTime;
    bool           hascommbreaktable;
    QDateTime      lastIgnoredManualSkip;
    int            maxskip;
    int            maxShortMerge;
    frm_dir_map_t  commBreakMap;
    frm_dir_map_t::Iterator commBreakIter;
};

#endif // BREAKMAP_H

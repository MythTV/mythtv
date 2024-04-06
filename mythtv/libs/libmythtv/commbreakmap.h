#ifndef BREAKMAP_H
#define BREAKMAP_H

#include <cstdint>

// Qt headers
#include <QtGlobal>
#include <QRecursiveMutex>
#include <QMap>
#include <QCoreApplication>

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/programtypes.h"
#include "libmythtv/tv.h"
#include "libmythtv/playercontext.h"

class CommBreakMap
{
    Q_DECLARE_TR_FUNCTIONS(CommBreakMap)

  public:
    CommBreakMap(void);

    bool HasMap(void) const { return m_hascommbreaktable; }

    CommSkipMode GetAutoCommercialSkip(void) const;
    void SetAutoCommercialSkip(CommSkipMode autoskip, uint64_t framesPlayed);

    int  GetSkipCommercials(void) const { return m_skipcommercials; }
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

    mutable QRecursiveMutex m_commBreakMapLock;
    int                     m_skipcommercials       {0};
    CommSkipMode            m_autocommercialskip    {kCommSkipOff};
    std::chrono::seconds    m_commrewindamount      {0s};
    std::chrono::seconds    m_commnotifyamount      {0s};
    int                     m_lastCommSkipDirection {0};
    time_t                  m_lastCommSkipTime      {0/*1970*/};
    uint64_t                m_lastCommSkipStart     {0};
    time_t                  m_lastSkipTime          {0 /*1970*/};
    bool                    m_hascommbreaktable     {false};
    QDateTime               m_lastIgnoredManualSkip;
    std::chrono::seconds    m_maxskip               {1h};
    std::chrono::seconds    m_maxShortMerge         {0s};
    frm_dir_map_t           m_commBreakMap;
    frm_dir_map_t::Iterator m_commBreakIter;
};

#endif // BREAKMAP_H

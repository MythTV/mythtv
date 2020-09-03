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

#include <cstdint>
#include "compat.h"

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

    mutable QMutex          m_commBreakMapLock      {QMutex::Recursive};
    int                     m_skipcommercials       {0};
    CommSkipMode            m_autocommercialskip    {kCommSkipOff};
    int                     m_commrewindamount      {0};
    int                     m_commnotifyamount      {0};
    int                     m_lastCommSkipDirection {0};
    time_t                  m_lastCommSkipTime      {0/*1970*/};
    uint64_t                m_lastCommSkipStart     {0};
    time_t                  m_lastSkipTime          {0 /*1970*/};
    bool                    m_hascommbreaktable     {false};
    QDateTime               m_lastIgnoredManualSkip;
    int                     m_maxskip               {3600};
    int                     m_maxShortMerge         {0};
    frm_dir_map_t           m_commBreakMap;
    frm_dir_map_t::Iterator m_commBreakIter;
};

#endif // BREAKMAP_H

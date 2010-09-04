#ifndef DELETEMAP_H
#define DELETEMAP_H

#include "programinfo.h"
#include "playercontext.h"

class DeleteMap
{
  public:
    DeleteMap(): m_editing(false),   m_nextCutStart(0), m_changed(true),
                 m_seekamountpos(4), m_seekamount(30) { }

    bool HandleAction(QString &action, uint64_t frame, uint64_t played,
                      uint64_t total, double rate);
    int  GetSeekAmount(void) { return m_seekamount; }
    void UpdateSeekAmount(int change, double framerate);
    void SetSeekAmount(int amount) { m_seekamount = amount; }

    void UpdateOSD(uint64_t frame, uint64_t total, double frame_rate,
                   PlayerContext *ctx, OSD *osd);

    bool IsEditing(void) { return m_editing; }
    void SetEditing(bool edit, OSD *osd = NULL);
    void SetFileEditing(PlayerContext *ctx, bool edit);
    bool IsFileEditing(PlayerContext *ctx);
    bool IsEmpty(void);

    void SetMap(const frm_dir_map_t &map);
    void LoadCommBreakMap(uint64_t total, frm_dir_map_t &map);
    void SaveMap(uint64_t total, PlayerContext *ctx);
    void LoadMap(uint64_t total, PlayerContext *ctx);
    void CleanMap(uint64_t total);

    void Clear(void);
    void ReverseAll(uint64_t total);
    void Add(uint64_t frame, uint64_t total, MarkTypes type);
    void NewCut(uint64_t frame, uint64_t total);
    void Delete(uint64_t frame, uint64_t total);
    void Reverse(uint64_t frame, uint64_t total);
    void MoveRelative(uint64_t frame, uint64_t total, bool right);
    void Move(uint64_t frame, uint64_t to, uint64_t total);

    bool     IsInDelete(uint64_t frame);
    uint64_t GetNearestMark(uint64_t frame, uint64_t total, bool right);
    bool     IsTemporaryMark(uint64_t frame);
    bool     HasTemporaryMark(uint64_t frame);
    uint64_t GetLastFrame(uint64_t total);

    void TrackerReset(uint64_t frame, uint64_t total);
    bool TrackerWantsToJump(uint64_t frame, uint64_t total, uint64_t &to);

  private:
    void Add(uint64_t frame, MarkTypes type);
    MarkTypes Delete(uint64_t frame);

    bool          m_editing;
    uint64_t      m_nextCutStart;
    frm_dir_map_t m_deleteMap;
    QString       m_seekText;
    bool          m_changed;
    int           m_seekamountpos;
    int           m_seekamount;
};

#endif // DELETEMAP_H

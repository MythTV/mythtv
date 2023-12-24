#ifndef DELETEMAP_H
#define DELETEMAP_H

#include <utility>

// Qt headers
#include <QCoreApplication>
#include <QVector>

// MythTV headers
#include "libmythbase/programtypes.h"   // for frm_dir_map_t, MarkTypes
#include "libmythtv/mythtvexp.h"

class OSD;
class PlayerContext;

struct DeleteMapUndoEntry
{
    frm_dir_map_t m_deleteMap;
    QString       m_message; // how we got from previous map to this map
    DeleteMapUndoEntry(frm_dir_map_t dm, QString msg)
        : m_deleteMap(std::move(dm)), m_message(std::move(msg)) { }
    DeleteMapUndoEntry(void) = default;
};

class MTV_PUBLIC DeleteMap
{
    Q_DECLARE_TR_FUNCTIONS(DeleteMap)
  public:
    DeleteMap() = default;

    void SetPlayerContext(PlayerContext *ctx) { m_ctx = ctx; }
    bool HandleAction(const QString &action, uint64_t frame);
    float GetSeekAmount(void) const { return m_seekamount; }
    void UpdateSeekAmount(int change);
    void SetSeekAmount(float amount) { m_seekamount = amount; }

    void UpdateOSD(uint64_t frame, double frame_rate, OSD *osd);
    static void UpdateOSD(std::chrono::milliseconds timecode, OSD *osd);

    bool IsEditing(void) const { return m_editing; }
    void SetEditing(bool edit, OSD *osd = nullptr);
    void SetFileEditing(bool edit);
    bool IsFileEditing(void);
    bool IsEmpty(void) const;
    bool IsSaved(void) const;
    bool IsChanged(void) const { return m_changed; }
    void SetChanged(bool changed = true) { m_changed = changed; }

    void SetMap(const frm_dir_map_t &map);
    void LoadCommBreakMap(frm_dir_map_t &map);
    void SaveMap(bool isAutoSave = false);
    void LoadMap(const QString& undoMessage = "");
    bool LoadAutoSaveMap(void);
    void CleanMap(void);

    void Clear(const QString& undoMessage = "");
    void ReverseAll(void);
    void NewCut(uint64_t frame);
    void Delete(uint64_t frame, const QString& undoMessage);
    void MoveRelative(uint64_t frame, bool right);
    void Move(uint64_t frame, uint64_t to);

    bool     IsInDelete(uint64_t frame) const;
    uint64_t GetNearestMark(uint64_t frame, bool right,
                            bool *hasMark = nullptr) const;
    bool     IsTemporaryMark(uint64_t frame) const;
    bool     HasTemporaryMark(void) const;
    uint64_t GetLastFrame(void) const;

    // Provide translations between frame numbers and millisecond
    // durations, optionally taking the custlist into account.
    std::chrono::milliseconds TranslatePositionFrameToMs(uint64_t position,
                                        float fallback_framerate,
                                        bool use_cutlist) const;
    uint64_t TranslatePositionMsToFrame(std::chrono::milliseconds dur_ms,
                                        float fallback_framerate,
                                        bool use_cutlist) const;
    uint64_t TranslatePositionAbsToRel(uint64_t position) const;
    uint64_t TranslatePositionRelToAbs(uint64_t position) const;

    void TrackerReset(uint64_t frame);
    bool TrackerWantsToJump(uint64_t frame, uint64_t &to) const;

    bool Undo(void);
    bool Redo(void);
    bool HasUndo(void) const { return m_undoStackPointer > 0; }
    bool HasRedo(void) const
        { return m_undoStackPointer < m_undoStack.size(); }
    QString GetUndoMessage(void) const;
    QString GetRedoMessage(void) const;

  private:
    void AddMark(uint64_t frame, MarkTypes type);
    void Add(uint64_t frame, MarkTypes type);
    MarkTypes Delete(uint64_t frame);

    void Push(const QString &undoMessage);
    void PushDeferred(const frm_dir_map_t &savedMap,
                      const QString &undoMessage);

    QString CreateTimeString(uint64_t frame, bool use_cutlist,
                             double frame_rate, bool full_resolution) const;

    bool          m_editing             {false};
    bool          m_nextCutStartIsValid {false};
    uint64_t      m_nextCutStart        {0};
    frm_dir_map_t m_deleteMap;
    QString       m_seekText;
    bool          m_changed             {true};
    int           m_seekamountpos       {4};
    float         m_seekamount          {1.0F};
    PlayerContext *m_ctx                {nullptr};
    uint64_t      m_cachedTotalForOSD   {0};

    QVector<DeleteMapUndoEntry> m_undoStack;
    int m_undoStackPointer              {0};
};

#endif // DELETEMAP_H

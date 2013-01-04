#ifndef DELETEMAP_H
#define DELETEMAP_H

#include <QCoreApplication>

#include "osd.h"
#include "programinfo.h"
#include "playercontext.h"
#include "mythtvexp.h"

class DeleteMap;

typedef struct DeleteMapUndoEntry
{
    frm_dir_map_t deleteMap;
    QString message; // how we got from previous map to this map
    DeleteMapUndoEntry(const frm_dir_map_t &dm, const QString &msg);
    DeleteMapUndoEntry(void);
} DeleteMapUndoEntry;

class MTV_PUBLIC DeleteMap
{
    Q_DECLARE_TR_FUNCTIONS(DeleteMap)
  public:
    DeleteMap(): m_editing(false),
                 m_nextCutStartIsValid(false),
                 m_nextCutStart(0), m_changed(true),
                 m_seekamountpos(4), m_seekamount(1.0),
                 m_ctx(NULL), m_cachedTotalForOSD(0), m_undoStackPointer(0)
    {
    }

    void SetPlayerContext(PlayerContext *ctx) { m_ctx = ctx; }
    bool HandleAction(QString &action, uint64_t frame, uint64_t played);
    float GetSeekAmount(void) const { return m_seekamount; }
    void UpdateSeekAmount(int change);
    void SetSeekAmount(float amount) { m_seekamount = amount; }

    void UpdateOSD(uint64_t frame, double frame_rate, OSD *osd);

    bool IsEditing(void) const { return m_editing; }
    void SetEditing(bool edit, OSD *osd = NULL);
    void SetFileEditing(bool edit);
    bool IsFileEditing(void);
    bool IsEmpty(void) const;
    bool IsSaved(void) const;

    void SetMap(const frm_dir_map_t &map);
    void LoadCommBreakMap(frm_dir_map_t &map);
    void SaveMap(bool isAutoSave = false);
    void LoadMap(QString undoMessage = "");
    bool LoadAutoSaveMap(void);
    void CleanMap(void);

    void Clear(QString undoMessage = "");
    void ReverseAll(void);
    void NewCut(uint64_t frame);
    void Delete(uint64_t frame, QString undoMessage);
    void MoveRelative(uint64_t frame, bool right);
    void Move(uint64_t frame, uint64_t to);

    bool     IsInDelete(uint64_t frame) const;
    uint64_t GetNearestMark(uint64_t frame, bool right,
                            bool *hasMark = NULL) const;
    bool     IsTemporaryMark(uint64_t frame) const;
    bool     HasTemporaryMark(void) const;
    uint64_t GetLastFrame(void) const;

    // Provide translations between frame numbers and millisecond
    // durations, optionally taking the custlist into account.
    uint64_t TranslatePositionFrameToMs(uint64_t position,
                                        float fallback_framerate,
                                        bool use_cutlist) const;
    uint64_t TranslatePositionMsToFrame(uint64_t dur_ms,
                                        float fallback_framerate,
                                        bool use_cutlist) const;
    uint64_t TranslatePositionAbsToRel(uint64_t position) const;
    uint64_t TranslatePositionRelToAbs(uint64_t position) const;

    void TrackerReset(uint64_t frame);
    bool TrackerWantsToJump(uint64_t frame, uint64_t &to);

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

    bool          m_editing;
    bool          m_nextCutStartIsValid;
    uint64_t      m_nextCutStart;
    frm_dir_map_t m_deleteMap;
    QString       m_seekText;
    bool          m_changed;
    int           m_seekamountpos;
    float         m_seekamount;
    PlayerContext *m_ctx;
    uint64_t      m_cachedTotalForOSD;

    QVector<DeleteMapUndoEntry> m_undoStack;
    int m_undoStackPointer;
};

#endif // DELETEMAP_H

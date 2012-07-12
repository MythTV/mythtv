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
    DeleteMapUndoEntry(frm_dir_map_t dm, QString msg);
    DeleteMapUndoEntry(void);
} DeleteMapUndoEntry;

class MTV_PUBLIC DeleteMap
{
    Q_DECLARE_TR_FUNCTIONS(DeleteMap)
  public:
    DeleteMap(): m_editing(false),
                 m_nextCutStartIsValid(false),
                 m_nextCutStart(0), m_changed(true),
                 m_seekamountpos(4), m_seekamount(30),
                 m_ctx(0), m_cachedTotalForOSD(0), m_undoStackPointer(-1)
    {
        Push("");
    }

    void SetPlayerContext(PlayerContext *ctx) { m_ctx = ctx; }
    bool HandleAction(QString &action, uint64_t frame, uint64_t played,
                      uint64_t total, double rate);
    int  GetSeekAmount(void) const { return m_seekamount; }
    void UpdateSeekAmount(int change, double framerate);
    void SetSeekAmount(int amount) { m_seekamount = amount; }

    void UpdateOSD(uint64_t frame, uint64_t total, double frame_rate, OSD *osd);

    bool IsEditing(void) const { return m_editing; }
    void SetEditing(bool edit, OSD *osd = NULL);
    void SetFileEditing(bool edit);
    bool IsFileEditing(void);
    bool IsEmpty(void) const;
    bool IsSaved(void) const;

    void SetMap(const frm_dir_map_t &map);
    void LoadCommBreakMap(uint64_t total, frm_dir_map_t &map);
    void SaveMap(uint64_t total, bool isAutoSave = false);
    void LoadMap(uint64_t total, QString undoMessage = "");
    bool LoadAutoSaveMap(uint64_t total);
    void CleanMap(uint64_t total);

    void Clear(QString undoMessage = "");
    void ReverseAll(uint64_t total);
    void Add(uint64_t frame, uint64_t total, MarkTypes type,
             QString undoMessage);
    void NewCut(uint64_t frame, uint64_t total);
    void Delete(uint64_t frame, uint64_t total, QString undoMessage = "");
    void MoveRelative(uint64_t frame, uint64_t total, bool right);
    void Move(uint64_t frame, uint64_t to, uint64_t total);

    bool     IsInDelete(uint64_t frame) const;
    uint64_t GetNearestMark(uint64_t frame, uint64_t total, bool right,
                            bool *hasMark = 0) const;
    bool     IsTemporaryMark(uint64_t frame) const;
    bool     HasTemporaryMark(void) const;
    uint64_t GetLastFrame(uint64_t total) const;
    uint64_t TranslatePositionAbsToRel(uint64_t absPosition) const {
        return TranslatePositionAbsToRel(m_deleteMap, absPosition);
    }
    uint64_t TranslatePositionRelToAbs(uint64_t relPosition) const {
        return TranslatePositionRelToAbs(m_deleteMap, relPosition);
    }
    static uint64_t TranslatePositionAbsToRel(const frm_dir_map_t &deleteMap,
                                              uint64_t absPosition);
    static uint64_t TranslatePositionRelToAbs(const frm_dir_map_t &deleteMap,
                                              uint64_t relPosition);

    void TrackerReset(uint64_t frame, uint64_t total);
    bool TrackerWantsToJump(uint64_t frame, uint64_t total, uint64_t &to);

    bool Undo(void);
    bool Redo(void);
    bool HasUndo(void) const { return m_undoStackPointer > 0; }
    bool HasRedo(void) const
        { return m_undoStackPointer < m_undoStack.size() - 1; }
    QString GetUndoMessage(void) const;
    QString GetRedoMessage(void) const;

  private:
    void Add(uint64_t frame, MarkTypes type);
    MarkTypes Delete(uint64_t frame);

    void Push(QString undoMessage);

    bool          m_editing;
    bool          m_nextCutStartIsValid;
    uint64_t      m_nextCutStart;
    frm_dir_map_t m_deleteMap;
    QString       m_seekText;
    bool          m_changed;
    int           m_seekamountpos;
    int           m_seekamount;
    PlayerContext *m_ctx;
    uint64_t      m_cachedTotalForOSD;

    // Invariant: m_undoStack[m_undoStackPointer].deleteMap == m_deleteMap
    QVector<DeleteMapUndoEntry> m_undoStack;
    int m_undoStackPointer;
};

#endif // DELETEMAP_H

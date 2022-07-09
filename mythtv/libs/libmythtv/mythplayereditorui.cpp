#include <algorithm>

// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythuiactions.h"
#include "tv_actions.h"
#include "tv_play.h"
#include "mythplayereditorui.h"

#define LOC QString("Editor: ")

MythPlayerEditorUI::MythPlayerEditorUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags)
  : MythPlayerVisualiserUI(MainWindow, Tv, Context, Flags)
{
    qRegisterMetaType<MythEditorState>();

    // Connect incoming TV signals
    connect(Tv, &TV::EnableEdit,  this, &MythPlayerEditorUI::EnableEdit);
    connect(Tv, &TV::DisableEdit, this, &MythPlayerEditorUI::DisableEdit);
    connect(Tv, &TV::RefreshEditorState, this, &MythPlayerEditorUI::RefreshEditorState);

    // New state
    connect(this, &MythPlayerEditorUI::EditorStateChanged, Tv, &TV::EditorStateChanged);
}

void MythPlayerEditorUI::InitialiseState()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Initialising editor");
    MythPlayerVisualiserUI::InitialiseState();
}

void MythPlayerEditorUI::RefreshEditorState(bool CheckSaved /*=false*/)
{
    auto frame = GetFramesPlayed();
    emit EditorStateChanged({
        frame,
        m_deleteMap.GetNearestMark(frame, false),
        m_deleteMap.GetNearestMark(frame, true),
        GetTotalFrameCount(),
        m_deleteMap.IsInDelete(frame),
        m_deleteMap.IsTemporaryMark(frame),
        m_deleteMap.HasTemporaryMark(),
        m_deleteMap.HasUndo(),
        m_deleteMap.GetUndoMessage(),
        m_deleteMap.HasRedo(),
        m_deleteMap.GetRedoMessage(),
        CheckSaved ? m_deleteMap.IsSaved() : false });
}

void MythPlayerEditorUI::EnableEdit()
{
    m_deleteMap.SetEditing(false);

    if (!m_hasFullPositionMap)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot edit - no full position map");
        SetOSDStatus(tr("No Seektable"), kOSDTimeout_Med);
        return;
    }

    if (m_deleteMap.IsFileEditing())
        return;

    QMutexLocker locker(&m_osdLock);
    SetupAudioGraph(m_videoFrameRate);

    m_savedAudioTimecodeOffset = m_tcWrap[TC_AUDIO];
    m_tcWrap[TC_AUDIO] = 0ms;

    m_speedBeforeEdit = m_playSpeed;
    m_pausedBeforeEdit = Pause();
    m_deleteMap.SetEditing(true);
    m_osd.DialogQuit();
    ResetCaptions();
    m_osd.HideAll();

    bool loadedAutoSave = m_deleteMap.LoadAutoSaveMap();
    if (loadedAutoSave)
        UpdateOSDMessage(tr("Using previously auto-saved cuts"), kOSDTimeout_Short);

    m_deleteMap.UpdateSeekAmount(0);
    m_deleteMap.UpdateOSD(m_framesPlayed, m_videoFrameRate, &m_osd);
    m_deleteMap.SetFileEditing(true);
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
        m_playerCtx->m_playingInfo->SaveEditing(true);
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    m_editUpdateTimer.start();
}

/*! \brief Leave cutlist edit mode, saving work in 1 of 3 ways.
 *
 *  \param HowToSave If 1, save all changes.  If 0, discard all
 *  changes.  If -1, do not explicitly save changes but leave
 *  auto-save information intact in the database.
 */
void MythPlayerEditorUI::DisableEdit(int HowToSave)
{
    QMutexLocker locker(&m_osdLock);
    m_deleteMap.SetEditing(false, &m_osd);
    if (HowToSave == 0)
        m_deleteMap.LoadMap();
    // Unconditionally save to remove temporary marks from the DB.
    if (HowToSave >= 0)
        m_deleteMap.SaveMap();
    m_deleteMap.TrackerReset(m_framesPlayed);
    m_deleteMap.SetFileEditing(false);
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
        m_playerCtx->m_playingInfo->SaveEditing(false);
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    ClearAudioGraph();
    m_tcWrap[TC_AUDIO] = m_savedAudioTimecodeOffset;
    m_savedAudioTimecodeOffset = 0ms;

    if (!m_pausedBeforeEdit)
        Play(m_speedBeforeEdit);
    else
        SetOSDStatus(tr("Paused"), kOSDTimeout_None);
}

void MythPlayerEditorUI::HandleArbSeek(bool Direction)
{
    if (qFuzzyCompare(m_deleteMap.GetSeekAmount() + 1000.0F, 1000.0F -2.0F))
    {
        uint64_t framenum = m_deleteMap.GetNearestMark(m_framesPlayed, Direction);
        if (Direction && (framenum > m_framesPlayed))
            DoFastForward(framenum - m_framesPlayed, kInaccuracyNone);
        else if (!Direction && (m_framesPlayed > framenum))
            DoRewind(m_framesPlayed - framenum, kInaccuracyNone);
    }
    else
    {
        if (Direction)
            DoFastForward(2, kInaccuracyFull);
        else
            DoRewind(2, kInaccuracyFull);
    }
}

bool MythPlayerEditorUI::HandleProgramEditorActions(const QStringList& Actions)
{
    bool handled = false;
    bool refresh = true;
    auto frame   = GetFramesPlayed();

    for (int i = 0; i < Actions.size() && !handled; i++)
    {
        static constexpr float FFREW_MULTICOUNT { 10.0F };
        const QString& action = Actions[i];
        handled = true;
        float seekamount = m_deleteMap.GetSeekAmount();
        bool  seekzero   = qFuzzyCompare(seekamount + 1.0F, 1.0F);
        if (action == ACTION_LEFT)
        {
            if (seekzero) // 1 frame
            {
                DoRewind(1, kInaccuracyNone);
            }
            else if (seekamount > 0)
            {
                // Use fully-accurate seeks for less than 1 second.
                DoRewindSecs(seekamount, seekamount < 1.0F ? kInaccuracyNone :
                             kInaccuracyEditor, false);
            }
            else
            {
                HandleArbSeek(false);
            }
        }
        else if (action == ACTION_RIGHT)
        {
            if (seekzero) // 1 frame
            {
                DoFastForward(1, kInaccuracyNone);
            }
            else if (seekamount > 0)
            {
                // Use fully-accurate seeks for less than 1 second.
                DoFastForwardSecs(seekamount, seekamount < 1.0F ? kInaccuracyNone :
                             kInaccuracyEditor, false);
            }
            else
            {
                HandleArbSeek(true);
            }
        }
        else if (action == ACTION_LOADCOMMSKIP)
        {
            if (m_commBreakMap.HasMap())
            {
                frm_dir_map_t map;
                m_commBreakMap.GetMap(map);
                m_deleteMap.LoadCommBreakMap(map);
            }
        }
        else if (action == ACTION_PREVCUT)
        {
            float old_seekamount = m_deleteMap.GetSeekAmount();
            m_deleteMap.SetSeekAmount(-2);
            HandleArbSeek(false);
            m_deleteMap.SetSeekAmount(old_seekamount);
        }
        else if (action == ACTION_NEXTCUT)
        {
            float old_seekamount = m_deleteMap.GetSeekAmount();
            m_deleteMap.SetSeekAmount(-2);
            HandleArbSeek(true);
            m_deleteMap.SetSeekAmount(old_seekamount);
        }
        else if (action == ACTION_BIGJUMPREW)
        {
            if (seekzero)
                DoRewind(FFREW_MULTICOUNT, kInaccuracyNone);
            else if (seekamount > 0)
                DoRewindSecs(seekamount * FFREW_MULTICOUNT, kInaccuracyEditor, false);
            else
                DoRewindSecs(FFREW_MULTICOUNT / 2, kInaccuracyNone, false);
        }
        else if (action == ACTION_BIGJUMPFWD)
        {
            if (seekzero)
                DoFastForward(FFREW_MULTICOUNT, kInaccuracyNone);
            else if (seekamount > 0)
                DoFastForwardSecs(seekamount * FFREW_MULTICOUNT, kInaccuracyEditor, false);
            else
                DoFastForwardSecs(FFREW_MULTICOUNT / 2, kInaccuracyNone, false);
        }
        else if (action == ACTION_SELECT)
        {
            m_deleteMap.NewCut(frame);
            UpdateOSDMessage(tr("New cut added."), kOSDTimeout_Short);
            refresh = true;
        }
        else if (action == "DELETE")
        {
            m_deleteMap.Delete(frame, tr("Delete"));
            refresh = true;
        }
        else if (action == "REVERT")
        {
            m_deleteMap.LoadMap(tr("Undo Changes"));
            refresh = true;
        }
        else if (action == "REVERTEXIT")
        {
            DisableEdit(0);
            refresh = false;
        }
        else if (action == ACTION_SAVEMAP)
        {
            m_deleteMap.SaveMap();
            refresh = true;
        }
        else if (action == "EDIT" || action == "SAVEEXIT")
        {
            DisableEdit(1);
            refresh = false;
        }
        else
        {
            QString undoMessage = m_deleteMap.GetUndoMessage();
            QString redoMessage = m_deleteMap.GetRedoMessage();
            handled = m_deleteMap.HandleAction(action, frame);
            if (handled && (action == "CUTTOBEGINNING" || action == "CUTTOEND" || action == "NEWCUT"))
                UpdateOSDMessage(tr("New cut added."), kOSDTimeout_Short);
            else if (handled && action == "UNDO")
                UpdateOSDMessage(tr("Undo - %1").arg(undoMessage), kOSDTimeout_Short);
            else if (handled && action == "REDO")
                UpdateOSDMessage(tr("Redo - %1").arg(redoMessage), kOSDTimeout_Short);
        }
    }

    if (handled && refresh)
    {
        m_osdLock.lock();
        m_deleteMap.UpdateOSD(m_framesPlayed, m_videoFrameRate, &m_osd);
        m_osdLock.unlock();
    }

    return handled;
}

bool MythPlayerEditorUI::DoFastForwardSecs(float Seconds, double Inaccuracy, bool UseCutlist)
{
    float current = ComputeSecs(m_framesPlayed, UseCutlist);
    uint64_t targetFrame = FindFrame(current + Seconds, UseCutlist);
    return DoFastForward(targetFrame - m_framesPlayed, Inaccuracy);
}

bool MythPlayerEditorUI::DoRewindSecs(float Seconds, double Inaccuracy, bool UseCutlist)
{
    float target = std::max(0.0F, ComputeSecs(m_framesPlayed, UseCutlist) - Seconds);
    uint64_t targetFrame = FindFrame(target, UseCutlist);
    return DoRewind(m_framesPlayed - targetFrame, Inaccuracy);
}

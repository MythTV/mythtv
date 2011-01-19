#include <cmath>

#include "mythverbose.h"
#include "mythcontext.h"
#include "osd.h"
#include "deletemap.h"

#define LOC     QString("DelMap: ")
#define LOC_ERR QString("DelMap Err: ")
#define EDIT_CHECK if(!m_editing) \
  { VERBOSE(VB_IMPORTANT, LOC_ERR + "Cannot edit outside editmode."); return; }

bool DeleteMap::HandleAction(QString &action, uint64_t frame,
                             uint64_t played, uint64_t total, double rate)
{
    bool handled = true;
    if (action == "REVERSE")
        Reverse(frame, total);
    else if (action == "UP")
        UpdateSeekAmount(1, rate);
    else if (action == "DOWN")
        UpdateSeekAmount(-1, rate);
    else if (action == ACTION_CLEARMAP)
        Clear();
    else if (action == ACTION_INVERTMAP)
        ReverseAll(total);
    else if (action == "MOVEPREV")
        MoveRelative(frame, total, false);
    else if (action == "MOVENEXT")
        MoveRelative(frame, total, true);
    else if (action == "CUTTOBEGINNING")
        Add(frame, total, MARK_CUT_END);
    else if (action == "CUTTOEND")
        Add(frame, total, MARK_CUT_START);
    else if (action == "NEWCUT")
        NewCut(frame, total);
    else if (action == "DELETE")
        Delete(frame, total);
    else
        handled = false;
    return handled;
}

void DeleteMap::UpdateSeekAmount(int change, double framerate)
{
    m_seekamountpos += change;
    if (m_seekamountpos > 9)
        m_seekamountpos = 9;
    if (m_seekamountpos < 0)
        m_seekamountpos = 0;

    m_seekText = "";
    switch (m_seekamountpos)
    {
        case 0: m_seekText = QObject::tr("cut point"); m_seekamount = -2; break;
        case 1: m_seekText = QObject::tr("keyframe"); m_seekamount = -1; break;
        case 2: m_seekText = QObject::tr("1 frame"); m_seekamount = 1; break;
        case 3: m_seekText = QObject::tr("0.5 seconds"); m_seekamount = (int)roundf(framerate / 2); break;
        case 4: m_seekText = QObject::tr("%n second(s)", "", 1); m_seekamount = (int)roundf(framerate); break;
        case 5: m_seekText = QObject::tr("%n second(s)", "", 5); m_seekamount = (int)roundf(framerate * 5); break;
        case 6: m_seekText = QObject::tr("%n second(s)", "", 20); m_seekamount = (int)roundf(framerate * 20); break;
        case 7: m_seekText = QObject::tr("%n minute(s)", "", 1); m_seekamount = (int)roundf(framerate * 60); break;
        case 8: m_seekText = QObject::tr("%n minute(s)", "", 5); m_seekamount = (int)roundf(framerate * 300); break;
        case 9: m_seekText = QObject::tr("%n minute(s)", "", 10); m_seekamount = (int)roundf(framerate * 600); break;
        default: m_seekText = QObject::tr("error"); m_seekamount = (int)roundf(framerate); break;
    }
}

/**
 * \brief Show and update the edit mode On Screen Display. The cut regions
 *        are only refreshed if the deleteMap has been updated.
 */
void DeleteMap::UpdateOSD(uint64_t frame, uint64_t total, double frame_rate,
                          PlayerContext *ctx, OSD *osd)
{
    if (!osd || !ctx)
        return;
    CleanMap(total);

    InfoMap infoMap;
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (ctx->playingInfo)
        ctx->playingInfo->ToMap(infoMap);
    infoMap.detach();
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    int secs   = (int)(frame / frame_rate);
    int frames = frame - (int)(secs * frame_rate);
    QString timestr = QString::number(secs / 3600) +
                      QString(":%1").arg((secs / 60) % 60, 2, 10, QChar(48)) +
                      QString(":%1").arg(secs % 60, 2, 10, QChar(48)) +
                      QString(".%1").arg(frames, 2, 10, QChar(48));

    QString cutmarker = " ";
    if (IsInDelete(frame))
        cutmarker = QObject::tr("cut");

    infoMap["timedisplay"]  = timestr;
    infoMap["framedisplay"] = QString::number(frame);
    infoMap["cutindicator"] = cutmarker;
    infoMap["title"]        = QObject::tr("Edit");
    infoMap["seekamount"]   = m_seekText;;

    QHash<QString,float> posMap;
    posMap.insert("position", (float)((double)frame/(double)total));
    osd->SetValues("osd_program_editor", posMap, kOSDTimeout_None);
    osd->SetText("osd_program_editor", infoMap,  kOSDTimeout_None);
    if (m_changed)
        osd->SetRegions("osd_program_editor", m_deleteMap, total);
    m_changed = false;
}

/// Set the edit mode and optionally hide the edit mode OSD.
void DeleteMap::SetEditing(bool edit, OSD *osd)
{
    if (osd && !edit)
        osd->HideWindow("osd_program_editor");
    m_editing = edit;
}

/// Update the editing status in the file's ProgramInfo.
void DeleteMap::SetFileEditing(PlayerContext *ctx, bool edit)
{
    if (ctx)
    {
        ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (ctx->playingInfo)
            ctx->playingInfo->SetEditing(edit);
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }
}

/// Determines whether the file is currently in edit mode.
bool DeleteMap::IsFileEditing(PlayerContext *ctx)
{
    bool result = false;
    if (ctx)
    {
        ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (ctx->playingInfo)
            result = ctx->playingInfo->QueryIsEditing();
        ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }
    return result;
}

bool DeleteMap::IsEmpty(void)
{
    return m_deleteMap.empty();
}

/// Clears the deleteMap.
void DeleteMap::Clear(void)
{
    m_deleteMap.clear();
    m_changed = true;
}

/// Reverses the direction of each mark in the map.
void DeleteMap::ReverseAll(uint64_t total)
{
    EDIT_CHECK
    frm_dir_map_t::Iterator it = m_deleteMap.begin();
    for ( ; it != m_deleteMap.end(); ++it)
        Add(it.key(), it.value() == MARK_CUT_END ? MARK_CUT_START :
                                                   MARK_CUT_END);
    CleanMap(total);
}

/**
 * \brief Add a new mark of the given type. Before the new mark is added, any
 *        existing redundant mark of that type is removed. This simplifies
 *        the cleanup code.
 */
void DeleteMap::Add(uint64_t frame, uint64_t total, MarkTypes type)
{
    EDIT_CHECK
    if ((MARK_CUT_START != type) && (MARK_CUT_END != type) &&
        (MARK_PLACEHOLDER != type))
        return;

    frm_dir_map_t::Iterator find_temporary = m_deleteMap.find(frame);
    if (find_temporary != m_deleteMap.end())
    {
        if (MARK_PLACEHOLDER == find_temporary.value())
        {
            // Delete the temporary mark before putting a real mark at its
            // location
            Delete(frame, total);
        }
        else // Don't add a mark on top of a mark
            return;
    }

    int       lasttype  = MARK_UNSET;
    long long lastframe = -1;
    long long remove    = -1;
    QMutableMapIterator<uint64_t, MarkTypes> it(m_deleteMap);

    if (type == MARK_CUT_END)
    {
        // remove curent end marker if it exists
        while (it.hasNext())
        {
            it.next();
            if (it.key() > frame)
            {
                if ((lasttype == MARK_CUT_END) && (lastframe > -1))
                    remove = lastframe;
                break;
            }
            lasttype  = it.value();
            lastframe = it.key();
        }
        if ((remove < 0) && (lasttype == MARK_CUT_END) &&
            (lastframe > -1) && (lastframe < (int64_t)frame))
            remove = lastframe;
    }
    else if (type == MARK_CUT_START)
    {
        // remove curent start marker if it exists
        it.toBack();
        while (it.hasPrevious())
        {
            it.previous();
            if (it.key() <= frame)
            {
                if (lasttype == MARK_CUT_START && (lastframe > -1))
                    remove = lastframe;
                break;
            }
            lasttype  = it.value();
            lastframe = it.key();
        }
        if ((remove < 0) && (lasttype == MARK_CUT_START) &&
            (lastframe > -1) && (lastframe > (int64_t)frame))
            remove = lastframe;
    }

    if (remove > -1)
        Delete((uint64_t)remove);
    Add(frame, type);
    CleanMap(total);
}

/// Remove the mark at the given frame.
void DeleteMap::Delete(uint64_t frame, uint64_t total)
{
    EDIT_CHECK
    if (m_deleteMap.isEmpty())
        return;

    uint64_t prev = GetNearestMark(frame, total, false);
    uint64_t next = GetNearestMark(frame, total, true);

    // If frame is a cut point, GetNearestMark() would return the previous/next
    // mark (not this frame), so check to see if we need to use frame, instead
    frm_dir_map_t::Iterator it = m_deleteMap.find(frame);
    if (it != m_deleteMap.end())
    {
        int type = it.value();
        if (MARK_PLACEHOLDER == type)
            next = prev = frame;
        else if (MARK_CUT_END == type)
            next  = frame;
        else if (MARK_CUT_START == type)
            prev = frame;
    }

    Delete(prev);
    if (prev != next)
        Delete(next);
    CleanMap(total);
}

/// Reverse the direction of the mark at the given frame.
void DeleteMap::Reverse(uint64_t frame, uint64_t total)
{
    EDIT_CHECK
    int type = Delete(frame);
    Add(frame, total, type == MARK_CUT_END ? MARK_CUT_START : MARK_CUT_END);
}

/// Add a new cut marker (to start or end a cut region)
void DeleteMap::NewCut(uint64_t frame, uint64_t total)
{
    EDIT_CHECK

    // find any existing temporary marker to determine cut range
    int64_t existing = -1;
    frm_dir_map_t::Iterator it;
    for (it = m_deleteMap.begin() ; it != m_deleteMap.end(); ++it)
    {
        if (MARK_PLACEHOLDER == it.value())
        {
            existing = it.key();
            break;
        }
    }

    if (existing > -1)
    {
        uint64_t otherframe = static_cast<uint64_t>(existing);
        if (otherframe == frame)
            Delete(otherframe);
        else
        {
            uint64_t startframe;
            uint64_t endframe;
            int64_t cut_start = -1;
            int64_t cut_end = -1;
            if (IsInDelete(frame))
            {
                MarkTypes type = MARK_UNSET;
                cut_start = GetNearestMark(frame, total, false);
                cut_end = GetNearestMark(frame, total, true);
                frm_dir_map_t::Iterator it = m_deleteMap.find(frame);
                if (it != m_deleteMap.end())
                    type = it.value();
                if (MARK_CUT_START == type)
                {
                    cut_start = frame;
                }
                else if (MARK_CUT_END == type)
                {
                    cut_end = frame;
                }
            }

            if (otherframe < frame)
            {
                startframe = otherframe;
                endframe = cut_end != -1 ? static_cast<uint64_t>(cut_end)
                                         : frame;
            }
            else
            {
                startframe = cut_start != -1 ? static_cast<uint64_t>(cut_start)
                                             : frame;
                endframe = otherframe;
            }

            // Don't place a cut marker on first or last frame; instead cut
            // to beginning or end
            if (startframe == 1)
                startframe = 0;
            if (endframe == total - 1)
                endframe = total;

            // Don't cut the entire recording
            if ((startframe == 0) && (endframe == total))
            {
                VERBOSE(VB_IMPORTANT, "Refusing to cut entire recording.");
                return;
            }

            Delete(otherframe);
            Add(startframe, MARK_CUT_START);
            Add(endframe, MARK_CUT_END);
            // Clear out any markers between the start and end frames
            otherframe = 0;
            frm_dir_map_t::Iterator it = m_deleteMap.find(startframe);
            for ( ; it != m_deleteMap.end() && otherframe < endframe; ++it)
            {
                otherframe = it.key();
                if ((startframe < otherframe) && (endframe > otherframe))
                {
                    VERBOSE(VB_PLAYBACK, QString("Deleting bounded marker: %1")
                                                 .arg(otherframe));
                    Delete(otherframe);
                }
            }
        }
    }
    else
        Add(frame, MARK_PLACEHOLDER);

    CleanMap(total);
}

/// Move the previous (!right) or next (right) cut to frame.
void DeleteMap::MoveRelative(uint64_t frame, uint64_t total, bool right)
{
    frm_dir_map_t::Iterator it = m_deleteMap.find(frame);
    if (it != m_deleteMap.end())
    {
        int type = it.value();
        if (((MARK_CUT_START == type) && !right) ||
            ((MARK_CUT_END == type) && right))
        {
            // If on a mark, don't move a mark from a different cut region;
            // instead, "move" this mark onto itself
            return;
        }
        else if (((MARK_CUT_START == type) && right) ||
                 ((MARK_CUT_END == type) && !right))
        {
            // If on a mark, don't collapse a cut region to 0;
            // instead, delete the region
            Delete(frame, total);
            return;
        }
        else if (MARK_PLACEHOLDER == type)
        {
            // Delete the temporary mark before putting a real mark at its
            // location
            Delete(frame, total);
        }
    }

    uint64_t from = GetNearestMark(frame, total, right);
    Move(from, frame, total);
}

/// Move an existing mark to a new frame.
void DeleteMap::Move(uint64_t frame, uint64_t to, uint64_t total)
{
    EDIT_CHECK
    MarkTypes type = Delete(frame);
    if (MARK_UNSET == type)
    {
        if (frame == 0)
            type = MARK_CUT_START;
        else if (frame == total)
            type = MARK_CUT_END;
    }
    Add(to, total, type);
}

/// Private addition to the deleteMap.
void DeleteMap::Add(uint64_t frame, MarkTypes type)
{
    m_deleteMap[frame] = type;
    m_changed = true;
}

/// Private removal from the deleteMap.
MarkTypes DeleteMap::Delete(uint64_t frame)
{
    if (m_deleteMap.contains(frame))
    {
        m_changed = true;
        return m_deleteMap.take(frame);
    }
    return MARK_UNSET;
}

/**
 * \brief Returns true if the given frame is deemed to be within a region
 *        that should be cut.
 */
bool DeleteMap::IsInDelete(uint64_t frame)
{
    if (m_deleteMap.isEmpty())
        return false;

    frm_dir_map_t::Iterator it = m_deleteMap.find(frame);
    if (it != m_deleteMap.end())
        return true;

    int      lasttype  = MARK_UNSET;
    uint64_t lastframe = -1;
    for (it = m_deleteMap.begin() ; it != m_deleteMap.end(); ++it)
    {
        if (it.key() > frame)
            return MARK_CUT_END == it.value();
        lasttype  = it.value();
        lastframe = it.key();
    }

    if (lasttype == MARK_CUT_START && lastframe <= frame)
        return true;
    return false;
}

/**
 * \brief Returns true if the given frame is a temporary/placeholder mark
 */
bool DeleteMap::IsTemporaryMark(uint64_t frame)
{
    if (m_deleteMap.isEmpty())
        return false;

    frm_dir_map_t::Iterator it = m_deleteMap.find(frame);
    return (it != m_deleteMap.end()) && (MARK_PLACEHOLDER == it.value());
}

/**
 * \brief Returns the next or previous mark. If these do not exist, returns
 *        either zero (the first frame) or total (the last frame).
 */
uint64_t DeleteMap::GetNearestMark(uint64_t frame, uint64_t total, bool right)
{
    uint64_t result;
    frm_dir_map_t::Iterator it = m_deleteMap.begin();
    if (right)
    {
        result = total;
        for (; it != m_deleteMap.end(); ++it)
            if (it.key() > frame)
                return it.key();
    }
    else
    {
        result = 0;
        for (; it != m_deleteMap.end(); ++it)
        {
            if (it.key() >= frame)
                return result;
            result = it.key();
        }
    }
    return result;
}

/**
 * \brief Returns true if a temporary placeholder mark is defined.
 */
bool DeleteMap::HasTemporaryMark(void)
{
    if (!m_deleteMap.isEmpty())
    {
        frm_dir_map_t::Iterator it = m_deleteMap.begin();
        for ( ; it != m_deleteMap.end(); ++it)
            if (MARK_PLACEHOLDER == it.value())
                return true;
    }

    return false;
}

/**
 * \brief Removes redundant marks and ensures the markup sequence is valid.
 *        A valid sequence is 1 or more marks of alternating values and does
 *        not include the first or last frames.
 */
void DeleteMap::CleanMap(uint64_t total)
{
    if (IsEmpty())
        return;

    Delete(0);
    Delete(total);

    bool clear = false;
    while (!IsEmpty() && !clear)
    {
        clear = true;
        int     lasttype  = MARK_UNSET;
        int64_t lastframe = -1;
        int64_t tempframe = -1;
        frm_dir_map_t::iterator it = m_deleteMap.begin();
        for ( ; it != m_deleteMap.end(); ++it)
        {
            int      thistype  = it.value();
            uint64_t thisframe = it.key();
            if (lasttype == thistype)
            {
                Delete(thistype == MARK_CUT_END ? thisframe :
                                                  (uint64_t)lastframe);
                clear = false;
                break;
            }
            if (MARK_PLACEHOLDER == thistype)
            {
                if (tempframe > 0)
                    Delete(tempframe);
                tempframe = thisframe;
            }
            else
            {
                lasttype  = thistype;
                lastframe = thisframe;
            }
        }
    }
}

/// Use the given map.
void DeleteMap::SetMap(const frm_dir_map_t &map)
{
    Clear();
    m_deleteMap = map;
    m_deleteMap.detach();
}

/// Loads the given commercial break map into the deleteMap.
void DeleteMap::LoadCommBreakMap(uint64_t total, frm_dir_map_t &map)
{
    Clear();
    frm_dir_map_t::Iterator it = map.begin();
    for ( ; it != map.end(); ++it)
        Add(it.key(), it.value() == MARK_COMM_START ?
                MARK_CUT_START : MARK_CUT_END);
    CleanMap(total);
}

/// Loads the delete map from the database.
void DeleteMap::LoadMap(uint64_t total, PlayerContext *ctx)
{
    if (!ctx || !ctx->playingInfo || gCoreContext->IsDatabaseIgnored())
        return;

    Clear();
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    ctx->playingInfo->QueryCutList(m_deleteMap);
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    CleanMap(total);
}

/// Saves the delete map to the database.
void DeleteMap::SaveMap(uint64_t total, PlayerContext *ctx)
{
    if (!ctx || !ctx->playingInfo || gCoreContext->IsDatabaseIgnored())
        return;

    // Remove temporary placeholder marks
    QMutableMapIterator<uint64_t, MarkTypes> it(m_deleteMap);
    while (it.hasNext())
    {
        it.next();
        if (MARK_PLACEHOLDER == it.value())
        {
            it.remove();
            m_changed = true;
        }
    }

    CleanMap(total);
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    ctx->playingInfo->SaveMarkupFlag(MARK_UPDATED_CUT);
    ctx->playingInfo->SaveCutList(m_deleteMap);
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

/**
 * \brief Resets the internal state tracker.
 *        The tracker records the frame at which the next cut sequence begins.
 *        This is used by the player to avoid iterating over the entire map
 *        many times per second.
 */
void DeleteMap::TrackerReset(uint64_t frame, uint64_t total)
{
    m_nextCutStart = 0;
    if (IsEmpty())
        return;

    frm_dir_map_t::iterator cutpoint = m_deleteMap.find(frame);
    if (cutpoint != m_deleteMap.end())
    {
        if (cutpoint.value() == MARK_CUT_START)
        {
            m_nextCutStart = cutpoint.key();
        }
        else
        {
            ++cutpoint;
            m_nextCutStart = cutpoint != m_deleteMap.end() ? cutpoint.key() : total;
        }
    }
    else
        m_nextCutStart = GetNearestMark(frame, total, !IsInDelete(frame));
    VERBOSE(VB_PLAYBACK, LOC + QString("Tracker next CUT_START: %1")
                                       .arg(m_nextCutStart));
}

/**
 * \brief Returns true if the given frame has passed the last cut point start
 *        and provides the frame number of the next jump.
 */
bool DeleteMap::TrackerWantsToJump(uint64_t frame, uint64_t total, uint64_t &to)
{
    if (IsEmpty() || frame < m_nextCutStart)
        return false;

    to = GetNearestMark(m_nextCutStart, total, true);
    VERBOSE(VB_PLAYBACK, LOC + QString("Tracker wants to jump to: %1").arg(to));
    return true;
}

/**
 * \brief Returns the number of the last frame in the video that is not in a
 *        cut sequence.
 */
uint64_t DeleteMap::GetLastFrame(uint64_t total)
{
    uint64_t result = total;
    if (IsEmpty())
        return result;

    frm_dir_map_t::Iterator it = m_deleteMap.end();
    --it;

    if (it.value() == MARK_CUT_START)
        result = it.key();
    return result;
}

/**
 * \brief Compares the current cut list with the saved cut list
 */
bool DeleteMap::IsSaved(PlayerContext *ctx)
{
    if (!ctx || !ctx->playingInfo || gCoreContext->IsDatabaseIgnored())
        return true;

    frm_dir_map_t currentMap(m_deleteMap);
    frm_dir_map_t savedMap;
    ctx->LockPlayingInfo(__FILE__, __LINE__);
    ctx->playingInfo->QueryCutList(savedMap);
    ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    // Remove temporary placeholder marks from currentMap
    QMutableMapIterator<uint64_t, MarkTypes> it(currentMap);
    while (it.hasNext())
    {
        it.next();
        if (MARK_PLACEHOLDER == it.value())
            it.remove();
    }

    return currentMap == savedMap;
}

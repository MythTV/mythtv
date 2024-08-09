#include <algorithm>

#include "libmyth/mythcontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsocket.h"
#include "libmythbase/programinfo.h"

#include "livetvchain.h"
#include "cardutil.h"

#define LOC QString("LiveTVChain(%1): ").arg(m_id)

static inline void clear(LiveTVChainEntry &entry)
{
    entry.chanid = 0;
    entry.starttime.setSecsSinceEpoch(0);
    entry.endtime = QDateTime();
    entry.discontinuity = true;
    entry.hostprefix = QString();
    entry.inputtype = QString();
    entry.channum = QString();
    entry.inputname = QString();
}

/** \class LiveTVChain
 *  \brief Keeps track of recordings in a current LiveTV instance
 */
LiveTVChain::LiveTVChain() : ReferenceCounter("LiveTVChain")
{
    clear(m_switchEntry);
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "ctor");
}

LiveTVChain::~LiveTVChain()
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "dtor");
}

QString LiveTVChain::InitializeNewChain(const QString &seed)
{
    QDateTime curdt = MythDate::current();
    m_id = QString("live-%1-%2").arg(seed, curdt.toString(Qt::ISODate));
    return m_id;
}

void LiveTVChain::SetHostPrefix(const QString &prefix)
{
    m_hostPrefix = prefix;
}

void LiveTVChain::SetInputType(const QString &type)
{
    m_inputType = type;
}

void LiveTVChain::LoadFromExistingChain(const QString &id)
{
    m_id = id;
    ReloadAll();
}

void LiveTVChain::AppendNewProgram(ProgramInfo *pginfo, const QString& channum,
                                   const QString& inputname, bool discont)
{
    QMutexLocker lock(&m_lock);

    LiveTVChainEntry newent;
    newent.chanid = pginfo->GetChanID();
    newent.starttime = pginfo->GetRecordingStartTime();
    newent.endtime = pginfo->GetRecordingEndTime();
    newent.discontinuity = discont;
    newent.hostprefix = m_hostPrefix;
    newent.inputtype = m_inputType;
    newent.channum = channum;
    newent.inputname = inputname;

    m_chain.append(newent);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO tvchain (chanid, starttime, endtime, chainid,"
                  " chainpos, discontinuity, watching, hostprefix, cardtype, "
                  " channame, input) "
                  "VALUES(:CHANID, :START, :END, :CHAINID, :CHAINPOS, "
                  " :DISCONT, :WATCHING, :PREFIX, :INPUTTYPE, :CHANNAME, "
                  " :INPUT );");
    query.bindValue(":CHANID", pginfo->GetChanID());
    query.bindValue(":START", pginfo->GetRecordingStartTime());
    query.bindValue(":END", pginfo->GetRecordingEndTime());
    query.bindValueNoNull(":CHAINID", m_id);
    query.bindValue(":CHAINPOS", m_maxPos);
    query.bindValue(":DISCONT", discont);
    query.bindValue(":WATCHING", 0);
    query.bindValue(":PREFIX", m_hostPrefix);
    query.bindValue(":INPUTTYPE", m_inputType);
    query.bindValue(":CHANNAME", channum);
    query.bindValue(":INPUT", inputname);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Chain: AppendNewProgram", query);
    else
    {
        LOG(VB_RECORD, LOG_INFO, QString("Chain: Appended@%3 '%1_%2'")
            .arg(newent.chanid)
            .arg(MythDate::toString(newent.starttime, MythDate::kFilename))
            .arg(m_maxPos));
    }

    m_maxPos++;
    BroadcastUpdate();
}

void LiveTVChain::FinishedRecording(ProgramInfo *pginfo)
{
    QMutexLocker lock(&m_lock);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE tvchain SET endtime = :END "
                  "WHERE chanid = :CHANID AND starttime = :START ;");
    query.bindValue(":END", pginfo->GetRecordingEndTime());
    query.bindValue(":CHANID", pginfo->GetChanID());
    query.bindValue(":START", pginfo->GetRecordingStartTime());

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Chain: FinishedRecording", query);
    else
    {
        LOG(VB_RECORD, LOG_INFO,
            QString("Chain: Updated endtime for '%1_%2' to %3")
                .arg(QString::number(pginfo->GetChanID()),
                     pginfo->GetRecordingStartTime(MythDate::kFilename),
                     pginfo->GetRecordingEndTime(MythDate::kFilename)));
    }

    QList<LiveTVChainEntry>::iterator it;
    for (it = m_chain.begin(); it != m_chain.end(); ++it)
    {
        if ((*it).chanid    == pginfo->GetChanID() &&
            (*it).starttime == pginfo->GetRecordingStartTime())
        {
            (*it).endtime = pginfo->GetRecordingEndTime();
        }
    }
    BroadcastUpdate();
}

void LiveTVChain::DeleteProgram(ProgramInfo *pginfo)
{
    QMutexLocker lock(&m_lock);

    for (auto it = m_chain.begin(); it != m_chain.end(); ++it)
    {
        if ((*it).chanid    == pginfo->GetChanID() &&
            (*it).starttime == pginfo->GetRecordingStartTime())
        {
            auto del = it;
            ++it;

            MSqlQuery query(MSqlQuery::InitCon());
            if (it != m_chain.end())
            {
                (*it).discontinuity = true;
                query.prepare("UPDATE tvchain SET discontinuity = :DISCONT "
                              "WHERE chanid = :CHANID AND starttime = :START "
                              "AND chainid = :CHAINID ;");
                query.bindValue(":CHANID", (*it).chanid);
                query.bindValue(":START", (*it).starttime);
                query.bindValue(":CHAINID", m_id);
                query.bindValue(":DISCONT", (*it).discontinuity);
                if (!query.exec())
                    MythDB::DBError("LiveTVChain::DeleteProgram -- "
                                    "discontinuity", query);
            }

            query.prepare("DELETE FROM tvchain WHERE chanid = :CHANID "
                          "AND starttime = :START AND chainid = :CHAINID ;");
            query.bindValue(":CHANID", (*del).chanid);
            query.bindValue(":START", (*del).starttime);
            query.bindValue(":CHAINID", m_id);
            if (!query.exec())
                MythDB::DBError("LiveTVChain::DeleteProgram -- delete", query);

            m_chain.erase(del);

            BroadcastUpdate();
            break;
        }
    }
}

void LiveTVChain::BroadcastUpdate(void)
{
    QString message = QString("LIVETV_CHAIN UPDATE %1").arg(m_id);
    MythEvent me(message, entriesToStringList());
    gCoreContext->dispatch(me);
}

void LiveTVChain::DestroyChain(void)
{
    QMutexLocker lock(&m_lock);

    m_chain.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM tvchain WHERE chainid = :CHAINID ;");
    query.bindValue(":CHAINID", m_id);

    if (!query.exec())
        MythDB::DBError("LiveTVChain::DestroyChain", query);
}

void LiveTVChain::ReloadAll(const QStringList &data)
{
    QMutexLocker lock(&m_lock);

    int prev_size = m_chain.size();
    if (data.isEmpty() || !entriesFromStringList(data))
    {
        m_chain.clear();

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT chanid, starttime, endtime, discontinuity, "
                      "chainpos, hostprefix, cardtype, channame, input "
                      "FROM tvchain "
                      "WHERE chainid = :CHAINID ORDER BY chainpos;");
        query.bindValue(":CHAINID", m_id);

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while (query.next())
            {
                LiveTVChainEntry entry;
                entry.chanid = query.value(0).toUInt();
                entry.starttime =
                    MythDate::as_utc(query.value(1).toDateTime());
                entry.endtime =
                    MythDate::as_utc(query.value(2).toDateTime());
                entry.discontinuity = query.value(3).toBool();
                entry.hostprefix = query.value(5).toString();
                entry.inputtype = query.value(6).toString();
                entry.channum = query.value(7).toString();
                entry.inputname = query.value(8).toString();

                m_maxPos = query.value(4).toInt() + 1;

                m_chain.append(entry);
            }
        }
    }

    m_curPos = ProgramIsAt(m_curChanId, m_curStartTs);
    m_curPos = std::max(m_curPos, 0);

    if (m_switchId >= 0)
        m_switchId = ProgramIsAt(m_switchEntry.chanid,m_switchEntry.starttime);

    if (prev_size > m_chain.size())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("ReloadAll(): Removed %1 recording(s)")
            .arg(prev_size - m_chain.size()));
        LOG(VB_PLAYBACK, LOG_INFO, LOC + toString());
    }
    else if (prev_size < m_chain.size())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("ReloadAll(): Added %1 recording(s)")
            .arg(m_chain.size() - prev_size));
        LOG(VB_PLAYBACK, LOG_INFO, LOC + toString());
    }
}

void LiveTVChain::GetEntryAt(int at, LiveTVChainEntry &entry) const
{
    QMutexLocker lock(&m_lock);

    int size = m_chain.count();
    int new_at = (size && (at < 0 || at >= size)) ? size - 1 : at;

    if (size && new_at >= 0 && new_at < size)
        entry = m_chain[new_at];
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("GetEntryAt(%1) failed.").arg(at));
        if (at == -1)
        {
            LOG(VB_GENERAL, LOG_ERR, "It appears that your backend may "
                "be misconfigured.  Check your backend logs to determine "
                "whether your inputs, lineups, channels, or storage "
                "configuration are reporting errors.  This issue is commonly "
                "caused by failing to complete all setup steps properly.  You "
                "may wish to review the documentation for mythtv-setup.");
        }
        clear(entry);
    }
}

ProgramInfo *LiveTVChain::EntryToProgram(const LiveTVChainEntry &entry)
{
    auto *pginfo = new ProgramInfo(entry.chanid, entry.starttime);

    if (pginfo->GetChanID())
    {
        pginfo->SetPathname(entry.hostprefix + pginfo->GetBasename());
        return pginfo;
    }

    LOG(VB_GENERAL, LOG_ERR,
        QString("EntryToProgram(%1@%2) failed to get pginfo")
        .arg(entry.chanid).arg(entry.starttime.toString(Qt::ISODate)));
    delete pginfo;
    return nullptr;
}

/** \fn LiveTVChain::GetProgramAt(int) const
 *  \brief Returns program at the desired location.
 *
 *   NOTE: The caller must delete the returned program.
 *
 *  \param at  ProgramInfo to return [0..TotalSize()-1] or -1 for last program
 */
ProgramInfo *LiveTVChain::GetProgramAt(int at) const
{
    LiveTVChainEntry entry;
    GetEntryAt(at, entry);

    return EntryToProgram(entry);
}

/** \fn LiveTVChain::ProgramIsAt(uint, const QDateTime&) const
 *  \returns program location or -1 for not found.
 */
int LiveTVChain::ProgramIsAt(uint chanid, const QDateTime &starttime) const
{
    QMutexLocker lock(&m_lock);

    int count = 0;
    QList<LiveTVChainEntry>::const_iterator it;
    for (it = m_chain.begin(); it != m_chain.end(); ++it, ++count)
    {
        if ((*it).chanid == chanid &&
            (*it).starttime == starttime)
        {
            return count;
        }
    }

    return -1;
}

/** \fn LiveTVChain::ProgramIsAt(const ProgramInfo&) const
 *  \returns program location or -1 for not found.
 */
int LiveTVChain::ProgramIsAt(const ProgramInfo &pginfo) const
{
    return ProgramIsAt(pginfo.GetChanID(), pginfo.GetRecordingStartTime());
}

/** \fn LiveTVChain::GetLengthAtCurPos(void)
 *  \returns length in seconds of recording at current position
 */
std::chrono::seconds LiveTVChain::GetLengthAtCurPos(void)
{
    return GetLengthAtPos(m_curPos);
}

/**
 *  \returns length in seconds of recording at m_curPos
 */
std::chrono::seconds LiveTVChain::GetLengthAtPos(int pos)
{
    QMutexLocker lock(&m_lock);

    LiveTVChainEntry entry = m_chain[pos];
    if (pos == (m_chain.count() - 1))
    {
        // We're on live program, it hasn't ended. Use current time as end time
        return MythDate::secsInPast(entry.starttime);
    }

    // use begin time from the following program, as it's certain to be right
    // the end time is set as per the EPG, but should playback be interrupted
    // such as a channel change, the end value wouldn't have reflected the actual
    // duration of the program
    LiveTVChainEntry nextentry = m_chain[pos+1];
    return std::chrono::seconds(entry.starttime.secsTo(nextentry.starttime));
}

int LiveTVChain::TotalSize(void) const
{
    return m_chain.count();
}

void LiveTVChain::SetProgram(const ProgramInfo &pginfo)
{
    QMutexLocker lock(&m_lock);

    m_curChanId  = pginfo.GetChanID();
    m_curStartTs = pginfo.GetRecordingStartTime();

    m_curPos = ProgramIsAt(pginfo);
    m_curPos = std::max(m_curPos, 0);
    m_switchId = -1;
}

bool LiveTVChain::HasNext(void) const
{
    return (m_chain.count() - 1 > m_curPos);
}

void LiveTVChain::ClearSwitch(void)
{
    QMutexLocker lock(&m_lock);

    m_switchId = -1;
    m_jumpPos = std::chrono::seconds::max();
}

/**
 *  \brief Returns the recording we should switch to
 *
 *   This returns a ProgramInfo* and tells us if this is a discontiuous
 *   switch and whether the recording type is changing.
 *
 *   This also clears the NeedsToSwitch()/NeedsToJump() state.
 *
 *   NOTE: The caller is responsible for deleting the ProgramInfo
 */
ProgramInfo *LiveTVChain::GetSwitchProgram(bool &discont, bool &newtype,
                                           int &newid)
{
    ReloadAll();
    QMutexLocker lock(&m_lock);

    int id = m_switchId;
    ProgramInfo *pginfo = DoGetNextProgram(m_switchId >= m_curPos, m_curPos, id,
                                           discont, newtype);
    if (pginfo)
    {
        newid = id;
    }
    ClearSwitch();

    return pginfo;
}

ProgramInfo *LiveTVChain::DoGetNextProgram(bool up, int curpos, int &newid,
                                           bool &discont, bool &newtype)
{
    LiveTVChainEntry oldentry;
    LiveTVChainEntry entry;
    ProgramInfo *pginfo = nullptr;

    GetEntryAt(curpos, oldentry);

    if (newid < 0 || curpos == newid)
    {
        // already on the program
        entry = oldentry;
        pginfo = EntryToProgram(entry);
        newid = curpos;
    }
    else
    {
        // try to find recordings during first pass
        // we'll skip dummy and empty recordings
        while (!pginfo && newid < m_chain.count() && newid >= 0)
        {
            GetEntryAt(newid, entry);

            bool at_last_entry =
                ((newid > curpos) &&
                 (newid == m_chain.count()-1)) ||
                ((newid <= curpos) && (newid == 0));

            // Skip dummy recordings, if possible.
            if (at_last_entry || (entry.inputtype != "DUMMY"))
                pginfo = EntryToProgram(entry);

            // Skip empty recordings, if possible
            if (pginfo && (0 == pginfo->GetFilesize()) &&
                newid < m_chain.count()-1)
            {
                LOG(VB_GENERAL, LOG_WARNING,
                    QString("Skipping empty program %1")
                    .arg(pginfo->MakeUniqueKey()));
                delete pginfo;
                pginfo = nullptr;
            }

            if (!pginfo)
            {
                newid += up ? 1 : -1;
            }
        }

        if (!pginfo)
        {
            // didn't find in first pass, now get back to the next good one
            // as this is the one we will use
            do
            {
                newid += up ? -1 : 1;

                GetEntryAt(newid, entry);

                bool at_last_entry =
                    ((newid > curpos) &&
                     (newid == m_chain.count()-1)) ||
                    ((newid <= curpos) && (newid == 0));

                // Skip dummy recordings, if possible.
                if (at_last_entry || (entry.inputtype != "DUMMY"))
                    pginfo = EntryToProgram(entry);

                // Skip empty recordings, if possible
                if (pginfo && (0 == pginfo->GetFilesize()) &&
                    newid < m_chain.count()-1)
                {
                    LOG(VB_GENERAL, LOG_WARNING,
                        QString("Skipping empty program %1")
                        .arg(pginfo->MakeUniqueKey()));
                    delete pginfo;
                    pginfo = nullptr;
                }
            }
            while (!pginfo && newid < m_chain.count() && newid >= 0);

            if (!pginfo)
            {
                // still not found so abort (will never happen once playback has started)
                return nullptr;
            }
        }
    }

    discont = true;
    if (curpos == newid - 1)
        discont = entry.discontinuity;

    newtype = (oldentry.inputtype != entry.inputtype);

    // Some inputs can change their streams dramatically on a channel change...
    if (discont)
        newtype |= CardUtil::IsChannelChangeDiscontinuous(entry.inputtype);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("DoGetNextProgram: %1 -> ").arg(newid) + pginfo->toString());

    return pginfo;
}

/** \fn LiveTVChain::SwitchTo(int)
 *  \brief Sets the recording to switch to.
 *  \param num Index of recording to switch to, -1 for last recording.
 */
void LiveTVChain::SwitchTo(int num)
{
    QMutexLocker lock(&m_lock);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SwitchTo(%1)").arg(num));

    int size = m_chain.count();
    if ((num < 0) || (num >= size))
        num = size - 1;

    if (m_curPos != num)
    {
        m_switchId = num;
        GetEntryAt(num, m_switchEntry);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "SwitchTo() not switching to current");
    }

    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
    {
        LiveTVChainEntry e;
        GetEntryAt(num, e);
        QString msg = QString("%1_%2")
            .arg(e.chanid)
            .arg(MythDate::toString(e.starttime, MythDate::kFilename));
        LOG(VB_PLAYBACK, LOG_DEBUG,
            LOC + QString("Entry@%1: '%2')").arg(num).arg(msg));
    }
}

/** \fn LiveTVChain::SwitchToNext(bool)
 *  \brief Sets the recording to switch to.
 *  \param up Set to true to switch to the next recording,
 *            false to switch to the previous recording.
 */
void LiveTVChain::SwitchToNext(bool up)
{
#if 0
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "SwitchToNext("<<(up?"up":"down")<<")");
#endif
    if (up && HasNext())
        SwitchTo(m_curPos + 1);
    else if (!up && HasPrev())
        SwitchTo(m_curPos - 1);
}

void LiveTVChain::JumpTo(int num, std::chrono::seconds pos)
{
    m_jumpPos = pos;
    SwitchTo(num);
}

/**
 * jump to the next (up == true) or previous (up == false) liveTV program
 * If pos > 0: indicate the absolute position where to start the next program
 * If pos < 0: indicate offset position; in which case the right liveTV program
 * will be found accordingly.
 * Offset is in reference to the beginning of the current recordings when going down
 * and in reference to the end of the current recording when going up
 */
void LiveTVChain::JumpToNext(bool up, std::chrono::seconds pos)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("JumpToNext: %1 -> %2").arg(up).arg(pos.count()));
    if (pos >= 0s)
    {
        m_jumpPos = pos;
        SwitchToNext(up);
    }
    else
    {
        QMutexLocker lock(&m_lock);

        int current = m_curPos;
        int switchto = m_curPos;
        bool discont = false;
        bool newtype = false;

        while (current >= 0 && current < m_chain.size())
        {
            switchto = current + (up ? 1 : -1);

            ProgramInfo *pginfo = DoGetNextProgram(up, current, switchto,
                                                   discont, newtype);
            delete pginfo;

            if (switchto == current)
            {
                // we've reached the end
                pos = up ? GetLengthAtPos(switchto) : 0s;
                break;
            }

            std::chrono::seconds duration = GetLengthAtPos(switchto);

            pos += duration;

            if (pos >= 0s)
            {
                if (up)
                {
                    pos = - (pos - duration);
                }
                break;
            }

            current = switchto;
        }
        m_switchId = switchto;
        m_jumpPos = pos;
        GetEntryAt(m_switchId, m_switchEntry);
    }
}

/**
 *  \brief Returns the jump position in seconds and clears it.
 */
std::chrono::seconds LiveTVChain::GetJumpPos(void)
{
    std::chrono::seconds ret = m_jumpPos;
    m_jumpPos = 0s;
    return ret;
}

QString LiveTVChain::GetChannelName(int pos) const
{
    LiveTVChainEntry entry;
    GetEntryAt(pos, entry);

    return entry.channum;
}

QString LiveTVChain::GetInputName(int pos) const
{
    LiveTVChainEntry entry;
    GetEntryAt(pos, entry);

    return entry.inputname;
}

QString LiveTVChain::GetInputType(int pos) const
{
    LiveTVChainEntry entry;
    GetEntryAt(pos, entry);

    return entry.inputtype;
}

void LiveTVChain::SetHostSocket(MythSocket *sock)
{
    QMutexLocker lock(&m_sockLock);

    if (!m_inUseSocks.contains(sock))
        m_inUseSocks.append(sock);
}

bool LiveTVChain::IsHostSocket(MythSocket *sock)
{
    QMutexLocker lock(&m_sockLock);
    return m_inUseSocks.contains(sock);
}

uint LiveTVChain::HostSocketCount(void) const
{
    QMutexLocker lock(&m_sockLock);
    return m_inUseSocks.count();
}

void LiveTVChain::DelHostSocket(MythSocket *sock)
{
    QMutexLocker lock(&m_sockLock);
    m_inUseSocks.removeAll(sock);
}

static QString toString(const LiveTVChainEntry &v)
{
    return QString("%1: %2 (%3 to %4)%5")
        .arg(v.inputtype,6).arg(v.chanid,4)
        .arg(v.starttime.time().toString(),
             v.endtime.time().toString(),
             v.discontinuity?" discontinuous":"");
}

QString LiveTVChain::toString() const
{
    QMutexLocker lock(&m_lock);
    QString ret = QString("LiveTVChain has %1 entries\n").arg(m_chain.size());
    for (uint i = 0; i < (uint)m_chain.size(); i++)
    {
        ret += (QString((i==(uint)m_curPos) ? "* " : "  ") +
                ::toString(m_chain[i]) + "\n");
    }
    return ret;
}

QStringList LiveTVChain::entriesToStringList() const
{
    QMutexLocker lock(&m_lock);
    QStringList ret;
    ret << QString::number(m_maxPos);
    for (const auto & entry : std::as_const(m_chain))
    {
        ret << QString::number(entry.chanid);
        ret << entry.starttime.toString(Qt::ISODate);
        ret << entry.endtime.toString(Qt::ISODate);
        ret << QString::number(static_cast<int>(entry.discontinuity));
        ret << entry.hostprefix;
        ret << entry.inputtype;
        ret << entry.channum;
        ret << entry.inputname;
    }
    return ret;
}

bool LiveTVChain::entriesFromStringList(const QStringList &items)
{
    int numItems = items.size();
    QList<LiveTVChainEntry> chain;
    int itemIdx = 0;
    int maxpos = 0;
    bool ok = false;
    if (itemIdx < numItems)
        maxpos = items[itemIdx++].toInt(&ok);
    while (ok && itemIdx < numItems)
    {
        LiveTVChainEntry entry;
        entry.chanid = items[itemIdx++].toUInt(&ok);
        if (ok && itemIdx < numItems)
        {
            entry.starttime =
                QDateTime::fromString(items[itemIdx++], Qt::ISODate);
            ok = entry.starttime.isValid();
        }
        if (ok && itemIdx < numItems)
        {
            entry.endtime =
                QDateTime::fromString(items[itemIdx++], Qt::ISODate);
            ok = entry.endtime.isValid();
        }
        if (ok && itemIdx < numItems)
            entry.discontinuity = (items[itemIdx++].toInt(&ok) != 0);
        if (ok && itemIdx < numItems)
            entry.hostprefix = items[itemIdx++];
        if (ok && itemIdx < numItems)
            entry.inputtype = items[itemIdx++];
        if (ok && itemIdx < numItems)
            entry.channum = items[itemIdx++];
        if (ok && itemIdx < numItems)
            entry.inputname = items[itemIdx++];
        if (ok)
            chain.append(entry);
    }
    if (ok)
    {
        QMutexLocker lock(&m_lock);
        m_maxPos = maxpos;
        m_chain = chain;
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Failed to deserialize TVChain - ") + items.join("|"));
    }
    return ok;
}

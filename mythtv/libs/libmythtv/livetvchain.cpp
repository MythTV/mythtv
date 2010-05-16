#include "livetvchain.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "programinfo.h"
#include "mythsocket.h"
#include "cardutil.h"

#define LOC QString("LiveTVChain(%1): ").arg(m_id)

/** \class LiveTVChain
 *  \brief Keeps track of recordings in a current LiveTV instance
 */
LiveTVChain::LiveTVChain() :
    m_id(""), m_maxpos(0), m_lock(QMutex::Recursive),
    m_curpos(0), m_cur_chanid(0),
    m_switchid(-1), m_jumppos(0)
{
}

LiveTVChain::~LiveTVChain()
{
}

QString LiveTVChain::InitializeNewChain(const QString &seed)
{
    QDateTime curdt = QDateTime::currentDateTime();
    m_id = QString("live-%1-%2").arg(seed).arg(curdt.toString(Qt::ISODate));
    return m_id;
}

void LiveTVChain::SetHostPrefix(const QString &prefix)
{
    m_hostprefix = prefix;
}

void LiveTVChain::SetCardType(const QString &type)
{
    m_cardtype = type;
}

void LiveTVChain::LoadFromExistingChain(const QString &id)
{
    m_id = id;
    ReloadAll();
}

void LiveTVChain::AppendNewProgram(ProgramInfo *pginfo, QString channum,
                                   QString inputname, bool discont)
{
    QMutexLocker lock(&m_lock);

    QTime tmptime = pginfo->GetRecordingStartTime().time();

    LiveTVChainEntry newent;
    newent.chanid = pginfo->GetChanID();
    newent.starttime = pginfo->GetRecordingStartTime();
    newent.starttime.setTime(QTime(tmptime.hour(), tmptime.minute(),
                                   tmptime.second()));
    newent.discontinuity = discont;
    newent.hostprefix = m_hostprefix;
    newent.cardtype = m_cardtype;
    newent.channum = channum;
    newent.inputname = inputname;

    m_chain.append(newent);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO tvchain (chanid, starttime, endtime, chainid,"
                  " chainpos, discontinuity, watching, hostprefix, cardtype, "
                  " channame, input) "
                  "VALUES(:CHANID, :START, :END, :CHAINID, :CHAINPOS, "
                  " :DISCONT, :WATCHING, :PREFIX, :CARDTYPE, :CHANNAME, "
                  " :INPUT );");
    query.bindValue(":CHANID", pginfo->GetChanID());
    query.bindValue(":START", pginfo->GetRecordingStartTime());
    query.bindValue(":END", pginfo->GetRecordingEndTime());
    query.bindValue(":CHAINID", m_id);
    query.bindValue(":CHAINPOS", m_maxpos);
    query.bindValue(":DISCONT", discont);
    query.bindValue(":WATCHING", 0);
    query.bindValue(":PREFIX", m_hostprefix);
    query.bindValue(":CARDTYPE", m_cardtype);
    query.bindValue(":CHANNAME", channum);
    query.bindValue(":INPUT", inputname);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Chain: AppendNewProgram", query);
    else
        VERBOSE(VB_RECORD, QString("Chain: Appended@%3 '%1_%2'")
                .arg(newent.chanid)
                .arg(newent.starttime.toString("yyyyMMddhhmmss"))
                .arg(m_maxpos));

    m_maxpos++;
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
        VERBOSE(VB_RECORD, QString("Chain: Updated endtime for '%1_%2' to %3")
                .arg(pginfo->GetChanID())
                .arg(pginfo->GetRecordingStartTime(MythDate))
                .arg(pginfo->GetRecordingEndTime(MythDate)));

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

    QList<LiveTVChainEntry>::iterator it, del;
    for (it = m_chain.begin(); it != m_chain.end(); ++it)
    {
        if ((*it).chanid    == pginfo->GetChanID() &&
            (*it).starttime == pginfo->GetRecordingStartTime())
        {
            del = it;
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
                query.bindValue(":DISCONT", true);
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
    MythEvent me(message);
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

void LiveTVChain::ReloadAll(void)
{
    QMutexLocker lock(&m_lock);

    int prev_size = m_chain.size();
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
            entry.starttime = query.value(1).toDateTime();
            entry.endtime = query.value(2).toDateTime();
            entry.discontinuity = query.value(3).toInt();
            entry.hostprefix = query.value(5).toString();
            entry.cardtype = query.value(6).toString();
            entry.channum = query.value(7).toString();
            entry.inputname = query.value(8).toString();

            m_maxpos = query.value(4).toInt() + 1;

            m_chain.append(entry);
        }
    }

    m_curpos = ProgramIsAt(m_cur_chanid, m_cur_startts);
    if (m_curpos < 0)
        m_curpos = 0;

    if (m_switchid >= 0)
        m_switchid = ProgramIsAt(m_switchentry.chanid,m_switchentry.starttime);

    if (prev_size!=m_chain.size())
    {
        VERBOSE(VB_PLAYBACK, LOC +
                "ReloadAll(): Added new recording");
    }
}

void LiveTVChain::GetEntryAt(int at, LiveTVChainEntry &entry) const
{
    QMutexLocker lock(&m_lock);

    int size = m_chain.count();
    int new_at = (at < 0 || at >= size) ? size - 1 : at;

    if (new_at >= 0 && new_at <= size)
        entry = m_chain[new_at];
    else
    {
        VERBOSE(VB_IMPORTANT, QString("GetEntryAt(%1) failed.").arg(at));
        entry.chanid = 0;
        entry.starttime.setTime_t(0);
    }
}

ProgramInfo *LiveTVChain::EntryToProgram(const LiveTVChainEntry &entry)
{
    ProgramInfo *pginfo = new ProgramInfo(entry.chanid, entry.starttime);

    if (pginfo->GetChanID())
    {
        pginfo->SetPathname(entry.hostprefix + pginfo->GetBasename());
        return pginfo;
    }

    VERBOSE(VB_IMPORTANT,
            QString("EntryToProgram(%1@%2) failed to get pginfo")
            .arg(entry.chanid).arg(entry.starttime.toString()));
    delete pginfo;
    return NULL;
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
 *  \returns length in seocnds of recording at m_curpos
 */
int LiveTVChain::GetLengthAtCurPos(void)
{
    QMutexLocker lock(&m_lock);
    LiveTVChainEntry entry;

    entry = m_chain[m_curpos];
    if (m_curpos == ((int)m_chain.count() - 1))
        return entry.starttime.secsTo(QDateTime::currentDateTime());
    else
        return entry.starttime.secsTo(entry.endtime);
}

int LiveTVChain::TotalSize(void) const
{
    return m_chain.count();
}

void LiveTVChain::SetProgram(const ProgramInfo &pginfo)
{
    QMutexLocker lock(&m_lock);

    m_cur_chanid  = pginfo.GetChanID();
    m_cur_startts = pginfo.GetRecordingStartTime();

    m_curpos = ProgramIsAt(pginfo);
    m_switchid = -1;
}

bool LiveTVChain::HasNext(void) const
{
    return ((int)m_chain.count() - 1 > m_curpos);
}

void LiveTVChain::ClearSwitch(void)
{
    QMutexLocker lock(&m_lock);

    m_switchid = -1;
    m_jumppos = 0;
}

/**
 *  \brief Returns the recording we should switch to
 *
 *   This returns a ProgramInfo* and tells us if this is a discontiuous
 *   switch and whether the recording type is changing.
 *
 *   This also clears the NeedsToSwitch()/NeedsToJump() state.
 *
 *   NOTE: The caller is resposible for deleting the ProgramInfo
 */
ProgramInfo *LiveTVChain::GetSwitchProgram(bool &discont, bool &newtype,
                                           int &newid)
{
    QMutexLocker lock(&m_lock);

    if (m_switchid < 0 || m_curpos == m_switchid)
    {
        ClearSwitch();
        return NULL;
    }

    LiveTVChainEntry oldentry, entry;
    GetEntryAt(m_curpos, oldentry);

    ProgramInfo *pginfo = NULL;
    while (!pginfo && m_switchid < (int)m_chain.count() && m_switchid >= 0)
    {
        GetEntryAt(m_switchid, entry);
        pginfo = EntryToProgram(entry);

        if (!pginfo)
        {
            if (m_switchid > m_curpos)
                m_switchid++;
            else
                m_switchid--;
        }
    }

    if (!pginfo)
    {
        ClearSwitch();
        return NULL;
    }

    // Skip dummy recordings, if possible.
    if (entry.cardtype == "DUMMY")
    {
        if (m_switchid > m_curpos && m_switchid + 1 < (int)m_chain.count())
            m_switchid++;
        else if (m_switchid < m_curpos && m_switchid > 0)
            m_switchid--;

        GetEntryAt(m_switchid, entry);
        delete pginfo;
        pginfo = EntryToProgram(entry);
        if (!pginfo)
        {
            ClearSwitch();
            return NULL;
        }
    }

    discont = true;
    if (m_curpos == m_switchid - 1)
        discont = entry.discontinuity;

    newtype = (oldentry.cardtype != entry.cardtype);

    // Some cards can change their streams dramatically on a channel change...
    if (discont)
        newtype |= CardUtil::IsChannelChangeDiscontinuous(entry.cardtype);

    newid = m_switchid;

    ClearSwitch();

    return pginfo;
}

/** \fn LiveTVChain::SwitchTo(int)
 *  \brief Sets the recording to switch to.
 *  \param num Index of recording to switch to, -1 for last recording.
 */
void LiveTVChain::SwitchTo(int num)
{
    QMutexLocker lock(&m_lock);

    VERBOSE(VB_PLAYBACK, LOC + "SwitchTo("<<num<<")");

    int size = m_chain.count();
    if ((num < 0) || (num >= size))
        num = size - 1;

    if (m_curpos != num)
    {
        m_switchid = num;
        GetEntryAt(num, m_switchentry);
    }
    else
        VERBOSE(VB_IMPORTANT, LOC + "SwitchTo() not switching to current");

    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK))
    {
        LiveTVChainEntry e;
        GetEntryAt(num, e);
        QString msg = QString("%1_%2")
            .arg(e.chanid).arg(e.starttime.toString("yyyyMMddhhmmss"));
        VERBOSE(VB_PLAYBACK, LOC + "Entry@"<<num<<": '"<<msg<<"'");
    }
}

/** \fn LiveTVChain::SwitchToNext(bool)
 *  \brief Sets the recording to switch to.
 *  \param up Set to true to switch to the next recording,
 *            false to switch to the previous recording.
 */
void LiveTVChain::SwitchToNext(bool up)
{
    //VERBOSE(VB_PLAYBACK, LOC + "SwitchToNext("<<(up?"up":"down")<<")");
    if (up && HasNext())
        SwitchTo(m_curpos + 1);
    else if (!up && HasPrev())
        SwitchTo(m_curpos - 1);
}

void LiveTVChain::JumpTo(int num, int pos)
{
    m_jumppos = pos;
    SwitchTo(num);
}

void LiveTVChain::JumpToNext(bool up, int pos)
{
    m_jumppos = pos;
    SwitchToNext(up);
}

/** \fn LiveTVChain::GetJumpPos(void)
 *  \brief Returns the jump position and clears it.
 */
int LiveTVChain::GetJumpPos(void)
{
    int ret = m_jumppos;
    m_jumppos = 0;
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

QString LiveTVChain::GetCardType(int pos) const
{
    LiveTVChainEntry entry;
    GetEntryAt(pos, entry);

    return entry.cardtype;
}

void LiveTVChain::SetHostSocket(MythSocket *sock)
{
    QMutexLocker lock(&m_sockLock);

    if (!m_inUseSocks.contains(sock))
        m_inUseSocks.append(sock);
}

bool LiveTVChain::IsHostSocket(const MythSocket *sock) const
{
    QMutexLocker lock(&m_sockLock);
    return m_inUseSocks.contains(const_cast<MythSocket*>(sock));
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


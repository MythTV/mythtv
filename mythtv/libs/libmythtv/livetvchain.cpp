#include "livetvchain.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "programinfo.h"
#include "mythsocket.h"
#include "cardutil.h"

#define LOC QString("LiveTVChain(%1): ").arg(m_id)

static inline void clear(LiveTVChainEntry &entry)
{
    entry.chanid = 0;
    entry.starttime.setTime_t(0);
    entry.endtime = QDateTime();
    entry.discontinuity = true;
    entry.hostprefix = QString();
    entry.cardtype = QString();
    entry.channum = QString();
    entry.inputname = QString();
}

/** \class LiveTVChain
 *  \brief Keeps track of recordings in a current LiveTV instance
 */
LiveTVChain::LiveTVChain() :
    m_id(""), m_maxpos(0), m_lock(QMutex::Recursive),
    m_curpos(0), m_cur_chanid(0),
    m_switchid(-1), m_jumppos(0)
{
    clear(m_switchentry);
}

LiveTVChain::~LiveTVChain()
{
}

QString LiveTVChain::InitializeNewChain(const QString &seed)
{
    QDateTime curdt = MythDate::current();
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

    LiveTVChainEntry newent;
    newent.chanid = pginfo->GetChanID();
    newent.starttime = pginfo->GetRecordingStartTime();
    newent.endtime = pginfo->GetRecordingEndTime();
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
        LOG(VB_RECORD, LOG_INFO, QString("Chain: Appended@%3 '%1_%2'")
            .arg(newent.chanid)
            .arg(MythDate::toString(newent.starttime, MythDate::kFilename))
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
        LOG(VB_RECORD, LOG_INFO,
            QString("Chain: Updated endtime for '%1_%2' to %3")
                .arg(pginfo->GetChanID())
                .arg(pginfo->GetRecordingStartTime(MythDate::kFilename))
                .arg(pginfo->GetRecordingEndTime(MythDate::kFilename)));

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
                entry.discontinuity = query.value(3).toInt();
                entry.hostprefix = query.value(5).toString();
                entry.cardtype = query.value(6).toString();
                entry.channum = query.value(7).toString();
                entry.inputname = query.value(8).toString();

                m_maxpos = query.value(4).toInt() + 1;

                m_chain.append(entry);
            }
        }
    }

    m_curpos = ProgramIsAt(m_cur_chanid, m_cur_startts);
    if (m_curpos < 0)
        m_curpos = 0;

    if (m_switchid >= 0)
        m_switchid = ProgramIsAt(m_switchentry.chanid,m_switchentry.starttime);

    if (prev_size!=m_chain.size())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "ReloadAll(): Added new recording");
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
            LOG(VB_GENERAL, LOG_ERR, "It appears that your backend may "
                "be misconfigured.  Check your backend logs to determine "
                "whether your capture cards, lineups, channels, or storage "
                "configuration are reporting errors.  This issue is commonly "
                "caused by failing to complete all setup steps properly.  You "
                "may wish to review the documentation for mythtv-setup.");
        clear(entry);
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

    LOG(VB_GENERAL, LOG_ERR,
        QString("EntryToProgram(%1@%2) failed to get pginfo")
        .arg(entry.chanid).arg(entry.starttime.toString(Qt::ISODate)));
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
        return entry.starttime.secsTo(MythDate::current());
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
    ReloadAll();
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

        bool at_last_entry = 
            ((m_switchid > m_curpos) &&
             (m_switchid == (int)(m_chain.count()-1))) ||
            ((m_switchid <= m_curpos) && (m_switchid == 0));

        // Skip dummy recordings, if possible.
        if (at_last_entry || (entry.cardtype != "DUMMY"))
            pginfo = EntryToProgram(entry);

        // Skip empty recordings, if possible
        if (pginfo && (0 == pginfo->GetFilesize()) &&
            m_switchid < (int)(m_chain.count()-1))
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("Skipping empty program %1")
                .arg(pginfo->MakeUniqueKey()));
            delete pginfo;
            pginfo = NULL;
        }

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

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SwitchTo(%1)").arg(num));

    int size = m_chain.count();
    if ((num < 0) || (num >= size))
        num = size - 1;

    if (m_curpos != num)
    {
        m_switchid = num;
        GetEntryAt(num, m_switchentry);
    }
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + "SwitchTo() not switching to current");

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

static QString toString(const LiveTVChainEntry &v)
{
    return QString("%1: %2 (%3 to %4)%5")
        .arg(v.cardtype,6).arg(v.chanid,4)
        .arg(v.starttime.time().toString())
        .arg(v.endtime.time().toString())
        .arg(v.discontinuity?" discontinuous":"");
}

QString LiveTVChain::toString() const
{
    QMutexLocker lock(&m_lock);
    QString ret = QString("LiveTVChain has %1 entries\n").arg(m_chain.size());
    for (uint i = 0; i < (uint)m_chain.size(); i++)
    {
        ret += (QString((i==(uint)m_curpos) ? "* " : "  ") +
                ::toString(m_chain[i]) + "\n");
    }
    return ret;
}

QStringList LiveTVChain::entriesToStringList() const
{
    QMutexLocker lock(&m_lock);
    QStringList ret;
    ret << QString::number(m_maxpos);
    for (int i = 0; i < m_chain.size(); i++)
    {
        ret << QString::number(m_chain[i].chanid);
        ret << m_chain[i].starttime.toString(Qt::ISODate);
        ret << m_chain[i].endtime.toString(Qt::ISODate);
        ret << QString::number(m_chain[i].discontinuity);
        ret << m_chain[i].hostprefix;
        ret << m_chain[i].cardtype;
        ret << m_chain[i].channum;
        ret << m_chain[i].inputname;
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
        if (ok && itemIdx < numItems)
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
            entry.discontinuity = items[itemIdx++].toInt(&ok);
        if (ok && itemIdx < numItems)
            entry.hostprefix = items[itemIdx++];
        if (ok && itemIdx < numItems)
            entry.cardtype = items[itemIdx++];
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
        m_maxpos = maxpos;
        m_chain = chain;
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Failed to deserialize TVChain - ") + items.join("|"));
    }
    return ok;
}

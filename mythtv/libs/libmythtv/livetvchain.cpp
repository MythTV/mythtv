#include "livetvchain.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "programinfo.h"

LiveTVChain::LiveTVChain()
           : m_id(""), m_maxpos(0), m_curpos(0), m_cur_chanid(""),
             m_switchid(-1)
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

QString LiveTVChain::GetID(void)
{
    return m_id;
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
    newent.chanid = pginfo->chanid;
    newent.starttime = pginfo->recstartts;
    newent.discontinuity = discont;
    newent.hostprefix = m_hostprefix;
    newent.cardtype = m_cardtype;
    newent.channum = channum;
    newent.inputname = inputname;

    m_chain.append(newent);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO tvchain (chanid, starttime, chainid,"
                  " chainpos, discontinuity, watching, hostprefix, cardtype, "
                  " channame, input) "
                  "VALUES(:CHANID, :START, :CHAINID, :CHAINPOS, :DISCONT, "
                  " :WATCHING, :PREFIX, :CARDTYPE, :CHANNAME, :INPUT );");
    query.bindValue(":CHANID", pginfo->chanid);
    query.bindValue(":START", pginfo->recstartts);
    query.bindValue(":CHAINID", m_id);
    query.bindValue(":CHAINPOS", m_maxpos);
    query.bindValue(":DISCONT", discont);
    query.bindValue(":WATCHING", 0);
    query.bindValue(":PREFIX", m_hostprefix);
    query.bindValue(":CARDTYPE", m_cardtype);
    query.bindValue(":CHANNAME", channum.utf8());
    query.bindValue(":INPUT", inputname.utf8());

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Chain: AppendNewProgram", query);

    m_maxpos++;
    BroadcastUpdate();
}

void LiveTVChain::DeleteProgram(ProgramInfo *pginfo)
{
    QMutexLocker lock(&m_lock);

    QValueList<LiveTVChainEntry>::iterator it, del;
    for (it = m_chain.begin(); it != m_chain.end(); ++it)
    {
        if ((*it).chanid == pginfo->chanid &&
            (*it).starttime == pginfo->recstartts)
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
                query.exec();
            }

            query.prepare("DELETE FROM tvchain WHERE chanid = :CHANID "
                          "AND starttime = :START AND chainid = :CHAINID ;");
            query.bindValue(":CHANID", (*del).chanid);
            query.bindValue(":START", (*del).starttime);
            query.bindValue(":CHAINID", m_id);
            query.exec();

            m_chain.remove(del);

            BroadcastUpdate();
            break;
        }
    }
}

void LiveTVChain::BroadcastUpdate(void)
{
    QString message = QString("LIVETV_CHAIN UPDATE %1").arg(m_id);
    MythEvent me(message);
    gContext->dispatch(me);
}

void LiveTVChain::DestroyChain(void)
{
    QMutexLocker lock(&m_lock);

    m_chain.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM tvchain WHERE chainid = :CHAINID ;");
    query.bindValue(":CHAINID", m_id);

    query.exec();
}

void LiveTVChain::ReloadAll(void)
{
    m_lock.lock();

    m_chain.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, starttime, discontinuity, chainpos, "
                  "hostprefix, cardtype, channame, input "
                  "FROM tvchain "
                  "WHERE chainid = :CHAINID ORDER BY chainpos;");
    query.bindValue(":CHAINID", m_id);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
        {

            LiveTVChainEntry entry;
            entry.chanid = query.value(0).toString();
            entry.starttime = query.value(1).toDateTime();
            entry.discontinuity = query.value(2).toInt();
            entry.hostprefix = query.value(4).toString();
            entry.cardtype = query.value(5).toString();
            entry.channum = QString::fromUtf8(query.value(6).toString());
            entry.inputname = QString::fromUtf8(query.value(7).toString());

            m_maxpos = query.value(3).toInt() + 1;

            m_chain.append(entry);
        }
    }

    m_lock.unlock();

    m_curpos = ProgramIsAt(m_cur_chanid, m_cur_startts);
}

void LiveTVChain::GetEntryAt(int at, LiveTVChainEntry &entry)
{
    QMutexLocker lock(&m_lock);

    int size = m_chain.count();
    int new_at = (at < 0 || at >= size) ? size - 1 : at;

    if (new_at >= 0 && new_at <= size)
        entry = m_chain[new_at];
    else
    {
        VERBOSE(VB_IMPORTANT, QString("GetEntryAt(%1) failed.").arg(at));
        entry.chanid = "0";
        entry.starttime.setTime_t(0);
    }
}

ProgramInfo *LiveTVChain::EntryToProgram(LiveTVChainEntry &entry)
{
    ProgramInfo *pginfo;
    pginfo = ProgramInfo::GetProgramFromRecorded(entry.chanid,
                                                 entry.starttime);
    if (pginfo)
        pginfo->pathname = entry.hostprefix + pginfo->pathname;
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("EntryToProgram(%1@%2) failed to get pginfo")
                .arg(entry.chanid).arg(entry.starttime.toString()));
    }

    return pginfo;
}

ProgramInfo *LiveTVChain::GetProgramAt(int at)
{
    LiveTVChainEntry entry;
    GetEntryAt(at, entry);

    return EntryToProgram(entry);
}

int LiveTVChain::ProgramIsAt(const QString &chanid, const QDateTime &starttime)
{
    QMutexLocker lock(&m_lock);

    int count = 0;
    QValueList<LiveTVChainEntry>::iterator it, del;
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

int LiveTVChain::GetCurPos(void)
{
    return m_curpos;
}

int LiveTVChain::ProgramIsAt(ProgramInfo *pginfo)
{
    return ProgramIsAt(pginfo->chanid, pginfo->recstartts);
}

int LiveTVChain::TotalSize(void)
{
    return m_chain.count();
}

void LiveTVChain::SetProgram(ProgramInfo *pginfo)
{
    if (!pginfo)
        return;

    m_cur_chanid = pginfo->chanid;
    m_cur_startts = pginfo->recstartts;

    m_curpos = ProgramIsAt(pginfo);
    m_switchid = -1;
}

bool LiveTVChain::HasNext(void)
{
    return ((int)m_chain.count() - 1 > m_curpos);
}

bool LiveTVChain::HasPrev(void)
{
    return (m_curpos > 0);
}

bool LiveTVChain::NeedsToSwitch(void)
{
    return (m_switchid >= 0);
}

ProgramInfo *LiveTVChain::GetSwitchProgram(bool &discont, bool &newtype)
{
    if (m_switchid < 0 || m_curpos == m_switchid)
        return NULL;

    LiveTVChainEntry oldentry, entry;
    GetEntryAt(m_curpos, oldentry);
    GetEntryAt(m_switchid, entry);

    ProgramInfo *pginfo = EntryToProgram(entry);

    discont = true;
    if (m_curpos == m_switchid - 1)
        discont = entry.discontinuity;

    newtype = (oldentry.cardtype !=  entry.cardtype);

    m_switchid = -1;

    return pginfo;
}
    
void LiveTVChain::SwitchTo(int num)
{
    int size = m_chain.count();
    if (num < 0 || num >= (int)m_chain.count())
        num = size - 1;

    if (m_curpos != num)
        m_switchid = num;
}

void LiveTVChain::SwitchToNext(bool up)
{
    if (up && HasNext())
        SwitchTo(m_curpos + 1);
    else if (!up && HasPrev())
        SwitchTo(m_curpos - 1);
}

QString LiveTVChain::GetChannelName(int pos)
{
    LiveTVChainEntry entry;
    GetEntryAt(pos, entry);

    return entry.channum;
}

QString LiveTVChain::GetInputName(int pos)
{
    LiveTVChainEntry entry;
    GetEntryAt(pos, entry);

    return entry.inputname;
}


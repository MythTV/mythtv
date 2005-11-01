#include "livetvchain.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "programinfo.h"

LiveTVChain::LiveTVChain()
           : m_id(""), m_maxpos(0)
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

void LiveTVChain::LoadFromExistingChain(const QString &id)
{
    m_id = id;
    ReloadAll();
}

void LiveTVChain::AppendNewProgram(ProgramInfo *pginfo, bool discont)
{
    QMutexLocker lock(&m_lock);

    LiveTVChainEntry newent;
    newent.chanid = pginfo->chanid;
    newent.starttime = pginfo->recstartts;
    newent.discontinuity = discont;

    m_chain.append(newent);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO livetvchain (chanid, starttime, chainid,"
                  " chainpos, discontinuity, watching) "
                  "VALUES(:CHANID, :START, :CHAINID, :CHAINPOS, :DISCONT, "
                  " :WATCHING);");
    query.bindValue(":CHANID", pginfo->chanid);
    query.bindValue(":START", pginfo->recstartts);
    query.bindValue(":CHAINID", m_id);
    query.bindValue(":CHAINPOS", m_maxpos);
    query.bindValue(":DISCONT", discont);
    query.bindValue(":WATCHING", 0);

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
                query.prepare("UPDATE livetvchain SET discontinuity = :DISCONT "
                              "WHERE chanid = :CHANID AND starttime = :START "
                              "AND chainid = :CHAINID ;");
                query.bindValue(":CHANID", (*it).chanid);
                query.bindValue(":START", (*it).starttime);
                query.bindValue(":CHAINID", m_id);
                query.bindValue(":DISCONT", true);
                query.exec();
            }

            query.prepare("DELETE FROM livetvchain WHERE chanid = :CHANID "
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
    query.prepare("DELETE FROM livetvchain WHERE chainid = :CHAINID ;");
    query.bindValue(":CHAINID", m_id);

    query.exec();
}

void LiveTVChain::ReloadAll(void)
{
    QMutexLocker lock(&m_lock);

    m_chain.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, starttime, discontinuity, chainpos "
                  "FROM livetvchain "
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

            m_maxpos = query.value(3).toInt() + 1;

            m_chain.append(entry);
        }
    }
}


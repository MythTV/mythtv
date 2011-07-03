#include <vector>

#include <QList>

#include "programinfo.h"
#include "mythlogging.h"
#include "jobqueue.h"

#include "lookup.h"

LookerUpper::LookerUpper() :
    m_busyRecList(QList<ProgramInfo*>())
{
    m_metadataFactory = new MetadataFactory(this);
}

LookerUpper::~LookerUpper()
{
}

bool LookerUpper::StillWorking()
{
    if (m_metadataFactory->IsRunning() ||
        m_busyRecList.count())
    {
        return true;
    }

    return false;
}

void LookerUpper::HandleSingleRecording(const uint chanid,
                                        const QDateTime starttime)
{
    ProgramInfo *pginfo = new ProgramInfo(chanid, starttime);

    if (!pginfo)
    {
        VERBOSE(VB_IMPORTANT, "No valid program info for supplied chanid/starttime");
        return;
    }

    m_busyRecList.append(pginfo);
    m_metadataFactory->Lookup(pginfo, false, false);
}

void LookerUpper::HandleAllRecordings()
{
    QMap< QString, ProgramInfo* > recMap;
    QMap< QString, uint32_t > inUseMap = ProgramInfo::QueryInUseMap();
    QMap< QString, bool > isJobRunning = ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, -1 );

    for( int n = 0; n < (int)progList.size(); n++)
    {
        ProgramInfo *pginfo = new ProgramInfo(*(progList[n]));
        if (pginfo->GetInetRef().isEmpty() ||
            (!pginfo->GetSubtitle().isEmpty() &&
              (pginfo->GetSeason() == 0) &&
              (pginfo->GetEpisode() == 0)))
        {
            QString msg = QString("Looking up: %1 %2").arg(pginfo->GetTitle())
                                           .arg(pginfo->GetSubtitle());
            VERBOSE(VB_IMPORTANT, msg);

            m_busyRecList.append(pginfo);
            m_metadataFactory->Lookup(pginfo, false, false);
        }
    }
}

void LookerUpper::customEvent(QEvent *levent)
{
    if (levent->type() == MetadataFactoryMultiResult::kEventType)
    {
        VERBOSE(VB_IMPORTANT, "Got a multiresult.");
        // We shouldn't get any of these.  If we do, metadataFactory->Lookup
        // was called with the wrong arguments.
    }
    else if (levent->type() == MetadataFactorySingleResult::kEventType)
    {
        MetadataFactorySingleResult *mfsr = dynamic_cast<MetadataFactorySingleResult*>(levent);

        if (!mfsr)
            return;

        MetadataLookup *lookup = mfsr->result;

        if (!lookup)
            return;

        ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(lookup->GetData());

        // This null check could hang us as this pginfo would then never be removed
        if (!pginfo)
            return;

        VERBOSE(VB_GENERAL|VB_EXTRA, QString("I found the following data:"));
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("        Input Title: %1").arg(pginfo->GetTitle()));
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("        Input Sub:   %1").arg(pginfo->GetSubtitle()));
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("        Title:       %1").arg(lookup->GetTitle()));
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("        Subtitle:    %1").arg(lookup->GetSubtitle()));
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("        Season:      %1").arg(lookup->GetSeason()));
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("        Episode:     %1").arg(lookup->GetEpisode()));
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("        Inetref:     %1").arg(lookup->GetInetref()));
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("        User Rating: %1").arg(lookup->GetUserRating()));

        pginfo->SaveSeasonEpisode(lookup->GetSeason(), lookup->GetEpisode());
        pginfo->SaveInetRef(lookup->GetInetref());

        m_busyRecList.removeAll(pginfo);
    }
    else if (levent->type() == MetadataFactoryNoResult::kEventType)
    {
        MetadataFactoryNoResult *mfnr = dynamic_cast<MetadataFactoryNoResult*>(levent);

        if (!mfnr)
            return;

        MetadataLookup *lookup = mfnr->result;

        if (!lookup)
            return;

        ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(lookup->GetData());

        // This null check could hang us as this pginfo would then never be removed
        if (!pginfo)
            return;

        m_busyRecList.removeAll(pginfo);
    }
}

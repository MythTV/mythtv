// C++
#include <vector>

// Qt
#include <QList>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remoteutil.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/metadataimagehelper.h"
#include "libmythtv/recordingrule.h"

// MythMetadataLookup
#include "lookup.h"

LookerUpper::LookerUpper()
  : m_metadataFactory(new MetadataFactory(this))
{
}

LookerUpper::~LookerUpper()
{
    while (!m_busyRecList.isEmpty())
        delete m_busyRecList.takeFirst();
}

bool LookerUpper::StillWorking()
{
    return m_metadataFactory->IsRunning() ||
        (m_busyRecList.count() != 0);
}

void LookerUpper::HandleSingleRecording(const uint chanid,
                                        const QDateTime &starttime,
                                        bool updaterules)
{
    auto *pginfo = new ProgramInfo(chanid, starttime);

    if (!pginfo)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "No valid program info for supplied chanid/starttime");
        return;
    }

    m_updaterules = updaterules;
    m_updateartwork = true;

    m_busyRecList.append(pginfo);
    m_metadataFactory->Lookup(pginfo, true, m_updateartwork, false);
}

void LookerUpper::HandleAllRecordings(bool updaterules)
{
    QMap< QString, ProgramInfo* > recMap;
    QMap< QString, uint32_t > inUseMap = ProgramInfo::QueryInUseMap();
    QMap< QString, bool > isJobRunning = ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    m_updaterules = updaterules;

    ProgramList progList;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, -1 );

    for (auto *pg : progList)
    {
        auto *pginfo = new ProgramInfo(*pg);
        if ((pginfo->GetRecordingGroup() != "Deleted") &&
            (pginfo->GetRecordingGroup() != "LiveTV") &&
            (pginfo->GetInetRef().isEmpty() ||
            (!pginfo->GetSubtitle().isEmpty() &&
            (pginfo->GetSeason() == 0) &&
            (pginfo->GetEpisode() == 0))))
        {
            QString msg = QString("Looking up: %1 %2")
                .arg(pginfo->GetTitle(), pginfo->GetSubtitle());
            LOG(VB_GENERAL, LOG_INFO, msg);

            m_busyRecList.append(pginfo);
            m_metadataFactory->Lookup(pginfo, true, false, false);
        }
        else
        {
            delete pginfo;
        }
    }
}

void LookerUpper::HandleAllRecordingRules()
{
    m_updaterules = true;

    std::vector<ProgramInfo *> recordingList;

    RemoteGetAllScheduledRecordings(recordingList);

    for (auto & pg : recordingList)
    {
        auto *pginfo = new ProgramInfo(*pg);
        if (pginfo->GetInetRef().isEmpty())
        {
            QString msg = QString("Looking up: %1 %2")
                .arg(pginfo->GetTitle(), pginfo->GetSubtitle());
            LOG(VB_GENERAL, LOG_INFO, msg);

            m_busyRecList.append(pginfo);
            m_metadataFactory->Lookup(pginfo, true, false, true);
        }
        else
        {
            delete pginfo;
        }
    }
}

void LookerUpper::HandleAllArtwork(bool aggressive)
{
    m_updateartwork = true;

    if (aggressive)
        m_updaterules = true;

    // First, handle all recording rules w/ inetrefs
    std::vector<ProgramInfo *> recordingList;

    RemoteGetAllScheduledRecordings(recordingList);
    int maxartnum = 3;

    for (auto & pg : recordingList)
    {
        auto *pginfo = new ProgramInfo(*pg);
        bool dolookup = true;

        if (pginfo->GetInetRef().isEmpty())
            dolookup = false;
        if (dolookup || aggressive)
        {
            ArtworkMap map = GetArtwork(pginfo->GetInetRef(), pginfo->GetSeason(), true);
            if (map.isEmpty() || (aggressive && map.count() < maxartnum))
            {
                QString msg = QString("Looking up artwork for recording rule: %1 %2")
                    .arg(pginfo->GetTitle(), pginfo->GetSubtitle());
                LOG(VB_GENERAL, LOG_INFO, msg);

                m_busyRecList.append(pginfo);
                m_metadataFactory->Lookup(pginfo, true, true, true);
                continue;
            }
        }
        delete pginfo;
    }

    // Now, Attempt to fill in the gaps for recordings
    QMap< QString, ProgramInfo* > recMap;
    QMap< QString, uint32_t > inUseMap = ProgramInfo::QueryInUseMap();
    QMap< QString, bool > isJobRunning = ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, -1 );

    for (auto *pg : progList)
    {
        auto *pginfo = new ProgramInfo(*pg);

        bool dolookup = true;

        LookupType type = GuessLookupType(pginfo);

        if (type == kProbableMovie)
           maxartnum = 2;

        if ((!aggressive && type == kProbableGenericTelevision) ||
             pginfo->GetRecordingGroup() == "Deleted" ||
             pginfo->GetRecordingGroup() == "LiveTV")
            dolookup = false;
        if (dolookup || aggressive)
        {
            ArtworkMap map = GetArtwork(pginfo->GetInetRef(), pginfo->GetSeason(), true);
            if (map.isEmpty() || (aggressive && map.count() < maxartnum))
            {
               QString msg = QString("Looking up artwork for recording: %1 %2")
                   .arg(pginfo->GetTitle(), pginfo->GetSubtitle());
                LOG(VB_GENERAL, LOG_INFO, msg);

                m_busyRecList.append(pginfo);
                m_metadataFactory->Lookup(pginfo, true, true, aggressive);
                continue;
            }
        }
        delete pginfo;
    }

}

void LookerUpper::CopyRuleInetrefsToRecordings()
{
    QMap< QString, ProgramInfo* > recMap;
    QMap< QString, uint32_t > inUseMap = ProgramInfo::QueryInUseMap();
    QMap< QString, bool > isJobRunning = ProgramInfo::QueryJobsRunning(JOB_COMMFLAG);

    ProgramList progList;

    LoadFromRecorded( progList, false, inUseMap, isJobRunning, recMap, -1 );

    for (auto *pg : progList)
    {
        auto *pginfo = new ProgramInfo(*pg);
        if (pginfo && pginfo->GetInetRef().isEmpty())
        {
            auto *rule = new RecordingRule();
            rule->m_recordID = pginfo->GetRecordingRuleID();
            rule->Load();
            if (!rule->m_inetref.isEmpty())
            {
                QString msg = QString("%1").arg(pginfo->GetTitle());
                if (!pginfo->GetSubtitle().isEmpty())
                    msg += QString(": %1").arg(pginfo->GetSubtitle());
                msg += " has no inetref, but its recording rule does. Copying...";
                LOG(VB_GENERAL, LOG_INFO, msg);
                pginfo->SaveInetRef(rule->m_inetref);
            }
            delete rule;
        }
        delete pginfo;
    }
}

void LookerUpper::customEvent(QEvent *levent)
{
    if (levent->type() == MetadataFactoryMultiResult::kEventType)
    {
        auto *mfmr = dynamic_cast<MetadataFactoryMultiResult*>(levent);

        if (!mfmr)
            return;

        MetadataLookupList list = mfmr->m_results;

        if (list.count() > 1)
        {
            int yearindex = -1;
            MetadataLookup *exactTitleMeta = nullptr;
            QDate exactTitleDate;
            float exactTitlePopularity = 0.0;
            bool foundMatchWithArt = false;

            for (int p = 0; p != list.size(); ++p)
            {
                auto *pginfo = list[p]->GetData().value<ProgramInfo *>();

                if (pginfo && (QString::compare(pginfo->GetTitle(), list[p]->GetBaseTitle(), Qt::CaseInsensitive)) == 0)
                {
                    bool hasArtwork = ((!(list[p]->GetArtwork(kArtworkFanart)).empty()) ||
                                       (!(list[p]->GetArtwork(kArtworkCoverart)).empty()) ||
                                       (!(list[p]->GetArtwork(kArtworkBanner)).empty()));

                    // After the first exact match, prefer any more popular one.
                    // Most of the Movie database entries have Popularity fields.
                    // The TV series database generally has no Popularity values specified,
                    // so if none are found so far in the search, pick the most recently
                    // released entry with artwork. Also, if the first exact match had
                    // no artwork, prefer any later exact match with artwork.
                    if ((exactTitleMeta == nullptr) ||
                        (hasArtwork &&
                         ((!foundMatchWithArt) ||
                          ((list[p]->GetPopularity() > exactTitlePopularity)) ||
                          ((exactTitlePopularity == 0.0F) && (list[p]->GetReleaseDate() > exactTitleDate)))))
                    {
                        // remember the most popular or most recently released exact match
                        exactTitleDate = list[p]->GetReleaseDate();
                        exactTitlePopularity = list[p]->GetPopularity();
                        exactTitleMeta = list[p];
                    }
                }

                if (pginfo && !pginfo->GetSeriesID().isEmpty() &&
                    pginfo->GetSeriesID() == (list[p])->GetTMSref())
                {
                    MetadataLookup *lookup = list[p];
                    if (lookup->GetSubtype() != kProbableGenericTelevision)
                        pginfo->SaveSeasonEpisode(lookup->GetSeason(), lookup->GetEpisode());
                    pginfo->SaveInetRef(lookup->GetInetref());
                    m_busyRecList.removeAll(pginfo);
                    return;
                }
                if (pginfo && pginfo->GetYearOfInitialRelease() != 0 &&
                         (list[p])->GetYear() != 0 &&
                         pginfo->GetYearOfInitialRelease() == (list[p])->GetYear())
                {
                    if (yearindex != -1)
                    {
                        LOG(VB_GENERAL, LOG_INFO, "Multiple results matched on year. No definite "
                                      "match could be found.");
                        m_busyRecList.removeAll(pginfo);
                        return;
                    }
                    LOG(VB_GENERAL, LOG_INFO, "Matched from multiple results based on year. ");
                    yearindex = p;
                }
            }

            if (yearindex > -1)
            {
                MetadataLookup *lookup = list[yearindex];
                auto *pginfo = lookup->GetData().value<ProgramInfo *>();
                if (lookup->GetSubtype() != kProbableGenericTelevision)
                    pginfo->SaveSeasonEpisode(lookup->GetSeason(), lookup->GetEpisode());
                pginfo->SaveInetRef(lookup->GetInetref());
                m_busyRecList.removeAll(pginfo);
                return;
            }

            if (exactTitleMeta != nullptr)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Best match released %1").arg(exactTitleDate.toString()));
                MetadataLookup *lookup = exactTitleMeta;
                auto *pginfo = exactTitleMeta->GetData().value<ProgramInfo *>();
                if (lookup->GetSubtype() != kProbableGenericTelevision)
                    pginfo->SaveSeasonEpisode(lookup->GetSeason(), lookup->GetEpisode());
                pginfo->SaveInetRef(lookup->GetInetref());
                m_busyRecList.removeAll(pginfo);
                return;
            }

            LOG(VB_GENERAL, LOG_INFO, "Unable to match this title, too many possible matches. "
                                      "You may wish to manually set the season, episode, and "
                                      "inetref in the 'Watch Recordings' screen.");

            auto *pginfo = list[0]->GetData().value<ProgramInfo *>();

            if (pginfo)
            {
                m_busyRecList.removeAll(pginfo);
            }
        }
    }
    else if (levent->type() == MetadataFactorySingleResult::kEventType)
    {
        auto *mfsr = dynamic_cast<MetadataFactorySingleResult*>(levent);

        if (!mfsr)
            return;

        MetadataLookup *lookup = mfsr->m_result;

        if (!lookup)
            return;

        auto *pginfo = lookup->GetData().value<ProgramInfo *>();

        // This null check could hang us as this pginfo would then never be
        // removed
        if (!pginfo)
            return;

        LOG(VB_GENERAL, LOG_DEBUG, "I found the following data:");
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Input Title: %1").arg(pginfo->GetTitle()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Input Sub:   %1").arg(pginfo->GetSubtitle()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Title:       %1").arg(lookup->GetTitle()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Subtitle:    %1").arg(lookup->GetSubtitle()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Season:      %1").arg(lookup->GetSeason()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Episode:     %1").arg(lookup->GetEpisode()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        Inetref:     %1").arg(lookup->GetInetref()));
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("        User Rating: %1").arg(lookup->GetUserRating()));

        if (lookup->GetSubtype() != kProbableGenericTelevision)
            pginfo->SaveSeasonEpisode(lookup->GetSeason(), lookup->GetEpisode());
        pginfo->SaveInetRef(lookup->GetInetref());

        if (m_updaterules)
        {
            auto *rule = new RecordingRule();
            if (rule)
            {
                rule->LoadByProgram(pginfo);
                if (rule->m_inetref.isEmpty() &&
                    (rule->m_searchType == kNoSearch))
                {
                    rule->m_inetref = lookup->GetInetref();
                }
                rule->m_season = lookup->GetSeason();
                rule->m_episode = lookup->GetEpisode();
                rule->Save();

                delete rule;
            }
        }

        if (m_updateartwork)
        {
            DownloadMap dlmap = lookup->GetDownloads();
            // Convert from QMap to QMultiMap
            ArtworkMap artmap;
            for (auto it = dlmap.cbegin(); it != dlmap.cend(); it++)
                artmap.insert(it.key(), it.value());
            SetArtwork(lookup->GetInetref(),
                       lookup->GetIsCollection() ? 0 : lookup->GetSeason(),
                       gCoreContext->GetMasterHostName(), artmap);
        }

        m_busyRecList.removeAll(pginfo);
    }
    else if (levent->type() == MetadataFactoryNoResult::kEventType)
    {
        auto *mfnr = dynamic_cast<MetadataFactoryNoResult*>(levent);

        if (!mfnr)
            return;

        MetadataLookup *lookup = mfnr->m_result;

        if (!lookup)
            return;

        auto *pginfo = lookup->GetData().value<ProgramInfo *>();

        // This null check could hang us as this pginfo would then never be removed
        if (!pginfo)
            return;

        m_busyRecList.removeAll(pginfo);
    }
}

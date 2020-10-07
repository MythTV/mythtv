// C/C++
#include <cstdlib>

// qt
#include <QCoreApplication>
#include <QEvent>
#include <QDir>
#include <QUrl>

// myth
#include "mythcorecontext.h"
#include "mythdirs.h"
#include "mythuihelper.h"
#include "mythsystemlegacy.h"
#include "storagegroup.h"
#include "metadatadownload.h"
#include "metadatafactory.h"
#include "mythmiscutil.h"
#include "remotefile.h"
#include "mythlogging.h"

QEvent::Type MetadataLookupEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

QEvent::Type MetadataLookupFailure::kEventType =
    (QEvent::Type) QEvent::registerEventType();

MetadataDownload::~MetadataDownload()
{
    cancel();
    wait();
}

/**
 * addLookup: Add lookup to bottom of the queue
 * MetadataDownload::m_lookupList takes ownership of the given lookup
 */
void MetadataDownload::addLookup(MetadataLookup *lookup)
{
    // Add a lookup to the queue
    QMutexLocker lock(&m_mutex);

    m_lookupList.append(lookup);
    lookup->DecrRef();
    if (!isRunning())
        start();
}

/**
 * prependLookup: Add lookup to top of the queue
 * MetadataDownload::m_lookupList takes ownership of the given lookup
 */
void MetadataDownload::prependLookup(MetadataLookup *lookup)
{
    // Add a lookup to the queue
    QMutexLocker lock(&m_mutex);

    m_lookupList.prepend(lookup);
    lookup->DecrRef();
    if (!isRunning())
        start();
}

void MetadataDownload::cancel()
{
    QMutexLocker lock(&m_mutex);

    m_lookupList.clear();
    m_parent = nullptr;
}

void MetadataDownload::run()
{
    RunProlog();

    while (true)
    {
        m_mutex.lock();
        if (m_lookupList.isEmpty())
        {
            // no more to process, we're done
            m_mutex.unlock();
            break;
        }
        // Ref owns the MetadataLookup object for the duration of the loop
        // and it will be deleted automatically when the loop completes
        RefCountHandler<MetadataLookup> ref = m_lookupList.takeFirstAndDecr();
        m_mutex.unlock();
        MetadataLookup *lookup = ref;
        MetadataLookupList list;

        // Go go gadget Metadata Lookup
        if (lookup->GetType() == kMetadataVideo ||
            lookup->GetType() == kMetadataRecording)
        {
            // First, look for mxml and nfo files in video storage groups
            if (lookup->GetType() == kMetadataVideo &&
                !lookup->GetFilename().isEmpty())
            {
                QString mxml = getMXMLPath(lookup->GetFilename());
                QString nfo = getNFOPath(lookup->GetFilename());

                if (!mxml.isEmpty())
                    list = readMXML(mxml, lookup);
                else if (!nfo.isEmpty())
                    list = readNFO(nfo, lookup);
            }

            // If nothing found, create lookups based on filename
            if (list.isEmpty())
            {
                if (lookup->GetSubtype() == kProbableTelevision)
                {
                    list = handleTelevision(lookup);
                    if ((findExactMatchCount(list, lookup->GetBaseTitle(), true) == 0) ||
                        (list.size() > 1 && !lookup->GetAutomatic()))
                    {
                        // There are no exact match prospects with artwork from TV search,
                        // so add in movies, where we might find a better match.
                        // In case of manual mode and ambiguous result, add it as well.
                        list.append(handleMovie(lookup));
                    }
                }
                else if (lookup->GetSubtype() == kProbableMovie)
                {
                    list = handleMovie(lookup);
                    if ((findExactMatchCount(list, lookup->GetBaseTitle(), true) == 0) ||
                        (list.size() > 1 && !lookup->GetAutomatic()))
                    {
                        // There are no exact match prospects with artwork from Movie search
                        // so add in television, where we might find a better match.
                        // In case of manual mode and ambiguous result, add it as well.
                        list.append(handleTelevision(lookup));
                    }
                }
                else
                {
                    // will try both movie and TV
                    list = handleVideoUndetermined(lookup);
                }
            }
        }
        else if (lookup->GetType() == kMetadataGame)
            list = handleGame(lookup);

        // inform parent we have lookup ready for it
        if (m_parent && !list.isEmpty())
        {
            // If there's only one result, don't bother asking
            // our parent about it, just add it to the back of
            // the queue in kLookupData mode.
            if (list.count() == 1 && list[0]->GetStep() == kLookupSearch)
            {
                MetadataLookup *newlookup = list.takeFirst();

                newlookup->SetStep(kLookupData);
                prependLookup(newlookup);
                // Type may have changed
                LookupType ret = GuessLookupType(newlookup);
                if (ret != kUnknownVideo)
                {
                    newlookup->SetSubtype(ret);
                }
                continue;
            }

            // If we're in automatic mode, we need to make
            // these decisions on our own.  Pass to title match.
            if (list[0]->GetAutomatic() && list.count() > 1
                && list[0]->GetStep() == kLookupSearch)
            {
                MetadataLookup *bestLookup = findBestMatch(list, lookup->GetBaseTitle());
                if (bestLookup)
                {
                    MetadataLookup *newlookup = bestLookup;

                    // pass through automatic type
                    newlookup->SetAutomatic(true);
                    // bestlookup is owned by list, we need an extra reference
                    newlookup->IncrRef();
                    newlookup->SetStep(kLookupData);
                    // Type may have changed
                    LookupType ret = GuessLookupType(newlookup);
                    if (ret != kUnknownVideo)
                    {
                        newlookup->SetSubtype(ret);
                    }
                    prependLookup(newlookup);
                    continue;
                }

                // Experimental:
                // If nothing matches, always return the first found item
                if (getenv("EXPERIMENTAL_METADATA_GRAB"))
                {
                    MetadataLookup *newlookup = list.takeFirst();

                    // pass through automatic type
                    newlookup->SetAutomatic(true);   // ### XXX RER
                    newlookup->SetStep(kLookupData);
                    // Type may have changed
                    LookupType ret = GuessLookupType(newlookup);
                    if (ret != kUnknownVideo)
                    {
                        newlookup->SetSubtype(ret);
                    }
                    prependLookup(newlookup);
                    continue;
                }

                // nothing more we can do in automatic mode
                QCoreApplication::postEvent(m_parent,
                    new MetadataLookupFailure(MetadataLookupList() << lookup));
                continue;
            }

            LOG(VB_GENERAL, LOG_INFO,
                QString("Returning Metadata Results: %1 %2 %3")
                    .arg(lookup->GetBaseTitle()).arg(lookup->GetSeason())
                    .arg(lookup->GetEpisode()));
            QCoreApplication::postEvent(m_parent,
                new MetadataLookupEvent(list));
        }
        else
        {
            if (list.isEmpty())
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Metadata Lookup Failed: No Results %1 %2 %3")
                        .arg(lookup->GetBaseTitle()).arg(lookup->GetSeason())
                        .arg(lookup->GetEpisode()));
            }
            if (m_parent)
            {
                // list is always empty here
                list.append(lookup);
                QCoreApplication::postEvent(m_parent,
                    new MetadataLookupFailure(list));
            }
        }
    }

    RunEpilog();
}

unsigned int MetadataDownload::findExactMatchCount(MetadataLookupList list,
                                                   const QString &originaltitle,
                                                   bool withArt)
{
    unsigned int exactMatches = 0;
    unsigned int exactMatchesWithArt = 0;

    for (MetadataLookupList::const_iterator i = list.begin();
            i != list.end(); ++i)
    {
        // Consider exact title matches (ignoring case)
        if ((QString::compare((*i)->GetTitle(), originaltitle, Qt::CaseInsensitive) == 0))
        {
            // In lookup by name, the television database tends to only include Banner artwork.
            // In lookup by name, the movie database tends to include only Fan and Cover artwork.
            if ((!((*i)->GetArtwork(kArtworkFanart)).empty()) ||
                (!((*i)->GetArtwork(kArtworkCoverart)).empty()) ||
                (!((*i)->GetArtwork(kArtworkBanner)).empty()))
            {
                exactMatchesWithArt++;
            }
            exactMatches++;
        }
    }

    if (withArt)
        return exactMatchesWithArt;
    return exactMatches;
}

MetadataLookup* MetadataDownload::findBestMatch(MetadataLookupList list,
                                            const QString &originaltitle)
{
    QStringList titles;
    MetadataLookup *ret = nullptr;
    QDate exactTitleDate;
    float exactTitlePopularity = 0.0F;
    int exactMatches = 0;
    int exactMatchesWithArt = 0;
    bool foundMatchWithArt = false;

    // Build a list of all the titles
    for (MetadataLookupList::const_iterator i = list.begin();
            i != list.end(); ++i)
    {
        QString title = (*i)->GetTitle();
        LOG(VB_GENERAL, LOG_INFO, QString("Comparing metadata title '%1' [%2] to recording title '%3'")
                .arg(title)
                .arg((*i)->GetReleaseDate().toString())
                .arg(originaltitle));
        // Consider exact title matches (ignoring case), which have some artwork available.
        if (QString::compare(title, originaltitle, Qt::CaseInsensitive) == 0)
        {
            bool hasArtwork = ((!((*i)->GetArtwork(kArtworkFanart)).empty()) ||
                               (!((*i)->GetArtwork(kArtworkCoverart)).empty()) ||
                               (!((*i)->GetArtwork(kArtworkBanner)).empty()));

            LOG(VB_GENERAL, LOG_INFO, QString("'%1', popularity = %2, ReleaseDate = %3")
                    .arg(title)
                    .arg((*i)->GetPopularity())
                    .arg((*i)->GetReleaseDate().toString()));

            // After the first exact match, prefer any more popular one.
            // Most of the Movie database entries have Popularity fields.
            // The TV series database generally has no Popularity values specified,
            // so if none are found so far in the search, pick the most recently
            // released entry with artwork. Also, if the first exact match had
            // no artwork, prefer any later exact match with artwork.
            if ((ret == nullptr) ||
                (hasArtwork &&
                 ((!foundMatchWithArt) ||
                  (((*i)->GetPopularity() > exactTitlePopularity)) ||
                  ((exactTitlePopularity == 0.0F) && ((*i)->GetReleaseDate() > exactTitleDate)))))
            {
                exactTitleDate = (*i)->GetReleaseDate();
                exactTitlePopularity = (*i)->GetPopularity();
                ret = (*i);
            }
            exactMatches++;
            if (hasArtwork)
            {
                foundMatchWithArt = true;
                exactMatchesWithArt++;
            }
        }

        titles.append(title);
    }

    LOG(VB_GENERAL, LOG_DEBUG, QString("exactMatches = %1, exactMatchesWithArt = %2")
            .arg(exactMatches)
            .arg(exactMatchesWithArt));

    // If there was one or more exact matches then we can skip a more intensive
    // and time consuming search
    if (exactMatches > 0)
    {
        if (exactMatches == 1)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Single exact title match for '%1'")
                    .arg(originaltitle));
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Multiple exact title matches found for '%1'. "
                        "Selecting most popular or most recent [%2]")
                    .arg(originaltitle)
                    .arg(exactTitleDate.toString()));
        }
        return ret;
    }

    // Apply Levenshtein distance algorithm to determine closest match
    QString bestTitle = nearestName(originaltitle, titles);

    // If no "best" was chosen, give up.
    if (bestTitle.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("No adequate match or multiple "
                    "matches found for %1.  Update manually.")
                    .arg(originaltitle));
        return nullptr;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Best Title Match For %1: %2")
                    .arg(originaltitle).arg(bestTitle));

    // Grab the one item that matches the besttitle (IMPERFECT)
    foreach (auto & item, list)
    {
        if (item->GetTitle() == bestTitle)
        {
            ret = item;
            break;
        }
    }

    return ret;
}

MetadataLookupList MetadataDownload::runGrabber(const QString& cmd, const QStringList& args,
                                                MetadataLookup *lookup,
                                                bool passseas)
{
    MythSystemLegacy grabber(cmd, args, kMSStdOut);
    MetadataLookupList list;

    LOG(VB_GENERAL, LOG_INFO, QString("Running Grabber: %1 %2")
        .arg(cmd).arg(args.join(" ")));

    grabber.Run();
    grabber.Wait();
    QByteArray result = grabber.ReadAll();
    if (!result.isEmpty())
    {
        QDomDocument doc;
        doc.setContent(result, true);
        QDomElement root = doc.documentElement();
        QDomElement item = root.firstChildElement("item");

        while (!item.isNull())
        {
            MetadataLookup *tmp = ParseMetadataItem(item, lookup, passseas);
            list.append(tmp);
            // MetadataLookup is to be owned by list
            tmp->DecrRef();
            item = item.nextSiblingElement("item");
        }
    }
    return list;
}

QString MetadataDownload::GetMovieGrabber()
{
    return MetaGrabberScript::GetType(kGrabberMovie).GetPath();
}

QString MetadataDownload::GetTelevisionGrabber()
{
    return MetaGrabberScript::GetType(kGrabberTelevision).GetPath();
}

QString MetadataDownload::GetGameGrabber()
{
    return MetaGrabberScript::GetType(kGrabberGame).GetPath();
}

bool MetadataDownload::runGrabberTest(const QString &grabberpath)
{
    return MetaGrabberScript(grabberpath).Test();
}

bool MetadataDownload::MovieGrabberWorks()
{
    if (!runGrabberTest(GetMovieGrabber()))
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Movie grabber not functional.  Aborting this run."));
        return false;
    }

    return true;
}

bool MetadataDownload::TelevisionGrabberWorks()
{
    if (!runGrabberTest(GetTelevisionGrabber()))
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Television grabber not functional.  Aborting this run."));
        return false;
    }

    return true;
}

MetadataLookupList MetadataDownload::readMXML(const QString& MXMLpath,
                                             MetadataLookup *lookup,
                                             bool passseas)
{
    MetadataLookupList list;

    LOG(VB_GENERAL, LOG_INFO,
        QString("Matching MXML file found. Parsing %1 for metadata...")
               .arg(MXMLpath));

    if (lookup->GetType() == kMetadataVideo)
    {
        QByteArray mxmlraw;
        QDomElement item;
        auto *rf = new RemoteFile(MXMLpath);

        if (rf->isOpen())
        {
            bool loaded = rf->SaveAs(mxmlraw);
            if (loaded)
            {
                QDomDocument doc;
                if (doc.setContent(mxmlraw, true))
                {
                    lookup->SetStep(kLookupData);
                    QDomElement root = doc.documentElement();
                    item = root.firstChildElement("item");
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Corrupt or invalid MXML file."));
                }
            }
        }

        delete rf;
        rf = nullptr;

        MetadataLookup *tmp = ParseMetadataItem(item, lookup, passseas);
        list.append(tmp);
        // MetadataLookup is owned by the MetadataLookupList returned
        tmp->DecrRef();
    }

    return list;
}

MetadataLookupList MetadataDownload::readNFO(const QString& NFOpath,
                                             MetadataLookup *lookup)
{
    MetadataLookupList list;

    LOG(VB_GENERAL, LOG_INFO,
        QString("Matching NFO file found. Parsing %1 for metadata...")
               .arg(NFOpath));

    bool error = false;

    if (lookup->GetType() == kMetadataVideo)
    {
        QByteArray nforaw;
        QDomElement item;
        auto *rf = new RemoteFile(NFOpath);

        if (rf->isOpen())
        {
            bool loaded = rf->SaveAs(nforaw);

            if (loaded)
            {
                QDomDocument doc;

                if (doc.setContent(nforaw, true))
                {
                    lookup->SetStep(kLookupData);
                    item = doc.documentElement();
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Invalid NFO file found."));
                    error = true;
                }
            }
        }

        delete rf;
        rf = nullptr;

        if (!error)
        {
            MetadataLookup *tmp = ParseMetadataMovieNFO(item, lookup);

            list.append(tmp);
            // MetadataLookup is owned by the MetadataLookupList returned
            tmp->DecrRef();
        }
    }

    return list;
}

MetadataLookupList MetadataDownload::handleGame(MetadataLookup *lookup)
{
    MetadataLookupList list;
    MetaGrabberScript grabber =
        MetaGrabberScript::GetGrabber(kGrabberGame, lookup);

    // If the inetref is populated, even in kLookupSearch mode,
    // become a kLookupData grab and use that.
    if (lookup->GetStep() == kLookupSearch &&
        (!lookup->GetInetref().isEmpty() &&
         lookup->GetInetref() != "00000000"))
    {
        lookup->SetStep(kLookupData);
    }

    if (lookup->GetStep() == kLookupSearch)
    {
        if (lookup->GetTitle().isEmpty())
        {
            // no point searching on nothing...
            return list;
        }
        // we're searching
        list = grabber.Search(lookup->GetTitle(), lookup);
    }
    else if (lookup->GetStep() == kLookupData)
    {
        // we're just grabbing data
        list = grabber.LookupData(lookup->GetInetref(), lookup);
    }

    return list;
}

/**
 * handleMovie:
 * attempt to find movie data via the following (in order)
 * 1- Local MXML: already done before
 * 2- Local NFO: already done
 * 3- By title
 * 4- By inetref (if present)
 */
MetadataLookupList MetadataDownload::handleMovie(MetadataLookup *lookup)
{
    MetadataLookupList list;

    MetaGrabberScript grabber =
        MetaGrabberScript::GetGrabber(kGrabberMovie, lookup);

    // initial search mode
    if (!lookup->GetInetref().isEmpty() && lookup->GetInetref() != "00000000" &&
        (lookup->GetStep() == kLookupSearch || lookup->GetStep() == kLookupData))
    {
        // with inetref
        lookup->SetStep(kLookupData);
        // we're just grabbing data
        list = grabber.LookupData(lookup->GetInetref(), lookup);
    }
    else if (lookup->GetStep() == kLookupSearch)
    {
        if (lookup->GetBaseTitle().isEmpty())
        {
            // no point searching on nothing...
            return list;
        }
        list = grabber.Search(lookup->GetBaseTitle(), lookup);
    }

    return list;
}

/**
 * handleTelevision
 * attempt to find television data via the following (in order)
 * 1- Local MXML: already done before
 * 2- Local NFO: already done
 * 3- By inetref with subtitle
 * 4- By inetref with season and episode
 * 5- By inetref
 * 6- By title and subtitle
 * 7- By title
 */
MetadataLookupList MetadataDownload::handleTelevision(MetadataLookup *lookup)
{
    MetadataLookupList list;

    MetaGrabberScript grabber =
        MetaGrabberScript::GetGrabber(kGrabberTelevision, lookup);
    bool searchcollection = false;

    // initial search mode
    if (!lookup->GetInetref().isEmpty() && lookup->GetInetref() != "00000000" &&
        (lookup->GetStep() == kLookupSearch || lookup->GetStep() == kLookupData))
    {
        // with inetref
        lookup->SetStep(kLookupData);
        if (!lookup->GetSubtitle().isEmpty())
        {
            list = grabber.SearchSubtitle(lookup->GetInetref(),
                                          lookup->GetBaseTitle() /* unused */,
                                          lookup->GetSubtitle(), lookup, false);
        }

        if (list.isEmpty() && (lookup->GetSeason() || lookup->GetEpisode()))
        {
            list = grabber.LookupData(lookup->GetInetref(), lookup->GetSeason(),
                                      lookup->GetEpisode(), lookup);
        }

        if (list.isEmpty() && !lookup->GetCollectionref().isEmpty())
        {
            list = grabber.LookupCollection(lookup->GetCollectionref(), lookup);
            searchcollection = true;
        }
        else if (list.isEmpty())
        {
            // We do not store CollectionRef in our database
            // so try with the inetref, for all purposes with TVDB, they are
            // always identical
            list = grabber.LookupCollection(lookup->GetInetref(), lookup);
            searchcollection = true;
        }
    }
    else if (lookup->GetStep() == kLookupSearch)
    {
        if (lookup->GetBaseTitle().isEmpty())
        {
            // no point searching on nothing...
            return list;
        }
        if (!lookup->GetSubtitle().isEmpty())
        {
            list = grabber.SearchSubtitle(lookup->GetBaseTitle(),
                                          lookup->GetSubtitle(), lookup, false);
        }
        if (list.isEmpty())
        {
            list = grabber.Search(lookup->GetBaseTitle(), lookup);
        }
    }
    else if (lookup->GetStep() == kLookupCollection)
    {
        list = grabber.LookupCollection(lookup->GetCollectionref(), lookup);
    }

    // Collection Fallback
    // If the lookup allows generic metadata, and the specific
    // season and episode are not available, try for series metadata.
    if (!searchcollection && list.isEmpty() &&
        !lookup->GetCollectionref().isEmpty() &&
        lookup->GetAllowGeneric() && lookup->GetStep() == kLookupData)
    {
        lookup->SetStep(kLookupCollection);
        list = grabber.LookupCollection(lookup->GetCollectionref(), lookup);
    }

    if (!list.isEmpty())
    {
        // mark all results so that search collection is properly handled later
        lookup->SetIsCollection(searchcollection);
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (auto it = list.begin(); it != list.end(); ++it)
        {
            (*it)->SetIsCollection(searchcollection);
        }
    }

    return list;
}

MetadataLookupList MetadataDownload::handleVideoUndetermined(MetadataLookup *lookup)
{
    MetadataLookupList list;

    if (lookup->GetSubtype() != kProbableMovie &&
        !lookup->GetSubtitle().isEmpty())
    {
        list.append(handleTelevision(lookup));
    }

    if (lookup->GetSubtype() != kProbableTelevision)
    {
        list.append(handleMovie(lookup));
    }

    if (list.count() == 1)
    {
        list[0]->SetStep(kLookupData);
    }

    return list;
}

MetadataLookupList MetadataDownload::handleRecordingGeneric(MetadataLookup *lookup)
{
    // We only enter this mode if we are pretty darn sure this is a TV show,
    // but we're for some reason looking up a generic, or the title didn't
    // exactly match in one of the earlier lookups.  This is a total
    // hail mary to try to get at least *series* level info and art/inetref.

    MetadataLookupList list;

    if (lookup->GetBaseTitle().isEmpty())
    {
        // no point searching on nothing...
        return list;
    }

    // no inetref known, just pull the default grabber
    MetaGrabberScript grabber = MetaGrabberScript::GetType(kGrabberTelevision);

    // cache some initial values so we can change them in the lookup later
    LookupType origtype = lookup->GetSubtype();
    int origseason = lookup->GetSeason();
    int origepisode = lookup->GetEpisode();

    if (origseason == 0 && origepisode == 0)
    {
        lookup->SetSeason(1);
        lookup->SetEpisode(1);
    }

    list = grabber.Search(lookup->GetBaseTitle(), lookup);

    if (list.count() == 1)
    {
        // search was successful, rerun as normal television mode
        lookup->SetInetref(list[0]->GetInetref());
        lookup->SetCollectionref(list[0]->GetCollectionref());
        list = handleTelevision(lookup);
    }

    lookup->SetSeason(origseason);
    lookup->SetEpisode(origepisode);
    lookup->SetSubtype(origtype);

    return list;
}

static QString getNameWithExtension(const QString &filename, const QString &type)
{
    QString ret;
    QString newname;
    QUrl qurl(filename);
    QString ext = QFileInfo(qurl.path()).suffix();

    if (ext.isEmpty())
    {
        // no extension, assume it is a directory
        newname = filename + "/" + QFileInfo(qurl.path()).fileName() + "." + type;
    }
    else
    {
        newname = filename.left(filename.size() - ext.size()) + type;
    }
    QUrl xurl(newname);

    if (RemoteFile::Exists(newname))
        ret = newname;

    return ret;
}

QString MetadataDownload::getMXMLPath(const QString& filename)
{
    return getNameWithExtension(filename, "mxml");
}

QString MetadataDownload::getNFOPath(const QString& filename)
{
    return getNameWithExtension(filename, "nfo");
}

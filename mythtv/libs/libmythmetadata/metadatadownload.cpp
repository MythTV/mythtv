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

MetadataDownload::MetadataDownload(QObject *parent) :
    MThread("MetadataDownload")
{
    m_parent = parent;
}

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
    m_parent = NULL;
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
            if (lookup->GetSubtype() == kProbableTelevision)
                list = handleTelevision(lookup);
            else if (lookup->GetSubtype() == kProbableMovie)
                list = handleMovie(lookup);
            else
            {
                // will try both movie and TV
                list = handleVideoUndetermined(lookup);
            }

            if ((list.isEmpty() ||
                 (list.size() > 1 && !lookup->GetAutomatic())) &&
                lookup->GetSubtype() == kProbableTelevision)
            {
                list.append(handleMovie(lookup));
            }
            else if ((list.isEmpty() ||
                      (list.size() > 1 && !lookup->GetAutomatic())) &&
                     lookup->GetSubtype() == kProbableMovie)
            {
                list.append(handleTelevision(lookup));
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
                MetadataLookup *bestLookup = findBestMatch(list, lookup->GetTitle());
                if (bestLookup)
                {
                    MetadataLookup *newlookup = bestLookup;

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

                QCoreApplication::postEvent(m_parent,
                    new MetadataLookupFailure(MetadataLookupList() << lookup));
            }

            LOG(VB_GENERAL, LOG_INFO,
                QString("Returning Metadata Results: %1 %2 %3")
                    .arg(lookup->GetTitle()).arg(lookup->GetSeason())
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
                        .arg(lookup->GetTitle()).arg(lookup->GetSeason())
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

MetadataLookup* MetadataDownload::findBestMatch(MetadataLookupList list,
                                            const QString &originaltitle) const
{
    QStringList titles;
    MetadataLookup *ret = NULL;

    // Build a list of all the titles
    int exactMatches = 0;
    for (MetadataLookupList::const_iterator i = list.begin();
            i != list.end(); ++i)
    {
        QString title = (*i)->GetTitle();
        if (title == originaltitle)
        {
            ret = (*i);
            exactMatches++;
        }

        titles.append(title);
    }

    // If there was one or more exact matches then we can skip a more intensive
    // and time consuming search
    if (exactMatches > 0)
    {
        if (exactMatches == 1)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Single Exact Title Match For %1")
                    .arg(originaltitle));
            return ret;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Multiple exact title matches found for %1. "
                        "Need to match on other criteria.")
                    .arg(originaltitle));
            return NULL;
        }
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
        return NULL;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Best Title Match For %1: %2")
                    .arg(originaltitle).arg(bestTitle));

    // Grab the one item that matches the besttitle (IMPERFECT)
    MetadataLookupList::const_iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        if ((*i)->GetTitle() == bestTitle)
        {
            ret = (*i);
            break;
        }
    }

    return ret;
}

MetadataLookupList MetadataDownload::runGrabber(QString cmd, QStringList args,
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

MetadataLookupList MetadataDownload::readMXML(QString MXMLpath,
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
        RemoteFile *rf = new RemoteFile(MXMLpath);

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
        rf = NULL;

        MetadataLookup *tmp = ParseMetadataItem(item, lookup, passseas);
        list.append(tmp);
        // MetadataLookup is owned by the MetadataLookupList returned
        tmp->DecrRef();
    }

    return list;
}

MetadataLookupList MetadataDownload::readNFO(QString NFOpath,
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
        RemoteFile *rf = new RemoteFile(NFOpath);

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
        rf = NULL;

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
 * 1- Local MXML
 * 2- Local NFO
 * 3- By title
 * 4- By inetref (if present)
 */
MetadataLookupList MetadataDownload::handleMovie(MetadataLookup *lookup)
{
    MetadataLookupList list;

    QString mxml;
    QString nfo;

    if (!lookup->GetFilename().isEmpty())
    {
        mxml = getMXMLPath(lookup->GetFilename());
        nfo = getNFOPath(lookup->GetFilename());
    }

    if (!mxml.isEmpty())
        list = readMXML(mxml, lookup);
    else if (!nfo.isEmpty())
        list = readNFO(nfo, lookup);

    if (!list.isEmpty())
        return list;

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
        if (lookup->GetTitle().isEmpty())
        {
            // no point searching on nothing...
            return list;
        }
        list = grabber.Search(lookup->GetTitle(), lookup);
    }

    return list;
}

/**
 * handleTelevision
 * attempt to find television data via the following (in order)
 * 1- By inetref with subtitle
 * 2- By inetref with season and episode
 * 3- By inetref
 * 4- By title and subtitle
 * 5- By title
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
                                          lookup->GetTitle() /* unused */,
                                          lookup->GetSubtitle(), lookup, false);
        }

        if (list.isEmpty() && lookup->GetSeason() && lookup->GetEpisode())
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
        if (lookup->GetTitle().isEmpty())
        {
            // no point searching on nothing...
            return list;
        }
        if (!lookup->GetSubtitle().isEmpty())
        {
            list = grabber.SearchSubtitle(lookup->GetTitle(),
                                          lookup->GetSubtitle(), lookup, false);
        }
        if (list.isEmpty())
        {
            list = grabber.Search(lookup->GetTitle(), lookup);
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
        for (MetadataLookupList::iterator it = list.begin();
             it != list.end(); ++it)
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

    if (lookup->GetTitle().isEmpty())
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

    list = grabber.Search(lookup->GetTitle(), lookup);

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

QString MetadataDownload::getMXMLPath(QString filename)
{
    QString ret;
    QString xmlname;
    QUrl qurl(filename);
    QString ext = QFileInfo(qurl.path()).suffix();
    xmlname = filename.left(filename.size() - ext.size()) + "mxml";
    QUrl xurl(xmlname);

    if (RemoteFile::isLocal(xmlname) ||
        (xmlname.startsWith("myth://") &&
         !gCoreContext->IsThisHost(qurl.host())))
    {
        if (RemoteFile::Exists(xmlname))
            ret = xmlname;
    }
    else
    {
        StorageGroup sg;
        QString fn = sg.FindFile(xurl.path());

        if (!fn.isEmpty() && QFile::exists(fn))
            ret = xmlname;
    }

    return ret;
}

QString MetadataDownload::getNFOPath(QString filename)
{
    QString ret;
    QString nfoname;
    QUrl qurl(filename);
    QString ext = QFileInfo(qurl.path()).suffix();
    nfoname = filename.left(filename.size() - ext.size()) + "nfo";
    QUrl nurl(nfoname);

    if (RemoteFile::isLocal(nfoname) ||
        (nfoname.startsWith("myth://") &&
         !gCoreContext->IsThisHost(qurl.host())))
    {
        if (RemoteFile::Exists(nfoname))
            ret = nfoname;
    }
    else
    {
        StorageGroup sg;
        QString fn = sg.FindFile(nurl.path());

        if (!fn.isEmpty() && QFile::exists(fn))
            ret = nfoname;
    }

    return ret;
}

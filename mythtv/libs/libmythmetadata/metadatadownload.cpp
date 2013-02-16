// qt
#include <QCoreApplication>
#include <QEvent>
#include <QDir>
#include <QUrl>

// myth
#include "mythcorecontext.h"
#include "mythdirs.h"
#include "mythuihelper.h"
#include "mythsystem.h"
#include "storagegroup.h"
#include "metadatadownload.h"
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

void MetadataDownload::addLookup(MetadataLookup *lookup)
{
    // Add a lookup to the queue
    m_mutex.lock();
    m_lookupList.append(lookup);
    if (!isRunning())
        start();
    m_mutex.unlock();
}

void MetadataDownload::prependLookup(MetadataLookup *lookup)
{
    // Add a lookup to the queue
    m_mutex.lock();
    m_lookupList.prepend(lookup);
    if (!isRunning())
        start();
    m_mutex.unlock();
}

void MetadataDownload::cancel()
{
    m_mutex.lock();
    qDeleteAll(m_lookupList);
    m_lookupList.clear();
    m_parent = NULL;
    m_mutex.unlock();
}

void MetadataDownload::run()
{
    RunProlog();

    MetadataLookup* lookup;
    while ((lookup = moreWork()) != NULL)
    {
        MetadataLookupList list;
        // Go go gadget Metadata Lookup
        if (lookup->GetType() == kMetadataVideo)
        {
            if (lookup->GetSubtype() == kProbableTelevision)
                list = handleTelevision(lookup);
            else if (lookup->GetSubtype() == kProbableMovie)
                list = handleMovie(lookup);
            else
                list = handleVideoUndetermined(lookup);

            if (!list.size() &&
                lookup->GetSubtype() == kUnknownVideo)
            {
                list = handleMovie(lookup);
            }
        }
        else if (lookup->GetType() == kMetadataRecording)
        {
            if (lookup->GetSubtype() == kProbableTelevision)
            {
                if (lookup->GetSeason() > 0 || lookup->GetEpisode() > 0)
                    list = handleTelevision(lookup);
                else if (!lookup->GetSubtitle().isEmpty())
                    list = handleVideoUndetermined(lookup);

                if (list.isEmpty())
                    list = handleRecordingGeneric(lookup);
            }
            else if (lookup->GetSubtype() == kProbableMovie)
            {
                list = handleMovie(lookup);
                if (lookup->GetInetref().isEmpty())
                    list.append(handleRecordingGeneric(lookup));
            }
            else
            {
                list = handleRecordingGeneric(lookup);
                if (lookup->GetInetref().isEmpty())
                    list.append(handleMovie(lookup));
            }
        }
        else if (lookup->GetType() == kMetadataGame)
            list = handleGame(lookup);

        // inform parent we have lookup ready for it
        if (m_parent && list.count() >= 1)
        {
            // If there's only one result, don't bother asking
            // our parent about it, just add it to the back of
            // the queue in kLookupData mode.
            if (list.count() == 1 && list.at(0)->GetStep() == kLookupSearch)
            {
                MetadataLookup *newlookup = list.takeFirst();
                newlookup->SetStep(kLookupData);
                prependLookup(newlookup);
                continue;
            }

            // If we're in automatic mode, we need to make
            // these decisions on our own.  Pass to title match.
            if (list.at(0)->GetAutomatic() && list.count() > 1
                && list.at(0)->GetStep() == kLookupSearch)
            {
                MetadataLookup *bestLookup = findBestMatch(list, lookup->GetTitle());
                if (bestLookup)
                {
                    MetadataLookup *newlookup = bestLookup;
                    newlookup->SetStep(kLookupData);
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
            list.append(lookup);
            QCoreApplication::postEvent(m_parent,
                new MetadataLookupFailure(list));
        }
    }

    RunEpilog();
}

MetadataLookup* MetadataDownload::moreWork()
{
    MetadataLookup* result = NULL;
    m_mutex.lock();
    if (!m_lookupList.isEmpty())
        result = m_lookupList.takeFirst();
    m_mutex.unlock();
    return result;
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
    for (MetadataLookupList::const_iterator i = list.begin();
            i != list.end(); ++i)
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
                                                MetadataLookup* lookup,
                                                bool passseas)
{
    MythSystem grabber(cmd, args, kMSNoRunShell | kMSStdOut | kMSBuffered);
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
            MetadataLookup *tmp = ParseMetadataItem(item, lookup,
                passseas);
            list.append(tmp);
            item = item.nextSiblingElement("item");
        }
    }
    return list;
}

QString MetadataDownload::GetMovieGrabber()
{
    QString def_cmd = "metadata/Movie/tmdb.py";
    QString db_cmd = gCoreContext->GetSetting("MovieGrabber", def_cmd);

    return QDir::cleanPath(QString("%1/%2")
            .arg(GetShareDir())
            .arg(db_cmd));
}

QString MetadataDownload::GetTelevisionGrabber()
{
    QString def_cmd = "metadata/Television/ttvdb.py";
    QString db_cmd = gCoreContext->GetSetting("TelevisionGrabber", def_cmd);

    return QDir::cleanPath(QString("%1/%2")
            .arg(GetShareDir())
            .arg(db_cmd));
}

QString MetadataDownload::GetGameGrabber()
{
    QString def_cmd = "metadata/Game/giantbomb.py";
    QString db_cmd = gCoreContext->GetSetting("mythgame.MetadataGrabber", def_cmd);

    return QDir::cleanPath(QString("%1/%2")
            .arg(GetShareDir())
            .arg(db_cmd));
}

bool MetadataDownload::runGrabberTest(const QString &grabberpath)
{
    QStringList args;
    args.append("-t");

    MythSystem grabber(grabberpath, args, kMSNoRunShell | kMSStdOut | kMSBuffered);

    grabber.Run();
    uint exitcode = grabber.Wait();

    if (exitcode != 0)
        return false;

    return true;
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
                                             MetadataLookup* lookup,
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
        if (MXMLpath.startsWith("myth://"))
        {
            RemoteFile *rf = new RemoteFile(MXMLpath);
            if (rf && rf->Open())
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
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("Corrupt or invalid MXML file."));
                }
                rf->Close();
            }

            delete rf;
            rf = NULL;
        }
        else
        {
            QFile file(MXMLpath);
            if (file.open(QIODevice::ReadOnly))
            {
                mxmlraw = file.readAll();
                QDomDocument doc;
                if (doc.setContent(mxmlraw, true))
                {
                    lookup->SetStep(kLookupData);
                    QDomElement root = doc.documentElement();
                    item = root.firstChildElement("item");
                }
                else
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("Corrupt or invalid MXML file."));
                file.close();
            }
        }

        MetadataLookup *tmp = ParseMetadataItem(item, lookup, passseas);
        list.append(tmp);
    }

    return list;
}

MetadataLookupList MetadataDownload::readNFO(QString NFOpath,
                                             MetadataLookup* lookup)
{
    MetadataLookupList list;

    LOG(VB_GENERAL, LOG_INFO,
        QString("Matching NFO file found. Parsing %1 for metadata...")
               .arg(NFOpath));

    if (lookup->GetType() == kMetadataVideo)
    {
        QByteArray nforaw;
        QDomElement item;
        if (NFOpath.startsWith("myth://"))
        {
            RemoteFile *rf = new RemoteFile(NFOpath);
            if (rf && rf->Open())
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
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("PIRATE ERROR: Invalid NFO file found."));
                }
                rf->Close();
            }

            delete rf;
            rf = NULL;
        }
        else
        {
            QFile file(NFOpath);
            if (file.open(QIODevice::ReadOnly))
            {
                nforaw = file.readAll();
                QDomDocument doc;
                if (doc.setContent(nforaw, true))
                {
                    lookup->SetStep(kLookupData);
                    item = doc.documentElement();
                }
                else
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("PIRATE ERROR: Invalid NFO file found."));
                file.close();
            }
        }

        MetadataLookup *tmp = ParseMetadataMovieNFO(item, lookup);
        list.append(tmp);
    }

    return list;
}

MetadataLookupList MetadataDownload::handleGame(MetadataLookup* lookup)
{
    MetadataLookupList list;

    QString cmd = GetGameGrabber();

    QStringList args;
    args.append(QString("-l")); // Language Flag
    args.append(gCoreContext->GetLanguage()); // UI Language
    args.append(QString("-a"));
    args.append(gCoreContext->GetLocale()->GetCountryCode());

    // If the inetref is populated, even in kLookupSearch mode,
    // become a kLookupData grab and use that.
    if (lookup->GetStep() == kLookupSearch &&
        (!lookup->GetInetref().isEmpty() &&
         lookup->GetInetref() != "00000000"))
        lookup->SetStep(kLookupData);

    if (lookup->GetStep() == kLookupSearch)
    {
        args.append(QString("-M"));
        QString title = lookup->GetTitle();
        args.append(title);
    }
    else if (lookup->GetStep() == kLookupData)
    {
        args.append(QString("-D"));
        args.append(lookup->GetInetref());
    }
    list = runGrabber(cmd, args, lookup);

    return list;
}

MetadataLookupList MetadataDownload::handleMovie(MetadataLookup* lookup)
{
    MetadataLookupList list;

    QString mxml;
    QString nfo;

    if (!lookup->GetFilename().isEmpty())
    {
        mxml = getMXMLPath(lookup->GetFilename());
        nfo = getNFOPath(lookup->GetFilename());
    }

    if (mxml.isEmpty() && nfo.isEmpty())
    {
        QString cmd = GetMovieGrabber();

        QStringList args;
        args.append(QString("-l")); // Language Flag
        args.append(gCoreContext->GetLanguage()); // UI Language

        args.append(QString("-a"));
        args.append(gCoreContext->GetLocale()->GetCountryCode());

        // If the inetref is populated, even in kLookupSearch mode,
        // become a kLookupData grab and use that.
        if (lookup->GetStep() == kLookupSearch &&
            (!lookup->GetInetref().isEmpty() &&
             lookup->GetInetref() != "00000000"))
            lookup->SetStep(kLookupData);

        if (lookup->GetStep() == kLookupSearch)
        {
            args.append(QString("-M"));
            QString title = lookup->GetTitle();
            args.append(title);
        }
        else if (lookup->GetStep() == kLookupData)
        {
            args.append(QString("-D"));
            args.append(lookup->GetInetref());
        }
        list = runGrabber(cmd, args, lookup);
    }
    else if (!mxml.isEmpty())
        list = readMXML(mxml, lookup);
    else if (!nfo.isEmpty())
        list = readNFO(nfo, lookup);

    return list;
}

MetadataLookupList MetadataDownload::handleTelevision(MetadataLookup* lookup)
{
    MetadataLookupList list;

    QString cmd = GetTelevisionGrabber();

    QStringList args;
    args.append(QString("-l")); // Language Flag
    args.append(gCoreContext->GetLanguage()); // UI Language
    
    args.append(QString("-a"));
    args.append(gCoreContext->GetLocale()->GetCountryCode());

    // If the inetref is populated, even in kLookupSearch mode,
    // become a kLookupData grab and use that.
    if (lookup->GetStep() == kLookupSearch &&
        (!lookup->GetInetref().isEmpty() &&
         lookup->GetInetref() != "00000000"))
        lookup->SetStep(kLookupData);

    if (lookup->GetStep() == kLookupSearch)
    {
        args.append(QString("-M"));
        if (lookup->GetInetref().isEmpty() ||
            lookup->GetInetref() == "00000000")
        {
            QString title = lookup->GetTitle();
            args.append(title);
        }
        else
        {
            QString inetref = lookup->GetInetref();
            args.append(inetref);
        }
    }
    else if (lookup->GetStep() == kLookupData)
    {
        args.append(QString("-D"));
        args.append(lookup->GetInetref());
        args.append(QString::number(lookup->GetSeason()));
        args.append(QString::number(lookup->GetEpisode()));
    }
    else if (lookup->GetStep() == kLookupCollection)
    {
        args.append(QString("-C"));
        args.append(lookup->GetCollectionref());
    }
    list = runGrabber(cmd, args, lookup);

    // Collection Fallback
    // If the lookup allows generic metadata, and the specific
    // season and episode are not available, try for series metadata.
    if (list.isEmpty() &&
        lookup->GetAllowGeneric() &&
        lookup->GetStep() == kLookupData)
    {
        lookup->SetStep(kLookupCollection);
        list = handleTelevision(lookup);
    }

    return list;
}

MetadataLookupList MetadataDownload::handleVideoUndetermined(
                                                    MetadataLookup* lookup)
{
    MetadataLookupList list;

    QString cmd = GetTelevisionGrabber();

    // Can't trust the inetref with so little information.

    QStringList args;
    args.append(QString("-l")); // Language Flag
    args.append(gCoreContext->GetLanguage()); // UI Language

    args.append(QString("-a"));
    args.append(gCoreContext->GetLocale()->GetCountryCode());

    args.append(QString("-N"));
    if (!lookup->GetInetref().isEmpty())
    {
        QString inetref = lookup->GetInetref();
        args.append(inetref);
    }
    else
    {
        QString title = lookup->GetTitle();
        args.append(title);
    }
    QString subtitle = lookup->GetSubtitle();
    args.append(subtitle);

    // Try to do a title/subtitle lookup
    list = runGrabber(cmd, args, lookup, false);

    if (list.count() == 1)
        list.at(0)->SetStep(kLookupData);

    return list;
}

MetadataLookupList MetadataDownload::handleRecordingGeneric(
                                                    MetadataLookup* lookup)
{
    // We only enter this mode if we are pretty darn sure this is a TV show,
    // but we're for some reason looking up a generic, or the title didn't
    // exactly match in one of the earlier lookups.  This is a total
    // hail mary to try to get at least *series* level info and art/inetref.

    MetadataLookupList list;

    QString cmd = GetTelevisionGrabber();

    QStringList args;

    args.append(QString("-l")); // Language Flag
    args.append(gCoreContext->GetLanguage()); // UI Language

    args.append(QString("-a"));
    args.append(gCoreContext->GetLocale()->GetCountryCode());


    args.append("-M");
    QString title = lookup->GetTitle();
    args.append(title);
    LookupType origtype = lookup->GetSubtype();
    int origseason = lookup->GetSeason();
    int origepisode = lookup->GetEpisode();

    lookup->SetSubtype(kProbableGenericTelevision);

    if (origseason == 0 && origepisode == 0)
    {
        lookup->SetSeason(1);
        lookup->SetEpisode(1);
    }

    list = runGrabber(cmd, args, lookup, true);

    if (list.count() == 1)
    {
        lookup->SetInetref(list.at(0)->GetInetref());
        lookup->SetCollectionref(list.at(0)->GetCollectionref());
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

    if (xmlname.startsWith("myth://"))
    {
        if (qurl.host().toLower() != gCoreContext->GetHostName().toLower() &&
            (!gCoreContext->IsThisHost(qurl.host())))
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
    }
    else
    {
        if (QFile::exists(xmlname))
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

    if (nfoname.startsWith("myth://"))
    {
        if (qurl.host().toLower() != gCoreContext->GetHostName().toLower() &&
            (!gCoreContext->IsThisHost(qurl.host())))
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
    }
    else
    {
        if (QFile::exists(nfoname))
            ret = nfoname;
    }

    return ret;
}

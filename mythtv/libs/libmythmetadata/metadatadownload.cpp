// qt
#include <QCoreApplication>
#include <QEvent>
#include <QProcess>
#include <QDir>

// myth
#include "mythcorecontext.h"
#include "mythdirs.h"
#include "mythverbose.h"
#include "mythuihelper.h"

#include "metadatadownload.h"

QEvent::Type MetadataLookupEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

QEvent::Type MetadataLookupFailure::kEventType =
    (QEvent::Type) QEvent::registerEventType();

MetadataDownload::MetadataDownload(QObject *parent)
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
    MetadataLookup* lookup;
    while ((lookup = moreWork()) != NULL)
    {
        MetadataLookupList list;
        // Go go gadget Metadata Lookup
        if (lookup->GetType() == VID)
        {
            if (lookup->GetSeason() > 0 || lookup->GetEpisode() > 0)
                list = handleTelevision(lookup);
            else if (!lookup->GetSubtitle().isEmpty())
                list = handleVideoUndetermined(lookup);
            else
                list = handleMovie(lookup);
        }
//        else if (lookup->GetType() == MUSIC)
//            list = handleMusic(lookup);
        else if (lookup->GetType() == GAME)
            list = handleGame(lookup);

        // inform parent we have lookup ready for it
        if (m_parent && list.count() >= 1)
        {
            // If there's only one result, don't bother asking
            // our parent about it, just add it to the back of
            // the queue in GETDATA mode.
            if (list.count() == 1 && list.at(0)->GetStep() == SEARCH)
            {
                MetadataLookup *newlookup = list.takeFirst();
                newlookup->SetStep(GETDATA);
                prependLookup(newlookup);
                continue;
            }

            // If we're in automatic mode, we need to make
            // these decisions on our own.  Pass to title match.
            if (list.at(0)->GetAutomatic() && list.count() > 1)
            {
                if (!findBestMatch(list, lookup->GetTitle()))
                    QCoreApplication::postEvent(m_parent,
                        new MetadataLookupFailure(MetadataLookupList() << lookup));
                continue;
            }

            VERBOSE(VB_GENERAL, QString("Returning Metadata Results: %1 %2 %3")
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

bool MetadataDownload::findBestMatch(MetadataLookupList list,
                                           QString originaltitle)
{
    QStringList titles;

    // Build a list of all the titles
    for (MetadataLookupList::const_iterator i = list.begin();
            i != list.end(); ++i)
    {
        titles.append((*i)->GetTitle());
    }

    // Apply Levenshtein distance algorithm to determine closest match
    QString bestTitle = nearestName(originaltitle, titles);

    // If no "best" was chosen, give up.
    if (bestTitle.isEmpty())
    {
        VERBOSE(VB_GENERAL, QString("No adequate match or multiple "
                    "matches found for %1.  Update manually.")
                    .arg(originaltitle));
        return false;
    }

    VERBOSE(VB_GENERAL, QString("Best Title Match For %1: %2")
                    .arg(originaltitle).arg(bestTitle));

    // Grab the one item that matches the besttitle (IMPERFECT)
    for (MetadataLookupList::const_iterator i = list.begin();
            i != list.end(); ++i)
    {
        if ((*i)->GetTitle() == bestTitle)
        {
            MetadataLookup *newlookup = (*i);
            newlookup->SetStep(GETDATA);
            prependLookup(newlookup);
            return true;
        }
    }

    return false;
}

MetadataLookupList MetadataDownload::runGrabber(QString cmd, QStringList args,
                                                MetadataLookup* lookup,
                                                bool passseas)
{
    QProcess grabber;
    MetadataLookupList list;

    VERBOSE(VB_GENERAL, QString("Running Grabber: %1 %2")
        .arg(cmd).arg(args.join(" ")));

    grabber.setReadChannel(QProcess::StandardOutput);
    grabber.start(cmd, args);
    grabber.waitForFinished();
    QByteArray result = grabber.readAll();
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

MetadataLookupList MetadataDownload::handleGame(MetadataLookup* lookup)
{
    MetadataLookupList list;

    QString def_cmd = QDir::cleanPath(QString("%1/%2")
        .arg(GetShareDir())
        .arg("metadata/Game/giantbomb.py"));

    QString cmd = gCoreContext->GetSetting("mythgame.MetadataGrabber", def_cmd);

    QStringList args;
    args.append(QString("-l")); // Language Flag
    args.append(GetMythUI()->GetLanguage()); // UI Language

    // If the inetref is populated, even in search mode,
    // become a getdata grab and use that.
    if (lookup->GetStep() == SEARCH &&
        (!lookup->GetInetref().isEmpty() &&
         lookup->GetInetref() != "00000000"))
        lookup->SetStep(GETDATA);

    if (lookup->GetStep() == SEARCH)
    {
        args.append(QString("-M"));
        args.append(lookup->GetTitle());
    }
    else if (lookup->GetStep() == GETDATA)
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

    QString def_cmd = QDir::cleanPath(QString("%1/%2")
        .arg(GetShareDir())
        .arg("metadata/Movie/tmdb.py"));

    QString cmd = gCoreContext->GetSetting("MovieGrabber", def_cmd);

    QStringList args;
    args.append(QString("-l")); // Language Flag
    args.append(GetMythUI()->GetLanguage()); // UI Language

    // If the inetref is populated, even in search mode,
    // become a getdata grab and use that.
    if (lookup->GetStep() == SEARCH &&
        (!lookup->GetInetref().isEmpty() &&
         lookup->GetInetref() != "00000000"))
        lookup->SetStep(GETDATA);

    if (lookup->GetStep() == SEARCH)
    {
        args.append(QString("-M"));
        args.append(lookup->GetTitle());
    }
    else if (lookup->GetStep() == GETDATA)
    {
        args.append(QString("-D"));
        args.append(lookup->GetInetref());
    }
    list = runGrabber(cmd, args, lookup);

    return list;
}

MetadataLookupList MetadataDownload::handleTelevision(MetadataLookup* lookup)
{
    MetadataLookupList list;

    QString def_cmd = QDir::cleanPath(QString("%1/%2")
        .arg(GetShareDir())
        .arg("metadata/Television/ttvdb.py"));

    QString cmd = gCoreContext->GetSetting("TelevisionGrabber", def_cmd);

    QStringList args;
    args.append(QString("-l")); // Language Flag
    args.append(GetMythUI()->GetLanguage()); // UI Language

    // If the inetref is populated, even in search mode,
    // become a getdata grab and use that.
    if (lookup->GetStep() == SEARCH &&
        (!lookup->GetInetref().isEmpty() &&
         lookup->GetInetref() != "00000000"))
        lookup->SetStep(GETDATA);

    if (lookup->GetStep() == SEARCH)
    {
        args.append(QString("-M"));
        args.append(lookup->GetTitle());
    }
    else if (lookup->GetStep() == GETDATA)
    {
        args.append(QString("-D"));
        args.append(lookup->GetInetref());
        args.append(QString::number(lookup->GetSeason()));
        args.append(QString::number(lookup->GetEpisode()));
    }
    list = runGrabber(cmd, args, lookup);
    return list;
}

MetadataLookupList MetadataDownload::handleVideoUndetermined(
                                                    MetadataLookup* lookup)
{
    MetadataLookupList list;

    QString def_cmd = QDir::cleanPath(QString("%1/%2")
        .arg(GetShareDir())
        .arg("metadata/Television/ttvdb.py"));

    QString cmd = gCoreContext->GetSetting("TelevisionGrabber", def_cmd);

    // Can't trust the inetref with so little information.

    QStringList args;
    args.append(QString("-l")); // Language Flag
    args.append(GetMythUI()->GetLanguage()); // UI Language
    args.append(QString("-N"));
    args.append(lookup->GetTitle());
    args.append(lookup->GetSubtitle());

    // Try to do a title/subtitle lookup
    list = runGrabber(cmd, args, lookup, false);

    // If there were no results for that, fall back to a movie lookup.
    if (!list.size())
        list = handleMovie(lookup);

    if (list.count() == 1)
        list.at(0)->SetStep(GETDATA);

    return list;
}


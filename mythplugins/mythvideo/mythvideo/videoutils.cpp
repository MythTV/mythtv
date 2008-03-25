#include <QDir>

#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>

#include "globals.h"
#include "videoutils.h"
#include "metadata.h"
#include "metadatalistmanager.h"

namespace
{
    const int WATCHED_WATERMARK = 10000; // Less than this and the chain of
                                         // videos will not be followed when
                                         // playing.
    const QString VIDEO_COVERFILE_DEFAULT_OLD = QObject::tr("None");
}

void PlayVideo(const QString &filename, const MetadataListManager &video_list)
{
    MetadataListManager::MetadataPtr item = video_list.byFilename(filename);

    if (!item) return;

    QTime playing_time;

    do
    {
        playing_time.start();

        QString internal_mrl;
        QString handler = Metadata::getPlayer(item.get(), internal_mrl);
        // See if this is being handled by a plugin..
        if (!gContext->GetMainWindow()->
                HandleMedia(handler, internal_mrl, item->Plot(), item->Title(),
                            item->Director(), item->Length(),
                            QString::number(item->Year())))
        {
            // No internal handler for this, play external
            QString command = Metadata::getPlayCommand(item.get());
            if (command.length())
            {
                gContext->sendPlaybackStart();
                myth_system(command);
                gContext->sendPlaybackEnd();
            }
        }

        if (item->ChildID() > 0)
        {
            item = video_list.byID(item->ChildID());
        }
        else
        {
            break;
        }
    }
    while (item && playing_time.elapsed() > WATCHED_WATERMARK);
}

void checkedSetText(LayerSet *container, const QString &item_name,
                           const QString &text)
{
    if (container)
    {
        UITextType *utt = dynamic_cast<UITextType *>
                (container->GetType(item_name));
        if (utt)
            utt->SetText(text);
    }
}

QStringList GetVideoDirs()
{
    QStringList tmp = QStringList::split(":",
            gContext->GetSetting("VideoStartupDir", DEFAULT_VIDEOSTARTUP_DIR));
    for (QStringList::iterator p = tmp.begin(); p != tmp.end(); ++p)
    {
        *p = QDir::cleanDirPath(*p);
    }
    return tmp;
}

QString getDisplayYear(int year)
{
    return year == VIDEO_YEAR_DEFAULT ? "?" : QString::number(year);
}

QString getDisplayRating(const QString &rating)
{
    if (rating == "<NULL>")
        return QObject::tr("No rating available.");
    return rating;
}

QString getDisplayUserRating(float userrating)
{
    return QString::number(userrating, 'f', 1);
}

QString getDisplayLength(int length)
{
    return QString::number(length) + " " + QObject::tr("minutes");
}

QString getDisplayBrowse(bool browse)
{
    return browse ? QObject::tr("Yes") : QObject::tr("No");
}

bool isDefaultCoverFile(const QString &coverfile)
{
    return coverfile == VIDEO_COVERFILE_DEFAULT ||
            coverfile == VIDEO_COVERFILE_DEFAULT_OLD;
}

QStringList GetCastList(const Metadata &item)
{
    QStringList al;

    const Metadata::cast_list &cast = item.getCast();
    for (Metadata::cast_list::const_iterator p = cast.begin();
         p != cast.end(); ++p)
    {
        al.push_back(p->second);
    }

    if (!al.count())
        al << QObject::tr("None defined");

    return al;
}

QString GetCast(const Metadata &item, const QString &sep /*= ", "*/)
{
    return GetCastList(item).join(sep);
}

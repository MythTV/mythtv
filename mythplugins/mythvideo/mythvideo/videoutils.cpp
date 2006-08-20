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

    if (!item.get()) return;

    QTime playing_time;

    do
    {
        playing_time.start();

        QString internal_mrl;
        QString handler = Metadata::getPlayer(item.get(), internal_mrl);
        // See if this is being handled by a plugin..
        if (!gContext->GetMainWindow()->
                HandleMedia(handler, internal_mrl,item->Plot(), item->Title(),
                            item->Director(), item->Length(),
                            getDisplayYear(item->Year())))
        {
            // No internal handler for this, play external
            QString command = Metadata::getPlayCommand(item.get());
            if (command.length())
                myth_system(command.local8Bit());
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
    while (item.get() && playing_time.elapsed() > WATCHED_WATERMARK);
}

bool checkParentPassword()
{
    QDateTime curr_time = QDateTime::currentDateTime();
    QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
    QString password = gContext->GetSetting("VideoAdminPassword");

    if (password.length() < 1)
        return true;

    // See if we recently (and succesfully) asked for a password

    if (last_time_stamp.length() < 1)
    {
        // Probably first time used

        VERBOSE(VB_IMPORTANT,
                QString("%1: Could not read password/pin time "
                        "stamp. This is only an issue if it "
                        "happens repeatedly.").arg(__FILE__));
    }
    else
    {
        QDateTime last_time = QDateTime::fromString(last_time_stamp,
                                                    Qt::TextDate);
        if (last_time.secsTo(curr_time) < 120)
        {
            // Two minute window
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }

    // See if there is a password set

    if (password.length() > 0)
    {
        bool ok = false;
        MythPasswordDialog *pwd =
                new MythPasswordDialog(QObject::tr("Parental Pin:"), &ok,
                                       password, gContext->GetMainWindow());
        pwd->exec();
        delete pwd;

        if (ok)
        {
            // All is good
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    else
        return true;

    return false;
}

QStringList GetVideoDirs()
{
    return QStringList::split(":", gContext->GetSetting("VideoStartupDir",
                                                DEFAULT_VIDEOSTARTUP_DIR));
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

#include <set>

#include <QDir>
#include <QImageReader>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdirs.h>

#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythsystem.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythuistatetype.h>

#include "globals.h"
#include "videoutils.h"
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

template <>
void CheckedSet(MythUIStateType *ui_item, QString state)
{
    if (ui_item)
        ui_item->DisplayState(state);
}

void CheckedSet(MythUIType *container, QString itemName, QString text)
{
    if (container)
    {
        MythUIText *uit =
                dynamic_cast<MythUIText *>(container->GetChild(itemName));
        CheckedSet(uit, text);
    }
}

void ETNop::Child(QString container_name, QString child_name)
{
    (void) container_name;
    (void) child_name;
}

void ETNop::Container(QString child_name)
{
    (void) child_name;
}

void ETPrintWarning::Child(QString container_name, QString child_name)
{
    VERBOSE(VB_GENERAL | VB_EXTRA, QObject::tr("Warning: container '%1' is "
                    "missing child '%2'").arg(container_name).arg(child_name));
}

void ETPrintWarning::Container(QString child_name)
{
    VERBOSE(VB_GENERAL | VB_EXTRA, QObject::tr("Warning: no valid container to "
                    "search for child '%1'").arg(child_name));
}

struct UIUtilChildError : public UIUtilException
{
    UIUtilChildError(QString container, QString child)
    {
        m_what = QString(QObject::tr("Error: container '%1' is missing a child "
                        "element named '%2'")).arg(container).arg(child);
    }

    QString What()
    {
        return m_what;
    }

  private:
    QString m_what;
};

struct UIUtilContainerError : public UIUtilException
{
    UIUtilContainerError(QString child)
    {
        m_what = QString(QObject::tr("Error: an invalid container was passed "
                        "while searching for child element '%1'")).arg(child);
    }

    QString What()
    {
        return m_what;
    }

  private:
    QString m_what;
};

void ETErrorException::Child(QString container_name, QString child_name)
{
    throw UIUtilChildError(container_name, child_name);
}

void ETErrorException::Container(QString child_name)
{
    throw UIUtilContainerError(child_name);
}

QStringList GetVideoDirs()
{
    QStringList tmp = gContext->GetSetting("VideoStartupDir",
                DEFAULT_VIDEOSTARTUP_DIR).split(":", QString::SkipEmptyParts);
    for (QStringList::iterator p = tmp.begin(); p != tmp.end(); ++p)
    {
        *p = QDir::cleanPath(*p);
    }
    return tmp;
}

QString getDisplayYear(int year)
{
    return (year == VIDEO_YEAR_DEFAULT || year == 0) ? "" : QString::number(year);
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

bool GetLocalVideoPoster(const QString &video_uid, const QString &filename,
                         const QStringList &in_dirs, QString &poster)
{
    QStringList search_dirs(in_dirs);

    QFileInfo qfi(filename);
    search_dirs += qfi.absolutePath();

    const QString base_name = qfi.completeBaseName();
    QList<QByteArray> image_types = QImageReader::supportedImageFormats();

    typedef std::set<QString> image_type_list;
    image_type_list image_exts;

    for (QList<QByteArray>::const_iterator it = image_types.begin();
            it != image_types.end(); ++it)
    {
        image_exts.insert(QString(*it).toLower());
    }

    const QString fntm("%1/%2.%3");

    for (QStringList::const_iterator dir = search_dirs.begin();
            dir != search_dirs.end(); ++dir)
    {
        if (!(*dir).length()) continue;

        for (image_type_list::const_iterator ext = image_exts.begin();
                ext != image_exts.end(); ++ext)
        {
            QStringList sfn;
            sfn += fntm.arg(*dir).arg(base_name).arg(*ext);
            sfn += fntm.arg(*dir).arg(video_uid).arg(*ext);

            for (QStringList::const_iterator i = sfn.begin();
                    i != sfn.end(); ++i)
            {
                if (QFile::exists(*i))
                {
                    poster = *i;
                    return true;
                }
            }
        }
    }

    return false;
}

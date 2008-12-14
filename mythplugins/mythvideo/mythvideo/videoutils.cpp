#include <QDir>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdirs.h>

#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythsystem.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythuistatetype.h>

#include "globals.h"
#include "metadatalistmanager.h"
#include "videoutils.h"

namespace
{
    const QString VIDEO_COVERFILE_DEFAULT_OLD = QObject::tr("None");
}

template <>
void CheckedSet(MythUIStateType *ui_item, const QString &state)
{
    if (ui_item)
        ui_item->DisplayState(state);
}

void CheckedSet(MythUIType *container, const QString &itemName, const QString &text)
{
    if (container)
    {
        MythUIText *uit =
                dynamic_cast<MythUIText *>(container->GetChild(itemName));
        CheckedSet(uit, text);
    }
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

bool IsDefaultCoverFile(const QString &coverfile)
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

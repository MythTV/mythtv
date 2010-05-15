#include <QDir>

#include <mythcontext.h>
#include <mythdirs.h>

#include <mythmainwindow.h>
#include <mythsystem.h>
#include <mythdialogbox.h>
#include <mythuistatetype.h>
#include <mythuiimage.h>

#include "globals.h"
#include "metadatalistmanager.h"
#include "videoutils.h"
#include "storagegroup.h"

namespace
{
    const QString VIDEO_COVERFILE_DEFAULT_OLD = QObject::tr("None");
    const QString VIDEO_COVERFILE_DEFAULT_OLD2 = QObject::tr("No Cover");

    template <typename T>
    void CopySecond(const T &src, QStringList &dest)
    {
        for (typename T::const_iterator p = src.begin(); p != src.end(); ++p)
        {
            dest.push_back((*p).second);
        }
    }
}

template <>
void CheckedSet(MythUIStateType *uiItem, const QString &state)
{
    if (uiItem)
    {
        uiItem->Reset();
        uiItem->DisplayState(state);
    }
}

void CheckedSet(MythUIType *container, const QString &itemName,
        const QString &value)
{
    if (container)
    {
        MythUIType *uit = container->GetChild(itemName);
        MythUIText *tt = dynamic_cast<MythUIText *>(uit);
        if (tt)
            CheckedSet(tt, value);
        else
        {
            MythUIStateType *st = dynamic_cast<MythUIStateType *>(uit);
            CheckedSet(st, value);
        }
    }
}

void CheckedSet(MythUIImage *uiItem, const QString &filename)
{
    if (uiItem)
    {
        uiItem->Reset();
        uiItem->SetFilename(filename);
        uiItem->Load();
    }
}

QStringList GetVideoDirsByHost(QString host)
{
    QStringList tmp;

    QStringList tmp2 = StorageGroup::getGroupDirs("Videos", host); 
    for (QStringList::iterator p = tmp2.begin(); p != tmp2.end(); ++p) 
        tmp.append(*p); 

    if (host.isEmpty())
    {
        QStringList tmp3 = gCoreContext->GetSetting("VideoStartupDir",
                    DEFAULT_VIDEOSTARTUP_DIR).split(":", QString::SkipEmptyParts);
        for (QStringList::iterator p = tmp3.begin(); p != tmp3.end(); ++p)
        {
            bool matches = false;
            QString newpath = *p;
            if (!newpath.endsWith("/"))
                newpath.append("/");

            for (QStringList::iterator q = tmp2.begin(); q != tmp2.end(); ++q)
            {
                QString comp = *q;

                if (comp.endsWith(newpath))
                {
                    matches = true;
                    break;
                }
            }
            if (!matches)
                tmp.append(QDir::cleanPath(*p));
        }
    }

    return tmp;
}

QStringList GetVideoDirs()
{
    return GetVideoDirsByHost("");
}

bool IsDefaultCoverFile(const QString &coverfile)
{
    return coverfile == VIDEO_COVERFILE_DEFAULT ||
            coverfile == VIDEO_COVERFILE_DEFAULT_OLD ||
            coverfile == VIDEO_COVERFILE_DEFAULT_OLD2 ||
            coverfile.endsWith(VIDEO_COVERFILE_DEFAULT_OLD) ||
            coverfile.endsWith(VIDEO_COVERFILE_DEFAULT_OLD2);
}

bool IsDefaultScreenshot(const QString &screenshot)
{
    return screenshot == VIDEO_SCREENSHOT_DEFAULT;
}

bool IsDefaultBanner(const QString &banner)
{
    return banner == VIDEO_BANNER_DEFAULT;
}

bool IsDefaultFanart(const QString &fanart)
{
    return fanart == VIDEO_FANART_DEFAULT;
}

QString GetDisplayUserRating(float userrating)
{
    return QString::number(userrating, 'f', 1);
}

QString GetDisplayLength(int length)
{
    return QString("%1 minutes").arg(length);
}

QString GetDisplaySeasonEpisode(int seasEp, int digits)
{
    QString seasEpNum = QString::number(seasEp);

    if (digits == 2 && seasEpNum.size() < 2)
        seasEpNum.prepend("0");
        
    return seasEpNum;
}

QString GetDisplayBrowse(bool browse)
{
    return browse ? QObject::tr("Yes") : QObject::tr("No");
}

QString GetDisplayWatched(bool watched)
{
    return watched ? QObject::tr("Yes") : QObject::tr("No");
}

QString GetDisplayYear(int year)
{
    return year == VIDEO_YEAR_DEFAULT ? "?" : QString::number(year);
}

QString GetDisplayRating(const QString &rating)
{
    if (rating == "<NULL>")
        return QObject::tr("No rating available.");
    return rating;
}

QString GetDisplayGenres(const Metadata &item)
{
    QStringList ret;
    CopySecond(item.GetGenres(), ret);
    return ret.join(", ");
}

QString GetDisplayCountries(const Metadata &item)
{
    QStringList ret;
    CopySecond(item.GetCountries(), ret);
    return ret.join(", ");
}

QStringList GetDisplayCast(const Metadata &item)
{
    QStringList ret;
    CopySecond(item.GetCast(), ret);
    return ret;
}

int editDistance( const QString& s, const QString& t )
{
#define D( i, j ) d[(i) * n + (j)]
    int i;
    int j;
    int m = s.length() + 1;
    int n = t.length() + 1;
    int *d = new int[m * n];
    int result;

    for ( i = 0; i < m; i++ )
      D( i, 0 ) = i;
    for ( j = 0; j < n; j++ )
      D( 0, j ) = j;
    for ( i = 1; i < m; i++ )
    {
        for ( j = 1; j < n; j++ )
        {
            if ( s[i - 1] == t[j - 1] )
                D( i, j ) = D( i - 1, j - 1 );
            else 
            {
                int x = D( i - 1, j );
                int y = D( i - 1, j - 1 );
                int z = D( i, j - 1 );
                D( i, j ) = 1 + qMin( qMin(x, y), z );
            }
        }
    }
    result = D( m - 1, n - 1 );
    delete[] d;
    return result;
#undef D
}

QString nearestName(const QString& actual, const QStringList& candidates, bool mythvideomode)
{
    int deltaBest = 10000;
    int numBest = 0;
    int tolerance = gCoreContext->GetNumSetting("mythvideo.lookupTolerance", 10);
    QString best;
    QString bestKey;
    QStringList possibles; 
    QMap<QString,QString> inetrefTitle;

    if (mythvideomode)
    {
        QStringList tmp;

        // Build the inetref/titles into a map
        QStringList::ConstIterator i = candidates.begin();
        while ( i != candidates.end() )
        {
            QString ref = (*i).left((*i).indexOf(':'));
            QString tit = (*i).right((*i).length() - (*i).indexOf(":") - 1);
            inetrefTitle.insert(ref, tit);
            tmp.append(tit);
            ++i;
        }

        // Throw away the inetrefs for the check
        possibles = tmp;
    }
    else
        possibles = candidates;

    QStringList::ConstIterator i = possibles.begin();
    while ( i != possibles.end() )
    {
        if ( (*i)[0] == actual[0] ) 
        {
            int delta = editDistance( actual, *i );
            if ( delta < deltaBest ) 
            {
                deltaBest = delta;
                numBest = 1;
                best = *i;
            }
            else if ( delta == deltaBest )
            {
                numBest++;
            }
        }
        ++i;
    }

    if ( numBest == 1 && deltaBest <= tolerance &&
       actual.length() + best.length() >= 5 )
    {
        if (mythvideomode)
        {
            // Now that we have a best value, reassociate it with the inetref
            // For MythVideo purposes, all we need back is the inetref.
            QString inetref = inetrefTitle.key(best);
            return inetref;
        }
        else
            return best;
    }
    else
        return QString();
}

QString ParentalLevelToState(const ParentalLevel &level)
{
    QString ret;
    switch (level.GetLevel())
    {
         case ParentalLevel::plLowest :
            ret = "Lowest";
            break;
        case ParentalLevel::plLow :
            ret = "Low";
            break;
        case ParentalLevel::plMedium :
            ret = "Medium";
            break;
        case ParentalLevel::plHigh :
            ret = "High";
            break;
        default:
            ret = "None";
    }

    return ret;
}

QString TrailerToState(const QString &trailerFile)
{
    QString ret;
    if (!trailerFile.isEmpty())
        ret = "hasTrailer";
    else
        ret = "None";
    return ret;
}

QString WatchedToState(bool watched)
{
    QString ret;
    if (watched)
        ret = "yes";
    else
        ret = "no";
    return ret;
}


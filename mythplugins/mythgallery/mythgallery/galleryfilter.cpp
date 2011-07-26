
#include <set>
#include <mythtv/libmythui/mythuitextedit.h>

#include "galleryfilter.h"

#include <mythtv/mythcontext.h>

#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuibutton.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuitextedit.h>

#include "config.h"
#include "galleryfilter.h"
#include "galleryutil.h"

#define LOC QString("GalleryFilter:")
#define LOC_ERR QString("GalleryFilter, Error:")

GalleryFilter::GalleryFilter(bool loaddefaultsettings) :
    m_dirFilter(""), m_typeFilter(kTypeFilterAll),
    m_sort(kSortOrderUnsorted),
    m_changed_state(0)
{
    // do nothing yet
    if (loaddefaultsettings)
    {
        m_dirFilter = gCoreContext->GetSetting("GalleryFilterDirectory", "");
        m_typeFilter = gCoreContext->GetNumSetting("GalleryFilterType", kTypeFilterAll);
        m_sort = gCoreContext->GetNumSetting("GallerySortOrder", kSortOrderUnsorted);
    }
}

GalleryFilter::GalleryFilter(const GalleryFilter &gfs) :
    m_changed_state(0)
{
    *this = gfs;
}

GalleryFilter &
GalleryFilter::operator=(const GalleryFilter &gfs)
{
    if (m_dirFilter != gfs.m_dirFilter)
    {
        m_dirFilter = gfs.m_dirFilter;
        m_changed_state = true;
    }

    if (m_typeFilter != gfs.m_typeFilter)
    {
        m_typeFilter = gfs.m_typeFilter;
        m_changed_state = true;
    }

    if (m_sort != gfs.m_sort)
    {
        m_sort = gfs.m_sort;
        m_changed_state = true;
    }

    return *this;
}

void GalleryFilter::saveAsDefault()
{
    gCoreContext->SaveSetting("GalleryFilterDirectory", m_dirFilter);
    gCoreContext->SaveSetting("GalleryFilterType", m_typeFilter);
    gCoreContext->SaveSetting("GallerySortOrder", m_sort);
}

bool GalleryFilter::TestFilter(const QString& dir, const GalleryFilter& flt,
                               int *dirCount, int *imageCount, int *movieCount)
{
    QStringList splitFD;
    const QFileInfo *fi;

    QDir d(dir);
    QString currDir = d.absolutePath();
    QFileInfoList list = d.entryInfoList(GalleryUtil::GetMediaFilter(),
                                         QDir::Files | QDir::AllDirs,
                                         (QDir::SortFlag)flt.getSort());

    if (list.isEmpty())
        return false;

    if (!flt.getDirFilter().isEmpty())
    {
        splitFD = flt.getDirFilter().split(":");
    }


    for (QFileInfoList::const_iterator it = list.begin(); it != list.end(); it++)
    {
        fi = &(*it);
        if (fi->fileName() == "."
            || fi->fileName() == "..")
        {
            continue;
        }

        // remove these already-resized pictures.
        if ((fi->fileName().indexOf(".thumb.") > 0) ||
            (fi->fileName().indexOf(".sized.") > 0) ||
            (fi->fileName().indexOf(".highlight.") > 0))
        {
            continue;
        }

        // skip filtered directory
        if (fi->isDir())
        {
            if (!splitFD.filter(fi->fileName(), Qt::CaseInsensitive).isEmpty())
            {
                continue;
            }
            // add directory
            (*dirCount)++;
            GalleryFilter::TestFilter(QDir::cleanPath(fi->absoluteFilePath()), flt,
                                      dirCount, imageCount, movieCount);
        }
        else
        {
            if (GalleryUtil::IsImage(fi->absoluteFilePath())
                && flt.getTypeFilter() != kTypeFilterMoviesOnly)
                (*imageCount)++;
            else if (GalleryUtil::IsMovie(fi->absoluteFilePath())
                && flt.getTypeFilter() != kTypeFilterImagesOnly)
                (*movieCount)++;
        }
    }

    return true;
}

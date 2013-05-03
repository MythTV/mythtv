#include <set>
#include <mythuitextedit.h>

#include <mythcontext.h>
#include <mythuibuttonlist.h>
#include <mythuibutton.h>
#include <mythuitext.h>
#include <mythuitextedit.h>
#include <mythlogging.h>

#include "config.h"
#include "galleryfilter.h"
#include "galleryutil.h"

GalleryFilter::GalleryFilter(bool loaddefaultsettings) :
    m_dirFilter(""), m_typeFilter(kTypeFilterAll),
    m_sort(kSortOrderUnsorted),
    m_changed_state(0)
{
    // do nothing yet
    if (loaddefaultsettings)
    {
        m_dirFilter = gCoreContext->GetSetting("GalleryFilterDirectory", "");
        m_typeFilter = gCoreContext->GetNumSetting("GalleryFilterType",
                                                   kTypeFilterAll);
        m_sort = gCoreContext->GetNumSetting("GallerySortOrder",
                                             kSortOrderUnsorted);
    }
}

GalleryFilter::GalleryFilter(const GalleryFilter &gfs) :
    m_changed_state(0)
{
    *this = gfs;
}

GalleryFilter & GalleryFilter::operator=(const GalleryFilter &gfs)
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
                                         QDir::Files | QDir::AllDirs |
                                         QDir::NoDotAndDotDot,
                                         (QDir::SortFlag)flt.getSort());

    if (list.isEmpty())
        return false;

    if (!flt.getDirFilter().isEmpty())
        splitFD = flt.getDirFilter().split(":");

    for (QFileInfoList::const_iterator it = list.begin();
         it != list.end(); ++it)
    {
        fi = &(*it);

        // remove these already-resized pictures.
        if ((fi->fileName().indexOf(".thumb.") > 0) ||
            (fi->fileName().indexOf(".sized.") > 0) ||
            (fi->fileName().indexOf(".highlight.") > 0))
            continue;

        // skip filtered directory
        if (fi->isDir())
        {
            if (!splitFD.filter(fi->fileName(), Qt::CaseInsensitive).isEmpty())
                continue;

            // add directory
            (*dirCount)++;
            GalleryFilter::TestFilter(QDir::cleanPath(fi->absoluteFilePath()),
                                      flt, dirCount, imageCount, movieCount);
        }
        else
        {
            if (GalleryUtil::IsImage(fi->absoluteFilePath()) &&
                flt.getTypeFilter() != kTypeFilterMoviesOnly)
                (*imageCount)++;
            else if (GalleryUtil::IsMovie(fi->absoluteFilePath()) &&
                     flt.getTypeFilter() != kTypeFilterImagesOnly)
                (*movieCount)++;
        }
    }

    return true;
}


void GalleryFilter::dumpFilter(QString src)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Dumping GalleryFilter from: %1")
                      .arg(src));
    LOG(VB_GENERAL, LOG_DEBUG, QString("directory fiter: %1")
                      .arg(m_dirFilter));
    LOG(VB_GENERAL, LOG_DEBUG, QString("type filter: %1")
                      .arg(m_typeFilter));
    LOG(VB_GENERAL, LOG_DEBUG, QString("sort options: %1")
                      .arg(m_sort));
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

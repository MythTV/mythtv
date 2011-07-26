#ifndef GALLERYFILTER_H
#define GALLERYFILTER_H

// qt
#include <QDir>

#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythdb/mythverbose.h>

enum SortOrder {
    kSortOrderUnsorted = QDir::Unsorted,
    kSortOrderNameAsc = QDir::Name + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderNameDesc = QDir::Name + QDir::Reversed + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderModTimeAsc = QDir::Time + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderModTimeDesc = QDir::Time + QDir::Reversed + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderExtAsc = QDir::Size + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderExtDesc = QDir::Size + QDir::Reversed + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderSizeAsc = QDir::Type + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderSizeDesc = QDir::Type + QDir::Reversed + QDir::DirsFirst + QDir::IgnoreCase
};
Q_DECLARE_METATYPE(SortOrder)

enum TypeFilter {
    kTypeFilterAll = 0,
    kTypeFilterImagesOnly = 1,
    kTypeFilterMoviesOnly = 2
};
Q_DECLARE_METATYPE(TypeFilter)

class GalleryFilter
{
  public:
    static bool TestFilter(const QString& dir, const GalleryFilter& flt,
                           int *dirCount, int *imageCount, int *movieCount);

    GalleryFilter(bool loaddefaultsettings = true);
    GalleryFilter(const GalleryFilter &gfs);
    GalleryFilter &operator=(const GalleryFilter &gfs);

    void saveAsDefault();

    QString getDirFilter() const { return m_dirFilter; }
    void setDirFilter(QString dirFilter)
    {
        m_changed_state  = 1;
        m_dirFilter = dirFilter;
    }

    int getTypeFilter() const { return m_typeFilter; }
    void setTypeFilter(int typeFilter)
    {
        m_changed_state = 1;
        m_typeFilter = typeFilter;
    }

    int getSort() const { return m_sort; }
    void setSort(int sort)
    {
        m_changed_state = 1;
        m_sort = sort;
    }

    unsigned int getChangedState()
    {
        unsigned int ret = m_changed_state;
        m_changed_state = 0;
        return ret;
    }
    void dumpFilter(QString src)
    {
        VERBOSE(VB_EXTRA, QString("Dumping GalleryFilter from: %1")
                          .arg(src));
        VERBOSE(VB_EXTRA, QString("directory fiter: %1")
                          .arg(m_dirFilter));
        VERBOSE(VB_EXTRA, QString("type filter: %1")
                          .arg(m_typeFilter));
        VERBOSE(VB_EXTRA, QString("sort options: %1")
                          .arg(m_sort));
    }


  private:
    QString m_dirFilter;
    int m_typeFilter;
    int m_sort;

    unsigned int m_changed_state;
};

#endif /* GALLERYFILTER_H */

#ifndef GALLERYFILTER_H
#define GALLERYFILTER_H

// qt
#include <QDir>

#include <mythscreentype.h>

enum SortOrder {
    kSortOrderUnsorted    = QDir::Unsorted,
    kSortOrderNameAsc     = QDir::Name + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderNameDesc    = QDir::Name + QDir::DirsFirst + QDir::IgnoreCase +
                            QDir::Reversed,
    kSortOrderModTimeAsc  = QDir::Time + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderModTimeDesc = QDir::Time + QDir::DirsFirst + QDir::IgnoreCase +
                            QDir::Reversed,
    kSortOrderExtAsc      = QDir::Size + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderExtDesc     = QDir::Size + QDir::DirsFirst + QDir::IgnoreCase +
                            QDir::Reversed,
    kSortOrderSizeAsc     = QDir::Type + QDir::DirsFirst + QDir::IgnoreCase,
    kSortOrderSizeDesc    = QDir::Type + QDir::DirsFirst + QDir::IgnoreCase +
                            QDir::Reversed
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
    void dumpFilter(QString src);

  private:
    QString m_dirFilter;
    int m_typeFilter;
    int m_sort;

    unsigned int m_changed_state;
};

#endif /* GALLERYFILTER_H */

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

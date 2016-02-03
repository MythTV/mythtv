/*!
  \file
  \brief Provides view datastores for Gallery screens

  Manages lists of images and directories that constitute the items displayed by
  Thumbnail & Slideshow screens. A view populates itself from the database, manages
  its own ordering, selections and marked items, and provides iterators for navigation.
  Also provides the image details overlay.
*/

#ifndef GALLERYVIEWS_H
#define GALLERYVIEWS_H

#include "imagemanager.h"


//! Order of images in slideshow
enum SlideOrderType {
    kOrdered  = 0, //!< Ordered as per user setting GallerySortOrder
    kShuffle  = 1, //!< Each image appears exactly once, but in random order
    kRandom   = 2, //!< Random selection from view. An image may be absent or appear multiple times
    kSeasonal = 3  //!< Biased random selection so that images are more likely to appear on anniversaries
};


//! Seasonal weightings for images in a view
typedef QVector<double> WeightList;


//! A container of images/dirs that have been marked
class MarkedFiles : public QSet<int>
{
public:
    MarkedFiles() : QSet(), m_valid(false), m_parent(0) {}
    void Initialise(int id)      { m_valid = true; m_parent = id; clear();}
    void Clear()                 { m_valid = false; clear(); }
    bool IsFor(int id)           { return m_valid && m_parent == id; }
    void Add(ImageIdList newIds) { unite(newIds.toSet()); }
    void Add(int id)             { insert(id); }
    void Invert(ImageIdList all) { QSet tmp(all.toSet() - *this); swap(tmp); }

private:
    bool m_valid;
    int  m_parent;
};


//! A snapshot of current selection, markings & dir info when menu is invoked
class MenuSubjects
{
public:
    MenuSubjects()
        : m_selected(),          m_selectedMarked(false),
          m_markedId(),          m_prevMarkedId(),          m_childCount(0),
          m_hiddenMarked(false), m_unhiddenMarked(false)
    {}

    MenuSubjects(ImagePtrK selection, int childCount,
                 MarkedFiles &marked, MarkedFiles &prevMarked,
                 bool hiddenMarked, bool unhiddenMarked)
        : m_selected(selection),
          m_selectedMarked(selection && marked.contains(selection->m_id)),
          m_markedId(marked.toList()),  m_prevMarkedId(prevMarked.toList()),
          m_childCount(childCount),
          m_hiddenMarked(hiddenMarked), m_unhiddenMarked(unhiddenMarked)
    {}

    ImagePtrK   m_selected;       //!< Selected item
    bool        m_selectedMarked; //!< Is selected item marked ?
    ImageIdList m_markedId;       //!< Ids of all marked items
    ImageIdList m_prevMarkedId;   //!< Ids of marked items in previous dir
    int         m_childCount;     //!< Number of images & dirs excl parent
    bool        m_hiddenMarked;   //!< Is any marked item hidden ?
    bool        m_unhiddenMarked; //!< Is any marked item unhidden ?
};


//! Records info of displayed image files to enable clean-up of the UI image cache
class FileCacheEntry
{
public:
    // Default constructor for QHash 'undefined entry'
    FileCacheEntry() : m_parent(GALLERY_DB_ID), m_url(), m_thumbUrl() {}

    FileCacheEntry(int parent, const QString &url, const QString &thumbUrl)
        : m_parent(parent), m_url(url), m_thumbUrl(thumbUrl)  {}

    QString ToString(int id)
    { return QString("File %1 Parent %2").arg(id).arg(m_parent); }

    int     m_parent;
    QString m_url;
    QString m_thumbUrl;
};


/*!
 \brief A datastore of images for display by a screen.
 \details A flat view provides a list of ordered images (no dirs) from a single
 directory (as required by a slideshow of a directory).
*/
class FlatView
{
public:
    explicit FlatView(SlideOrderType order)
        : m_parentId(-1), m_order(order), m_mgr(ImageManagerFe::getInstance()),
          m_images(), m_sequence(), m_active(0) {}
    virtual ~FlatView()         { Clear(); }

    int          GetParentId() const { return m_parentId; }
    ImageListK   GetAllNodes() const;
    ImagePtrK    GetSelected() const;
    ImagePtrK    HasNext(int inc) const;
    ImagePtrK    HasPrev(int inc) const;
    ImagePtrK    Next(int inc);
    ImagePtrK    Prev(int inc);
    QString      GetPosition() const;
    bool         Select(int id, int fallback = 0);
    virtual bool LoadFromDb(int parentId);
    QStringList  ClearImage(int id, bool remove = false);
    void         ClearCache();
    bool         Update(int id);
    void         Rotate(int id);
    void         Clear(bool resetParent = true);

    QString      GetCachedThumbUrl(int id) const
    { return m_fileCache.value(id).m_thumbUrl; }

protected:
    static WeightList CalculateSeasonalWeights(ImageList &files);

    void Populate(ImageList &files);
    void Cache(int id, int parent, const QString &url, const QString &thumb);

    int                   m_parentId;
    SlideOrderType        m_order;
    ImageManagerFe       &m_mgr;
    QHash<int, ImagePtrK> m_images;   //!< Image objects currently displayed
    ImageIdList           m_sequence; //!< The sequence in which to display images.
    int                   m_active;   //!< Sequence index of current selected image

    //! Caches displayed image files
    QHash<int, FileCacheEntry> m_fileCache;
};


//! Records dir info for every displayed dir. Populating dir thumbnails may
//! entail lengthy interrogation of the dir subtree even though it doesn't change
//! often. Caching this info improves browsing response.
class DirCacheEntry
{
public:
    DirCacheEntry()
        : m_parent(), m_thumbCount(),
          m_dirCount(-1), m_fileCount(-1), m_thumbs() {}

    DirCacheEntry(int parentId, int dirs, int files,
                  const QList<ThumbPair> &thumbs, int thumbCount)
        : m_parent(parentId), m_thumbCount(thumbCount),
          m_dirCount(dirs), m_fileCount(files), m_thumbs(thumbs) {}

    QString ToString(int id) const;

    int              m_parent;
    int              m_thumbCount;
    int              m_dirCount;
    int              m_fileCount;
    QList<ThumbPair> m_thumbs;
};


/*!
 \brief A datastore of images for display by a screen.
Provides an ordered list of dirs & images from a single directory, as
required by a Thumbnail view. Permits marking of items and populates dir
thumbnails from their subtree
*/
class DirectoryView : public FlatView
{
public:
    explicit DirectoryView(SlideOrderType);

    ImagePtrK GetParent() const
    { return m_sequence.isEmpty() ? ImagePtrK() : m_images.value(m_sequence.at(0)); }

    QString      GetPosition() const;
    bool         LoadFromDb(int parentId);
    void         Clear(bool resetParent = true);
    MenuSubjects GetMenuSubjects();
    QStringList  RemoveImage(int id, bool deleted = false);
    void         ClearCache();
    void         MarkAll();
    void         Mark(int, bool);
    void         InvertMarked();
    void         ClearMarked();
    bool         IsMarked(int id) const
    { return m_marked.contains(id) || m_prevMarked.contains(id); }

protected:
    void         SetDirectory(int);
    void         LoadDirThumbs(ImageItem &, int limit, int level = 0);
    void         PopulateThumbs(ImageItem &, int limit,
                                const ImageList &files, const ImageList &dirs,
                                int level = 0);
    ImageIdList  GetChildren() const  { return m_sequence.mid(1); }
    bool         PopulateFromCache(ImageItem &, int required);
    void         Cache(ImageItemK &, int thumbCount);

    MarkedFiles m_marked;       //!< Marked items in current dir/view
    MarkedFiles m_prevMarked;   //!< Marked items in previous dir

    //! Caches displayed image dirs
    QHash<int, DirCacheEntry>  m_dirCache;
};


/*!
 \brief A datastore of images for display by a screen.
Provides an ordered list of images (no dirs) from a directory subtree.
Default ordering is a depth-first traversal of the tree
*/
class TreeView : public FlatView
{
public:
    explicit TreeView(SlideOrderType order) : FlatView(order) {}

    bool LoadFromDb(int parentId);
};


#endif // GALLERYVIEWS_H

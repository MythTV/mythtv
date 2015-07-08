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

#include <QVector>

#include <mythscreentype.h>
#include <mythuibuttonlist.h>
#include <imagemetadata.h>
#include <imageutils.h>


//! Order of images in slideshow
enum SlideOrderType {
    kOrdered  = 0, //!< Ordered as per user setting GallerySortOrder
    kShuffle  = 1, //!< Each image appears exactly once, but in random order
    kRandom   = 2, //!< Random selection from view. An image may be absent or appear multiple times
    kSeasonal = 3  //!< Biased random selection so that images are more likely to appear on anniversaries
};

//! Displayed info/details about an image.
enum InfoVisibleState { kNoInfo,    //!< Details not displayed
                        kBasicInfo, //!< Shows just the most useful exif tags
                        kFullInfo   //!< Shows all exif tags
                      };

//! Seasonal weightings for images in a view
typedef QVector<double> WeightList;

//! The image info/details buttonlist overlay that displays exif tags
class InfoList
{
public:
    static InfoList *Create(MythScreenType *, bool, QString);

    void             Toggle(const ImageItem *);
    bool             Hide();
    void             Update(const ImageItem *);
    InfoVisibleState GetState();

private:
    InfoList(MythScreenType *, bool, QString, MythUIButtonList *infoList);
    void              CreateButton(QString, QString);

    MythScreenType   *m_screen;
    MythUIButtonList *m_btnList;
    InfoVisibleState  m_infoVisible;
    QString           m_undefinedText;
};


//! A snapshot of current selection, markings & dir info when menu is invoked
class MenuSubjects
{
public:
    void Clear();

    ImageItem   *m_selected;       //!< Selected item
    bool         m_selectedMarked; //!< Is selected item marked ?
    ImageIdList  m_markedId;       //!< Ids of all marked items
    ImageIdList  m_prevMarkedId;   //!< Ids of marked items in previous dir
    int          m_childCount;     //!< Number of images & dirs excl parent
    bool         m_hiddenMarked;   //!< Is any marked item hidden ?
    bool         m_unhiddenMarked; //!< Is any marked item unhidden ?
};


//! A manager of images/dirs that have been marked
class MarkedFiles
{
public:
                MarkedFiles(void): parent(-1) {}
    void        Reset(int id = -1)      { ids.clear(); parent = id; }
    void        Add(int id)             { ids.insert(id); }
    void        Add(ImageIdList newIds) { ids += newIds.toSet(); }
    void        Remove(int id)          { ids.remove(id); }
    void        Invert(ImageIdList all) { ids = all.toSet() - ids; }
    bool        Contains(int id) const  { return ids.contains(id); }
    bool        IsEmpty()               { return ids.isEmpty(); }
    bool        IsFor(int id)           { return parent == id; }
    ImageIdList GetIds()                { return ids.toList(); }
private:
    int       parent;
    QSet<int> ids;
};



/*!
 \brief A datastore of images for display by a screen.
 \details A flat view provides a list of ordered images (no dirs) from a single
directory, as required by a Normal Slideshow.
*/
class FlatView
{
public:
    FlatView(SlideOrderType order, ImageDbReader *db)
        : m_parentId(-1), m_order(order), m_db(db), m_images(), m_sequence(), m_active(0) {}
    virtual ~FlatView()             { Clear(); }

    int           GetParentId() const { return m_parentId; }
    ImageList     GetAllNodes() const;
    ImageItem    *GetSelected() const;
    ImageItem    *HasNext() const;
    ImageItem    *HasPrev() const;
    ImageItem    *Next();
    ImageItem    *Prev();
    QString       GetPosition() const;
    bool          Select(int id, int fallback = 0);
    virtual bool  LoadFromDb(int parentId = -1);
    bool          Remove(const QStringList);
    bool          Update(int);
    void          Rotate(int id, int offset = 0);
    void          Clear(bool resetParent = true);

protected:
    static WeightList CalculateSeasonalWeights(ImageList &files);

    void              Populate(ImageList &files);

    //! A pointer to a list slot that contains a pointer to an image
    typedef ImageItem *const *ImageRef;

    int             m_parentId;
    SlideOrderType  m_order;
    ImageDbReader  *m_db;
    //! The set of images (from Db) used by this view. Ordered as per GallerySortOrder
    ImageList       m_images;
    //! The sequence in which to display images. References slots in m_images as
    //! random sequences may contain a specific image many times
    QList<ImageRef> m_sequence;
    int             m_active;   //!< Sequence index of current selected image
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
    DirectoryView(SlideOrderType, ImageDbReader *);

    ImageItem   *GetParent() const { return m_sequence.isEmpty() ? NULL : *m_sequence[0]; }
    bool         LoadFromDb(int parentId = -1);
    void         Clear(bool resetParent = true);
    void         MarkAll();
    void         Mark(int, bool);
    void         InvertMarked();
    void         ClearMarked();
    bool         IsMarked(int id) const
    { return m_marked.Contains(id) || m_prevMarked.Contains(id); }

    MenuSubjects GetMenuSubjects();

protected:
    void         SetDirectory(int);
    ImageIdList  GetChildren() const;
    ImageItem   *GetDbParent(int ) const;
    void         GetDbTree(ImageList &, ImageList &, ImageItem * = NULL) const;
    void         GetDbSubTree(ImageItem *, int limit, int level = 0) const;

    MarkedFiles m_marked;       //!< Marked items in current dir/view
    MarkedFiles m_prevMarked;   //!< Marked items in previous dir
};


/*!
 \brief A datastore of images for display by a screen.

Provides an ordered list of images (no dirs) from a directory subtree.
Default ordering is a depth-first traversal of the tree
*/
class TreeView : public FlatView
{
public:
    TreeView(SlideOrderType order, ImageDbReader *db)
        : FlatView(order, db) {}

    bool LoadFromDb(int parentId = -1);
};


#endif // GALLERYVIEWS_H

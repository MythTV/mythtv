#ifndef VIDEOLIST_H_
#define VIDEOLIST_H_

#include <qapplication.h>
#include <qdialog.h>
#include <qmap.h>

#include <mythtv/mythwidgets.h>
#include <mythtv/uitypes.h>

#include "videofilter.h"
#include "metadata.h"

// Type of the item added to the tree
#define SUB_FOLDER      -1
#define UP_FOLDER       -2
#define ROOT_NODE       -3

// Ordering of items on a tree level (low -> high)
#define ORDER_UP        0
#define ORDER_SUB       1
#define ORDER_ITEM      2

class VideoList
{
    public:
        VideoList(const QString& _prefix = "");
        virtual ~VideoList();

        GenericTree *buildVideoList(bool filebrowser, bool flatlist,
                                                        int parental_level);
        Metadata *getVideoListMetadata(int index);
        void wantVideoListUpdirs(bool yes);
        VideoFilterSettings *getCurrentVideoFilter() { return currentVideoFilter; }
        unsigned int count(void) const { return metas.count(); }

    private:
        void buildFsysList(bool flatlist, int parental_level);
        void buildDbList(bool flatlist, int parental_level);
        void buildFileList(const QString& directory);
        bool ignoreExtension(const QString& extension) const;

        void removeUpnodes(GenericTree *parent);
        void addUpnodes(GenericTree *parent);
        GenericTree *addDirNode(GenericTree *where_to_add,
                                                        const QString& dname);
        GenericTree *addFileNode(GenericTree *where_to_add,
                                                const QString& fname, int id);

        bool m_ListUnknown;
        bool m_LoadMetaData;
        QMap<QString,bool> m_IgnoreList;

        QSqlDatabase *db;
        int nitems;      // Number of real items in the tree
        bool has_updirs; // True if tree has updirs
        QStringList  browser_mode_files;
        GenericTree *video_tree_root;
        QValueVector<Metadata> metas;   // sorted/indexed by current filter
        VideoFilterSettings *currentVideoFilter;
};

#endif // VIDEOLIST_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */

#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>
#include <mythtv/mythmediamonitor.h>

#include "videolist.h"
#include "videofilter.h"
#include "metadata.h"
#include "metadatalistmanager.h"
#include "dbaccess.h"

#include "quicksp.h"
#include "dirscan.h"
#include "videoutils.h"

#include <memory>
#include <algorithm>
#include <iterator>
#include <map>

namespace
{
    class meta_node
    {
      public:
        meta_node(meta_node *parent, bool is_path_root = false) :
            m_parent(parent), m_path_root(is_path_root) {}
        virtual ~meta_node() {}

        virtual const QString &getName() const = 0;

        virtual const QString &getPath() const
        {
            return m_empty_path;
        }

        const QString &getFQPath()
        {
            if (m_fq_path.length())
                return m_fq_path;

            if (m_parent && !m_path_root)
                m_fq_path = m_parent->getFQPath() + "/" + getPath();
            else
            {
                QString p = getPath();
                m_fq_path = ((p.length() && p[0] != '/') ? "/" : "") + p;
            }

            return m_fq_path;
        }

        void setParent(meta_node *parent)
        {
            m_parent = parent;
        }

        void setPathRoot(bool is_root = true)
        {
            m_path_root = is_root;
        }

      protected:
        meta_node *m_parent;

      private:
        QString m_fq_path;
        bool m_path_root;
        static const QString m_empty_path;
    };
    const QString meta_node::m_empty_path;

    class meta_data_node : public meta_node
    {
      public:
        meta_data_node(Metadata *data, meta_node *parent = NULL) :
            meta_node(parent), m_data(data)
        {
        }

        const QString &getName() const
        {
            if (m_data)
            {
                return m_data->Title();
            }

            return m_meta_bug;
        }

        const Metadata *getData() const
        {
            return m_data;
        }

        Metadata *getData()
        {
            return m_data;
        }

      private:
        Metadata *m_data;
        static const QString m_meta_bug;
    };
    const QString meta_data_node::m_meta_bug = "Bug";

    class meta_dir_node;

    typedef simple_ref_ptr<meta_dir_node> smart_dir_node;
    typedef simple_ref_ptr<meta_data_node> smart_meta_node;

    typedef std::list<smart_dir_node> meta_dir_list;
    typedef std::list<smart_meta_node> meta_data_list;

    class meta_dir_node : public meta_node
    {
      public:
        typedef meta_dir_list::iterator dir_iterator;
        typedef meta_dir_list::const_iterator const_dir_iterator;

        typedef meta_data_list::iterator entry_iterator;
        typedef meta_data_list::const_iterator const_entry_iterator;

      public:
        meta_dir_node(const QString &path, const QString &name = "",
                      meta_dir_node *parent = NULL, bool is_path_root = false) :
            meta_node(parent, is_path_root), m_path(path), m_name(name)
        {
            if (!name.length())
            {
                m_name = path;
            }
        }

        meta_dir_node() : meta_node(NULL)
        {
        }

        void setName(const QString &name)
        {
            m_name = name;
        }

        const QString &getName() const
        {
            return m_name;
        }

        const QString &getPath() const
        {
            return m_path;
        }

        void setPath(const QString &path)
        {
            m_path = path;
        }

        smart_dir_node addSubDir(const QString &subdir,
                                 const QString &name = "")
        {
            return getSubDir(subdir, name, true);
        }

        void addSubDir(const smart_dir_node &subdir)
        {
            m_subdirs.push_back(subdir);
        }

        smart_dir_node getSubDir(const QString &subdir,
                                 const QString &name = "", bool create = true)
        {
            for (meta_dir_list::const_iterator p = m_subdirs.begin();
                 p != m_subdirs.end(); ++p)
            {
                if (subdir == (*p)->getPath())
                {
                    return *p;
                }
            }

            if (create)
            {
                smart_dir_node node(new meta_dir_node(subdir, name, this));
                m_subdirs.push_back(node);
                return node;
            }

            return smart_dir_node();
        }

        void addEntry(const smart_meta_node &entry)
        {
            entry->setParent(this);
            m_entires.push_back(entry);
        }

        void clear()
        {
            m_subdirs.clear();
            m_entires.clear();
        }

        bool empty() const
        {
            return m_subdirs.empty() && m_entires.empty();
        }

        int subdir_count() const {
            return m_subdirs.size();
        }

        template <typename DirSort, typename EntrySort>
        void sort(DirSort dir_sort, EntrySort entry_sort)
        {
            m_subdirs.sort(dir_sort);
            m_entires.sort(entry_sort);

            for (meta_dir_list::iterator p = m_subdirs.begin();
                 p != m_subdirs.end(); ++p)
            {
                (*p)->sort(dir_sort, entry_sort);
            }
        }

        dir_iterator dirs_begin()
        {
            return m_subdirs.begin();
        }

        dir_iterator dirs_end()
        {
            return m_subdirs.end();
        }

        const_dir_iterator dirs_begin() const
        {
            return m_subdirs.begin();
        }

        const_dir_iterator dirs_end() const
        {
            return m_subdirs.end();
        }

        entry_iterator entries_begin()
        {
            return m_entires.begin();
        }

        entry_iterator entries_end()
        {
            return m_entires.end();
        }

        const_entry_iterator entries_begin() const
        {
            return m_entires.begin();
        }

        const_entry_iterator entries_end() const
        {
            return m_entires.end();
        }

      private:
        QString m_path;
        QString m_name;
        meta_dir_list m_subdirs;
        meta_data_list m_entires;
    };

    /// metadata sort functor
    struct metadata_sort
    {
        metadata_sort(const VideoFilterSettings &vfs) : m_vfs(vfs) {}

        bool operator()(const Metadata *lhs, const Metadata *rhs)
        {
            return m_vfs.meta_less_than(*lhs, *rhs);
        }

        bool operator()(const smart_meta_node &lhs, const smart_meta_node &rhs)
        {
            return m_vfs.meta_less_than(*(lhs->getData()), *(rhs->getData()));
        }

      private:
        const VideoFilterSettings &m_vfs;
    };

    struct metadata_path_sort
    {
        metadata_path_sort(bool ignore_case) : m_ignore_case(ignore_case) {}

        bool operator()(const Metadata &lhs, const Metadata &rhs)
        {
            return sort(&lhs, &rhs);
        }

        bool operator()(const Metadata *lhs, const Metadata *rhs)
        {
            return sort(lhs, rhs);
        }

        bool operator()(const smart_dir_node &lhs, const smart_dir_node &rhs)
        {
            return sort(lhs->getPath(), rhs->getPath());
        }

      private:
        bool sort(const Metadata *lhs, const Metadata *rhs)
        {
            return sort(lhs->Filename(), rhs->Filename());
        }

        bool sort(const QString &lhs, const QString &rhs)
        {
            QString lhs_comp(lhs);
            QString rhs_comp(rhs);
            if (m_ignore_case)
            {
                lhs_comp = lhs_comp.lower();
                rhs_comp = rhs_comp.lower();
            }
            return QString::localeAwareCompare(lhs_comp, rhs_comp) < 0;
        }

        bool m_ignore_case;
    };

    QString path_to_node_name(const QString &path)
    {
        QString ret;
        int slashLoc = path.findRev("/", -2) + 1;
        if (path.right(1) == "/")
            ret = path.mid(slashLoc, path.length() - slashLoc - 2);
        else
            ret = path.mid(slashLoc);

        return ret;
    }

    meta_dir_node *AddMetadataToDir(Metadata *metadata, meta_dir_node *dir,
                     meta_dir_node *hint = NULL)
    {
        meta_dir_node *start = dir;
        QString insert_chunk = metadata->Filename();

        if (hint)
        {
            if (metadata->Filename().startsWith(hint->getFQPath()))
            {
                start = hint;
                insert_chunk =
                        metadata->Filename().mid(hint->getFQPath().length());
            }
        }

        if (insert_chunk.startsWith(dir->getFQPath()))
        {
            insert_chunk = metadata->Filename().mid(dir->getFQPath().length());
        }

        QStringList path = QStringList::split("/", insert_chunk);
        if (path.size() > 1)
        {
            path.pop_back();
        }
        else
        {
            path.clear();
        }

        for (QStringList::const_iterator p = path.begin(); p != path.end(); ++p)
        {
            smart_dir_node sdn = start->addSubDir(*p);
            start = sdn.get();
        }

        start->addEntry(smart_meta_node(new meta_data_node(metadata)));

        return start;
    }

    struct to_metadata_ptr
    {
        Metadata *operator()(smart_meta_node &smn)
        {
            return smn->getData();
        }

        Metadata *operator()(Metadata &data)
        {
            return &data;
        }

        Metadata *operator()(const MetadataListManager::MetadataPtr &data)
        {
            return data.get();
        }
    };

    // TODO: AEW We don't actually use this now
    // Ordering of items on a tree level (low -> high)
    enum NodeOrder {
        kOrderUp,
        kOrderSub,
        kOrderItem
    };
}

class VideoListImp
{
  public:
    typedef std::vector<Metadata *> metadata_view_list;

  private:
    enum metadata_list_type { ltNone, ltFileSystem, ltDBMetadata };
    typedef MetadataListManager::metadata_list metadata_list;
    typedef MetadataListManager::MetadataPtr MetadataPtr;

  public:
    VideoListImp();

    void build_generic_tree(GenericTree *dst, meta_dir_node *src,
                            bool include_updirs);
    GenericTree *buildVideoList(bool filebrowser, bool flatlist,
                                int parental_level, bool include_updirs);

    void refreshList(bool filebrowser, int parental_level, bool flat_list);
    void resortList(bool flat_list);

    Metadata *getVideoListMetadata(int index);

    unsigned int count() const
    {
        return m_metadata_view_flat.size();
    }

    const VideoFilterSettings &getCurrentVideoFilter()
    {
        return m_video_filter;
    }

    void setCurrentVideoFilter(const VideoFilterSettings &filter)
    {
        m_video_filter = filter;
    }

    int test_filter(const VideoFilterSettings &filter) const
    {
        int ret = 0;
        for (metadata_list::const_iterator p = m_metadata.getList().begin();
             p != m_metadata.getList().end(); ++p)
        {
            if (filter.matches_filter(**p)) ++ret;
        }
        return ret;
    }

    const MetadataListManager &getListCache() const
    {
        return m_metadata;
    }

    QString getFolderPath(int folder_id) const
    {
        QString ret;
        id_string_map::const_iterator p = m_folder_id_to_path.find(folder_id);
        if (p != m_folder_id_to_path.end())
            ret = p->second;
        return ret;
    }

    unsigned int getFilterChangedState()
    {
        return m_video_filter.getChangedState();
    }

    bool Delete(unsigned int video_id)
    {
        bool ret = false;
        MetadataPtr mp = m_metadata.byID(video_id);
        if (mp)
        {
            ret = mp->deleteFile();
            if (ret) ret = m_metadata.purgeByID(video_id);
        }

        return ret;
    }

  private:
    void update_flat_index();
    void sort_view_data(bool flat_list);
    void fillMetadata(metadata_list_type whence);

    void buildFsysList();
    void buildDbList();
    void buildFileList(smart_dir_node &directory, metadata_list &metalist,
                       const QString &prefix);

    GenericTree *addDirNode(GenericTree *where_to_add, const QString &dname,
                            bool add_up_dirs);
    int addFileNode(GenericTree *where_to_add, const QString &name, int id);

    void update_meta_view(bool flat_list);

  private:
    bool m_ListUnknown;
    bool m_LoadMetaData;

    std::auto_ptr<GenericTree> video_tree_root;

    MetadataListManager m_metadata;
    meta_dir_node m_metadata_tree; // master list for tree views

    metadata_view_list m_metadata_view_flat;
    meta_dir_node m_metadata_view_tree;

    metadata_list_type m_metadata_list_type;

    VideoFilterSettings m_video_filter;

    bool m_sort_ignores_case;

    // TODO: ugly
    typedef std::map<int, QString> id_string_map;
    id_string_map m_folder_id_to_path;
    int m_folder_id;
};

VideoList::VideoList()
{
    m_imp = new VideoListImp;
}

VideoList::~VideoList()
{
    delete m_imp;
}

GenericTree *VideoList::buildVideoList(bool filebrowser, bool flatlist,
    int parental_level, bool include_updirs)
{
    return m_imp->buildVideoList(filebrowser, flatlist, parental_level,
                                 include_updirs);
}

void VideoList::refreshList(bool filebrowser, int parental_level,
                            bool flat_list)
{
    m_imp->refreshList(filebrowser, parental_level, flat_list);
}

void VideoList::resortList(bool flat_list)
{
    m_imp->resortList(flat_list);
}

Metadata *VideoList::getVideoListMetadata(int index)
{
    return m_imp->getVideoListMetadata(index);
}

const Metadata *VideoList::getVideoListMetadata(int index) const
{
    return m_imp->getVideoListMetadata(index);
}

unsigned int VideoList::count() const
{
    return m_imp->count();
}

const VideoFilterSettings &VideoList::getCurrentVideoFilter()
{
    return m_imp->getCurrentVideoFilter();
}

void VideoList::setCurrentVideoFilter(const VideoFilterSettings &filter)
{
    m_imp->setCurrentVideoFilter(filter);
}

int VideoList::test_filter(const VideoFilterSettings &filter) const
{
    return m_imp->test_filter(filter);
}

const MetadataListManager &VideoList::getListCache() const
{
    return m_imp->getListCache();
}

QString VideoList::getFolderPath(int folder_id) const
{
    return m_imp->getFolderPath(folder_id);
}

unsigned int VideoList::getFilterChangedState()
{
    return m_imp->getFilterChangedState();
}

bool VideoList::Delete(int video_id)
{
    return m_imp->Delete(video_id);
}

//////////////////////////////
// VideoListImp
//////////////////////////////
VideoListImp::VideoListImp() : m_metadata_view_tree("", "top"),
    m_metadata_list_type(ltNone)
{
    m_ListUnknown = gContext->GetNumSetting("VideoListUnknownFileTypes", 1);

    m_LoadMetaData = gContext->GetNumSetting("VideoTreeLoadMetaData", 0);

    m_sort_ignores_case =
            gContext->GetNumSetting("mythvideo.sort_ignores_case", 1);
}

void VideoListImp::build_generic_tree(GenericTree *dst, meta_dir_node *src,
                                      bool include_updirs)
{
    for (meta_dir_node::const_dir_iterator dir = src->dirs_begin();
         dir != src->dirs_end(); ++dir)
    {
        GenericTree *t = addDirNode(dst, (*dir)->getName(), include_updirs);
        t->setAttribute(kFolderPath, m_folder_id);
        m_folder_id_to_path.
                insert(id_string_map::value_type(m_folder_id,
                                                 (*dir)->getFQPath()));
        ++m_folder_id;

        build_generic_tree(t, dir->get(), include_updirs);
    }

    for (meta_dir_node::const_entry_iterator entry = src->entries_begin();
         entry != src->entries_end(); ++entry)
    {
        addFileNode(dst, (*entry)->getData()->Title(),
                    (*entry)->getData()->getFlatIndex());
    }
}

// Build a generic tree containing the video files. You can control the
// contents and the shape of the tree in de following ways:
//   filebrowser:
//      If true, the actual state of the filesystem is examined. If a video
//      is already known to the system, this info is retrieved. If not, some
//      basic info is provided.
//      If false, only video information already present in the database is
//      presented.
//   flatlist:
//      If true, the tree is reduced to a single level containing all the
//      videos found.
//      If false, the hierarchy present on the filesystem or in the database
//      is preserved. In this mode, both sub-dirs and updirs are present.
GenericTree *VideoListImp::buildVideoList(bool filebrowser, bool flatlist,
                                          int parental_level,
                                          bool include_updirs)
{
    refreshList(filebrowser, parental_level, flatlist);

    typedef std::map<QString, GenericTree *> string_to_tree;
    string_to_tree prefix_tree_map;

    video_tree_root.reset(new GenericTree("video root", kRootNode, false));

    m_folder_id_to_path.clear();
    m_folder_id = 1;
    build_generic_tree(video_tree_root.get(), &m_metadata_view_tree,
                       include_updirs);

    if (m_metadata_view_flat.empty())
    {
        video_tree_root.reset(new GenericTree("video root", kRootNode, false));
        addDirNode(video_tree_root.get(), QObject::tr("No files found"),
                   include_updirs);
    }

    return video_tree_root.get();
}

void VideoListImp::refreshList(bool filebrowser, int parental_level,
                               bool flat_list)
{
    m_video_filter.setParentalLevel(parental_level);

    fillMetadata(filebrowser ? ltFileSystem : ltDBMetadata);

    update_meta_view(flat_list);
}

void VideoListImp::resortList(bool flat_list)
{
    sort_view_data(flat_list);
    update_flat_index();
}

Metadata *VideoListImp::getVideoListMetadata(int index)
{
    if (index < 0)
        return NULL;    // Special node types

    if ((unsigned int)index < m_metadata_view_flat.size())
        return m_metadata_view_flat[index];

    VERBOSE(VB_IMPORTANT,
            QString("%1: getVideoListMetadata: index out of bounds: %2")
            .arg(__FILE__).arg(index));
    return NULL;
}

void VideoListImp::update_flat_index()
{
    // Update the flat index (tree building helper)
    // TODO: fix VideoList to have an iterator and add ability to fetch
    // metadata via cookie.
    int flat_index = 0;
    for (metadata_view_list::iterator p = m_metadata_view_flat.begin();
         p != m_metadata_view_flat.end(); ++p)
    {
        (*p)->setFlatIndex(flat_index++);
    }
}

void VideoListImp::sort_view_data(bool flat_list)
{
    if (flat_list)
    {
        std::sort(m_metadata_view_flat.begin(), m_metadata_view_flat.end(),
                  metadata_sort(m_video_filter));
    }
    else
    {
        m_metadata_view_tree.sort(metadata_path_sort(m_sort_ignores_case),
                                  metadata_sort(m_video_filter));
    }
}

void VideoListImp::fillMetadata(metadata_list_type whence)
{
    if (m_metadata_list_type != whence)
    {
        m_metadata_list_type = whence;
        // flush existing data
        metadata_list ml;
        m_metadata.setList(ml);
        m_metadata_tree.clear();

        if (whence == ltFileSystem)
        {
            buildFsysList();
        }
        else
        {
            buildDbList();
        }
    }
}

void VideoListImp::buildDbList()
{
    metadata_list ml;
    MetadataListManager::loadAllFromDatabase(ml);
    m_metadata.setList(ml);

    metadata_view_list mlist;
    mlist.reserve(m_metadata.getList().size());

    std::back_insert_iterator<metadata_view_list> mli(mlist);
    std::transform(m_metadata.getList().begin(), m_metadata.getList().end(),
                   mli, to_metadata_ptr());

//    print_meta_list(mlist);

    metadata_path_sort mps(m_sort_ignores_case);
    std::sort(mlist.begin(), mlist.end(), mps);

    // TODO: break out the prefix in the DB so this isn't needed
    typedef std::map<QString, meta_dir_node *> prefix_to_node_map;
    prefix_to_node_map ptnm;

    QStringList dirs = GetVideoDirs();
    QString test_prefix(dirs[0]);

    meta_dir_node *video_root = &m_metadata_tree;
    if (dirs.size() == 1)
    {
        video_root->setPathRoot();
        video_root->setPath(test_prefix);
        video_root->setName("videos");
        ptnm.insert(prefix_to_node_map::value_type(test_prefix, video_root));
    }

    smart_dir_node unknown_prefix_root(new meta_dir_node("",
                                               QObject::tr("Unknown Prefix"),
                                               NULL, true));

    meta_dir_node *insert_hint = NULL;
    for (metadata_view_list::iterator p = mlist.begin(); p != mlist.end(); ++p)
    {
        bool found_prefix = false;
        if ((*p)->Filename().startsWith(test_prefix))
        {
            found_prefix = true;
        }
        else
        {
            for (QStringList::const_iterator prefix = dirs.begin();
                 prefix != dirs.end(); ++prefix)
            {
                if ((*p)->Filename().startsWith(*prefix))
                {
                    test_prefix = *prefix;
                    found_prefix = true;
                    break;
                }
            }
        }

        if (found_prefix)
        {
            meta_dir_node *insert_base;
            prefix_to_node_map::iterator np = ptnm.find(test_prefix);
            if (np == ptnm.end())
            {
                smart_dir_node sdn =
                        video_root->addSubDir(test_prefix,
                                              path_to_node_name(test_prefix));
                insert_base = sdn.get();
                insert_base->setPathRoot();

                ptnm.insert(prefix_to_node_map::value_type(test_prefix,
                                                           insert_base));
            }
            else
            {
                insert_base = np->second;
            }

            (*p)->setPrefix(test_prefix);
            insert_hint = AddMetadataToDir(*p, insert_base, insert_hint);
        }
        else
        {
            AddMetadataToDir(*p, unknown_prefix_root.get());
        }
    }

    if (!unknown_prefix_root->empty())
    {
        video_root->addSubDir(unknown_prefix_root);
    }

//    print_dir_tree(m_metadata_tree); // AEW DEBUG
}

void VideoListImp::buildFsysList()
{
    //
    //  Fill metadata from directory structure
    //

    typedef std::vector<std::pair<QString, QString> > node_to_path_list;

    node_to_path_list node_paths;

    QStringList dirs = GetVideoDirs();
    if (dirs.size() > 1)
    {
        for (QStringList::iterator iter = dirs.begin(); iter != dirs.end();
             ++iter)
        {
            node_paths.push_back(
                    node_to_path_list::value_type(path_to_node_name(*iter),
                                                  *iter));
        }
    }
    else
    {
        node_paths.push_back(
                node_to_path_list::value_type(QObject::tr("videos"), dirs[0]));
    }

    //
    // See if there are removable media available, so we can add them
    // to the tree.
    //
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon)
    {
        QValueList <MythMediaDevice*> medias = mon->GetMedias(MEDIATYPE_DATA);

        for (QValueList <MythMediaDevice*>::Iterator itr = medias.begin();
             itr != medias.end(); ++itr)
        {
            MythMediaDevice *pDev = *itr;
            if (mon->ValidateAndLock(pDev))
            {
                QString path = pDev->getMountPath();
                node_paths.push_back(
                        node_to_path_list::value_type(path_to_node_name(path),
                                                      path));

                mon->Unlock(pDev);
            }
        }
    }

    //
    // Add all root-nodes to the tree.
    //
    metadata_list ml;
    for (node_to_path_list::iterator p = node_paths.begin();
         p != node_paths.end(); ++p)
    {
        smart_dir_node root = m_metadata_tree.addSubDir(p->second, p->first);
        root->setPathRoot();

        buildFileList(root, ml, p->second);
    }

    // See if we can find this filename in DB
    if (m_LoadMetaData)
    {
        // Load the DB data so metadata lookups work
        // TODO: ugly, pass the list
        MetadataListManager mdlm;
        metadata_list db_metadata;
        MetadataListManager::loadAllFromDatabase(db_metadata);
        mdlm.setList(db_metadata);
        for (metadata_list::iterator p = ml.begin(); p != ml.end(); ++p)
        {
            (*p)->fillDataFromFilename(mdlm);
        }
    }
    m_metadata.setList(ml);
}

GenericTree *VideoListImp::addDirNode(GenericTree *where_to_add,
                                      const QString &dname, bool add_up_dirs)
{
    // Add the subdir node...
    GenericTree *sub_node = where_to_add->addNode(dname, kSubFolder, false);
    sub_node->setAttribute(kNodeSort, kOrderSub);
    sub_node->setOrderingIndex(kNodeSort);

    // ...and the updir node.
    if (add_up_dirs)
    {
        GenericTree *up_node = sub_node->addNode(where_to_add->getString(),
                                                 kUpFolder, true);
        up_node->setAttribute(kNodeSort, kOrderUp);
        up_node->setOrderingIndex(kNodeSort);
    }

    return sub_node;
}

int VideoListImp::addFileNode(GenericTree *where_to_add, const QString &name,
                              int index)
{
    GenericTree *sub_node = where_to_add->addNode(name, index, true);
    sub_node->setAttribute(kNodeSort, kOrderItem);
    sub_node->setOrderingIndex(kNodeSort);

    return 1;
}

namespace
{
    void copy_entries(meta_dir_node &dst, meta_dir_node &src,
                      const VideoFilterSettings &filter)
    {
        for (meta_dir_node::entry_iterator e = src.entries_begin();
             e != src.entries_end(); ++e)
        {
            if (filter.matches_filter(*((*e)->getData())))
            {
                dst.addEntry(
                    smart_meta_node(new meta_data_node((*e)->getData())));
            }
        }
    }

    void copy_filtered_tree(meta_dir_node &dst, meta_dir_node &src,
                            const VideoFilterSettings &filter)
    {
        copy_entries(dst, src, filter);
        for (meta_dir_node::dir_iterator dir = src.dirs_begin();
             dir != src.dirs_end(); ++dir)
        {
            smart_dir_node sdn = dst.addSubDir((*dir)->getPath(),
                                               (*dir)->getName());
            copy_filtered_tree(*sdn, *(dir->get()), filter);
        }
    }

    void tree_view_to_flat(meta_dir_node &tree,
                           VideoListImp::metadata_view_list &flat);
    struct call_tree_flat
    {
        call_tree_flat(VideoListImp::metadata_view_list &list) : m_list(list) {}

        void operator()(smart_dir_node &sdn)
        {
            tree_view_to_flat(*(sdn.get()), m_list);
        }

        VideoListImp::metadata_view_list &m_list;
    };

    // Fills a flat view with pointers to all entries in a tree.
    void tree_view_to_flat(meta_dir_node &tree,
                           VideoListImp::metadata_view_list &flat)
    {
        std::back_insert_iterator<VideoListImp::metadata_view_list> bip(flat);
        std::transform(tree.entries_begin(), tree.entries_end(), bip,
                       to_metadata_ptr());

        std::for_each(tree.dirs_begin(), tree.dirs_end(), call_tree_flat(flat));
    }
}

void VideoListImp::update_meta_view(bool flat_list)
{
    m_metadata_view_flat.clear();
    m_metadata_view_flat.reserve(m_metadata.getList().size());

    m_metadata_view_tree.clear();

    // a big punt on setting the sort key
    // TODO: put this in the DB, half the time in this function is spent
    // doing this.
    for (metadata_list::const_iterator si = m_metadata.getList().begin();
         si != m_metadata.getList().end(); ++si)
    {
        if (!(*si)->hasSortKey())
        {
            QString skey =
                    Metadata::GenerateDefaultSortKey(*(*si),
                                                     m_sort_ignores_case);
            (*si)->setSortKey(skey);
        }
    }

    if (flat_list)
    {
        for (metadata_list::const_iterator p = m_metadata.getList().begin();
             p != m_metadata.getList().end(); ++p)
        {
            if (m_video_filter.matches_filter(*(*p)))
            {
                m_metadata_view_flat.push_back(p->get());
            }
        }

        sort_view_data(flat_list);

        for (metadata_view_list::iterator p = m_metadata_view_flat.begin();
             p != m_metadata_view_flat.end(); ++p)
        {
            m_metadata_view_tree.addEntry(new meta_data_node(*p));
        }
    }
    else
    {
        m_metadata_view_tree.setPath(m_metadata_tree.getPath());
        m_metadata_view_tree.setName(m_metadata_tree.getName());
        copy_filtered_tree(m_metadata_view_tree, m_metadata_tree,
                           m_video_filter);

        sort_view_data(flat_list);

        tree_view_to_flat(m_metadata_view_tree, m_metadata_view_flat);
    }

    update_flat_index();
}

namespace
{
    class dirhandler : public DirectoryHandler
    {
      public:
        typedef std::list<simple_ref_ptr<DirectoryHandler> > free_list;

      public:
        dirhandler(smart_dir_node &directory, const QString &prefix,
                   MetadataListManager::metadata_list &metalist,
                   free_list &dh_free_list) :
            m_directory(directory), m_prefix(prefix), m_metalist(metalist),
            m_dh_free_list(dh_free_list)
        {
        }

        DirectoryHandler *newDir(const QString &dir_name,
                                 const QString &fq_dir_name)
        {
            (void)fq_dir_name;
            smart_dir_node dir = m_directory->addSubDir(dir_name);
            DirectoryHandler *dh = new dirhandler(dir, m_prefix, m_metalist,
                                                  m_dh_free_list);
            m_dh_free_list.push_back(dh);
            return dh;
        }

        void handleFile(const QString &file_name,
                        const QString &fq_file_name,
                        const QString &extension)
        {
            (void)file_name;
            (void)extension;
            QString file_string(fq_file_name);

            MetadataListManager::MetadataPtr myData(new Metadata(file_string));
            QString title = Metadata::FilenameToTitle(file_string);
            if (!title.length()) title = file_string.section("/", -1);
            myData->setTitle(title);
            myData->setPrefix(m_prefix);

            m_metalist.push_back(myData);

            m_directory->addEntry(new meta_data_node(myData.get()));
        }

      private:
        smart_dir_node m_directory;
        const QString &m_prefix;
        MetadataListManager::metadata_list &m_metalist;
        free_list &m_dh_free_list;
    };
}

void VideoListImp::buildFileList(smart_dir_node &directory,
                                 metadata_list &metalist, const QString &prefix)
{
    FileAssociations::ext_ignore_list ext_list;
    FileAssociations::getFileAssociation().getExtensionIgnoreList(ext_list);

    dirhandler::free_list fl;
    dirhandler dh(directory, prefix, metalist, fl);
    ScanVideoDirectory(directory->getFQPath(), &dh, ext_list, m_ListUnknown);
}

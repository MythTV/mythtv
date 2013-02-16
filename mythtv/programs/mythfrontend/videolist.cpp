#include <algorithm>
#include <iterator>
#include <map>
using namespace std;

#include <QScopedPointer>
#include <QFileInfo>
#include <QList>

#include "mythcontext.h"
#include "mythdate.h"

#include "mythgenerictree.h"
#include "videometadatalistmanager.h"
#include "dbaccess.h"
#include "quicksp.h"
#include "dirscan.h"
#include "videoutils.h"
#include "parentalcontrols.h"

#include "videofilter.h"
#include "videolist.h"
#include "videodlg.h"

#include "upnpscanner.h"

class TreeNodeDataPrivate
{
  public:
    TreeNodeDataPrivate(VideoMetadata *metadata) :
        m_metadata(metadata)
    {
        if (m_metadata)
            m_host = m_metadata->GetHost();
        else
            m_host = "";
    }

    TreeNodeDataPrivate(QString path, QString host, QString prefix) :
        m_metadata(0), m_host(host), m_path(path), m_prefix(prefix)
    {
    }

    VideoMetadata *GetMetadata(void)
    {
        return m_metadata;
    }

    const VideoMetadata *GetMetadata(void) const
    {
        return m_metadata;
    }

    QString GetPath(void) const
    {
        return m_path;
    }

    QString GetHost(void) const
    {
        return m_host;
    }

    QString GetPrefix(void) const
    {
        return m_prefix;
    }

  private:
    VideoMetadata *m_metadata;
    QString m_host;
    QString m_path;
    QString m_prefix;
};

TreeNodeData::TreeNodeData() : m_d(0)
{
}

TreeNodeData::TreeNodeData(VideoMetadata *metadata)
{
    m_d = new TreeNodeDataPrivate(metadata);
}

TreeNodeData::TreeNodeData(QString path, QString host, QString prefix)
{
    m_d = new TreeNodeDataPrivate(path, host, prefix);
}

TreeNodeData::TreeNodeData(const TreeNodeData &other) : m_d(0)
{
    *this = other;
}

TreeNodeData &TreeNodeData::operator=(const TreeNodeData &rhs)
{
    if (this != &rhs)
    {
        delete m_d;
        m_d = new TreeNodeDataPrivate(*rhs.m_d);
    }

    return *this;
}

TreeNodeData::~TreeNodeData()
{
    delete m_d;
}

VideoMetadata *TreeNodeData::GetMetadata(void)
{
    if (m_d)
        return m_d->GetMetadata();

    return NULL;
}

const VideoMetadata *TreeNodeData::GetMetadata(void) const
{
    if (m_d)
        return m_d->GetMetadata();

    return NULL;
}

QString TreeNodeData::GetPath(void) const
{
    if (m_d)
        return m_d->GetPath();
    return QString();
}

QString TreeNodeData::GetHost(void) const
{
    if (m_d)
        return m_d->GetHost();
    return QString();
}

QString TreeNodeData::GetPrefix(void) const
{
    if (m_d)
        return m_d->GetPrefix();
    return QString();
}

/// metadata sort function
struct metadata_sort
{
    metadata_sort(const VideoFilterSettings &vfs, bool sort_ignores_case) :
        m_vfs(vfs), m_sic(sort_ignores_case) {}

    bool operator()(const VideoMetadata *lhs, const VideoMetadata *rhs)
    {
        return m_vfs.meta_less_than(*lhs, *rhs, m_sic);
    }

    bool operator()(const smart_meta_node &lhs, const smart_meta_node &rhs)
    {
        return m_vfs.meta_less_than(*(lhs->getData()), *(rhs->getData()),
                                    m_sic);
    }

  private:
    const VideoFilterSettings &m_vfs;
    bool m_sic;
};

struct metadata_path_sort
{
    metadata_path_sort(bool ignore_case) : m_ignore_case(ignore_case) {}

    bool operator()(const VideoMetadata &lhs, const VideoMetadata &rhs)
    {
        return sort(&lhs, &rhs);
    }

    bool operator()(const VideoMetadata *lhs, const VideoMetadata *rhs)
    {
        return sort(lhs, rhs);
    }

    bool operator()(const smart_dir_node &lhs, const smart_dir_node &rhs)
    {
        return sort(lhs->getPath(), rhs->getPath());
    }

  private:
    bool sort(const VideoMetadata *lhs, const VideoMetadata *rhs)
    {
        return sort(lhs->GetFilename(), rhs->GetFilename());
    }

    bool sort(const QString &lhs, const QString &rhs)
    {
        QString lhs_comp(lhs);
        QString rhs_comp(rhs);
        if (m_ignore_case)
        {
            lhs_comp = lhs_comp.toLower();
            rhs_comp = rhs_comp.toLower();
        }
        return QString::localeAwareCompare(lhs_comp, rhs_comp) < 0;
    }

    bool m_ignore_case;
};

static QString path_to_node_name(const QString &path)
{
    QString ret;
    int slashLoc = path.lastIndexOf('/', -2) + 1;
    if (path.right(1) == "/")
        ret = path.mid(slashLoc, path.length() - slashLoc - 1);
    else
        ret = path.mid(slashLoc);

    return ret;
}

static meta_dir_node *AddMetadataToDir(VideoMetadata *metadata,
                                       meta_dir_node *dir,
                                       meta_dir_node *hint = NULL)
{
    meta_dir_node *start = dir;
    QString insert_chunk = metadata->GetFilename();
    QString host = metadata->GetHost();
    QString prefix = metadata->GetPrefix();

    if (hint)
    {
        if (metadata->GetFilename().startsWith(hint->getFQPath() + "/"))
        {
            start = hint;
            insert_chunk =
                metadata->GetFilename().mid(hint->getFQPath().length());
        }
    }

    if (insert_chunk.startsWith(dir->getFQPath() + "/"))
    {
        insert_chunk = metadata->GetFilename().mid(dir->getFQPath().length());
    }

    QStringList path = insert_chunk.split("/", QString::SkipEmptyParts);
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
        smart_dir_node sdn = start->addSubDir(*p, "" , host, prefix);
        start = sdn.get();
    }

    start->addEntry(smart_meta_node(new meta_data_node(metadata)));

    return start;
}

struct to_metadata_ptr
{
    VideoMetadata *operator()(smart_meta_node &smn)
    {
        return smn->getData();
    }

    VideoMetadata *operator()(VideoMetadata &data)
    {
        return &data;
    }

    VideoMetadata *operator()(
        const VideoMetadataListManager::VideoMetadataPtr &data)
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

static MythGenericTree *AddDirNode(
    MythGenericTree *where_to_add,
    QString name, QString fqPath, bool add_up_dirs,
    QString host = "", QString prefix = "")
{
    // Add the subdir node...
    MythGenericTree *sub_node =
        where_to_add->addNode(name, kSubFolder, false);
    sub_node->setAttribute(kNodeSort, kOrderSub);
    sub_node->setOrderingIndex(kNodeSort);
    sub_node->SetData(QVariant::fromValue(TreeNodeData(fqPath, host, prefix)));
    sub_node->SetText(name, "title");
    sub_node->DisplayState("subfolder", "nodetype");

    // ...and the updir node.
    if (add_up_dirs)
    {
        MythGenericTree *up_node =
            sub_node->addNode(where_to_add->getString(), kUpFolder,
                              true, false);
        up_node->setAttribute(kNodeSort, kOrderUp);
        up_node->setOrderingIndex(kNodeSort);
        up_node->DisplayState("subfolder", "nodetype");
    }

    return sub_node;
}

static int AddFileNode(MythGenericTree *where_to_add, QString name,
                       VideoMetadata *metadata)
{
    MythGenericTree *sub_node = where_to_add->addNode(name, 0, true);
    sub_node->setAttribute(kNodeSort, kOrderItem);
    sub_node->setOrderingIndex(kNodeSort);
    sub_node->SetData(QVariant::fromValue(TreeNodeData(metadata)));

    // Text
    QHash<QString, QString> textMap;
    metadata->toMap(textMap);
    sub_node->SetTextFromMap(textMap);

    // Images
    QHash<QString, QString> imageMap;
    metadata->GetImageMap(imageMap);
    sub_node->SetImageFromMap(imageMap);
    sub_node->SetImage("buttonimage", imageMap["smartimage"]);

    // Assign images to parent node if this is the first child
    if (where_to_add->visibleChildCount() == 1 &&
        where_to_add->getInt() == kSubFolder)
    {
        where_to_add->SetImageFromMap(imageMap);
        where_to_add->SetImage("buttonimage", imageMap["smartimage"]);
    }

    // Statetypes
    QHash<QString, QString> stateMap;
    metadata->GetStateMap(stateMap);
    sub_node->DisplayStateFromMap(stateMap);

    return 1;
}

class VideoListImp
{
  public:
    typedef vector<VideoMetadata *> metadata_view_list;

  private:
    enum metadata_list_type { ltNone, ltFileSystem, ltDBMetadata,
                              ltDBGenreGroup, ltDBCategoryGroup,
                              ltDBYearGroup, ltDBDirectorGroup,
                              ltDBStudioGroup, ltDBCastGroup,
                              ltDBUserRatingGroup, ltDBInsertDateGroup,
                              ltTVMetadata};
    typedef VideoMetadataListManager::metadata_list metadata_list;
    typedef VideoMetadataListManager::VideoMetadataPtr MetadataPtr;

  public:
    VideoListImp();

    void build_generic_tree(MythGenericTree *dst, meta_dir_node *src,
                            bool include_updirs);
    MythGenericTree *buildVideoList(
        bool filebrowser, bool flatlist,
        int group_type, const ParentalLevel &parental_level,
        bool include_updirs);

    void refreshList(bool filebrowser, const ParentalLevel &parental_level,
                     bool flatlist, int group_type);
    bool refreshNode(MythGenericTree *node);

    unsigned int count(void) const
    {
        return m_metadata_view_flat.size();
    }

    const VideoFilterSettings &getCurrentVideoFilter() const
    {
        return m_video_filter;
    }

    void setCurrentVideoFilter(const VideoFilterSettings &filter)
    {
        m_video_filter = filter;
    }

    int TryFilter(const VideoFilterSettings &filter) const
    {
        int ret = 0;
        for (metadata_list::const_iterator p = m_metadata.getList().begin();
             p != m_metadata.getList().end(); ++p)
        {
            if (filter.matches_filter(**p)) ++ret;
        }
        return ret;
    }

    const VideoMetadataListManager &getListCache(void) const
    {
        return m_metadata;
    }

    unsigned int getFilterChangedState(void)
    {
        return m_video_filter.getChangedState();
    }

    bool Delete(unsigned int video_id, VideoList &dummy)
    {
        bool ret = false;
        MetadataPtr mp = m_metadata.byID(video_id);
        if (mp)
        {
            ret = mp->DeleteFile();
            if (ret) ret = m_metadata.purgeByID(video_id);
        }

        return ret;
    }

    MythGenericTree *GetTreeRoot(void)
    {
        return video_tree_root.data();
    }

    void InvalidateCache() {
        // Set the type to none to avoid refreshList thinking it doesn't
        // need to.
        m_metadata_list_type = VideoListImp::ltNone;

        metadata_list ml;
        VideoMetadataListManager::loadAllFromDatabase(ml);
        m_metadata.setList(ml);
    }

  private:
    void sort_view_data(bool flat_list);
    void fillMetadata(metadata_list_type whence);

    void buildFsysList(void);
    void buildGroupList(metadata_list_type whence);
    void buildDbList(void);
    void buildTVList(void);
    void buildFileList(smart_dir_node &directory, metadata_list &metalist,
                       const QString &prefix);

    void update_meta_view(bool flat_list);

  private:
    bool m_ListUnknown;
    bool m_LoadMetaData;

    QScopedPointer <MythGenericTree> video_tree_root;

    VideoMetadataListManager m_metadata;
    meta_dir_node m_metadata_tree; // master list for tree views

    metadata_view_list m_metadata_view_flat;
    meta_dir_node m_metadata_view_tree;

    metadata_list_type m_metadata_list_type;

    VideoFilterSettings m_video_filter;
};

VideoList::VideoList()
{
    m_imp = new VideoListImp;
}

VideoList::~VideoList()
{
    delete m_imp;
}

MythGenericTree *VideoList::buildVideoList(
    bool filebrowser, bool flatlist,
    int group_type, const ParentalLevel &parental_level,
    bool include_updirs)
{
    return m_imp->buildVideoList(filebrowser, flatlist,
                                 group_type, parental_level, include_updirs);
}

void VideoList::refreshList(bool filebrowser,
                            const ParentalLevel &parental_level,
                            bool flat_list, int group_type)
{
    m_imp->refreshList(filebrowser, parental_level, flat_list, group_type);
}

bool VideoList::refreshNode(MythGenericTree *node)
{
    return m_imp->refreshNode(node);
}

unsigned int VideoList::count(void) const
{
    return m_imp->count();
}

const VideoFilterSettings &VideoList::getCurrentVideoFilter(void) const
{
    return m_imp->getCurrentVideoFilter();
}

void VideoList::setCurrentVideoFilter(const VideoFilterSettings &filter)
{
    m_imp->setCurrentVideoFilter(filter);
}

int VideoList::TryFilter(const VideoFilterSettings &filter) const
{
    return m_imp->TryFilter(filter);
}

const VideoMetadataListManager &VideoList::getListCache(void) const
{
    return m_imp->getListCache();
}

unsigned int VideoList::getFilterChangedState(void)
{
    return m_imp->getFilterChangedState();
}

bool VideoList::Delete(int video_id)
{
    return m_imp->Delete(video_id, *this);
}

MythGenericTree *VideoList::GetTreeRoot(void)
{
    return m_imp->GetTreeRoot();
}

void VideoList::InvalidateCache(void)
{
    return m_imp->InvalidateCache();
}

//////////////////////////////
// VideoListImp
//////////////////////////////
VideoListImp::VideoListImp() : m_metadata_view_tree("", "top"),
                               m_metadata_list_type(ltNone)
{
    m_ListUnknown = gCoreContext->GetNumSetting("VideoListUnknownFileTypes", 0);

    m_LoadMetaData = gCoreContext->GetNumSetting("VideoTreeLoadMetaData", 0);
}

void VideoListImp::build_generic_tree(MythGenericTree *dst, meta_dir_node *src,
                                      bool include_updirs)
{
    if (src->DataIsValid())
    {
        dst->setInt(kDynamicSubFolder);
        dst->SetData(src->GetData());
        return;
    }

    for (meta_dir_node::const_dir_iterator dir = src->dirs_begin();
         dir != src->dirs_end(); ++dir)
    {
        if ((*dir)->has_entries())
        {
            bool incUpDir = include_updirs;
            // Only include upnodes when there is a parent to move up to
            if (!dst->getParent())
                incUpDir = false;

            MythGenericTree *t = AddDirNode(
                dst, (*dir)->getName(),
                (*dir)->getFQPath(), incUpDir, (*dir)->GetHost(),
                (*dir)->GetPrefix());

            build_generic_tree(t, dir->get(), include_updirs);
        }
    }

    for (meta_dir_node::const_entry_iterator entry = src->entries_begin();
         entry != src->entries_end(); ++entry)
    {
        if (((*entry)->getData()->GetSeason() > 0) ||
            ((*entry)->getData()->GetEpisode() > 0))
        {
            QString seas = QString::number((*entry)->getData()->GetSeason());
            QString ep = QString::number((*entry)->getData()->GetEpisode());
            QString title = (*entry)->getData()->GetTitle();
            QString subtitle = (*entry)->getData()->GetSubtitle();

            if (ep.size() < 2)
                ep.prepend("0");

            QString displayTitle = QString("%1 %2x%3 - %4")
                .arg(title).arg(seas).arg(ep).arg(subtitle);

            if (src->getName() == title)
            {
                displayTitle = QString("%2x%3 - %4")
                    .arg(seas).arg(ep).arg(subtitle);
            }
            AddFileNode(dst, displayTitle, (*entry)->getData());
        }
        else if ((*entry)->getData()->GetSubtitle().isEmpty())
        {
            AddFileNode(
                dst, (*entry)->getData()->GetTitle(), (*entry)->getData());
        }
        else
        {
            QString TitleSub = QString("%1 - %2")
                .arg((*entry)->getData()->GetTitle())
                .arg((*entry)->getData()->GetSubtitle());
            AddFileNode(dst, TitleSub, (*entry)->getData());
        }
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
MythGenericTree *VideoListImp::buildVideoList(
    bool filebrowser, bool flatlist, int group_type,
    const ParentalLevel &parental_level, bool include_updirs)
{
    refreshList(filebrowser, parental_level, flatlist, group_type);

    typedef map<QString, MythGenericTree *> string_to_tree;
    string_to_tree prefix_tree_map;

    video_tree_root.reset(new MythGenericTree(QObject::tr("Video Home"),
                                              kRootNode, false));

    build_generic_tree(video_tree_root.data(), &m_metadata_view_tree,
                       include_updirs);

    if (m_metadata_view_flat.empty())
    {
        video_tree_root.reset(new MythGenericTree(QObject::tr("Video Home"),
                                                  kRootNode, false));
        video_tree_root.data()->addNode(QObject::tr("No files found"),
                                        kNoFilesFound, false);
    }

    return video_tree_root.data();
}

bool VideoListImp::refreshNode(MythGenericTree *node)
{
    if (!node)
        return false;

    // node->GetData() provides information on how/where to refresh the
    // data for this node

    QVariant data = node->GetData();
    if (!data.isValid())
        return false;

    // currently only UPNPScanner can refresh data
    if (UPNPScanner::Instance() && UPNPScanner::Instance()->GetMetadata(data))
    {
        // force a refresh
        m_metadata_list_type = VideoListImp::ltNone;
        return true;
    }

    return false;
}

void VideoListImp::refreshList(bool filebrowser,
                               const ParentalLevel &parental_level,
                               bool flat_list, int group_type)
{

    m_video_filter.setParentalLevel(parental_level.GetLevel());

    if (filebrowser)
    {
        fillMetadata(ltFileSystem);
    }
    else
    {
        switch (group_type)
        {
            case VideoDialog::BRS_FOLDER:
                fillMetadata(ltDBMetadata);
                LOG(VB_GENERAL, LOG_DEBUG, "Using Folder mode");
                break;
            case VideoDialog::BRS_GENRE:
                fillMetadata(ltDBGenreGroup);
                LOG(VB_GENERAL, LOG_DEBUG, "Using Genre mode");
                break;
            case VideoDialog::BRS_CATEGORY:
                fillMetadata(ltDBCategoryGroup);
                LOG(VB_GENERAL, LOG_DEBUG, "Using Category mode");
                break;
            case VideoDialog::BRS_YEAR:
                fillMetadata(ltDBYearGroup);
                LOG(VB_GENERAL, LOG_DEBUG, "Using Year mode");
                break;
            case VideoDialog::BRS_DIRECTOR:
                fillMetadata(ltDBDirectorGroup);
                LOG(VB_GENERAL, LOG_DEBUG, "Using Director mode");
                break;
            case VideoDialog::BRS_STUDIO:
                fillMetadata(ltDBStudioGroup);
                LOG(VB_GENERAL, LOG_DEBUG, "Using Studio mode");
                break;
            case VideoDialog::BRS_CAST:
                fillMetadata(ltDBCastGroup);
                LOG(VB_GENERAL, LOG_DEBUG, "Using Cast Mode");
                break;
            case VideoDialog::BRS_USERRATING:
                fillMetadata(ltDBUserRatingGroup);
                LOG(VB_GENERAL, LOG_DEBUG, "Using User Rating Mode");
                break;
            case VideoDialog::BRS_INSERTDATE:
                fillMetadata(ltDBInsertDateGroup);
                LOG(VB_GENERAL, LOG_DEBUG, "Using Insert Date Mode");
                break;
            case VideoDialog::BRS_TVMOVIE:
                fillMetadata(ltTVMetadata);
                LOG(VB_GENERAL, LOG_DEBUG, "Using TV/Movie Mode");
                break;
            default:
                fillMetadata(ltDBMetadata);
                break;
        }
    }
    update_meta_view(flat_list);
}

void VideoListImp::sort_view_data(bool flat_list)
{
    if (flat_list)
    {
        sort(m_metadata_view_flat.begin(), m_metadata_view_flat.end(),
             metadata_sort(m_video_filter, true));
    }
    else
    {
        m_metadata_view_tree.sort(metadata_path_sort(true),
                                  metadata_sort(m_video_filter,
                                                true));
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

        switch (whence)
        {
            case ltFileSystem:
                buildFsysList();
                break;
            case ltDBMetadata:
                buildDbList();
                break;
            case ltTVMetadata:
                buildTVList();
                break;
            case ltDBGenreGroup:
            case ltDBCategoryGroup:
            case ltDBYearGroup:
            case ltDBDirectorGroup:
            case ltDBStudioGroup:
            case ltDBCastGroup:
            case ltDBUserRatingGroup:
            case ltDBInsertDateGroup:
                buildGroupList(whence);
                break;
            case ltNone:
                break;
        }
    }
}

void VideoListImp::buildGroupList(metadata_list_type whence)
{
    metadata_list ml;
    VideoMetadataListManager::loadAllFromDatabase(ml);
    m_metadata.setList(ml);

    metadata_view_list mlist;
    mlist.reserve(m_metadata.getList().size());

    back_insert_iterator<metadata_view_list> mli(mlist);
    transform(m_metadata.getList().begin(), m_metadata.getList().end(),
              mli, to_metadata_ptr());

    metadata_path_sort mps(true);
    sort(mlist.begin(), mlist.end(), mps);

    typedef map<QString, meta_dir_node *> group_to_node_map;
    group_to_node_map gtnm;

    meta_dir_node *video_root = &m_metadata_tree;

    smart_dir_node sdn = video_root->addSubDir("All");
    meta_dir_node* all_group_node = sdn.get();

    for (metadata_view_list::iterator p = mlist.begin(); p != mlist.end(); ++p)
    {
        VideoMetadata *data = *p;

        all_group_node->addEntry(smart_meta_node(new meta_data_node(data)));

        vector<QString> groups;

        switch (whence)
        {
            case ltDBGenreGroup:
            {
                vector<pair <int, QString> > genres =
                    data->GetGenres();

                for (vector<pair <int, QString> >::iterator i =
                         genres.begin(); i != genres.end(); ++i)
                {
                    pair<int, QString> item = *i;
                    groups.push_back(item.second);
                }
                break;
            }
            case ltDBCategoryGroup:
            {
                groups.push_back(data->GetCategory());
                break;
            }
            case ltDBYearGroup:
            {
                groups.push_back(QString::number(data->GetYear()));
                break;
            }
            case ltDBDirectorGroup:
            {
                groups.push_back(data->GetDirector());
                break;
            }
            case ltDBStudioGroup:
            {
                groups.push_back(data->GetStudio());
                break;
            }
            case ltDBCastGroup:
            {
                vector<pair<int, QString> > cast = data->GetCast();

                for (vector<pair<int, QString> >::iterator i =
                         cast.begin(); i != cast.end(); ++i)
                {
                    pair<int, QString> item = *i;
                    groups.push_back(item.second);
                }
                break;
            }
            case ltDBUserRatingGroup:
            {
                int i = data->GetUserRating();
                groups.push_back(QString::number(i));
                break;
            }
            case ltDBInsertDateGroup:
            {
                QDate date = data->GetInsertdate();
                QString tmp = MythDate::toString(
                    date, MythDate::kDateFull | MythDate::kSimplify);
                groups.push_back(tmp);
                break;
            }
            default:
            {
                LOG(VB_GENERAL, LOG_ERR, "Invalid type of grouping");
                break;
            }
        }

        if (groups.empty())
        {
            meta_dir_node *group_node = gtnm["Unknown"];

            if (group_node == NULL)
            {
                smart_dir_node sdn = video_root->addSubDir("Unknown");
                group_node = sdn.get();
                gtnm["Unknown"] = group_node;
            }

            group_node->addEntry(smart_meta_node(new meta_data_node(data)));
        }

        for (vector<QString>::iterator i = groups.begin();
             i != groups.end(); ++i)
        {
            QString item = *i;

            meta_dir_node *group_node = gtnm[item];

            if (group_node == NULL)
            {
                smart_dir_node sdn = video_root->addSubDir(item);
                group_node = sdn.get();
                gtnm[item] = group_node;
            }

            group_node->addEntry(smart_meta_node(new meta_data_node(data)));
        }
    }
}

void VideoListImp::buildTVList(void)
{
    metadata_list ml;
    VideoMetadataListManager::loadAllFromDatabase(ml);
    m_metadata.setList(ml);

    metadata_view_list mlist;
    mlist.reserve(m_metadata.getList().size());

    back_insert_iterator<metadata_view_list> mli(mlist);
    transform(m_metadata.getList().begin(), m_metadata.getList().end(),
              mli, to_metadata_ptr());

    metadata_path_sort mps(true);
    sort(mlist.begin(), mlist.end(), mps);

    typedef map<QString, meta_dir_node *> group_to_node_map;
    group_to_node_map gtnm;

    meta_dir_node *video_root = &m_metadata_tree;

    smart_dir_node sdn = video_root->addSubDir(QObject::tr("Television"));
    meta_dir_node* television_node = sdn.get();

    smart_dir_node vdn = video_root->addSubDir(QObject::tr("Movies"));
    meta_dir_node* movie_node = vdn.get();

    for (metadata_view_list::iterator p = mlist.begin(); p != mlist.end(); ++p)
    {
        VideoMetadata *data = *p;

        if (((*p)->GetSeason() > 0) || ((*p)->GetEpisode() > 0))
        {
            smart_dir_node sdn = television_node->addSubDir((*p)->GetTitle());
            meta_dir_node* title_node = sdn.get();

            smart_dir_node ssdn = title_node->addSubDir(
                QObject::tr("Season %1").arg((*p)->GetSeason()));
            meta_dir_node* season_node = ssdn.get();

            season_node->addEntry(smart_meta_node(new meta_data_node(data)));
        }
        else
            movie_node->addEntry(smart_meta_node(new meta_data_node(data)));
    }
}

void VideoListImp::buildDbList()
{
    metadata_list ml;
    VideoMetadataListManager::loadAllFromDatabase(ml);
    m_metadata.setList(ml);

    metadata_view_list mlist;
    mlist.reserve(m_metadata.getList().size());

    back_insert_iterator<metadata_view_list> mli(mlist);
    transform(m_metadata.getList().begin(), m_metadata.getList().end(),
              mli, to_metadata_ptr());

//    print_meta_list(mlist);

    metadata_path_sort mps(true);
    sort(mlist.begin(), mlist.end(), mps);

    // TODO: break out the prefix in the DB so this isn't needed
    typedef map<QString, meta_dir_node *> prefix_to_node_map;
    prefix_to_node_map ptnm;

    QStringList dirs = GetVideoDirs();

    if (dirs.isEmpty())
        return;

    QString test_prefix(dirs[0]);

    meta_dir_node *video_root = &m_metadata_tree;
    if (dirs.size() == 1)
    {
        video_root->setPathRoot();
        video_root->setPath(test_prefix);
        video_root->setName("videos");
        ptnm.insert(prefix_to_node_map::value_type(test_prefix, video_root));
    }

    for (metadata_view_list::iterator p = mlist.begin(); p != mlist.end(); ++p)
    {
        AddMetadataToDir(*p, video_root);
    }

//    print_dir_tree(m_metadata_tree); // AEW DEBUG
}

void VideoListImp::buildFsysList()
{
    //
    //  Fill metadata from directory structure
    //

    typedef vector<pair<QString, QString> > node_to_path_list;

    node_to_path_list node_paths;

    QStringList dirs = GetVideoDirs();
    if (dirs.size() > 1)
    {
        for (QStringList::const_iterator iter = dirs.begin();
             iter != dirs.end(); ++iter)
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

    // retrieve any MediaServer data that may be available
    if (UPNPScanner::Instance())
        UPNPScanner::Instance()->GetInitialMetadata(&ml, &m_metadata_tree);

    // See if we can find this filename in DB
    if (m_LoadMetaData)
    {
        // Load the DB data so metadata lookups work
        // TODO: ugly, pass the list
        VideoMetadataListManager mdlm;
        metadata_list db_metadata;
        VideoMetadataListManager::loadAllFromDatabase(db_metadata);
        mdlm.setList(db_metadata);
        for (metadata_list::iterator p = ml.begin(); p != ml.end(); ++p)
        {
            (*p)->FillDataFromFilename(mdlm);
        }
    }
    m_metadata.setList(ml);
}


static void copy_entries(meta_dir_node &dst, meta_dir_node &src,
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

static void copy_filtered_tree(meta_dir_node &dst, meta_dir_node &src,
                               const VideoFilterSettings &filter)
{
    copy_entries(dst, src, filter);
    for (meta_dir_node::dir_iterator dir = src.dirs_begin();
         dir != src.dirs_end(); ++dir)
    {
        smart_dir_node sdn = dst.addSubDir((*dir)->getPath(),
                                           (*dir)->getName(),
                                           (*dir)->GetHost(),
                                           (*dir)->GetPrefix(),
                                           (*dir)->GetData());
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
    back_insert_iterator<VideoListImp::metadata_view_list> bip(flat);
    transform(tree.entries_begin(), tree.entries_end(), bip,
              to_metadata_ptr());

    for_each(tree.dirs_begin(), tree.dirs_end(), call_tree_flat(flat));
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
        if (!(*si)->HasSortKey())
        {
            VideoMetadata::SortKey skey =
                VideoMetadata::GenerateDefaultSortKey(*(*si), true);
            (*si)->SetSortKey(skey);
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
}

class dirhandler : public DirectoryHandler
{
  public:
    typedef list<simple_ref_ptr<DirectoryHandler> > free_list;

  public:
    dirhandler(smart_dir_node &directory, const QString &prefix,
               VideoMetadataListManager::metadata_list &metalist,
               free_list &dh_free_list, bool infer_title) :
        m_directory(directory), m_prefix(prefix), m_metalist(metalist),
        m_dh_free_list(dh_free_list), m_infer_title(infer_title)
    {
    }

    DirectoryHandler *newDir(const QString &dir_name,
                             const QString &fq_dir_name)
    {
        (void) fq_dir_name;
        smart_dir_node dir = m_directory->addSubDir(dir_name);
        DirectoryHandler *dh = new dirhandler(dir, m_prefix, m_metalist,
                                              m_dh_free_list,
                                              m_infer_title);
        m_dh_free_list.push_back(dh);
        return dh;
    }

    void handleFile(const QString &file_name,
                    const QString &fq_file_name,
                    const QString &extension)
    {
        handleFile(file_name, fq_file_name, extension, "");
    }

    void handleFile(const QString &file_name,
                    const QString &fq_file_name,
                    const QString &extension,
                    const QString &host)
    {
        (void) file_name;
        (void) extension;
        QString file_string(fq_file_name);

        VideoMetadataListManager::VideoMetadataPtr myData(
            new VideoMetadata(file_string));
        QFileInfo qfi(file_string);
        QString title = qfi.completeBaseName();
        if (m_infer_title)
        {
            QString tmptitle(VideoMetadata::FilenameToMeta(file_string, 1));
            if (tmptitle.length())
                title = tmptitle;
        }
        myData->SetTitle(title);
        myData->SetPrefix(m_prefix);

        myData->SetHost(host);
        m_metalist.push_back(myData);

        m_directory->addEntry(new meta_data_node(myData.get()));
    }

  private:
    smart_dir_node m_directory;
    const QString &m_prefix;
    VideoMetadataListManager::metadata_list &m_metalist;
    free_list &m_dh_free_list;
    const bool m_infer_title;
};

void VideoListImp::buildFileList(
    smart_dir_node &directory, metadata_list &metalist, const QString &prefix)
{
    FileAssociations::ext_ignore_list ext_list;
    FileAssociations::getFileAssociation().getExtensionIgnoreList(ext_list);

    dirhandler::free_list fl;
    dirhandler dh(directory, prefix, metalist, fl, false);
    (void) ScanVideoDirectory(
        directory->getFQPath(), &dh, ext_list, m_ListUnknown);
}

// C++
#include <algorithm>
#include <iterator>
#include <map>
#include <utility>

// Qt
#include <QFileInfo>
#include <QList>
#include <QScopedPointer>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/stringutil.h"
#include "libmythui/mythgenerictree.h"
#include "libmythmetadata/videometadatalistmanager.h"
#include "libmythmetadata/dbaccess.h"
#include "libmythmetadata/quicksp.h"
#include "libmythmetadata/dirscan.h"
#include "libmythmetadata/videoutils.h"
#include "libmythmetadata/parentalcontrols.h"

// MythFrontend
#include "upnpscanner.h"
#include "videodlg.h"
#include "videofilter.h"
#include "videolist.h"

// Sorting or not sorting the metadata list doesn't seem to have any
// effect. The metadataViewFlat and metadataViewTree that are
// constructed from this list get sorted, and those are what is used
// to build the UI screens.
#undef SORT_METADATA_LIST

class TreeNodeDataPrivate
{
  public:
    explicit TreeNodeDataPrivate(VideoMetadata *metadata) :
        m_metadata(metadata)
    {
        if (m_metadata)
            m_host = m_metadata->GetHost();
        else
            m_host = "";
    }

    TreeNodeDataPrivate(QString path, QString host, QString prefix) :
        m_host(std::move(host)), m_path(std::move(path)), m_prefix(std::move(prefix))
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
    VideoMetadata *m_metadata {nullptr};
    QString m_host;
    QString m_path;
    QString m_prefix;
};

TreeNodeData::TreeNodeData(VideoMetadata *metadata)
  : m_d(new TreeNodeDataPrivate(metadata))
{
}

TreeNodeData::TreeNodeData(QString path, QString host, QString prefix)
{
    m_d = new TreeNodeDataPrivate(std::move(path), std::move(host), std::move(prefix));
}

TreeNodeData::TreeNodeData(const TreeNodeData &other)
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

    return nullptr;
}

const VideoMetadata *TreeNodeData::GetMetadata(void) const
{
    if (m_d)
        return m_d->GetMetadata();

    return nullptr;
}

QString TreeNodeData::GetPath(void) const
{
    if (m_d)
        return m_d->GetPath();
    return {};
}

QString TreeNodeData::GetHost(void) const
{
    if (m_d)
        return m_d->GetHost();
    return {};
}

QString TreeNodeData::GetPrefix(void) const
{
    if (m_d)
        return m_d->GetPrefix();
    return {};
}

/// metadata sort function
struct metadata_sort
{
    explicit metadata_sort(const VideoFilterSettings &vfs) : m_vfs(vfs) {}

    bool operator()(const VideoMetadata *lhs, const VideoMetadata *rhs)
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
    explicit metadata_path_sort(void) = default;

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
        return sort(lhs->getSortPath(), rhs->getSortPath());
    }

  private:
    static bool sort(const VideoMetadata *lhs, const VideoMetadata *rhs)
    {
        return sort(lhs->GetSortFilename(), rhs->GetSortFilename());
    }

    static bool sort(const QString &lhs, const QString &rhs)
    {
        return StringUtil::naturalSortCompare(lhs, rhs);
    }
};

static QString path_to_node_name(const QString &path)
{
    QString ret;
    int slashLoc = path.lastIndexOf('/', -2) + 1;
    if (path.endsWith("/"))
        ret = path.mid(slashLoc, path.length() - slashLoc - 1);
    else
        ret = path.mid(slashLoc);

    return ret;
}

static meta_dir_node *AddMetadataToDir(VideoMetadata *metadata,
                                       meta_dir_node *dir,
                                       meta_dir_node *hint = nullptr)
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

    QStringList path = insert_chunk.split("/", Qt::SkipEmptyParts);
    if (path.size() > 1)
    {
        path.pop_back();
    }
    else
    {
        path.clear();
    }

    for (const auto & part : std::as_const(path))
    {
        smart_dir_node sdn = start->addSubDir(part, "" , host, prefix);
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

static MythGenericTree *AddDirNode(
    MythGenericTree *where_to_add,
    const QString& name, QString fqPath, bool add_up_dirs,
    QString host = "", QString prefix = "")
{
    // Add the subdir node...
    MythGenericTree *sub_node =
        where_to_add->addNode(name, kSubFolder, false);
    sub_node->SetData(QVariant::fromValue(TreeNodeData(std::move(fqPath), std::move(host), std::move(prefix))));
    sub_node->SetText(name, "title");
    sub_node->DisplayState("subfolder", "nodetype");

    // ...and the updir node.
    if (add_up_dirs)
    {
        MythGenericTree *up_node =
            sub_node->addNode(where_to_add->GetText(), kUpFolder,
                              true, false);
        up_node->DisplayState("subfolder", "nodetype");
    }

    return sub_node;
}

int AddFileNode(MythGenericTree *where_to_add, const QString& name,
                       VideoMetadata *metadata)
{
    MythGenericTree *sub_node = where_to_add->addNode(name, 0, true);
    sub_node->SetData(QVariant::fromValue(TreeNodeData(metadata)));
    sub_node->SetTextCb( &VideoMetadata::MetadataGetTextCb, metadata);
    sub_node->SetImageCb(&VideoMetadata::MetadataGetImageCb, metadata);
    sub_node->SetStateCb(&VideoMetadata::MetadataGetStateCb, metadata);

    // Assign images to parent node if this is the first child
    if (where_to_add->visibleChildCount() == 1 &&
        where_to_add->getInt() == kSubFolder)
    {
        InfoMap imageMap;
        metadata->GetImageMap(imageMap);
        where_to_add->SetImageFromMap(imageMap);
        where_to_add->SetImage("buttonimage", imageMap["smartimage"]);
    }

    return 1;
}

class VideoListImp
{
  public:
    using metadata_view_list = std::vector<VideoMetadata *>;

  private:
    enum metadata_list_type : std::uint8_t
                            { ltNone, ltFileSystem, ltDBMetadata,
                              ltDBGenreGroup, ltDBCategoryGroup,
                              ltDBYearGroup, ltDBDirectorGroup,
                              ltDBStudioGroup, ltDBCastGroup,
                              ltDBUserRatingGroup, ltDBInsertDateGroup,
                              ltTVMetadata};
    using metadata_list = VideoMetadataListManager::metadata_list;
    using MetadataPtr = VideoMetadataListManager::VideoMetadataPtr;

  public:
    VideoListImp();

    void build_generic_tree(MythGenericTree *dst, meta_dir_node *src,
                            bool include_updirs);
    MythGenericTree *buildVideoList(
        bool filebrowser, bool flatlist,
        int group_type, const ParentalLevel &parental_level,
        bool include_updirs);

    void refreshList(bool filebrowser, const ParentalLevel &parental_level,
                     bool flat_list, int group_type);
    bool refreshNode(MythGenericTree *node);

    unsigned int count(void) const
    {
        return m_metadataViewFlat.size();
    }

    const VideoFilterSettings &getCurrentVideoFilter() const
    {
        return m_videoFilter;
    }

    void setCurrentVideoFilter(const VideoFilterSettings &filter)
    {
        m_videoFilter = filter;
    }

    int TryFilter(const VideoFilterSettings &filter) const
    {
        auto list = m_metadata.getList();
        auto filtermatch =
            [filter](const auto & md){ return filter.matches_filter(*md); };
        return std::count_if(list.cbegin(), list.cend(), filtermatch);
    }

    const VideoMetadataListManager &getListCache(void) const
    {
        return m_metadata;
    }

    unsigned int getFilterChangedState(void)
    {
        return m_videoFilter.getChangedState();
    }

    bool Delete(unsigned int video_id, VideoList &/*dummy*/)
    {
        bool ret = false;
        MetadataPtr mp = m_metadata.byID(video_id);
        if (mp)
        {
            ret = mp->DeleteFile();
            if (ret)
            {
                ret = m_metadata.purgeByID(video_id);
                // Force refresh
                m_metadataListType = VideoListImp::ltNone;
            }
        }

        return ret;
    }

    MythGenericTree *GetTreeRoot(void)
    {
        return m_videoTreeRoot.data();
    }

    void InvalidateCache() {
        // Set the type to none to avoid refreshList thinking it doesn't
        // need to.
        m_metadataListType = VideoListImp::ltNone;

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
                       const QString &prefix) const;

    void update_meta_view(bool flat_list);

  private:
    bool m_listUnknown                      {false};
    bool m_loadMetaData                     {false};

    QScopedPointer <MythGenericTree> m_videoTreeRoot;

    VideoMetadataListManager m_metadata;
    meta_dir_node m_metadataTree; // master list for tree views

    metadata_view_list m_metadataViewFlat;
    meta_dir_node m_metadataViewTree;

    metadata_list_type m_metadataListType {ltNone};

    VideoFilterSettings m_videoFilter;
};

VideoList::VideoList()
  : m_imp(new VideoListImp)
{
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
    m_imp->InvalidateCache();
}

//////////////////////////////
// VideoListImp
//////////////////////////////
VideoListImp::VideoListImp() : m_metadataViewTree("", "top")
{
    m_listUnknown = gCoreContext->GetBoolSetting("VideoListUnknownFileTypes", false);

    m_loadMetaData = gCoreContext->GetBoolSetting("VideoTreeLoadMetaData", false);
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

    for (auto dir = src->dirs_begin();
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

    for (auto entry = src->entries_begin();
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
                .arg(title, seas, ep, subtitle);

            if (src->getName() == title)
            {
                displayTitle = QString("%2x%3 - %4")
                    .arg(seas, ep, subtitle);
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
                .arg((*entry)->getData()->GetTitle(),
                     (*entry)->getData()->GetSubtitle());
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

    m_videoTreeRoot.reset(new MythGenericTree(QObject::tr("Video Home"),
                                              kRootNode, false));

    build_generic_tree(m_videoTreeRoot.data(), &m_metadataViewTree,
                       include_updirs);

    if (m_metadataViewFlat.empty())
    {
        m_videoTreeRoot.reset(new MythGenericTree(QObject::tr("Video Home"),
                                                  kRootNode, false));
        m_videoTreeRoot.data()->addNode(QObject::tr("No files found"),
                                        kNoFilesFound, false);
    }

    return m_videoTreeRoot.data();
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
        m_metadataListType = VideoListImp::ltNone;
        return true;
    }

    return false;
}

void VideoListImp::refreshList(bool filebrowser,
                               const ParentalLevel &parental_level,
                               bool flat_list, int group_type)
{

    m_videoFilter.setParentalLevel(parental_level.GetLevel());

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
        sort(m_metadataViewFlat.begin(), m_metadataViewFlat.end(),
             metadata_sort(m_videoFilter));
    }
    else
    {
        m_metadataViewTree.sort(metadata_path_sort(),
                                  metadata_sort(m_videoFilter));
    }
}

void VideoListImp::fillMetadata(metadata_list_type whence)
{
    if (m_metadataListType != whence)
    {
        m_metadataListType = whence;
        // flush existing data
        metadata_list ml;
        m_metadata.setList(ml);
        m_metadataTree.clear();

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

    std::back_insert_iterator<metadata_view_list> mli(mlist);
    transform(m_metadata.getList().begin(), m_metadata.getList().end(),
              mli, to_metadata_ptr());

#ifdef SORT_METADATA_LIST
    metadata_path_sort mps = metadata_path_sort();
    std::sort(mlist.begin(), mlist.end(), mps);
#endif

    using group_to_node_map = std::map<QString, meta_dir_node *>;
    group_to_node_map gtnm;

    meta_dir_node *video_root = &m_metadataTree;

    smart_dir_node sdn1 = video_root->addSubDir("All");
    meta_dir_node* all_group_node = sdn1.get();

    for (auto *data : mlist)
    {
        all_group_node->addEntry(smart_meta_node(new meta_data_node(data)));

        std::vector<QString> groups;
        auto take_second = [](const auto& item){ return item.second; };

        switch (whence)
        {
            case ltDBGenreGroup:
            {
                const std::vector<std::pair <int, QString> >& genres =
                    data->GetGenres();

                std::transform(genres.cbegin(), genres.cend(),
                               std::back_inserter(groups), take_second);
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
                const std::vector<std::pair<int, QString> >& cast = data->GetCast();

                std::transform(cast.cbegin(), cast.cend(),
                               std::back_inserter(groups), take_second);
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

            if (group_node == nullptr)
            {
                smart_dir_node sdn2 = video_root->addSubDir("Unknown");
                group_node = sdn2.get();
                gtnm["Unknown"] = group_node;
            }

            group_node->addEntry(smart_meta_node(new meta_data_node(data)));
        }

        for (const auto& item : groups)
        {
            meta_dir_node *group_node = gtnm[item];

            if (group_node == nullptr)
            {
                smart_dir_node sdn2 = video_root->addSubDir(item);
                group_node = sdn2.get();
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

    std::back_insert_iterator<metadata_view_list> mli(mlist);
    transform(m_metadata.getList().begin(), m_metadata.getList().end(),
              mli, to_metadata_ptr());

#ifdef SORT_METADATA_LIST
    metadata_path_sort mps = metadata_path_sort();
    sort(mlist.begin(), mlist.end(), mps);
#endif

    meta_dir_node *video_root = &m_metadataTree;

    smart_dir_node sdn1 = video_root->addSubDir(QObject::tr("Television"));
    meta_dir_node* television_node = sdn1.get();

    smart_dir_node vdn = video_root->addSubDir(QObject::tr("Movies"));
    meta_dir_node* movie_node = vdn.get();

    for (auto & p : mlist)
    {
        VideoMetadata *data = p;

        if ((p->GetSeason() > 0) || (p->GetEpisode() > 0))
        {
            smart_dir_node sdn2 = television_node->addSubDir(p->GetTitle());
            meta_dir_node* title_node = sdn2.get();

            smart_dir_node ssdn = title_node->addSubDir(
                QObject::tr("Season %1").arg(p->GetSeason()));
            meta_dir_node* season_node = ssdn.get();

            season_node->addEntry(smart_meta_node(new meta_data_node(data)));
        }
        else
        {
            movie_node->addEntry(smart_meta_node(new meta_data_node(data)));
        }
    }
}

void VideoListImp::buildDbList()
{
    metadata_list ml;
    VideoMetadataListManager::loadAllFromDatabase(ml);
    m_metadata.setList(ml);

    metadata_view_list mlist;
    mlist.reserve(m_metadata.getList().size());

    std::back_insert_iterator<metadata_view_list> mli(mlist);
    transform(m_metadata.getList().begin(), m_metadata.getList().end(),
              mli, to_metadata_ptr());

//    print_meta_list(mlist);

#ifdef SORT_METADATA_LIST
    metadata_path_sort mps = metadata_path_sort();
    std::sort(mlist.begin(), mlist.end(), mps);
#endif

    // TODO: break out the prefix in the DB so this isn't needed
    using prefix_to_node_map = std::map<QString, meta_dir_node *>;
    prefix_to_node_map ptnm;

    QStringList dirs = GetVideoDirs();

    if (dirs.isEmpty())
        return;

    QString test_prefix(dirs[0]);

    meta_dir_node *video_root = &m_metadataTree;
    if (dirs.size() == 1)
    {
        video_root->setPathRoot();
        video_root->setPath(test_prefix);
        video_root->setName("videos");
        ptnm.insert(prefix_to_node_map::value_type(test_prefix, video_root));
    }

    for (auto & mv : mlist)
        AddMetadataToDir(mv, video_root);

//    print_dir_tree(m_metadataTree); // AEW DEBUG
}

void VideoListImp::buildFsysList()
{
    //
    //  Fill metadata from directory structure
    //

    using node_to_path_list = std::vector<std::pair<QString, QString> >;

    node_to_path_list node_paths;

    QStringList dirs = GetVideoDirs();
    if (dirs.size() > 1)
    {
        auto new_pl = [](const auto& dir)
            { return node_to_path_list::value_type(path_to_node_name(dir), dir); };
        std::transform(dirs.cbegin(), dirs.cend(), std::back_inserter(node_paths), new_pl);
    }
    else
    {
        node_paths.emplace_back(QObject::tr("videos"), dirs[0]);
    }

    //
    // Add all root-nodes to the tree.
    //
    metadata_list ml;
    for (auto & path : node_paths)
    {
        smart_dir_node root = m_metadataTree.addSubDir(path.second, path.first);
        root->setPathRoot();

        buildFileList(root, ml, path.second);
    }

    // retrieve any MediaServer data that may be available
    if (UPNPScanner::Instance())
        UPNPScanner::Instance()->GetInitialMetadata(&ml, &m_metadataTree);

    // See if we can find this filename in DB
    if (m_loadMetaData)
    {
        // Load the DB data so metadata lookups work
        // TODO: ugly, pass the list
        VideoMetadataListManager mdlm;
        metadata_list db_metadata;
        VideoMetadataListManager::loadAllFromDatabase(db_metadata);
        mdlm.setList(db_metadata);
        for (auto & list : ml)
            list->FillDataFromFilename(mdlm);
    }
    m_metadata.setList(ml);
}


static void copy_entries(meta_dir_node &dst, meta_dir_node &src,
                         const VideoFilterSettings &filter)
{
    for (auto e = src.entries_begin(); e != src.entries_end(); ++e)
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
    for (auto dir = src.dirs_begin(); dir != src.dirs_end(); ++dir)
    {
        const simple_ref_ptr<meta_dir_node>& node = *dir;
        if (node == nullptr)
            continue;

        smart_dir_node sdn = dst.addSubDir(node->getPath(),
                                           node->getName(),
                                           node->GetHost(),
                                           node->GetPrefix(),
                                           node->GetData());
        copy_filtered_tree(*sdn, **dir, filter);
    }
}

void tree_view_to_flat(meta_dir_node &tree,
                       VideoListImp::metadata_view_list &flat);
struct call_tree_flat
{
    explicit call_tree_flat(VideoListImp::metadata_view_list &list) : m_list(list) {}

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
    transform(tree.entries_begin(), tree.entries_end(), bip,
              to_metadata_ptr());

    for_each(tree.dirs_begin(), tree.dirs_end(), call_tree_flat(flat));
}

void VideoListImp::update_meta_view(bool flat_list)
{
    m_metadataViewFlat.clear();
    m_metadataViewFlat.reserve(m_metadata.getList().size());

    m_metadataViewTree.clear();

    if (flat_list)
    {
        for (const auto & md : m_metadata.getList())
        {
            if (m_videoFilter.matches_filter(*md))
            {
                m_metadataViewFlat.push_back(md.get());
            }
        }

        sort_view_data(flat_list);

        for (auto & md : m_metadataViewFlat)
            m_metadataViewTree.addEntry(new meta_data_node(md));
    }
    else
    {
        m_metadataViewTree.setPath(m_metadataTree.getPath());
        m_metadataViewTree.setName(m_metadataTree.getName());
        copy_filtered_tree(m_metadataViewTree, m_metadataTree,
                           m_videoFilter);

        sort_view_data(flat_list);

        tree_view_to_flat(m_metadataViewTree, m_metadataViewFlat);
    }
}

class dirhandler : public DirectoryHandler
{
  public:
    using free_list = std::list<simple_ref_ptr<DirectoryHandler> >;

  public:
    dirhandler(smart_dir_node &directory, const QString &prefix,
               VideoMetadataListManager::metadata_list &metalist,
               free_list &dh_free_list, bool infer_title) :
        m_directory(directory), m_prefix(prefix), m_metalist(metalist),
        m_dhFreeList(dh_free_list), m_inferTitle(infer_title)
    {
    }

    DirectoryHandler *newDir(const QString &dir_name,
                             [[maybe_unused]] const QString &fq_dir_name) override // DirectoryHandler
    {
        smart_dir_node dir = m_directory->addSubDir(dir_name);
        DirectoryHandler *dh = new dirhandler(dir, m_prefix, m_metalist,
                                              m_dhFreeList,
                                              m_inferTitle);
        m_dhFreeList.emplace_back(dh);
        return dh;
    }

    void handleFile(const QString &file_name,
                    const QString &fq_file_name,
                    const QString &extension)
    {
        handleFile(file_name, fq_file_name, extension, "");
    }

    void handleFile([[maybe_unused]] const QString &file_name,
                    const QString &fq_file_name,
                    [[maybe_unused]] const QString &extension,
                    const QString &host) override // DirectoryHandler
    {
        const QString& file_string(fq_file_name);

        VideoMetadataListManager::VideoMetadataPtr myData(
            new VideoMetadata(file_string));
        QFileInfo qfi(file_string);
        QString title = qfi.completeBaseName();
        if (m_inferTitle)
        {
            QString tmptitle(VideoMetadata::FilenameToMeta(file_string, 1));
            if (!tmptitle.isEmpty())
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
    free_list &m_dhFreeList;
    const bool m_inferTitle;
};

void VideoListImp::buildFileList(
    smart_dir_node &directory, metadata_list &metalist, const QString &prefix) const
{
    FileAssociations::ext_ignore_list ext_list;
    FileAssociations::getFileAssociation().getExtensionIgnoreList(ext_list);

    dirhandler::free_list fl;
    dirhandler dh(directory, prefix, metalist, fl, false);
    (void) ScanVideoDirectory(
        directory->getFQPath(), &dh, ext_list, m_listUnknown);
}

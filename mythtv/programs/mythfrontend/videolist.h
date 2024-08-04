#ifndef VIDEOLIST_H_
#define VIDEOLIST_H_

// Type of the item added to the tree
enum TreeNodeType : std::int8_t {
    kSubFolder = -1,
    kUpFolder = -2,
    kRootNode = -3,
    kNoFilesFound = -4,
    kDynamicSubFolder = -5,
};

// Tree node attribute index
enum TreeNodeAttributes : std::uint8_t {
    kNodeSort
};

class MythGenericTree;
class VideoFilterSettings;
class VideoMetadataListManager;
class ParentalLevel;

class VideoList
{
  public:
    VideoList();
    ~VideoList();

    // Deleted functions should be public.
    VideoList(const VideoList &) = delete;            // not copyable
    VideoList &operator=(const VideoList &) = delete; // not copyable

    MythGenericTree *buildVideoList(bool filebrowser, bool flatlist,
                                int group_type,
                                const ParentalLevel &parental_level,
                                bool include_updirs);

    void refreshList(bool filebrowser, const ParentalLevel &parental_level,
                     bool flat_list, int group_type);
    bool refreshNode(MythGenericTree *node);

    unsigned int count() const;

    const VideoFilterSettings &getCurrentVideoFilter() const;
    void setCurrentVideoFilter(const VideoFilterSettings &filter);

    // returns the number of videos matched by this filter
    int TryFilter(const VideoFilterSettings &filter) const;

    unsigned int getFilterChangedState();

    bool Delete(int video_id);

    const VideoMetadataListManager &getListCache() const;

    MythGenericTree *GetTreeRoot();

    void InvalidateCache();

  private:
    class VideoListImp *m_imp;
};

int AddFileNode(MythGenericTree *where_to_add, const QString& name,
                VideoMetadata *metadata);

class VideoMetadata;
class TreeNodeData
{
  public:
    TreeNodeData() = default;;
    explicit TreeNodeData(VideoMetadata *metadata);
    TreeNodeData(QString path, QString host, QString prefix);

    TreeNodeData(const TreeNodeData &other);
    TreeNodeData &operator=(const TreeNodeData &rhs);

    ~TreeNodeData();

    VideoMetadata *GetMetadata();
    const VideoMetadata *GetMetadata() const;
    QString GetPath() const;
    QString GetHost() const;
    QString GetPrefix() const;

  private:
    class TreeNodeDataPrivate *m_d {nullptr};
};

Q_DECLARE_METATYPE(TreeNodeData)

#endif // VIDEOLIST_H

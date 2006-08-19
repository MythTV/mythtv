#ifndef VIDEOLIST_H_
#define VIDEOLIST_H_

// Type of the item added to the tree
enum TreeNodeType {
    kSubFolder = -1,
    kUpFolder = -2,
    kRootNode = -3
};

// Tree node attribute index
enum TreeNodeAttributes {
    kNodeSort,
    kFolderPath
};

class GenericTree;
class VideoFilterSettings;
class Metadata;
class MetadataListManager;

class VideoList
{
  public:
    VideoList();
    ~VideoList();

    GenericTree *buildVideoList(bool filebrowser, bool flatlist,
                                int parental_level, bool include_updirs);

    void refreshList(bool filebrowser, int parental_level, bool flatlist);

    // If the only change to the underlying metadata requires
    // another sort (for video manager currently).
    void resortList(bool flat_list);

    Metadata *getVideoListMetadata(int index);
    const Metadata *getVideoListMetadata(int index) const;
    unsigned int count() const;

    const VideoFilterSettings &getCurrentVideoFilter();
    void setCurrentVideoFilter(const VideoFilterSettings &filter);

    // returns the number of videos matched by this filter
    int test_filter(const VideoFilterSettings &filter) const;

    unsigned int getFilterChangedState();

    bool Delete(int video_id);

    const MetadataListManager &getListCache() const;

    // returns the folder path associated with a returned tree
    QString getFolderPath(int folder_id) const;

  private:
    class VideoListImp *m_imp;
};

#endif // VIDEOLIST_H

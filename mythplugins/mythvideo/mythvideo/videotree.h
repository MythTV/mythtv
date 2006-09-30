#ifndef VIDEOTREE_H_
#define VIDEOTREE_H_

#include <memory>

#include <mythtv/mythdialogs.h>

class Metadata;
class VideoList;

class VideoTreeImp;
class VideoTree : public MythThemedDialog
{
    Q_OBJECT

  public:
    VideoTree(MythMainWindow *lparent, const QString &window_name,
              const QString &theme_filename, const QString &name,
              VideoList *video_list);
   ~VideoTree();

    void buildVideoList();

    void playVideo(Metadata *someItem);
    int videoExitType() { return m_exit_type; }

  public slots:
    void slotDoCancel();
    void slotVideoGallery();
    void slotVideoBrowser();
    void slotViewPlot();
    void slotDoFilter();
    void slotWatchVideo();

    void handleTreeListSelection(int node_int);
    void handleTreeListEntry(int node_int);
    void playVideo(int node_number);
    void setParentalLevel(int which_level);

  protected:
    void keyPressEvent(QKeyEvent *e);
    bool createPopup();
    void cancelPopup();
    void doMenu(bool info);

  private:
    MythPopupBox *popup;
    bool expectingPopup;
    Metadata *curitem;
    int current_parental_level;
    bool file_browser;
    bool m_db_folders;

    VideoList    *m_video_list;
    GenericTree *video_tree_root;

    int m_exit_type;

  private:
    void jumpTo(const QString &location);
    void setExitType(int exit_type) { m_exit_type = exit_type; }
    std::auto_ptr<VideoTreeImp> m_imp;
};

#endif

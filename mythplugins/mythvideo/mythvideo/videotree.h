#ifndef VIDEOTREE_H_
#define VIDEOTREE_H_

#include <qtimer.h>
#include <qmutex.h>
#include <qvaluevector.h>

#include <mythtv/mythdialogs.h>
#include <mythtv/uitypes.h>

#include "metadata.h"
#include "videolist.h"

class VideoFilterSettings;

class VideoTree : public MythThemedDialog
{
    Q_OBJECT

  public:

    typedef QValueVector<int> IntVector;

    VideoTree(MythMainWindow *parent, QString window_name, 
              QString theme_filename, const char *name = 0);
   ~VideoTree();

    void buildVideoList();
    
    virtual void playVideo(Metadata *someItem);
    QString getHandler(Metadata *someItem);
    QString getCommand(Metadata *someItem);
        
  public slots:
    void slotDoCancel();
    void slotVideoGallery();
    void slotVideoBrowser();
    void slotViewPlot();
    void slotDoFilter();
    virtual void slotWatchVideo();
    
    void handleTreeListSelection(int, IntVector*);
    void handleTreeListEntry(int, IntVector*);
    void playVideo(int node_number);
    bool checkParentPassword();
    void setParentalLevel(int which_level);

  protected:
    void keyPressEvent(QKeyEvent *e);
    bool createPopup();
    void cancelPopup(void);
    void doMenu(bool info);
    
  private:
    VideoFilterSettings *currentVideoFilter;
    MythPopupBox* popup;
    bool expectingPopup;
    Metadata* curitem;    
    void         wireUpTheme();
    int          current_parental_level;
    bool         file_browser;

    //
    //  Theme-related "widgets"
    //

    UIManagedTreeListType *video_tree_list;
    GenericTree           *video_tree_root;
    GenericTree           *video_tree_data;
    VideoList             *video_list;
    UITextType            *video_title;
    UITextType            *video_file;
    UITextType            *video_plot;
    UITextType            *video_player;
    UITextType            *pl_value;
    UIImageType           *video_poster;
};



#endif

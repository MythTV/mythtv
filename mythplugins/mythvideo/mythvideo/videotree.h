#ifndef VIDEOTREE_H_
#define VIDEOTREE_H_

#include <qtimer.h>
#include <qmutex.h>
#include <qvaluevector.h>

#include <mythtv/mythdialogs.h>
#include <mythtv/uitypes.h>

#include "metadata.h"

class QSqlDatabase;

class VideoTree : public MythThemedDialog
{
    Q_OBJECT

  public:

    typedef QValueVector<int> IntVector;

    VideoTree(MythMainWindow *parent, QSqlDatabase *ldb,
              QString window_name, QString theme_filename,
              const char *name = 0);
   ~VideoTree();

    void buildVideoList();
    void buildFileList(QString directory);
    bool ignoreExtension(QString extension);
    
  public slots:
  
    void handleTreeListSelection(int, IntVector*);
    void handleTreeListEntry(int, IntVector*);
    void playVideo(int node_number);
    bool checkParentPassword();
    void setParentalLevel(int which_level);

  protected:
    void keyPressEvent(QKeyEvent *e);

  private:

    void         wireUpTheme();
    int          current_parental_level;
    QSqlDatabase *db;
    bool         file_browser;
    QStringList  browser_mode_files;
       
    //
    //  Theme-related "widgets"
    //

    UIManagedTreeListType *video_tree_list;
    GenericTree           *video_tree_root;
    GenericTree           *video_tree_data;
    UITextType            *video_title;
    UITextType            *video_file;
    UITextType            *video_player;
    UITextType            *pl_value;
    UIImageType           *video_poster;
};



#endif

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

  public slots:
  
    void handleTreeListSelection(int, IntVector*);
    void handleTreeListEntry(int, IntVector*);

  protected:
    void keyPressEvent(QKeyEvent *e);

  private:

    void         wireUpTheme();
    int          currentParentalLevel;
    QSqlDatabase *db;
    
    //
    //  Theme-related "widgets"
    //

    UIManagedTreeListType *video_tree_list;
    GenericTree           *video_tree_root;
    GenericTree           *video_tree_data;
    UITextType            *video_title;
    UITextType            *video_file;
    UITextType            *video_player;
    UIImageType           *video_poster;
};



#endif

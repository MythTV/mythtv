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

    VideoTree(QSqlDatabase *ldb,
              QString window_name,
              QString theme_filename,
              QWidget *parent = 0, 
              const char *name = 0);

    void buildVideoList();

  public slots:
  
    void handleTreeListSelection(int, IntVector*);
    void handleTreeListEntry(int, IntVector*);

  private:

    void         wireUpTheme();
    int          currentParentalLevel;
    QAccel       *accel;
    QSqlDatabase *db;
    
    //
    //  Theme-related "widgets"
    //

    UIManagedTreeListType *video_tree_list;
    GenericTree           *video_tree_data;
    UITextType            *video_title;
    UITextType            *video_file;
    UITextType            *video_player;
    UIImageType           *video_poster;
};



#endif

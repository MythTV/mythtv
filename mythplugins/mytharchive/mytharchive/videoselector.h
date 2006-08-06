/*
	videoselector.h

    header for the video selector interface screen
*/

#ifndef VIDEOSELECTOR_H_
#define VIDEOSELECTOR_H_

#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/dialogbox.h>

typedef struct
{
    int id;
    QString title;
    QString plot;
    QString category;
    QString filename;
    QString coverfile;
    int     parentalLevel;
    uint    size;
} VideoInfo;

class VideoSelector : public MythThemedDialog
{

  Q_OBJECT

  public:
    VideoSelector(MythMainWindow *parent, QString window_name,
                    QString theme_filename, const char *name = 0);

    ~VideoSelector(void);

    void keyPressEvent(QKeyEvent *e);

  public slots:
    void OKPressed(void);
    void cancelPressed(void);

    void showMenu(void);
    void closePopupMenu(void);
    void selectAll(void);
    void clearAll(void);

    void setCategory(int);
    void titleChanged(UIListBtnTypeItem *item);

  private:
    void updateVideoList(void);
    void updateSelectedList(void);
    void toggleSelectedState(void);
    void getVideoList(void);
    void wireUpTheme(void);
    vector<VideoInfo *> *getVideoListFromDB(void);
    bool checkParentPassword(void);
    void setParentalLevel(int which_level);

    vector<VideoInfo *>  *videoList;
    QPtrList<VideoInfo>  selectedList;

    int              currentParentalLevel;
    UITextType       *pl_text;

    UIListBtnType    *video_list;
    UITextType       *warning_text;

    UITextButtonType *ok_button;
    UITextButtonType *cancel_button;

    UISelectorType   *category_selector;
    UITextType       *title_text;
    UITextType       *filesize_text;
    UITextType       *plot_text;
    UIImageType      *cover_image;

    MythPopupBox     *popupMenu;
};

#endif



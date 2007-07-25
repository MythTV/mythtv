#ifndef VIDEODIALOG_H_
#define VIDEODIALOG_H_

#include <qstring.h>

#include <mythtv/mythdialogs.h>

#include <memory>

class Metadata;
class VideoList;

class VideoDialog : public MythDialog
{
    Q_OBJECT

  public:
    enum DialogType { DLG_BROWSER = 0x1, DLG_GALLERY = 0x2, DLG_TREE = 0x4 };

    VideoDialog(DialogType ltype, MythMainWindow *lparent,
                const QString &lwinName, const QString &lname,
                VideoList *video_list);

    virtual ~VideoDialog();

    virtual void playVideo(Metadata *someItem);

    GenericTree *getVideoTreeRoot(void);

    void setFileBrowser(bool toWhat) { isFileBrowser = toWhat; }
    void setFlatList(bool toWhat) { isFlatList = toWhat;}

    int videoExitType() { return m_exit_type; }

  protected slots:
    void slotDoCancel();
    void slotVideoTree();
    void slotVideoGallery();
    void slotVideoBrowser();
    void slotViewPlot();
    void slotDoFilter();
    void exitWin();
    virtual void slotParentalLevelChanged();
    virtual void slotWatchVideo();

  protected:
    virtual void updateBackground(void);
    virtual void parseContainer(QDomElement&) = 0;
    virtual void loadWindow(QDomElement &element);
    virtual void fetchVideos();
    virtual void setParentalLevel(int which_level);
    void shiftParental(int amount);
    bool createPopup();
    void cancelPopup(void);
    void doMenu(bool info);
    QButton *AddPopupViews();

    QPixmap myBackground;
    int currentParentalLevel;
    bool isFileBrowser;
    bool isFlatList;
    Metadata* curitem;
    MythPopupBox* popup;
    bool expectingPopup;
    QRect fullRect;
    int m_type;

    bool allowPaint;

    std::auto_ptr<XMLParse> theme;
    QDomElement xmldata;

    VideoList *m_video_list;
    GenericTree *video_tree_root;

  private:
    void jumpTo(const QString &location);
    void setExitType(int exit_type) { m_exit_type = exit_type; }

  private:
    int m_exit_type;
};

#endif

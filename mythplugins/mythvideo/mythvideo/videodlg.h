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
    enum DialogType { DLG_BROWSER, DLG_GALLERY, DLG_TREE };

    VideoDialog(DialogType lmyType, MythMainWindow *lparent,
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
    QButton* addDests(MythPopupBox* _popup = NULL);

    QPixmap myBackground;
    int currentParentalLevel;
    bool isFileBrowser;
    bool isFlatList;
    Metadata* curitem;
    MythPopupBox* popup;
    bool expectingPopup;
    QRect fullRect;
    DialogType myType;

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

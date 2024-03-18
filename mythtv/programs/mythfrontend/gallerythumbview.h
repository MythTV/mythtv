//! \file
//! \brief Implements Gallery Thumbnail screen

#ifndef GALLERYVIEW_H
#define GALLERYVIEW_H

#include "galleryslideview.h"
#include "galleryviews.h"

class DirectoryView;
class MythMenu;

//! Type of captions to display
enum ImageCaptionType : std::uint8_t {
    kNoCaption   = 0, //!< None
    kNameCaption = 1, //!< Filenames
    kDateCaption = 2, //!< Dates
    kUserCaption = 3  //!< Exif comments
};


//! Thumbnail screen
class GalleryThumbView : public MythScreenType
{
    Q_OBJECT
public:
    GalleryThumbView(MythScreenStack *parent, const char *name);
    ~GalleryThumbView() override;
    bool    Create() override; // MythScreenType

public slots:
    void    Start();
    void    Close() override; // MythScreenType
    static void    ClearSgDb()  { ImageManagerFe::ClearStorageGroup(); }

private:
    bool    keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void    mediaEvent(MythMediaEvent *event) override // MythUIType
            { m_mgr.DeviceEvent(event); }
    void    customEvent(QEvent *event) override; // MythUIType
    void    RemoveImages(const QStringList &ids, bool deleted = true);
    void    BuildImageList();
    void    ResetUiSelection();
    void    TransformItem(ImageFileTransform tran = kRotateCW);
    void    TransformMarked(ImageFileTransform tran = kRotateCW);
    void    UpdateImageItem(MythUIButtonListItem *item);
    void    UpdateScanProgress(const QString &scanner, int current, int total);
    void    StartSlideshow(ImageSlideShowType mode);
    void    SelectZoomWidget(int change);
    QString CheckThumbnail(MythUIButtonListItem *item, const ImagePtrK &im,
                           ImageIdList &request, int index);
    static void    UpdateThumbnail(MythUIButtonListItem *button, const ImagePtrK &im,
                            const QString &url, int index);
    void    MenuMain();
    void    MenuShow(MythMenu *mainMenu);
    void    MenuMarked(MythMenu *mainMenu);
    void    MenuPaste(MythMenu *mainMenu);
    void    MenuTransform(MythMenu *mainMenu);
    void    MenuAction(MythMenu *mainMenu);
    void    MenuSlideshow(MythMenu *mainMenu);
    bool    DirSelectUp();
    void    DirSelectDown();
    void    ShowDialog(const QString &msg, const QString &event = "");

private slots:
    void    LoadData(int parent);
    void    SelectImage(int id);
    void    ItemClicked(MythUIButtonListItem *item);
    void    SetUiSelection(MythUIButtonListItem *item);
    void    Slideshow()            { StartSlideshow(kNormalSlideShow); }
    void    RecursiveSlideshow()   { StartSlideshow(kRecursiveSlideShow); }
    void    ShowDetails();
    void    RotateCW()             { TransformItem(kRotateCW); }
    void    RotateCCW()            { TransformItem(kRotateCCW); }
    void    FlipHorizontal()       { TransformItem(kFlipHorizontal); }
    void    FlipVertical()         { TransformItem(kFlipVertical); }
    void    ResetExif()            { TransformItem(kResetToExif); }
    void    RotateCWMarked()       { TransformMarked(kRotateCW); }
    void    RotateCCWMarked()      { TransformMarked(kRotateCCW); }
    void    FlipHorizontalMarked() { TransformMarked(kFlipHorizontal); }
    void    FlipVerticalMarked()   { TransformMarked(kFlipVertical); }
    void    ResetExifMarked()      { TransformMarked(kResetToExif); }
    void    DoMarkItem(bool mark);
    void    MarkItem()             { DoMarkItem(true); }
    void    UnmarkItem()           { DoMarkItem(false); }
    void    DoMarkAll(bool mark = true);
    void    MarkAll()              { DoMarkAll(true); }
    void    UnmarkAll()            { DoMarkAll(false); }
    void    MarkInvertAll();
    void    DoHideItem(bool hide = true);
    void    HideItem()             { DoHideItem(true); }
    void    Unhide()               { DoHideItem(false); }
    void    DoHideMarked(bool hide = true);
    void    HideMarked()           { DoHideMarked(true); }
    void    UnhideMarked()         { DoHideMarked(false); }
    void    ShowRenameInput();
    void    DoShowHidden(bool show = true);
    void    ShowHidden()           { DoShowHidden(true); }
    void    HideHidden()           { DoShowHidden(false); }
    void    DoSetCover(bool reset = false);
    void    SetCover()             { DoSetCover(false); }
    void    ResetCover()           { DoSetCover(true); }
    void    DoShowType(int type);
    void    ShowType()             { DoShowType(kPicAndVideo); }
    void    HidePictures()         { DoShowType(kVideoOnly); }
    void    HideVideos()           { DoShowType(kPicOnly); }
    void    ZoomIn();
    void    ZoomOut();
    void    ShowSettings();
    void    DoScanAction(bool start);
    void    StartScan()            { DoScanAction(true); }
    void    StopScan()             { DoScanAction(false); }
    void    DeleteItem();
    void    DeleteMarked();
    void    Import();
    void    MakeDir();
    void    Eject();
    void    Copy(bool deleteAfter);
    void    Copy()                     { Copy(false); }
    void    Move();
    void    ShowPassword();
    static void DoRepeat(int on)       { gCoreContext->SaveSetting("GalleryRepeat", on); }
    static void RepeatOn()             { DoRepeat(1); }
    static void RepeatOff()            { DoRepeat(0); }

private:
    using IntPair = QPair<int,int>;

    // Theme widgets
    MythUIButtonList  *m_imageList        {nullptr};
    MythUIText        *m_captionText      {nullptr};
    MythUIText        *m_crumbsText       {nullptr};
    MythUIText        *m_emptyText        {nullptr};
    MythUIText        *m_hideFilterText   {nullptr};
    MythUIText        *m_typeFilterText   {nullptr};
    MythUIText        *m_positionText     {nullptr};
    MythUIText        *m_scanProgressText {nullptr};
    MythUIProgressBar *m_scanProgressBar  {nullptr};

    //! Theme buttonlist widgets implementing zoom levels
    QList<MythUIButtonList *> m_zoomWidgets;
    int                       m_zoomLevel {0};

    MythScreenStack &m_popupStack;
    ImageManagerFe  &m_mgr;  //!< Manages the images
    DirectoryView   *m_view {nullptr}; //!< List of images comprising the view
    InfoList         m_infoList; //!< Image details overlay

    //! Last scan updates received from scanners
    QHash<QString, IntPair> m_scanProgress;
    //! Scanners currently scanning
    QSet<QString>          m_scanActive;

    //! Current selection/marked files when menu is invoked
    MenuSubjects m_menuState;

    using ThumbLocation = QPair<MythUIButtonListItem *, int>;
    //! Buttons waiting for thumbnails to be created
    QMultiHash<int, ThumbLocation> m_pendingMap;

    QSet<int> m_thumbExists;          //!< Images where thumbnails are known to exist
    bool      m_editsAllowed {false}; //!< Edit privileges
};


#endif // GALLERYVIEW_H

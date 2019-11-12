//! \file
//! \brief Implements Gallery Thumbnail screen

#ifndef GALLERYVIEW_H
#define GALLERYVIEW_H

#include "galleryslideview.h"
#include "galleryviews.h"

class DirectoryView;
class MythMenu;

//! Type of captions to display
enum ImageCaptionType {
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
    ~GalleryThumbView();
    bool    Create() override; // MythScreenType

public slots:
    void    Start();
    void    Close() override; // MythScreenType
    void    ClearSgDb()  { m_mgr.ClearStorageGroup(); }

private:
    bool    keyPressEvent(QKeyEvent *) override; // MythScreenType
    void    mediaEvent(MythMediaEvent *event) override // MythUIType
            { m_mgr.DeviceEvent(event); }
    void    customEvent(QEvent *) override; // MythUIType
    void    RemoveImages(const QStringList &ids, bool deleted = true);
    void    BuildImageList();
    void    ResetUiSelection();
    void    TransformItem(ImageFileTransform tran = kRotateCW);
    void    TransformMarked(ImageFileTransform tran = kRotateCW);
    void    UpdateImageItem(MythUIButtonListItem *);
    void    UpdateScanProgress(const QString &, int, int);
    void    StartSlideshow(ImageSlideShowType mode);
    void    SelectZoomWidget(int change);
    QString CheckThumbnail(MythUIButtonListItem *, const ImagePtrK&,
                           ImageIdList &request, int);
    static void    UpdateThumbnail(MythUIButtonListItem *, const ImagePtrK&,
                            const QString &url, int);
    void    MenuMain();
    void    MenuShow(MythMenu *);
    void    MenuMarked(MythMenu *);
    void    MenuPaste(MythMenu *);
    void    MenuTransform(MythMenu *);
    void    MenuAction(MythMenu *);
    void    MenuSlideshow(MythMenu *);
    bool    DirSelectUp();
    void    DirSelectDown();
    void    ShowDialog(const QString&, const QString& = "");

private slots:
    void    LoadData(int);
    void    SelectImage(int);
    void    ItemClicked(MythUIButtonListItem *);
    void    SetUiSelection(MythUIButtonListItem *);
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
    void    MarkItem(bool = true);
    void    UnmarkItem()           { MarkItem(false); }
    void    MarkAll(bool = true);
    void    UnmarkAll()            { MarkAll(false); }
    void    MarkInvertAll();
    void    HideItem(bool hide = true);
    void    Unhide()               { HideItem(false); }
    void    HideMarked(bool hide = true);
    void    UnhideMarked()         { HideMarked(false); }
    void    ShowRenameInput();
    void    ShowHidden(bool show = true);
    void    HideHidden()           { ShowHidden(false); }
    void    SetCover(bool reset = false);
    void    ResetCover()           { SetCover(true); }
    void    ShowType(int = kPicAndVideo);
    void    HidePictures()         { ShowType(kVideoOnly); }
    void    HideVideos()           { ShowType(kPicOnly); }
    void    ZoomIn();
    void    ZoomOut();
    void    ShowSettings();
    void    StartScan(bool start = true);
    void    StopScan()             { StartScan(false); }
    void    DeleteItem();
    void    DeleteMarked();
    void    Import();
    void    MakeDir();
    void    Eject();
    void    Copy(bool deleteAfter = false);
    void    Move();
    void    ShowPassword();
    void    RepeatOn(int on = 1)   { gCoreContext->SaveSetting("GalleryRepeat", on); }
    void    RepeatOff()            { RepeatOn(0); }

private:
    typedef QPair<int,int> IntPair;

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

    typedef QPair<MythUIButtonListItem *, int> ThumbLocation;
    //! Buttons waiting for thumbnails to be created
    QHash<int, ThumbLocation> m_pendingMap;

    QSet<int> m_thumbExists;          //!< Images where thumbnails are known to exist
    bool      m_editsAllowed {false}; //!< Edit privileges
};


#endif // GALLERYVIEW_H

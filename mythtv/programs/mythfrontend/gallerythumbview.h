//! \file
//! \brief Implements Gallery Thumbnail screen

#ifndef GALLERYVIEW_H
#define GALLERYVIEW_H

#include <QMap>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QPair>
#include <QEvent>
#include <QKeyEvent>
#include <QDir>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    #include <QTemporaryDir>
#else

#define DIRNAME "import"
class QTemporaryDir
{
public:
    QTemporaryDir() : m_dir(QDir::temp())
    {
        m_dir.mkdir(DIRNAME);
        m_dir.cd(DIRNAME);
    }

    ~QTemporaryDir()
    {
        foreach (const QString &name, m_dir.entryList(QDir::Files | QDir::NoDotAndDotDot))
            m_dir.remove(name);
        m_dir.cdUp();
        m_dir.rmdir(DIRNAME);
    }

    bool isValid() { return m_dir.exists(); }
    QString path() { return m_dir.absolutePath(); }

private:
    QDir m_dir;
};

#endif

#include <mythuitext.h>
#include <mythuibuttonlist.h>
#include <mythdialogbox.h>
#include <imagemetadata.h>
#include <mythmediamonitor.h>

#include "galleryviews.h"
#include "galleryslideview.h"



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
    bool    Create();

public slots:
    void    Start();

signals:
    void    ThumbnailChanged(int);

private:
    bool    keyPressEvent(QKeyEvent *);
    void    customEvent(QEvent *);
    void    BuildImageList();
    void    ResetUiSelection();
    void    TransformItem(ImageFileTransform tran = kRotateCW);
    void    TransformMarked(ImageFileTransform tran = kRotateCW);
    void    UpdateImageItem(MythUIButtonListItem *);
    void    UpdateScanProgress(QString, QString, QString);
    void    StartSlideshow(ImageSlideShowType mode);
    void    SelectZoomWidget(int change);
    void    ImportFiles(QStringList);
    void    SetUiSelection(MythUIButtonListItem *);
    QString CheckThumbnail(MythUIButtonListItem *, ImageItem *,
                           QStringList &, int = 0);
    void    UpdateThumbnail(MythUIButtonListItem *, ImageItem *,
                            const QString &url, int = 0);
    void    MenuMain();
    void    MenuShow(MythMenu *);
    void    MenuMarked(MythMenu *);
    void    MenuPaste(MythMenu *);
    void    MenuTransform(MythMenu *);
    void    MenuAction(MythMenu *);
    void    MenuImport(MythMenu *);
    void    MenuSlideshow(MythMenu *);
    bool    DirSelectUp();
    void    DirSelectDown();
    void    ShowDialog(QString, QString = "");

private slots:
    void    LoadData(int parent = -1);
    void    SelectImage(int id);
    void    ItemClicked(MythUIButtonListItem *);
    void    ItemSelected(MythUIButtonListItem *);
    void    Slideshow()            { StartSlideshow(kNormalSlideShow); }
    void    RecursiveSlideshow()   { StartSlideshow(kRecursiveSlideShow); }
    void    ShowDetails();
    void    RotateCW()             { TransformItem(kRotateCW); }
    void    RotateCCW()            { TransformItem(kRotateCCW); }
    void    FlipHorizontal()       { TransformItem(kFlipHorizontal); }
    void    FlipVertical()         { TransformItem(kFlipVertical); }
    void    ResetExif()            { TransformItem(kResetExif); }
    void    RotateCWMarked()       { TransformMarked(kRotateCW); }
    void    RotateCCWMarked()      { TransformMarked(kRotateCCW); }
    void    FlipHorizontalMarked() { TransformMarked(kFlipHorizontal); }
    void    FlipVerticalMarked()   { TransformMarked(kFlipVertical); }
    void    ResetExifMarked()      { TransformMarked(kResetExif); }
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
    void    ShowCaptions(int = kNoCaption);
    void    CaptionsName()         { ShowCaptions(kNameCaption); }
    void    CaptionsDate()         { ShowCaptions(kDateCaption); }
    void    CaptionsComment()      { ShowCaptions(kUserCaption); }
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
    void    ShowImport(bool deleteAfter = false);
    void    ShowMoveImport()           { ShowImport(true); }
    void    RunImportCmd();
    void    MakeDir();
    void    Eject()  { MediaMonitor::GetMediaMonitor()->ChooseAndEjectMedia(); }
    void    Copy();
    void    Move();
    void    ShowPassword();
    void    RepeatOn(int on = 1)  { gCoreContext->SaveSetting("GalleryRepeat", on); }
    void    RepeatOff()        { RepeatOn(0); }

private:
    ImageDbReader     *m_db;
    DirectoryView     *m_view;
    MythScreenStack   *m_popupStack,       *m_mainStack;
    MythUIButtonList  *m_imageList;
    MythUIText        *m_captionText,      *m_crumbsText;
    MythUIText        *m_hideFilterText,   *m_typeFilterText;
    MythUIText        *m_positionText,     *m_emptyText;
    MythUIText        *m_scanProgressText;
    MythUIProgressBar *m_scanProgressBar;
    bool               m_scanInProgress;
    int                m_zoomLevel;

    //! Image details overlay
    InfoList *m_infoList;

    //! Current selection/marked files when menu is invoked
    MenuSubjects m_menuState;

    //! Theme buttonlist widgets implementing zoom levels
    QList<MythUIButtonList *> m_zoomWidgets;

    //! Buttons waiting for BE to create thumbnail
    typedef QPair<MythUIButtonListItem *, int> ThumbLocation;
    QHash<int, ThumbLocation> m_pendingMap;

    //! Urls of images where thumbnails are known to exist
    QHash<int, QString> m_url;

    //! Edit privileges
    bool m_editsAllowed;

    //! Distinguishes Import(Copy) from Import(Move)
    bool m_deleteAfterImport;

    //! Temporary dir for import commands/scripts
    QTemporaryDir *m_importTmp;
};

#endif // GALLERYVIEW_H

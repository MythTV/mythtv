#ifndef GALLERYVIEW_H
#define GALLERYVIEW_H

// Qt headers
#include <QStringList>

// MythTV headers
#include "mythscreentype.h"
#include "mythuitext.h"
#include "mythuibuttonlist.h"
#include "mythuiimage.h"
#include "mythdialogbox.h"
#include "mythprogressdialog.h"
#include "mythuitextedit.h"
#include "imagemetadata.h"

#include "galleryviewhelper.h"
#include "gallerywidget.h"



class GalleryView : public MythScreenType
{
    Q_OBJECT

public:
    GalleryView(MythScreenStack *parent, const char *name);
    ~GalleryView();
    bool Create();

    GalleryViewHelper     *m_galleryViewHelper;

public slots:
    void LoadData();
    void UpdateThumbnail(ImageMetadata *, int);
    void ResetThumbnailProgress();
    void UpdateThumbnailProgress(int, int);

    void ResetSyncProgress();
    void UpdateSyncProgress(int, int);

private:
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);
    void UpdateImageList();
    void ResetImageItems();

private slots:
    void ItemSelected(MythUIButtonListItem *);
    void UpdateImageItem(MythUIButtonListItem *);
    void UpdateText(MythUIButtonListItem *);
    void UpdateThumbnail(MythUIButtonListItem *);

    void MenuMain();
    void MenuInformation();
    void MenuMetadata(MythMenu *);
    void MenuSelection(MythMenu *);
    void MenuFile(MythMenu *);
    void MenuSettings();

    void ShowFiles();
    void ShowRandomFiles();
    GalleryWidget* ShowFile();

    bool DirSelectUp();
    void DirSelectDown();

    void FileDetails();
    void FileRotateCW();
    void FileRotateCCW();
    void FileFlipHorizontal();
    void FileFlipVertical();
    void FileZoomIn();
    void FileZoomOut();
    void FileSelectOne();
    void FileDeselectOne();
    void FileSelectAll();
    void FileDeselectAll();
    void FileInvertAll();
    void FileHide();
    void FileUnhide();
    void FileDelete();
    void FileDeleteSelected();
    void FileRenameInput();
    void FileRename(QString &);

    void ConfirmStartSync();
    void ConfirmStopSync();
    void ConfirmFileDelete();
    void ConfirmFileDeleteSelected();

private:
    ImageMetadata         *GetImageMetadataFromSelectedButton();
    ImageMetadata         *GetImageMetadataFromButton(MythUIButtonListItem *item);

    GallerySyncStatusThread     *m_syncStatusThread;

    // used to show the menus and other popups
    MythDialogBox           *m_menuPopup;
    MythConfirmationDialog  *m_confirmPopup;
    MythTextInputDialog     *m_inputPopup;
    MythScreenStack         *m_popupStack;
    MythScreenStack         *m_mainStack;

    MythUIButtonList   *m_imageList;
    MythUIText         *m_captionText;
    MythUIText         *m_crumbsText;
    MythUIText         *m_positionText;
    MythUIText         *m_imageText;
    MythUIImage        *m_selectedImage;

    MythUIText         *m_syncProgressText;
    MythUIText         *m_thumbProgressText;
};

#endif // GALLERYVIEW_H

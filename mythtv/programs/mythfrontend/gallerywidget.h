#ifndef GALLERYWIDGET_H
#define GALLERYWIDGET_H

// Qt headers
#include <QTimer>

// MythTV headers
#include "mythscreentype.h"
#include "mythdialogbox.h"
#include "imagemetadata.h"

#include "galleryviewhelper.h"
#include "galleryfilehelper.h"



class ImageLoadingThread : public QThread
{
public:
    ImageLoadingThread();
    void setImage(MythUIImage *image,
                  ImageMetadata *imageData,
                  QString &url);
    void run();

private:
    MythUIImage     *m_image;
    ImageMetadata   *m_imageData;
    QString          m_url;
};



class GalleryWidget : public MythScreenType
{
    Q_OBJECT
public:
    GalleryWidget(MythScreenStack *parent,
                const char *name,
                GalleryViewHelper*);
    ~GalleryWidget();

    bool Create();
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *);
    void LoadFile();
    void ShowFileDetails();

public slots:
    void StartNormalSlideShow();
    void StartRandomSlideShow();

private:
    void HandleFadeTransition();
    void HandleNoTransition();
    void ShowInformation(QString &);
    void HideFileDetails();

    QString CreateImageUrl(QString &);

private slots:
    void MenuMain();
    void MenuMetadata(MythMenu *);
    void ShowPrevFile();
    void ShowNextFile();
    void ShowRandomFile();
    void HandleImageTransition();

    void StopSlideShow();
    void PauseSlideShow();
    void ResumeSlideShow();

private:
    MythDialogBox      *m_menuPopup;
    MythScreenStack    *m_popupStack;
    MythUIImage        *m_image1;
    MythUIImage        *m_image2;
    MythUIText         *m_status;

    MythUIButtonList       *m_infoList;
    QList<MythUIImage *>   *m_fileList;
    QList<ImageMetadata *>       *m_fileDataList;

    GalleryFileHelper  *m_fh;
    GalleryViewHelper  *m_gvh;
    MythGenericTree    *m_selectedNode;
    ImageLoadingThread *m_ilt;

    int                 m_index;
    int                 m_slideShowType;
    int                 m_slideShowTime;
    int                 m_transitionTime;
    int                 m_transitionType;
    QTimer             *m_timer;

    QString             m_backendHost;
    QString             m_backendPort;

    bool                m_infoVisible;
};

#endif // GALLERYWIDGET_H

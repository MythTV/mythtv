//! \file
//! \brief Slideshow screen

#ifndef GALLERYWIDGET_H
#define GALLERYWIDGET_H

#include <QTimer>
#include <QEvent>
#include <QPoint>

#include <mythscreentype.h>
#include <mythdialogbox.h>
#include <imagemetadata.h>

#include "gallerytransitions.h"
#include "galleryviews.h"
#include "galleryslide.h"


//! Type of slide show
enum ImageSlideShowType {
    kBrowseSlides       = 0,
    kNormalSlideShow    = 1,
    kRecursiveSlideShow = 2
};


//! \brief Slideshow screen
class GallerySlideView : public MythScreenType
{
    Q_OBJECT
public:
    GallerySlideView(MythScreenStack *parent,
                     const char *name);
    ~GallerySlideView();
    bool Create();

public slots:
    void Start(ImageSlideShowType type, int parentId, int selectedId = 0,
               bool newScreen = true);
    void Pulse();
    void ThumbnailChange(int id);

signals:
    void ImageSelected(int);

private:
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *);
    void MenuMain();
    void MenuTransforms(MythMenu *);
    void Suspend();
    void Release();
    void Transform(ImageFileTransform);
    void Zoom(int = 0);
    void Pan(QPoint = QPoint(0, 0));
    bool SlideShowNotActive()
    { return m_slideShowType == kBrowseSlides || m_paused; }
    void SetStatus(QString msg);

private slots:
    void ShowPrevSlide();
    void ShowNextSlide(bool useTransition = true);
    void SlideAvailable(int count);
    void TransitionComplete();
    void ShowSlide(int direction = 0);
    void StartNormal()     { Start(kNormalSlideShow, m_view->GetParentId(),
                                   m_view->GetSelected()->m_id, false); }
    void StartRecursive()  { Start(kRecursiveSlideShow, m_view->GetParentId(),
                                   m_view->GetSelected()->m_id, false); }
    void Stop();
    void Pause();
    void Resume();
    void ShowInfo();
    void HideInfo();
    void ShowCaptions();
    void HideCaptions();
    void PlayVideo();

private:
    // Theme widgets
    MythUIImage *m_uiImage;
    MythUIText  *m_uiStatus, *m_uiSlideCount, *m_uiCaptionText, *m_uiHideCaptions;

    ImageSlideShowType m_slideShowType;
    int          m_slideShowTime;
    QTimer      *m_timer;
    bool         m_paused, m_suspended, m_showCaptions;

    //! Image details overlay
    InfoList *m_infoList;

    //! List of images in the slideshow
    FlatView *m_view;

    //! \brief A queue of slides used to display images.
    //! \details Image requests go to successive slides. When loaded the slide is available
    //! for display. The head is removed once it is displayed. When no longer on display
    //! it is returned to the back of the queue for re-use.
    SlideBuffer *m_slides;

    //! Slide currently displayed or being transitioned from
    Slide *m_slideCurrent;
    //! Slide being transitioned to, NULL if no transition in progress.
    Slide *m_slideNext;

    //! Number of slides waiting to be displayed
    int m_pending;

    //! Transitions available
    TransitionRegistry m_availableTransitions;
    //! Selected transition
    Transition *m_transition;
    //! Instant transition that is always used for start-up & image updates
    TransitionNone m_updateTransition;

    //! \brief Filtered view of the image database
    //! \details A helper that provides read-only access to the database. Results
    //! are filtered by 'Show Hidden' setting and sorted iaw Thumbview ordering.
    ImageDbReader *m_db;
};

#endif // GALLERYWIDGET_H

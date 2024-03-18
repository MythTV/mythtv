//! \file
//! \brief Slideshow screen

#ifndef GALLERYWIDGET_H
#define GALLERYWIDGET_H

#include "gallerytransitions.h"
#include "galleryinfo.h"


class MythMenu;
class FlatView;

//! Type of slide show
enum ImageSlideShowType : std::uint8_t {
    kBrowseSlides       = 0,
    kNormalSlideShow    = 1,
    kRecursiveSlideShow = 2
};


//! Slideshow screen
class GallerySlideView : public MythScreenType
{
    Q_OBJECT
public:
    GallerySlideView(MythScreenStack *parent, const char *name, bool editsAllowed);
    ~GallerySlideView() override;
    bool Create() override; // MythScreenType

public slots:
    void Start(ImageSlideShowType type, int parentId, int selectedId = 0);
    void Close() override; // MythScreenType
    void Pulse() override; // MythScreenType

signals:
    void ImageSelected(int);

private:
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType
    void MenuMain();
    void MenuTransforms(MythMenu &mainMenu);
    void Suspend();
    void Release();
    void Transform(ImageFileTransform state);
    void Zoom(int increment = 0);
    void Pan(QPoint offset = QPoint(0, 0));
    void SetStatus(QString msg, bool delay = false);
    void ClearStatus(const Slide &slide);

private slots:
    void ShowPrevSlide(int inc = 1);
    void ShowNextSlide(int inc, bool useTransition = true);
    void ShowNextSlide();
    void SlideAvailable(int count);
    void TransitionComplete();
    void ShowSlide(int direction = 0);
    void Stop();
    void Play(bool useTransition);
    void Play() { Play(true); };
    static void RepeatOn(int on)       { gCoreContext->SaveSetting("GalleryRepeat", on); }
    static void RepeatOn()             { RepeatOn(1); }
    static void RepeatOff()            { RepeatOn(0); }
    void ShowInfo();
    void HideInfo();
    void ShowCaptions();
    void HideCaptions();
    void PlayVideo();
    void ShowStatus();

private:
    // Theme widgets
    MythUIImage *m_uiImage        {nullptr};
    MythUIText  *m_uiStatus       {nullptr};
    MythUIText  *m_uiSlideCount   {nullptr};
    MythUIText  *m_uiCaptionText  {nullptr};
    MythUIText  *m_uiHideCaptions {nullptr};

    ImageManagerFe &m_mgr;  //!< Manages the images
    FlatView       *m_view        {nullptr}; //!< List of images comprising the slideshow

    TransitionRegistry m_availableTransitions; //!< Transitions available
    Transition        &m_transition;           //!< Selected transition
    //! Instant transition that is always used for start-up & image updates
    TransitionNone     m_updateTransition;

    SlideBuffer m_slides;                  //!< A queue of slides used to display images.
    InfoList    m_infoList;                //!< Image details overlay
    std::chrono::milliseconds m_slideShowTime   {3s};  //!< Time to display a slide in a slideshow
    QTimer      m_timer;                   //!< Slide duration timer
    QTimer      m_delay;                   //!< Status delay timer
    QString     m_statusText;              //!< Text to display as status
    bool        m_playing         {false}; //!< True when slideshow is running
    bool        m_suspended       {false}; //!< True when transition is running or video playing
    bool        m_showCaptions    {true};  //!< If true, captions are shown
    bool        m_transitioning   {false}; //!< True when a transition is in progress
    bool        m_editsAllowed    {false}; //!< True when edits are enabled
};

#endif // GALLERYWIDGET_H

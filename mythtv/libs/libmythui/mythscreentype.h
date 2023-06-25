#ifndef MYTHSCREEN_TYPE_H_
#define MYTHSCREEN_TYPE_H_

#include <utility>

// Qt headers
#include <QEvent>
#include <QSemaphore>

// MythTV headers
#include "mythuicomposite.h"
#include "mythuiutils.h"

class QInputMethodEvent;
class ScreenLoadTask;
class MythScreenStack;
class MythUIBusyDialog;

/**
 * \class ScreenLoadCompletionEvent
 *
 * \brief Event that can be dispatched from a MythScreenType when it has
 *        completed loading.
 */
class MUI_PUBLIC ScreenLoadCompletionEvent : public QEvent
{
  public:
    explicit ScreenLoadCompletionEvent(QString id) :
        QEvent(kEventType), m_id(std::move(id)) { }

    QString GetId() { return m_id; }

    static const Type kEventType;

  private:
    QString m_id;
};

/**
 * \class MythScreenType
 * \brief Screen in which all other widgets are contained and rendered.
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythScreenType : public MythUIComposite
{
    Q_OBJECT

    friend class ScreenLoadTask;

  public:
    MythScreenType(MythScreenStack *parent, const QString &name,
                   bool fullscreen = true);
    ~MythScreenType() override;

    virtual bool Create(void); // do the actual work of making the screen.
    bool keyPressEvent(QKeyEvent *event) override; // MythUIType
    bool inputMethodEvent(QInputMethodEvent *event) override;// MythUIType
    bool gestureEvent(MythGestureEvent *event) override; // MythUIType
    virtual void ShowMenu(void);

    void doInit(void);
    void LoadInForeground(void);
    bool IsInitialized(void) const;

    // if the widget is full screen and obscures widgets below it
    bool IsFullscreen(void) const;
    void SetFullscreen(bool full);

    MythUIType *GetFocusWidget(void) const;
    bool SetFocusWidget(MythUIType *widget = nullptr);
    virtual bool NextPrevWidgetFocus(bool up_or_down);
    void BuildFocusList(void);

    MythScreenStack *GetScreenStack() const;

    virtual void aboutToHide(void);
    virtual void aboutToShow(void);

    bool IsDeleting(void) const;
    void SetDeleting(bool deleting);

    bool IsLoading(void) const { return m_isLoading; }
    bool IsLoaded(void) const { return m_isLoaded; }

    MythPainter *GetPainter(void) override; // MythUIType

  public slots:
    virtual void Close();

  signals:
    void Exiting();

  protected:
    // for the global store..
    MythScreenType(MythUIType *parent, const QString &name,
                   bool fullscreen = true);

    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
    bool ParseElement( const QString &filename,
                       QDomElement &element,
                       bool showWarnings) override; // MythUIType

    virtual void Load(void);   // ONLY to be used for loading data, NO UI WORK or it will segfault
    virtual void Init(void);   // UI work to draw data loaded

    void LoadInBackground(const QString& message = "");
    void ReloadInBackground(void);

    void OpenBusyPopup(const QString& message = "");
    void CloseBusyPopup(void);
    void SetBusyPopupMessage(const QString &message);
    void ResetBusyPopup(void);

    bool m_fullScreen                {false};
    bool m_isDeleting                {false};

    QSemaphore m_loadLock            {1};
    volatile bool m_isLoading        {false};
    volatile bool m_isLoaded         {false};
    bool m_isInitialized             {false};

    MythUIType *m_currentFocusWidget {nullptr};
    //TODO We are currently dependant on the internal sorting of QMap for
    //     entries to be iterated in the correct order, this should probably
    //     be changed.
    FocusInfoType m_focusWidgetList;

    MythScreenStack  *m_screenStack  {nullptr};
    MythUIBusyDialog *m_busyPopup    {nullptr};

    QRegion m_savedMask;

    friend class XMLParseBase;
};

#endif

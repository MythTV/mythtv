#ifndef MYTHSCREEN_TYPE_H_
#define MYTHSCREEN_TYPE_H_

#include "mythuitype.h"
#include "mythuitext.h"
#include "mythuiutils.h"

#include <QHash>

class MythScreenStack;
class MythUIBusyDialog;

/**
 * \class ScreenLoadCompletionEvent
 *
 * \brief Event that can be dispatched from a MythScreenType when it has
 *        completed loading.
 */
class MPUBLIC ScreenLoadCompletionEvent : public QEvent
{
  public:
    ScreenLoadCompletionEvent(const QString &id) :
        QEvent(kEventType), m_id(id) { }

    QString GetId() { return m_id; }

    static Type kEventType;

  private:
    QString m_id;
};

/**
 * \class MythScreenType
 * \brief Screen in which all other widgets are contained and rendered.
 *
 * \ingroup MythUI_Widgets
 */
class MPUBLIC MythScreenType : public MythUIType
{
    Q_OBJECT

  public:
    MythScreenType(MythScreenStack *parent, const QString &name,
                   bool fullscreen = true);
    virtual ~MythScreenType();

    virtual bool Create(void); // do the actual work of making the screen.
    virtual bool keyPressEvent(QKeyEvent *);
    virtual bool gestureEvent(MythGestureEvent *);
    virtual void ShowMenu(void);

    void doInit(void);
    void LoadInForeground(void);
    bool IsInitialized(void) const;

    // if the widget is full screen and obscures widgets below it
    bool IsFullscreen(void) const;
    void SetFullscreen(bool full);

    MythUIType *GetFocusWidget(void) const;
    bool SetFocusWidget(MythUIType *widget = NULL);
    virtual bool NextPrevWidgetFocus(bool up_or_down);
    void BuildFocusList(void);

    MythScreenStack *GetScreenStack() const;

    virtual void aboutToHide(void);
    virtual void aboutToShow(void);

    bool IsDeleting(void) const;
    void SetDeleting(bool deleting);

    bool IsLoading(void) { return m_IsLoading; }
    bool IsLoaded(void) { return m_IsLoaded; }

    void SetTextFromMap(QHash<QString, QString> &infoMap);
    void ResetMap(QHash<QString, QString> &infoMap);

    virtual MythPainter *GetPainter(void);

  public slots:
    virtual void Close();

  signals:
    void Exiting();

  protected:
    // for the global store..
    MythScreenType(MythUIType *parent, const QString &name,
                   bool fullscreen = true);

    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);

    virtual void Load(void);   // ONLY to be used for loading data, NO UI WORK
    virtual void Init(void);   // UI work to draw data loaded

    void LoadInBackground(QString message = "");
    void ReloadInBackground(void);

    void OpenBusyPopup(QString message = "");
    void CloseBusyPopup(void);
    void SetBusyPopupMessage(const QString &message);
    void ResetBusyPopup(void);

    bool m_FullScreen;
    bool m_IsDeleting;
    bool m_IsLoading;
    bool m_IsLoaded;
    bool m_IsInitialized;

    MythUIType *m_CurrentFocusWidget;
    //TODO We are currently dependant on the internal sorting of QMap for
    //     entries to be iterated in the correct order, this should probably
    //     be changed.
    QMap<int, MythUIType *> m_FocusWidgetList;

    MythScreenStack *m_ScreenStack;
    MythUIBusyDialog *m_BusyPopup;

    QRegion m_SavedMask;

    friend class XMLParseBase;
};

#endif

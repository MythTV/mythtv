#ifndef MYTHSCREEN_TYPE_H_
#define MYTHSCREEN_TYPE_H_

#include "mythuitype.h"
#include "mythuitext.h"
#include "mythuiutils.h"

#include <QHash>

class MythScreenStack;

/**
 * Base Screen type
 */
class MPUBLIC MythScreenType : public MythUIType
{
    Q_OBJECT

  public:
    MythScreenType(MythScreenStack *parent, const QString &name,
                   bool fullscreen = true);
    virtual ~MythScreenType();

    virtual bool Create(void); // do the actual work of making the screen.
    virtual void Init(void);
    virtual bool keyPressEvent(QKeyEvent *);

    // if the widget is full screen and obscures widgets below it
    bool IsFullscreen(void) const;
    void SetFullscreen(bool full);

    MythUIType *GetFocusWidget(void) const;
    bool SetFocusWidget(MythUIType *widget = NULL);
    virtual bool NextPrevWidgetFocus(bool up_or_down);
    bool BuildFocusList(void);

    MythScreenStack *GetScreenStack() const;

    virtual void aboutToHide(void);
    virtual void aboutToShow(void);

    bool IsDeleting(void) const;
    void SetDeleting(bool deleting);

    void SetTextFromMap(QHash<QString, QString> &infoMap);
    void ResetMap(QHash<QString, QString> &infoMap);

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
    virtual bool ParseElement(QDomElement &element);

    bool m_FullScreen;
    bool m_IsDeleting;

    MythUIType *m_CurrentFocusWidget;
    //TODO We are currently dependant on the internal sorting of QMap for
    //     entries to be iterated in the correct order, this should probably
    //     be changed.
    QMap<int, MythUIType *> m_FocusWidgetList;

    MythScreenStack *m_ScreenStack;
    
    QRegion m_SavedMask;

    friend class XMLParseBase;
};

#endif

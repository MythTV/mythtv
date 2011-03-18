#ifndef MYTHUI_BUTTON_H_
#define MYTHUI_BUTTON_H_

#include <QString>

#include "mythuitype.h"
#include "mythuistatetype.h"
#include "mythuitext.h"

#include "mythgesture.h"

/** \class MythUIButton
 *
 *  \brief A single button widget
 *
 *  Has multiple states with backgrounds and fonts, text and optional
 *  checkbox (dual/tri state)
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIButton : public MythUIType
{
    Q_OBJECT
  public:
    MythUIButton(MythUIType *parent, const QString &name);
   ~MythUIButton();

    virtual void Reset(void);

    virtual bool gestureEvent(MythGestureEvent *event);
    virtual bool keyPressEvent(QKeyEvent *);

    void SetText(const QString &msg);
    QString GetText(void) const;
    QString GetDefaultText(void) const;

    void Push(bool lock=false);

    void SetLockable(bool lockable) { m_Lockable = lockable; };
    void SetLocked(bool locked);

  protected slots:
    void Select();
    void Deselect();
    void Enable();
    void Disable();
    void UnPush();

  signals:
    void Clicked();

  protected:
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    void SetInitialStates(void);
    void SetState(QString state);

    QString m_Message;
    QString m_ValueText;

    MythUIStateType *m_BackgroundState;
    MythUIText *m_Text;

    QString m_state;

    bool m_Pushed;
    bool m_Lockable;
    class QTimer *m_clickTimer;
};

#endif

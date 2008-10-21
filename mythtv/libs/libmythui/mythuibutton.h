#ifndef MYTHUI_BUTTON_H_
#define MYTHUI_BUTTON_H_

#include <QString>

#include "mythuitype.h"
#include "mythuistatetype.h"
#include "mythuitext.h"

#include "mythgesture.h"

/** \class MythUIButton
 *
 *  \brief Single button widget
 *
 *  Has multiple states with backgrounds and fonts, text and optional
 *  checkbox (dual/tri state)
 *
 */
class MythUIButton : public MythUIType
{
    Q_OBJECT
  public:
    MythUIButton(MythUIType *parent, const QString &name);
   ~MythUIButton();

    virtual void Reset(void);

    virtual void gestureEvent(MythUIType *uitype, MythGestureEvent *event);
    virtual bool keyPressEvent(QKeyEvent *);

    void SetText(const QString &msg);
    QString GetText() const { return m_Text->GetText(); }

  protected slots:
    void Select();
    void Deselect();
    void Enable();
    void Disable();

  signals:
    void Clicked();

  protected:
    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    void SetInitialStates(void);

    MythUIStateType *m_BackgroundState;
    MythUIText *m_Text;

    QString m_state;
};

#endif

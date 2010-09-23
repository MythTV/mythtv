#ifndef MYTHUI_CHECKBOX_H_
#define MYTHUI_CHECKBOX_H_

// MythUI headers
#include "mythuitype.h"
#include "mythuistatetype.h"

/** \class MythUICheckBox
 *
 * \brief A checkbox widget supporting three check states - on,off,half and two
 *        conditions - selected and unselected.
 *
 * \ingroup MythUI_Widgets
 */
class MPUBLIC MythUICheckBox : public MythUIType
{
    Q_OBJECT

  public:
    MythUICheckBox(MythUIType *parent, const QString &name);
   ~MythUICheckBox();

    enum StateType { None = 0, Normal, Disabled, Active, Selected,
                     SelectedInactive };

    virtual bool gestureEvent(MythGestureEvent *event);
    virtual bool keyPressEvent(QKeyEvent *);

    void toggleCheckState(void);

    void SetCheckState(MythUIStateType::StateType state);
    void SetCheckState(bool onoff);

    MythUIStateType::StateType GetCheckState() const;
    bool GetBooleanCheckState(void) const;

  protected slots:
    void Select();
    void Deselect();
    void Enable();
    void Disable();

  signals:
    void valueChanged();
    void toggled(bool);

  protected:
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    void SetInitialStates(void);

    MythUIStateType *m_BackgroundState;
    MythUIStateType *m_CheckState;

    MythUIStateType::StateType m_currentCheckState;
    QString m_state;
};

#endif

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
class MUI_PUBLIC MythUICheckBox : public MythUIType
{
    Q_OBJECT

  public:
    MythUICheckBox(MythUIType *parent, const QString &name);
   ~MythUICheckBox() override = default;

    enum StateType : std::uint8_t
                   { None = 0, Normal, Disabled, Active, Selected,
                     SelectedInactive };

    bool gestureEvent(MythGestureEvent *event) override; // MythUIType
    bool keyPressEvent(QKeyEvent *event) override; // MythUIType

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
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
    void Finalize(void) override; // MythUIType

    void SetInitialStates(void);

    MythUIStateType *m_backgroundState {nullptr};
    MythUIStateType *m_checkState      {nullptr};

    MythUIStateType::StateType m_currentCheckState {MythUIStateType::Off};
    QString m_state                    {"active"};
};

#endif

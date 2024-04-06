#ifndef MYTHUISPINBOX_H_
#define MYTHUISPINBOX_H_

#include "mythuibuttonlist.h"

/** \class MythUISpinBox
 *
 * \brief A widget for offering a range of numerical values where only the
 *        the bounding values and interval are known.
 *
 * Where a list of specific values is required instead, then use
 * MythUIButtonList instead.
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUISpinBox : public MythUIButtonList
{
    Q_OBJECT
  public:
    MythUISpinBox(MythUIType *parent, const QString &name)
        : MythUIButtonList(parent, name) {}
    ~MythUISpinBox() override = default;

    void SetRange(int low, int high, int step, uint pageMultiple = 5);

    void SetValue(int val) override // MythUIButtonList
        { SetValueByData(val); }
    void SetValue(const QString &val) override // MythUIButtonList
        { SetValueByData(val.toInt()); }
    void AddSelection (int value, const QString &label = "");
    QString GetValue(void) const override  // MythUIButtonList
        { return GetDataValue().toString(); }
    int GetIntValue(void) const override // MythUIButtonList
        { return GetDataValue().toInt(); }
    bool keyPressEvent(QKeyEvent *event) override; // MythUIButtonList

    template <typename T>
        std::enable_if_t<std::chrono::__is_duration<T>::value, T>
        GetDuration()
    { return T(GetDataValue().toInt()); }
    template <typename T>
        std::enable_if_t<std::chrono::__is_duration<T>::value, void>
        SetDuration(T val)
    { SetValueByData(static_cast<int>(val.count())); }

  protected:
    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIButtonList
    void CopyFrom(MythUIType *base) override; // MythUIButtonList
    void CreateCopy(MythUIType *parent) override; // MythUIButtonList

    bool MoveDown(MovementUnit unit = MoveItem, uint amount = 0) override; // MythUIButtonList
    bool MoveUp(MovementUnit unit = MoveItem, uint amount = 0) override; // MythUIButtonList
    void ShowEntryDialog(QString initialEntry);

    bool    m_hasTemplate      {false};
    QString m_negativeTemplate;
    QString m_zeroTemplate;
    QString m_positiveTemplate;

    uint    m_moveAmount       {0};
    int     m_low              {0};
    int     m_high             {0};
    int     m_step             {0};
};

// Convenience Dialog to allow entry of a Spinbox value

class MUI_PUBLIC SpinBoxEntryDialog : public MythScreenType
{
    Q_OBJECT
  public:
    SpinBoxEntryDialog(MythScreenStack *parent, const char *name,
        MythUIButtonList *parentList, QString searchText,
        int low, int high, int step);
    ~SpinBoxEntryDialog(void) override = default;

    bool Create(void) override; // MythScreenType

  protected slots:
    void entryChanged(void);
    void okClicked(void);

  protected:
    MythUIButtonList  *m_parentList;
    QString            m_searchText;
    MythUITextEdit    *m_entryEdit    { nullptr };
    MythUIButton      *m_cancelButton { nullptr };
    MythUIButton      *m_okButton     { nullptr };
    MythUIText        *m_rulesText    { nullptr };
    int                m_selection;
    bool               m_okClicked    { false   };
    int                m_low;
    int                m_high;
    int                m_step;


};

#endif

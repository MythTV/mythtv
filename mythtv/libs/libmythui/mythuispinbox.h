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
    MythUISpinBox(MythUIType *parent, const QString &name);
    ~MythUISpinBox();

    void SetRange(int low, int high, int step, uint pageMultiple = 5);

    void SetValue(int val) { SetValueByData(val); }
    void SetValue(const QString &val) { SetValueByData(val.toInt()); }
    void AddSelection (int value, const QString &label = "");
    QString GetValue(void) const { return GetDataValue().toString(); }
    int GetIntValue(void) const { return GetDataValue().toInt(); }
    bool keyPressEvent(QKeyEvent *event);

  protected:
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

    virtual bool MoveDown(MovementUnit unit = MoveItem, uint amount = 0);
    virtual bool MoveUp(MovementUnit unit = MoveItem, uint amount = 0);
    void ShowEntryDialog(QString initialEntry);

    bool m_hasTemplate;
    QString m_negativeTemplate;
    QString m_zeroTemplate;
    QString m_positiveTemplate;

    uint m_moveAmount;
    int m_low;
    int m_high;
    int m_step;
};

// Convenience Dialog to allow entry of a Spinbox value

class MUI_PUBLIC SpinBoxEntryDialog : public MythScreenType
{
    Q_OBJECT
  public:
    SpinBoxEntryDialog(MythScreenStack *parent, const char *name,
        MythUIButtonList *parentList, QString searchText,
        int low, int high, int step);
    ~SpinBoxEntryDialog(void);

    bool Create(void);

  protected slots:
    void entryChanged(void);
    void okClicked(void);

  protected:
    MythUIButtonList  *m_parentList;
    QString            m_searchText;
    MythUITextEdit    *m_entryEdit;
    MythUIButton      *m_cancelButton;
    MythUIButton      *m_okButton;
    MythUIText        *m_rulesText;
    int                m_selection;
    bool               m_okClicked;
    int m_low;
    int m_high;
    int m_step;


};

#endif

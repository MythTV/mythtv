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

  protected:
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

    virtual bool MoveDown(MovementUnit unit = MoveItem, uint amount = 0);
    virtual bool MoveUp(MovementUnit unit = MoveItem, uint amount = 0);

    bool m_hasTemplate;
    QString m_negativeTemplate;
    QString m_zeroTemplate;
    QString m_positiveTemplate;

    uint m_moveAmount;
};

#endif

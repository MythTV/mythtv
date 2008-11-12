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
 */
class MPUBLIC MythUISpinBox : public MythUIButtonList
{
    Q_OBJECT
  public:
    MythUISpinBox(MythUIType *parent, const QString &name);
    ~MythUISpinBox();

    void SetRange(int low, int high, int step);

  protected:
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
};

#endif

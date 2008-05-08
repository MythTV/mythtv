#ifndef MYTHUISPINBOX_H_
#define MYTHUISPINBOX_H_

#include "mythlistbutton.h"

/** \class MythUISpinBox
 *
 * \brief A widget for offering a range of numerical values where only the
 *        the bounding values and interval are known.
 *
 * Where a list of specific values is required instead, then use mythlistbutton
 * instead.
 *
 */
class MythUISpinBox : public MythListButton
{
    Q_OBJECT
  public:
    MythUISpinBox(MythUIType *parent, const char *name);
    ~MythUISpinBox();

    void SetRange(int low, int high, int step);
};

#endif

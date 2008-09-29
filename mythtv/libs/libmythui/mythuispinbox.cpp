
#include "mythuispinbox.h"

MythUISpinBox::MythUISpinBox(MythUIType *parent, const QString &name)
              : MythUIButtonList(parent, name)
{
}

MythUISpinBox::~MythUISpinBox()
{
}

void MythUISpinBox::SetRange(int low, int high, int step)
{
    if ((high - low) == 0 || step == 0)
        return;

    Reset();

    int value = low;

    while (value <= high)
    {
        new MythUIButtonListItem(this, QString::number(value));
        value = value + step;
    }

    SetPositionArrowStates();
}

void MythUISpinBox::CreateCopy(MythUIType *parent)
{
    MythUISpinBox *spinbox = new MythUISpinBox(parent, objectName());
    spinbox->CopyFrom(this);
}

void MythUISpinBox::CopyFrom(MythUIType *base)
{
    MythUISpinBox *spinbox = dynamic_cast<MythUISpinBox *>(base);
    if (!spinbox)
        return;

    MythUIButtonList::CopyFrom(base);
}

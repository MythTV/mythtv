
#include "mythuigroup.h"

void MythUIGroup::Reset()
{
    MythUIType::Reset();
}

void MythUIGroup::CopyFrom(MythUIType *base)
{
    MythUIGroup *group = dynamic_cast<MythUIGroup *>(base);

    if (!group)
        return;

    MythUIType::CopyFrom(base);
}

void MythUIGroup::CreateCopy(MythUIType *parent)
{
    MythUIGroup *group = new MythUIGroup(parent, objectName());
    group->CopyFrom(this);
}

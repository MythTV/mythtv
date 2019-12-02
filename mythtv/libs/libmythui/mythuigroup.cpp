
#include "mythuigroup.h"

void MythUIGroup::Reset()
{
    MythUIType::Reset();
}

void MythUIGroup::CopyFrom(MythUIType *base)
{
    auto *group = dynamic_cast<MythUIGroup *>(base);
    if (!group)
        return;

    MythUIType::CopyFrom(base);
}

void MythUIGroup::CreateCopy(MythUIType *parent)
{
    auto *group = new MythUIGroup(parent, objectName());
    group->CopyFrom(this);
}

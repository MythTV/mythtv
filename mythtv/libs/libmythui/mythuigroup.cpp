
#include "mythuigroup.h"

MythUIGroup::MythUIGroup(MythUIType *parent, const char *name)
           : MythUIType(parent, name)
{
}

MythUIGroup::~MythUIGroup()
{
}

bool MythUIGroup::ParseElement(QDomElement &element)
{
    return MythUIType::ParseElement(element);
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


#include "mythuigroup.h"

MythUIGroup::MythUIGroup(MythUIType *parent, const QString &name)
    : MythUIComposite(parent, name)
{
}

MythUIGroup::~MythUIGroup()
{
}

void MythUIGroup::Reset()
{
    MythUIType::Reset();
}

/**
 * \brief Reset all children EXCEPT those listed on the exceptions list
 */
void MythUIGroup::Reset(const QStringList &exceptions)
{
    // Reset all children EXCEPT those listed on the exceptions list
    QMutableListIterator<MythUIType *> it(m_ChildrenList);

    while (it.hasNext())
    {
        it.next();
        MythUIType *type = it.value();
        if (!exceptions.contains(type->objectName()))
        {
            type->Reset();
        }
    }
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

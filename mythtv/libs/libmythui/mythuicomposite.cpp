#include "mythuicomposite.h"
#include "mythuitext.h"

MythUIComposite::MythUIComposite(QObject *parent, const QString &name) :
    MythUIType(parent, name)
{
}

void MythUIComposite::SetTextFromMap(const InfoMap &infoMap)
{
    QList<MythUIType *> *children = GetAllChildren();
    QMutableListIterator<MythUIType *> i(*children);

    while (i.hasNext())
    {
        MythUIType *type = i.next();

        auto *textType = dynamic_cast<MythUIText *> (type);
        if (textType)
            textType->SetTextFromMap(infoMap);

        auto *group = dynamic_cast<MythUIComposite *> (type);
        if (group)
            group->SetTextFromMap(infoMap);
    }
}

void MythUIComposite::ResetMap(const InfoMap &infoMap)
{
    if (infoMap.isEmpty())
        return;

    QList<MythUIType *> *children = GetAllChildren();
    QMutableListIterator<MythUIType *> i(*children);

    while (i.hasNext())
    {
        MythUIType *type = i.next();

        auto *textType = dynamic_cast<MythUIText *> (type);
        if (textType)
            textType->ResetMap(infoMap);

        auto *group = dynamic_cast<MythUIComposite *> (type);
        if (group)
            group->ResetMap(infoMap);
    }
}

#include "moc_mythuicomposite.cpp"

#include "mythuicomposite.h"
#include "mythuitext.h"

MythUIComposite::MythUIComposite(QObject *parent, const QString &name) :
    MythUIType(parent, name)
{
}

void MythUIComposite::SetTextFromMap(InfoMap &infoMap)
{
    QList<MythUIType *> *children = GetAllChildren();
    QMutableListIterator<MythUIType *> i(*children);

    while (i.hasNext())
    {
        MythUIType *type = i.next();

        MythUIText *textType = dynamic_cast<MythUIText *> (type);
        if (textType)
            textType->SetTextFromMap(infoMap);

        MythUIComposite *group = dynamic_cast<MythUIComposite *> (type);
        if (group)
            group->SetTextFromMap(infoMap);
    }
}

void MythUIComposite::ResetMap(InfoMap &infoMap)
{
    if (infoMap.isEmpty())
        return;

    QList<MythUIType *> *children = GetAllChildren();
    QMutableListIterator<MythUIType *> i(*children);

    while (i.hasNext())
    {
        MythUIType *type = i.next();

        MythUIText *textType = dynamic_cast<MythUIText *> (type);
        if (textType && infoMap.contains(textType->objectName()))
            textType->Reset();

        MythUIComposite *group = dynamic_cast<MythUIComposite *> (type);
        if (group)
            group->ResetMap(infoMap);
    }
}

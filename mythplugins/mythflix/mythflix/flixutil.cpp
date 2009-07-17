#include <mythtv/mythcontext.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythmainwindow.h>

#include "mythtv/mythdbcon.h"

#include "flixutil.h"

QString chooseQueue(MythScreenType *screen, QString excludedQueue)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString queueName = "";

    QString sql ="SELECT DISTINCT queue "
                    "FROM netflix";

    if (!excludedQueue.isEmpty())
        sql += QString(" WHERE queue <> '%1'").arg(excludedQueue);

    if (!query.exec(sql))
    {
        VERBOSE(VB_IMPORTANT,
                QString("MythFlixQueue: Error in loading queue from DB"));
    }
    else
    {
        if (query.size() > 1)
        {
            QString label = QObject::tr("Queue Name");

            MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

            MythDialogBox *menuPopup = new MythDialogBox(label, popupStack, "queuepopup");

            if (menuPopup->Create())
                popupStack->AddScreen(menuPopup);

            menuPopup->SetReturnEvent(screen, "queues");

            while ( query.next() )
            {
                QString name = query.value(0).toString();
                if (name.isEmpty())
                    name = "default";
                menuPopup->AddButton(name);
            }
        }
        else if (query.size() == 1)
        {
            queueName = query.value(0).toString();

            if (queueName.isEmpty())
                queueName = "default";
        }
    }

    return queueName;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

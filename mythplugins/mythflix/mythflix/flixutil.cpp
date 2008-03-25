#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>
#include <mythtv/mythdialogs.h>
#include "mythtv/mythdbcon.h"

#include "flixutil.h"
//Added by qt3to4:
#include <QLabel>

QString chooseQueue(QString excludedQueue)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString queueName = "";

    QString sql ="SELECT DISTINCT queue "
                    "FROM netflix "
                    "WHERE queue <> ''";

    if (excludedQueue != "")
        sql += QString(" AND queue <> '%1'").arg(excludedQueue);

    query.exec(sql);

    if (!query.isActive())
    {
        VERBOSE(VB_IMPORTANT,
                QString("MythFlixQueue: Error in loading queue from DB"));
    }
    else
    {
        QStringList queues;
        while ( query.next() )
        {
            queues += query.value(0).toString();
        }

        if (queues.size() > 1 || excludedQueue != "")
        {
            MythPopupBox *queuePopup;
            queuePopup = new MythPopupBox(gContext->GetMainWindow(),
                                          "queuepopup");
            QLabel *label = queuePopup->addLabel("Queue Name",
                MythPopupBox::Large, false);
            label->setAlignment(Qt::AlignCenter);

            MythListBox *queueList;
            queueList = new MythListBox(queuePopup);
            queueList->insertStringList(queues);

            queuePopup->addWidget(queueList);
            queueList->setFocus();

            MythDialog::connect(queueList, SIGNAL(accepted(int)),
                    queuePopup, SLOT(AcceptItem(int)));

            DialogCode result = queuePopup->ExecPopup();

            if (result != MythDialog::Rejected)
            {
                queueName = queueList->currentText();
            }
            else
            {
                queueName = "__NONE__";
            }

            queuePopup->hide();
            queuePopup->deleteLater();
        }
        else if (queues.size() == 1)
        {
            queueName = queues[0];
        }
    }

    return queueName;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

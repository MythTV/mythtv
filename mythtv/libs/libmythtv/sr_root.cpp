#include "sr_items.h"
#include "sr_root.h"

RootSRGroup::RootSRGroup(ScheduledRecording& _rec,ManagedList* _parentList, QObject* _parent)
           : ManagedListGroup( "rootGroup", NULL, _parentList, _parent, "rootGroup"),
             schedRec(_rec)
{
    cancelItem = new DialogDoneListItem( QString("[ %1 ]").arg(QObject::tr("Cancel")), MythDialog::Rejected, NULL,
                                        _parentList, this, "cancel");
    cancelItem->setState(MLS_BOLD);
    addItem(cancelItem);


    recType = new SRRecordingType(schedRec, _parentList, this);
    addItem(recType->getItem(), -1);
    connect(recType->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));

    schedOptions = new SRSchedOptionsGroup(_rec, _parentList, this, this);
    addItem(schedOptions, -1);

    storageOptions = new SRStorageOptionsGroup(_rec, _parentList, this, this);
    addItem(storageOptions, -1);

    jobQueue = new SRJobQueueGroup(_rec, _parentList, this, this);
    addItem(jobQueue, -1);

    detailsButton = new ManagedListItem(QObject::tr("Program details"),
                                         _parentList, this, "showDetails");
    addItem(detailsButton, -1);
    connect(detailsButton, SIGNAL(selected(ManagedListItem*)), &_rec, SLOT(runShowDetails()));

    upcomingButton = new ManagedListItem(QObject::tr("List upcoming episodes"),
                                         _parentList, this, "listUpcoming");
    addItem(upcomingButton, -1);
    connect(upcomingButton, SIGNAL(selected(ManagedListItem*)), &_rec, SLOT(runProgList()));

    recordAsShownItem = new DialogDoneListItem(QString("[ %1 ]").arg(QObject::tr("Save these settings")),
                                               MythDialog::Accepted, NULL, _parentList, this, "recordAsShown");
    recordAsShownItem->setState(MLS_BOLD);
    addItem(recordAsShownItem);
}

void RootSRGroup::setDialog(MythDialog* dlg)
{
    cancelItem->setDialog(dlg);
    recordAsShownItem->setDialog(dlg);
}

void RootSRGroup::itemChanged(ManagedListItem*)
{
    bool multiEpisode = true;
    bool isScheduled = true;
    int rtype = recType->getValue().toInt();

    switch(rtype)
    {
        case kNotRecording:
        case kDontRecord:
                isScheduled = false;
                break;

        case kSingleRecord:
        case kFindOneRecord:
        case kOverrideRecord:
                multiEpisode = false;
                break;
    }

    if (isScheduled || rtype == kDontRecord)
    {
        recordAsShownItem->setText(QString("[ %1 ]").arg(QObject::tr("Save these settings")));
        recordAsShownItem->setState(MLS_BOLD);
        recordAsShownItem->setEnabled(true);
    }
    else
    {
        if (schedRec.getRecordID() > 0)
        {
            recordAsShownItem->setState(MLS_BOLD);
        }
        else
        {
            recordAsShownItem->setEnabled(false);
            recordAsShownItem->setState(MLS_NORMAL);
        }
    }

    schedOptions->setEnabled(isScheduled, multiEpisode);
    storageOptions->setEnabled(isScheduled, multiEpisode);
    jobQueue->setEnabled(isScheduled);
}

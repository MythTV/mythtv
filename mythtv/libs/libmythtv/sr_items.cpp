#include "scheduledrecording.h"
#include "sr_items.h"
#include <qlayout.h>
#include "mythdialogs.h"



SRSchedOptionsGroup::SRSchedOptionsGroup(ScheduledRecording& _rec, ManagedList* _parentList, 
                                         ManagedListGroup* _group, QObject* _parent)
                   : ManagedListGroup( QObject::tr("Scheduling Options"), _group, _parentList,
                                       _parent, "schedOpts"),
                     schedRec(_rec)
{
    recPriority = new SRRecPriority(_rec, this, _parentList);
    addItem(recPriority->getItem(), -1);

    startOffset = new SRStartOffset(_rec, this, _parentList);
    addItem(startOffset->getItem());

    endOffset = new SREndOffset(_rec, this, _parentList);
    addItem(endOffset->getItem());

    dupMethItem = new SRDupMethod(_rec, _parentList, this);
    addItem(dupMethItem->getItem(), -1);
    connect(dupMethItem->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));

    dupLocItem = new SRDupIn(_rec, _parentList, this);
    addItem(dupLocItem->getItem(), -1);
    connect(dupMethItem->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
}


void SRSchedOptionsGroup::itemChanged(ManagedListItem*)
{

    if (dupMethItem->getItem()->getEnabled())
    {
        if (dupMethItem->getItem()->getValue().toInt() != kDupCheckNone)
            dupLocItem->getItem()->setEnabled(true);
        else
            dupLocItem->getItem()->setEnabled(false);
    }
}

void SRSchedOptionsGroup::setEnabled(bool isScheduled, bool multiEpisode)
{
    ManagedListGroup::setEnabled(isScheduled);

    dupMethItem->getItem()->setEnabled(isScheduled & multiEpisode);

    if (dupMethItem->getItem()->getEnabled())
    {
        if (dupMethItem->getItem()->getValue().toInt() != kDupCheckNone)
            dupLocItem->getItem()->setEnabled(true);
        else
            dupLocItem->getItem()->setEnabled(false);

    }
    else
    {
        dupLocItem->getItem()->setEnabled(false);
    }
}


SRStorageOptionsGroup::SRStorageOptionsGroup(ScheduledRecording& _rec, ManagedList* _parentList, 
                                             ManagedListGroup* _group, QObject* _parent)
                     : ManagedListGroup(QObject::tr("Storage Options"), _group, _parentList,
                                        _parent, "storageOpts"),
                       schedRec(_rec)
{
    profile = new SRProfileSelector(_rec, _parentList, this);
    addItem(profile->getItem(), -1);

    recGroup = new SRRecGroup(_rec, _parentList, this);
    addItem(recGroup->getItem(), -1);

    autoExpire = new SRAutoExpire(_rec, this, parentList);
    addItem(autoExpire->getItem(), -1);

    maxEpisodes = new SRMaxEpisodes(_rec, this, _parentList);
    addItem(maxEpisodes->getItem(), -1);

    connect(maxEpisodes->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));

    maxNewest = new SRMaxNewest(_rec, this, _parentList);
    addItem(maxNewest->getItem(), -1);

}

void SRStorageOptionsGroup::itemChanged(ManagedListItem*)
{
    maxNewest->getItem()->setEnabled(maxEpisodes->getValue().toInt() !=0);
}

void SRStorageOptionsGroup::setEnabled(bool isScheduled, bool)
{
    ManagedListGroup::setEnabled(isScheduled);
}

void SRRecGroup::onGoingBack()
{
    if ( selectItem->getCurItemValue() == "__NEW_GROUP__" )
    {
        selectItem->selectValue("Default");
    }
}


void SRRecGroup::showNewRecGroup()
{
    QString newGroup;

    bool ok = MythPopupBox::showGetTextPopup(gContext->GetMainWindow(), QObject::tr("Create New Recording Group"),
                                             QObject::tr("Using your keyboard or the numbers keys on your remote"
                                                         " enter the group name"), newGroup);
    if (ok)
    {
        addSelection(QString(QObject::tr("Store in the \"%1\" recording group")).arg(newGroup), newGroup, true);
        selectItem->selectValue(newGroup);
    }
}



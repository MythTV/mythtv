#include "scheduledrecording.h"
#include "sr_items.h"
#include <qlayout.h>
#include "mythdialogs.h"



SRSchedOptionsGroup::SRSchedOptionsGroup(ScheduledRecording *_rec, ManagedList* _parentList, 
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

    inactive = new SRInactive(_rec, this, _parentList);
    addItem(inactive->getItem(), -1);
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


SRJobQueueGroup::SRJobQueueGroup(ScheduledRecording *_rec, ManagedList* _parentList, ManagedListGroup* _group,
                                             QObject* _parent)
                     : ManagedListGroup(QObject::tr("Post Recording Processing"), _group, _parentList,
                                        _parent, "postProcessing"),
                       schedRec(_rec)
{
    autoCommFlag = new SRAutoCommFlag(_rec, _parentList, this);
    addItem(autoCommFlag->getItem(), -1);

    autoTranscode = new SRAutoTranscode(_rec, _parentList, this);
    addItem(autoTranscode->getItem(), -1);

    transcoder = new SRTranscoderSelector(_rec, _parentList, this);
    addItem(transcoder->getItem(), -1);

    autoUserJob1 = new SRAutoUserJob1(_rec, _parentList, this);
    addItem(autoUserJob1->getItem(), -1);

    autoUserJob2 = new SRAutoUserJob2(_rec, _parentList, this);
    addItem(autoUserJob2->getItem(), -1);

    autoUserJob3 = new SRAutoUserJob3(_rec, _parentList, this);
    addItem(autoUserJob3->getItem(), -1);

    autoUserJob4 = new SRAutoUserJob4(_rec, _parentList, this);
    addItem(autoUserJob4->getItem(), -1);
}

SRStorageOptionsGroup::SRStorageOptionsGroup(ScheduledRecording *_rec, ManagedList* _parentList, 
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
        addSelection(QString(QObject::tr("Store in the \"%1\" recording group")).arg(newGroup), newGroup);
        selectItem->selectValue(newGroup);
    }
}



#include "scheduledrecording.h"
#include "sr_items.h"


void SRDupSettingsGroup::itemChanged(ManagedListItem*) 
{ 
    syncText(); 
}


SRDupSettingsGroup::SRDupSettingsGroup(ScheduledRecording& _rec, ManagedList* _list, ManagedListGroup* _group, QObject* _parent)
                  : ManagedListGroup(QObject::tr("Duplicate detection"), _group, _list, _parent, "depGroup")
{
    dupMethItem = new SRDupMethod(_rec, _list, this);
    addItem(dupMethItem->getItem(), -1);
    
    connect(dupMethItem->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
    
    dupLocItem = new SRDupIn(_rec, _list, this);
    addItem(dupLocItem->getItem(), -1);
    
    connect(dupMethItem->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
    
    syncText();
}

void SRDupSettingsGroup::doGoBack()
{
    syncText();
    ManagedListGroup::doGoBack();
}


void SRDupSettingsGroup::syncText()
{
    int meth;
    int loc;
    //cerr << dupMethItem->getItem()->getValue() << " - " << dupMethItem->getItem()->getText() << endl;
    
    meth = dupMethItem->getItem()->getValue().toInt();
    loc = dupLocItem->getItem()->getValue().toInt();
    

    text = dupMethItem->getItem()->getText();
        
    if(meth != kDupCheckNone)
    {
        if(loc == kDupsInAll)
            text += ", in current and previous recordings";
        else if(kDupsInRecorded)
            text += ", in current recordings only";
        else if(kDupsInOldRecorded)
            text += ", in previous recordings only";
    }    
}


SROffsetGroup::SROffsetGroup(ScheduledRecording& _rec, ManagedList* _parentList, ManagedListGroup* _parentGroup)
                : ManagedListGroup(QObject::tr("Pre/Post Roll"), _parentGroup, _parentList, _parentGroup, "rollGroup")
    {
           
        startOffset = new SRStartOffset(_rec, _parentList);
        addItem(startOffset->getItem());
        
        endOffset = new SREndOffset(_rec, _parentList);
        addItem(endOffset->getItem());        
        
        syncText();
    }



void SROffsetGroup::syncText()
{
    text = QString("Start recording %1, end %2").arg(startOffset->getShortText()).arg(endOffset->getShortText());
}



void SROffsetGroup::itemChanged(ManagedListItem*) 
{ 
    syncText(); 
}


SREpisodesGroup::SREpisodesGroup(ScheduledRecording& _rec, ManagedList* _list, ManagedListGroup* _group, QObject* _parent)
                  : ManagedListGroup(QObject::tr("Duplicate detection"), _group, _list, _parent, "depGroup")
{

    maxEpisodes = new SRMaxEpisodes(_rec, _list);
    addItem(maxEpisodes->getItem(), -1);
    
    connect(maxEpisodes->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
    
    maxNewest = new SRMaxNewest(_rec, _list);
    addItem(maxNewest->getItem(), -1);
    
    connect(maxNewest->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
    
    syncText();
}


void SREpisodesGroup::syncText()
{
    int maxEpi;
    
    maxEpi = maxEpisodes->getItem()->getValue().toInt();
    
    text = maxEpisodes->getItem()->getText();
    
    if(maxEpi != 0)
        text += QString(", %1").arg(maxNewest->getItem()->getText());
    
}



SRSchedOptionsGroup::SRSchedOptionsGroup(ScheduledRecording& _rec, ManagedList* _parentList, ManagedListGroup* _group, QObject* _parent)
                   : ManagedListGroup( QString("[ %1 ]").arg(QObject::tr("Scheduling Options")), _group, _parentList, 
                                       _parent, "schedOpts"),
                     schedRec(_rec)
{
    recPriority = new SRRecPriority(_rec, _parentList);
    addItem(recPriority->getItem(), -1);
    
    startOffset = new SRStartOffset(_rec, _parentList);
    addItem(startOffset->getItem());
    
    endOffset = new SREndOffset(_rec, _parentList);
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
    
    if(dupMethItem->getItem()->getEnabled())
    {
        if(dupMethItem->getItem()->getValue().toInt() != kDupCheckNone)
            dupLocItem->getItem()->setEnabled(true);
        else
            dupLocItem->getItem()->setEnabled(false);
    }
}

void SRSchedOptionsGroup::setEnabled(bool isScheduled, bool multiEpisode)
{
    ManagedListGroup::setEnabled(isScheduled);
    
    dupMethItem->getItem()->setEnabled(isScheduled & multiEpisode);
    
    if(dupMethItem->getItem()->getEnabled())
    {
        if(dupMethItem->getItem()->getValue().toInt() != kDupCheckNone)
            dupLocItem->getItem()->setEnabled(true);
        else
            dupLocItem->getItem()->setEnabled(false);
    
    }
    else
    {
        dupLocItem->getItem()->setEnabled(false);
    }
}


SRStorageOptionsGroup::SRStorageOptionsGroup(ScheduledRecording& _rec, ManagedList* _parentList, ManagedListGroup* _group, 
                                             QObject* _parent)
                     : ManagedListGroup(QString("[ %1 ]").arg(QObject::tr("Storage Options")), _group, _parentList, 
                                        _parent, "storageOpts"),
                       schedRec(_rec)
{
    profile = new SRProfileSelector(_rec, _parentList, this);
    addItem(profile->getItem(), -1);
    
    recGroup = new SRRecGroup(_rec, _parentList, this);
    addItem(recGroup->getItem(), -1);
    
    autoExpire = new SRAutoExpire(_rec, _parentList);
    addItem(autoExpire->getItem(), -1);       
    
    maxEpisodes = new SRMaxEpisodes(_rec, _parentList);
    addItem(maxEpisodes->getItem(), -1);
    
    connect(maxEpisodes->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
    
    maxNewest = new SRMaxNewest(_rec, _parentList);
    addItem(maxNewest->getItem(), -1);    
   
}

void SRStorageOptionsGroup::itemChanged(ManagedListItem*)
{
    maxNewest->getItem()->setEnabled(maxEpisodes->getValue().toInt() !=0); 
}

void SRStorageOptionsGroup::setEnabled(bool isScheduled, bool multiEpisode)
{
    ManagedListGroup::setEnabled(isScheduled);
}

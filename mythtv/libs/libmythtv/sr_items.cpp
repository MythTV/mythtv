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
    _rec.setDupMethodObj(dupMethItem);
    addItem(dupMethItem->getItem(), -1);
    
    connect(dupMethItem->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
    
    dupLocItem = new SRDupIn(_rec, _list, this);
    _rec.setDupInObj(dupLocItem);
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
    cerr << dupMethItem->getItem()->getValue() << " - " << dupMethItem->getItem()->getText() << endl;
    
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
                : ManagedListGroup(QObject::tr("Pre/Post Roll"), parentGroup, _parentList, _parentGroup, "rollGroup")
    {
           
        startOffset = new SRStartOffset(_rec, _parentList);
        _rec.setStartOffsetObj(startOffset);
        addItem(startOffset->getItem());
        
        endOffset = new SREndOffset(_rec, _parentList);
        _rec.setEndOffsetObj(endOffset);
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

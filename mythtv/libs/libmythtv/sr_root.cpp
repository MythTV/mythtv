#include "sr_items.h"
#include "sr_root.h"

RootSRGroup::RootSRGroup(ScheduledRecording& _rec,ManagedList* _parentList, QObject* _parent)
                  : ManagedListGroup( "rootGroup", NULL, _parentList, _parent, "rootGroup"),
                    schedRec(_rec)
{
    cancelItem = new DialogDoneListItem( tr("Cancel"), MythDialog::Rejected, NULL, parentList, this, "cancel");
    cancelItem->setState(MLS_BOLD);
    addItem(cancelItem);
    
    recordAsShownItem = new DialogDoneListItem( tr("Record this program as shown below"), MythDialog::Accepted, NULL,
                                                parentList, this, "recordAsShown");
    recordAsShownItem->setState(MLS_BOLD);
    addItem(recordAsShownItem);
    
    recType = new SRRecordingType(schedRec, _parentList, this);
    addItem(recType->getItem(), -1);
    
    profile = new SRProfileSelector(_rec, _parentList, this);
    addItem(profile->getItem(), -1);
    
    offsetGroup = new SROffsetGroup(_rec, _parentList, this);
    addItem(offsetGroup, -1);
    
    recPriority = new SRRecPriority(_rec, _parentList);
    addItem(recPriority->getItem(), -1);

    dupSettings = new SRDupSettingsGroup(_rec, _parentList, this, this);
    addItem(dupSettings, -1);
    
    autoExpire = new SRAutoExpire(_rec, _parentList);
    addItem(autoExpire->getItem(), -1);

    maxEpisodes = new SRMaxEpisodes(_rec, _parentList);
    addItem(maxEpisodes->getItem(), -1);
    
    maxNewest = new SRMaxNewest(_rec, _parentList);
    addItem(maxNewest->getItem(), -1);
}
       
void RootSRGroup::setDialog(MythDialog* dlg) 
{
    cancelItem->setDialog(dlg); 
    recordAsShownItem->setDialog(dlg);
}

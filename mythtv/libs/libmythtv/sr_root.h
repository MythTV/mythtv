#ifndef SR_ROOT_GROUP_H
#define SR_ROOT_GROUP_H

#include "managedlist.h"
#include "scheduledrecording.h"

class MythDialog;

class RootSRGroup : public ManagedListGroup 
{
    Q_OBJECT
    public:
        RootSRGroup(ScheduledRecording& _rec,ManagedList* _parentList=NULL, QObject* _parent=NULL);
        void setDialog(MythDialog* dlg);
     
     public slots:
        void itemChanged(ManagedListItem*);
        
     protected:
         ScheduledRecording& schedRec;
         DialogDoneListItem* cancelItem;
         DialogDoneListItem* recordAsShownItem;
         ManagedListItem* upcomingButton;         
         class SRRecordingType* recType;
         class SRProfileSelector* profile;
         class SRDupSettingsGroup* dupSettings;
         class SROffsetGroup* offsetGroup;
         class SRAutoExpire* autoExpire;
         class SRRecPriority* recPriority;
         class SREpisodesGroup* episodes;
         class SRRecGroup* recGroup;
};
#endif

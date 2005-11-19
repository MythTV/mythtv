#ifndef SR_ROOT_GROUP_H
#define SR_ROOT_GROUP_H

#include "managedlist.h"
#include "scheduledrecording.h"

class MythDialog;

class RootSRGroup : public ManagedListGroup 
{
    Q_OBJECT
    public:
        RootSRGroup(ScheduledRecording *_rec, ManagedList* _parentList=NULL, QObject* _parent=NULL);
        void setDialog(MythDialog* dlg);
        
        class SRSchedOptionsGroup* getSchedOptions() {return schedOptions;}
        class SRStorageOptionsGroup* getStorageOptions() {return storageOptions;}
        
     
     public slots:
        void itemChanged(ManagedListItem*);
        
     protected:
         
         ScheduledRecording *schedRec;
         
         DialogDoneListItem* cancelItem;
         DialogDoneListItem* recordAsShownItem;
         ManagedListItem* testButton;         

         class SRSchedOptionsGroup* schedOptions;
         class SRStorageOptionsGroup* storageOptions;
         class SRJobQueueGroup* jobQueue;
         class SRSchedInfoGroup* schedInfo;
         
         class SRRecordingType* recType;
};
#endif

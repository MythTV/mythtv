#ifndef SR_ROOT_GROUP_H
#define SR_ROOT_GROUP_H

class RootSRGroup : public ManagedListGroup 
{
    public:
        RootSRGroup(ScheduledRecording& _rec,ManagedList* _parentList=NULL, QObject* _parent=NULL);
        void setDialog(MythDialog* dlg);
     
     protected:
         ScheduledRecording& schedRec;
         DialogDoneListItem* cancelItem;
         DialogDoneListItem* recordAsShownItem;
         
         SRRecordingType* recType;
         SRProfileSelector* profile;
         class SRDupSettingsGroup* dupSettings;
         class SROffsetGroup* offsetGroup;
         class SRAutoExpire* autoExpire;
         class SRMaxNewest* maxNewest;
         class SRMaxEpisodes* maxEpisodes;
         class SRRecPriority* recPriority;
};
#endif

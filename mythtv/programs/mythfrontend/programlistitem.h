#ifndef PROGRAMLISTITEM_H_
#define PROGRAMLISTITEM_H_

#include <qlistview.h>

class ProgramInfo;
class RecordingInfo;
class QPixmap;

class ProgramListItem : public QListViewItem
{   
  public:
    ProgramListItem(QListView *parent, ProgramInfo *lpginfo,
                    RecordingInfo *lrecinfo, int numcols, TV *ltv,
                    QString lprefix);
   ~ProgramListItem() { delete pginfo; delete recinfo; }
    
    ProgramInfo *getProgramInfo(void) { return pginfo; }
    RecordingInfo *getRecordingInfo(void) { return recinfo; }

    QPixmap *getPixmap(void); 

  protected:
    ProgramInfo *pginfo;
    RecordingInfo *recinfo;
    QPixmap *pixmap;

    QString prefix;
    TV *tv;
};  

#endif

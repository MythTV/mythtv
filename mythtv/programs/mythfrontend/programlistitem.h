#ifndef PROGRAMLISTITEM_H_
#define PROGRAMLISTITEM_H_

#include <qlistview.h>
#include <qpixmap.h>

#include "libmythtv/programinfo.h"

class MythContext;
class TV;

class ProgramListItem : public QListViewItem
{   
  public:
    ProgramListItem(MythContext *context, QListView *parent, 
                    ProgramInfo *lpginfo, int type);
   ~ProgramListItem() { delete pginfo; if (pixmap) delete pixmap; }

    virtual void paintCell(QPainter *p, const QColorGroup &cg,
                           int column, int width, int alignment);
    
    ProgramInfo *getProgramInfo(void) { return pginfo; }

    QPixmap *getPixmap(void); 

  protected:
    ProgramInfo *pginfo;
    QPixmap *pixmap;

    MythContext *m_context;
};  

#endif

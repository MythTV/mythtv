#ifndef PROGRAMLISTITEM_H_
#define PROGRAMLISTITEM_H_

#include <qlistview.h>

class ProgramInfo;
class RecordingInfo;
class QPixmap;

class MyListView : public QListView
{
  public:
    MyListView(QWidget *parent) : QListView(parent) {}

  protected:
    void keyPressEvent( QKeyEvent *e );
};

class ProgramListItem : public QListViewItem
{   
  public:
    ProgramListItem(QListView *parent, ProgramInfo *lpginfo,
                    RecordingInfo *lrecinfo, int numcols, TV *ltv,
                    QString lprefix);
   ~ProgramListItem() { delete pginfo; delete recinfo; 
                        if (pixmap) delete pixmap; }

    virtual void paintCell(QPainter *p, const QColorGroup &cg,
                           int column, int width, int alignment);
    
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

#ifndef PROGRAMLISTITEM_H_
#define PROGRAMLISTITEM_H_

#include <qlistview.h>

class ProgramInfo;
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
                    int numcols, TV *ltv, QString lprefix);
   ~ProgramListItem() { delete pginfo; if (pixmap) delete pixmap; }

    virtual void paintCell(QPainter *p, const QColorGroup &cg,
                           int column, int width, int alignment);
    
    ProgramInfo *getProgramInfo(void) { return pginfo; }

    QPixmap *getPixmap(void); 

  protected:
    ProgramInfo *pginfo;
    QPixmap *pixmap;

    QString prefix;
    TV *tv;
};  

#endif

#ifndef VISUAL_H_
#define VISUAL_H_
/*
	visual.h

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for mfe visualizations
	
*/
//  #include "polygon.h"
//  #include "constants.h"

#include <qwidget.h>
#include <qtimer.h>
#include <qmemarray.h>
#include <qpixmap.h>
//  #include <qptrlist.h>
//  #include <qstringlist.h>

#include "visualnode.h"

class MfeVisualizer : public QWidget
{

  Q_OBJECT

  public:

    MfeVisualizer();
    virtual ~MfeVisualizer();

    virtual void add(uchar *b, unsigned long b_len, unsigned long w, int c, int p);
    virtual bool process( VisualNode *node ) = 0;
    virtual bool draw( QPainter *, const QColor & ) = 0;
    virtual void resize( const QSize &size ) = 0;

    void paintEvent( QPaintEvent * );
    void resizeEvent( QResizeEvent * );
  
  public slots:

    void timeout();         

  private:

    QTimer  *timer;
    int     fps;
    QPixmap pixmap;
    QPtrList<VisualNode> nodes;
};

#endif

// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef __mainvisual_h
#define __mainvisual_h

#include "visual.h"
#include "polygon.h"
#include "constants.h"

#include <qwidget.h>
#include <qdialog.h>
#include <qarray.h>
#include <qpixmap.h>
#include <qptrlist.h>

class Buffer;
class Output;
class VisualNode;
class LogScale;
class QTimer;

class VisualNode
{
public:
    VisualNode(short *l, short *r, unsigned long n, unsigned long o)
	: left(l), right(r), length(n), offset(o)
    {
	// left and right are allocated and then passed to this class
	// the code that allocated left and right should give up all ownership
    }

    ~VisualNode()
    {
	delete [] left;
	delete [] right;
    }

    short *left, *right;
    long length, offset;
    
};

class VisualBase
{
public:
	virtual ~VisualBase();

    // return true if the output should stop
    virtual bool process( VisualNode *node ) = 0;
    virtual bool draw( QPainter *, const QColor & ) = 0;
    virtual void resize( const QSize &size ) = 0;
};



// base class to handle things like frame rate...
class MainVisual : public QDialog, public Visual
{
    Q_OBJECT

public:
    MainVisual(QWidget *parent = 0, const char * = 0);
    virtual ~MainVisual();

    VisualBase *visual() const { return vis; }
    void setVis( VisualBase *newvis );
    void setVisual( const QString &visualname );

    void add(Buffer *, unsigned long, int, int);
    void prepare();

    QSize minimumSizeHint() const { return sizeHint(); }
    QSize sizeHint() const { return QSize(4*4*4*2, 3*3*3*2); }

    void paintEvent( QPaintEvent * );
    void resizeEvent( QResizeEvent * );
    void customEvent( QCustomEvent * );
    void hideEvent( QHideEvent * );
    
    void setFrameRate( int newfps );
    int frameRate() const { return fps; }

public slots:
    void timeout();

signals:
	void hidingVisualization();

private:
    VisualBase *vis;
    QPixmap pixmap;
    QPtrList<VisualNode> nodes;
    QTimer *timer;
    bool playing;
    int fps;

};


//
//	thor	feb 13 2002
//
//	Not sure these ever worked in Myth
//
//


/*

class StereoScope : public VisualBase
{
public:
    StereoScope();
    virtual ~StereoScope();

    void resize( const QSize &size );
    bool process( VisualNode *node );
    void draw( QPainter *p, const QColor &back );

protected:
    QColor startColor, targetColor;
    QMemArray<double> magnitudes;
    QSize size;
    bool rubberband;
    double falloff;
    int fps;
};

class MonoScope : public StereoScope
{
public:
   MonoScope();
   virtual ~MonoScope();
			    
   bool process( VisualNode *node );
   void draw( QPainter *p, const QColor &back );
};  

*/

class LogScale
{
public:
    LogScale(int = 0, int = 0);
    ~LogScale();

    int scale() const { return s; }
    int range() const { return r; }

    void setMax(int, int);

    int operator[](int);


private:
    int *indices;
    int s, r;
};

    
#endif // __mainvisual_h


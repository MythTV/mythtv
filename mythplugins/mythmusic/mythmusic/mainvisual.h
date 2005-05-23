// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef __mainvisual_h
#define __mainvisual_h

#include <mythtv/visual.h>
#include "polygon.h"
#include "constants.h"

#include <qwidget.h>
#include <qdialog.h>
#include <qmemarray.h>
#include <qpixmap.h>
#include <qptrlist.h>
#include <qstringlist.h>

class Buffer;
class Output;
class VisualNode;
class LogScale;
class QTimer;
class VisFactory;

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
    VisualBase(bool screensaverenable = false);
    virtual ~VisualBase(void);

    // return true if the output should stop
    virtual bool process( VisualNode *node ) = 0;
    virtual bool draw( QPainter *, const QColor & ) = 0;
    virtual void resize( const QSize &size ) = 0;
    virtual int getDesiredFPS(void) { return fps; }

  protected:
    int fps;
    bool xscreensaverenable;
};

// base class to handle things like frame rate...
class MainVisual : public QWidget, public MythTV::Visual
{
    Q_OBJECT

public:
    MainVisual(QWidget *parent = 0, const char * = 0);
    virtual ~MainVisual();

    VisualBase *visual() const { return vis; }
    void setVis( VisualBase *newvis );
    void setVisual( const QString &visualname );

    QString getCurrentVisual() const;
    QString getCurrentVisualDesc() const;
    int numVisualizers() const;

    void add(uchar *, unsigned long, unsigned long, int, int);
    void prepare();

    QSize minimumSizeHint() const { return sizeHint(); }
    QSize sizeHint() const { return QSize(4*4*4*2, 3*3*3*2); }

    void paintEvent( QPaintEvent * );
    void resizeEvent( QResizeEvent * );
    void customEvent( QCustomEvent * );
    void hideEvent( QHideEvent * );
    
    void setFrameRate( int newfps );
    int frameRate() const { return fps; }

    static void registerVisFactory(VisFactory *);
    static VisualBase *createVis(const QString &name,
                                 MainVisual *parent, long int winid);

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

    QString current_visual_name;
    QStringList allowed_modes;
};

class VisFactory
{
  public:
    virtual const QString &name(void) const = 0;
    virtual const QString &description(void) const = 0;
    virtual VisualBase *create(MainVisual *parent, long int winid) = 0;
    virtual ~VisFactory() {}
};

class StereoScope : public VisualBase
{
public:
    StereoScope();
    virtual ~StereoScope();

    void resize( const QSize &size );
    bool process( VisualNode *node );
    bool draw( QPainter *p, const QColor &back );

protected:
    QColor startColor, targetColor;
    QMemArray<double> magnitudes;
    QSize size;
    bool rubberband;
    double falloff;
};

class MonoScope : public StereoScope
{
public:
   MonoScope();
   virtual ~MonoScope();
			    
   bool process( VisualNode *node );
   bool draw( QPainter *p, const QColor &back );
};  

class StereoScopeFactory : public VisFactory
{
  public:
    const QString &name(void) const;
    const QString &description(void) const;
    VisualBase *create(MainVisual *parent, long int winid);
};

class MonoScopeFactory : public VisFactory
{
  public:
    const QString &name(void) const;
    const QString &description(void) const;
    VisualBase *create(MainVisual *parent, long int winid);
};

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


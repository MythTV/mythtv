// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef __mainvisual_h
#define __mainvisual_h

#include <vector>
using namespace std;

#include "polygon.h"
#include "constants.h"

#include <QResizeEvent>
#include <QPaintEvent>
#include <QStringList>
#include <QHideEvent>
#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QList>

// MythTV headers
#include <visual.h>

class Buffer;
class Output;
class VisualNode;
class LogScale;
class InfoWidget;
class Metadata;
class MainVisual;

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
    virtual void handleKeyPress(const QString &action) = 0;
    virtual int getDesiredFPS(void) { return fps; }
    void drawWarning(QPainter *, const QColor &, const QSize &, QString);

  protected:
    int fps;
    bool xscreensaverenable;
};

class VisFactory
{
  public:
    VisFactory() {m_pNextVisFactory = g_pVisFactories; g_pVisFactories = this;}
    virtual ~VisFactory() {}
    const VisFactory* next() const {return m_pNextVisFactory;}
    virtual const QString &name(void) const = 0;
    virtual VisualBase* create(MainVisual *parent, long int winid,
                               const QString &pluginName) const = 0;
    virtual uint plugins(QStringList *list) const = 0;
    static const VisFactory* VisFactories() {return g_pVisFactories;}
  protected:
    static VisFactory* g_pVisFactories;
    VisFactory*        m_pNextVisFactory;
};

// base class to handle things like frame rate...
class MainVisual : public QWidget, public MythTV::Visual
{
    Q_OBJECT

public:
    MainVisual(QWidget *parent = 0, const char * = 0);
    virtual ~MainVisual();

    VisualBase *visual() const { return vis; }
    void setVisual(const QString &name);

    void add(uchar *, unsigned long, unsigned long, int, int);
    void prepare();

    QSize minimumSizeHint() const { return sizeHint(); }
    QSize sizeHint() const { return QSize(4*4*4*2, 3*3*3*2); }

    void paintEvent( QPaintEvent * );
    void resizeEvent( QResizeEvent * );
    void customEvent( QEvent * );
    void hideEvent( QHideEvent * );

    void setFrameRate( int newfps );
    int frameRate() const { return fps; }

    void showBanner(const QString &text, int showTime = 8000);
    void showBanner(Metadata *meta, bool fullScreen, int visMode, int showTime = 8000);
    void hideBanner();
    bool bannerIsShowing(void) {return bannerTimer->isActive(); }

    static QStringList Visualizations();

public slots:
    void timeout();
    void bannerTimeout();

signals:
    void hidingVisualization();

private:
    VisualBase *vis;
    QPixmap pixmap;
    QList<VisualNode*> nodes;
    bool playing;
    int fps;
    QTimer *timer;
    QTimer *bannerTimer;
    InfoWidget* info_widget;

    QString current_visual_name;
};

class InfoWidget : public QWidget
{
    Q_OBJECT

public:
    InfoWidget(QWidget *parent = 0);
    void showInformation(const QString &text);
    void showMetadata(Metadata *meta, bool fullScreen, int visMode);
    void paintEvent(QPaintEvent *);
    void setDisplayRect(QRect rect) { displayRect = rect; }

private:
    QString info;
    QPixmap info_pixmap;
    QRect   displayRect;
};

class StereoScope : public VisualBase
{
public:
    StereoScope();
    virtual ~StereoScope();

    void resize( const QSize &size );
    bool process( VisualNode *node );
    bool draw( QPainter *p, const QColor &back );
    void handleKeyPress(const QString &action) {(void) action;}

protected:
    QColor startColor, targetColor;
    vector<double> magnitudes;
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


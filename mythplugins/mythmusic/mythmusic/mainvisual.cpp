// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include "mainvisual.h"
#include "constants.h"
#include "buffer.h"
#include "output.h"
#include "synaesthesia.h"

#include <qtimer.h>
#include <qpainter.h>
#include <qevent.h>
#include <qapplication.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qsettings.h>
#include <qspinbox.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qcursor.h>

#include <math.h>

// fast inlines
#include "inlines.h"


#include <mythtv/settings.h>

extern Settings *globalsettings;

MainVisual::MainVisual( QWidget *parent, const char *name )
    : QDialog( parent, name ), vis( 0 ), playing( FALSE ), fps( 30 )
{
    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();
    
    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");
        
    float wmult = screenwidth / 800.0;
    float hmult = screenheight / 600.0;

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", 18 * hmult, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer->start(1000 / fps);
}

MainVisual::~MainVisual()
{
    delete vis;
    vis = 0;
}

void MainVisual::setVisual( const QString &visualname )
{
    VisualBase *newvis = 0;

    if (visualname == "Mono Scope")
        newvis = new MonoScope;
    else if (visualname == "Stereo Scope" )
	newvis = new StereoScope;
    else if (visualname == "Synaesthesia")
        newvis = new Synaesthesia;
    	
    setVisual( newvis );
}

void MainVisual::setVisual( VisualBase *newvis )
{
    if (vis)
        delete vis;

    vis = newvis;
    if ( vis )
	vis->resize( size() );

    // force an update
    timer->stop();
    timer->start( 1000 / fps );
}

void MainVisual::prepare()
{
    nodes.setAutoDelete(TRUE);
    nodes.clear();
    nodes.setAutoDelete(FALSE);
}

void MainVisual::add(Buffer *b, unsigned long w, int c, int p)
{
    long len = b->nbytes, cnt;
    short *l = 0, *r = 0;

    len /= c;
    len /= (p / 8);
    if (len > 512)
	len = 512;
    cnt = len;

    if (c == 2) {
	l = new short[len];
	r = new short[len];

	if (p == 8)
	    stereo16_from_stereopcm8(l, r, b->data, cnt);
	else if (p == 16)
	    stereo16_from_stereopcm16(l, r, (short *) b->data, cnt);
    } else if (c == 1) {
	l = new short[len];

	if (p == 8)
	    mono16_from_monopcm8(l, b->data, cnt);
	else if (p == 16)
	    mono16_from_monopcm16(l, (short *) b->data, cnt);
    } else
	len = 0;

    nodes.append(new VisualNode(l, r, len, w));
}

void MainVisual::timeout()
{
    VisualNode *node = 0;

    if (playing && output()) {
	output()->mutex()->lock();
	long olat = output()->latency();
	long owrt = output()->written();
	output()->mutex()->unlock();

	long synctime = owrt < olat ? 0 : owrt - olat;

	mutex()->lock();
	VisualNode *prev = 0;
	while ((node = nodes.first())) {
	    if (node->offset > synctime)
		break;

	    delete prev;
	    nodes.removeFirst();
	    prev = node;
	}
	mutex()->unlock();
	node = prev; 
    }

    bool stop = TRUE;
    if ( vis )
	stop = vis->process( node );
    if (node)
        delete node;

    if ( vis ) {
	QPainter p(&pixmap);
	vis->draw( &p, backgroundColor() );
    } else
	pixmap.fill( backgroundColor() );
    bitBlt(this, 0, 0, &pixmap);

    if (! playing && stop)
	timer->stop();
}

void MainVisual::paintEvent(QPaintEvent *)
{
    bitBlt(this, 0, 0, &pixmap);
}

void MainVisual::resizeEvent( QResizeEvent *event )
{
    pixmap.resize(event->size());
    pixmap.fill(backgroundColor());
    QWidget::resizeEvent( event );
    if ( vis )
	vis->resize( size() );
}

void MainVisual::customEvent(QCustomEvent *event)
{
    switch (event->type()) {
    case OutputEvent::Playing:
	playing = TRUE;
	// fall through intended

    case OutputEvent::Info:
    case OutputEvent::Buffering:
    case OutputEvent::Paused:
	if (! timer->isActive())
	    timer->start(1000 / fps);

	break;

    case OutputEvent::Stopped:
    case OutputEvent::Error:
	playing = FALSE;
	break;

    default:
	;
    }
}

void MainVisual::hideEvent(QHideEvent *e)
{
    setVisual(0);
    QDialog::hideEvent(e);
}

StereoScope::StereoScope()
    : rubberband( true ), falloff( 1.0 ), fps( 30 )
{
    startColor = Qt::black;
    targetColor = Qt::red;
}

StereoScope::~StereoScope()
{
}

void StereoScope::resize( const QSize &newsize )
{
    size = newsize;

    uint os = magnitudes.size();
    magnitudes.resize( size.width() * 2 );
    for ( ; os < magnitudes.size(); os++ )
	magnitudes[os] = 0.0;
}

bool StereoScope::process( VisualNode *node )
{
    bool allZero = TRUE;
    int i;
    long s, index, indexTo, step = 512 / size.width();
    double *magnitudesp = magnitudes.data();
    double valL, valR, tmpL, tmpR;

    if (node) {
	index = 0;
	for ( i = 0; i < size.width(); i++) {
	    indexTo = index + step;

	    if ( rubberband ) {
		valL = magnitudesp[ i ];
		valR = magnitudesp[ i + size.width() ];
		if (valL < 0.) {
		    valL += falloff;
		    if ( valL > 0. )
			valL = 0.;
		} else {
		    valL -= falloff;
		    if ( valL < 0. )
			valL = 0.;
		}
		if (valR < 0.) {
		    valR += falloff;
		    if ( valR > 0. )
			valR = 0.;
		} else {
		    valR -= falloff;
		    if ( valR < 0. )
			valR = 0.;
		}
	    } else
		valL = valR = 0.;

	    for (s = index; s < indexTo && s < node->length; s++) {
		tmpL = ( ( node->left ?
			   double( node->left[s] ) : 0.) *
			 double( size.height() / 4 ) ) / 32768.;
		tmpR = ( ( node->right ?
			   double( node->right[s]) : 0.) *
			 double( size.height() / 4 ) ) / 32768.;
		if (tmpL > 0)
		    valL = (tmpL > valL) ? tmpL : valL;
		else
		    valL = (tmpL < valL) ? tmpL : valL;
		if (tmpR > 0)
		    valR = (tmpR > valR) ? tmpR : valR;
		else
		    valR = (tmpR < valR) ? tmpR : valR;
	    }

	    if (valL != 0. || valR != 0.)
		allZero = FALSE;

	    magnitudesp[ i ] = valL;
	    magnitudesp[ i + size.width() ] = valR;

	    index = indexTo;
	}
    } else if (rubberband) {
	for ( i = 0; i < size.width(); i++) {
	    valL = magnitudesp[ i ];
	    if (valL < 0) {
		valL += 2;
		if (valL > 0.)
		    valL = 0.;
	    } else {
		valL -= 2;
		if (valL < 0.)
		    valL = 0.;
	    }

	    valR = magnitudesp[ i + size.width() ];
	    if (valR < 0.) {
		valR += falloff;
		if (valR > 0.)
		    valR = 0.;
	    } else {
		valR -= falloff;
		if (valR < 0.)
		    valR = 0.;
	    }

	    if (valL != 0. || valR != 0.)
		allZero = FALSE;

	    magnitudesp[ i ] = valL;
	    magnitudesp[ i + size.width() ] = valR;
	}
    } else {
	for ( i = 0; (unsigned) i < magnitudes.size(); i++ )
	    magnitudesp[ i ] = 0.;
    }

    return allZero;
}

void StereoScope::draw( QPainter *p, const QColor &back )
{
    double *magnitudesp = magnitudes.data();
    double r, g, b, per;

    p->fillRect(0, 0, size.width(), size.height(), back);
    for ( int i = 1; i < size.width(); i++ ) {
	// left
	per = double( magnitudesp[ i ] * 2 ) /
	      double( size.height() / 4 );
	if (per < 0.0)
	    per = -per;
	if (per > 1.0)
	    per = 1.0;
	else if (per < 0.0)
	    per = 0.0;

	r = startColor.red() + (targetColor.red() -
				startColor.red()) * (per * per);
	g = startColor.green() + (targetColor.green() -
				  startColor.green()) * (per * per);
	b = startColor.blue() + (targetColor.blue() -
				 startColor.blue()) * (per * per);

	if (r > 255.0)
	    r = 255.0;
	else if (r < 0.0)
	    r = 0;

	if (g > 255.0)
	    g = 255.0;
	else if (g < 0.0)
	    g = 0;

	if (b > 255.0)
	    b = 255.0;
	else if (b < 0.0)
	    b = 0;

	p->setPen( QColor( int(r), int(g), int(b) ) );
	p->drawLine( i - 1, ( size.height() / 4 ) + magnitudesp[ i - 1 ],
		     i, ( size.height() / 4 ) + magnitudesp[ i ] );

	// right
	per = double( magnitudesp[ i + size.width() ] * 2 ) /
	      double( size.height() / 4 );
	if (per < 0.0)
	    per = -per;
	if (per > 1.0)
	    per = 1.0;
	else if (per < 0.0)
	    per = 0.0;

	r = startColor.red() + (targetColor.red() -
				startColor.red()) * (per * per);
	g = startColor.green() + (targetColor.green() -
				  startColor.green()) * (per * per);
	b = startColor.blue() + (targetColor.blue() -
				 startColor.blue()) * (per * per);

	if (r > 255.0)
	    r = 255.0;
	else if (r < 0.0)
	    r = 0;

	if (g > 255.0)
	    g = 255.0;
	else if (g < 0.0)
	    g = 0;

	if (b > 255.0)
	    b = 255.0;
	else if (b < 0.0)
	    b = 0;

	p->setPen( QColor( int(r), int(g), int(b) ) );
	p->drawLine( i - 1, ( size.height() * 3 / 4 ) +
		     magnitudesp[ i + size.width() - 1 ],
		     i, ( size.height() * 3 / 4 ) + magnitudesp[ i + size.width() ] );
    }
}

MonoScope::MonoScope()
{       
}   
        
MonoScope::~MonoScope()
{       
}           
    
bool MonoScope::process( VisualNode *node )
{       
    bool allZero = TRUE;
    int i;  
    long s, index, indexTo, step = 512 / size.width();
    double *magnitudesp = magnitudes.data();
    double val, tmp;

    if (node) {
        index = 0;
        for ( i = 0; i < size.width(); i++) {
            indexTo = index + step;

            if ( rubberband ) {
                val = magnitudesp[ i ];
                if (val < 0.) {
                    val += falloff;
                    if ( val > 0. )
                        val = 0.;
                } else {
                    val -= falloff;
                    if ( val < 0. )
                        val = 0.;
                }
            } else
                val = 0.;

            for (s = index; s < indexTo && s < node->length; s++) {
                tmp = ( double( node->left[s] ) +
                        (node->right ? double( node->right[s] ) : 0) *
                        double( size.height() / 2 ) ) / 65536.;
                if (tmp > 0)
                    val = (tmp > val) ? tmp : val;
                else
                    val = (tmp < val) ? tmp : val;
            }

            if ( val != 0. )
                allZero = FALSE;
            magnitudesp[ i ] = val;
            index = indexTo;
        }
    } else if (rubberband) {
        for ( i = 0; i < size.width(); i++) {
            val = magnitudesp[ i ];
            if (val < 0) {
                val += 2;
                if (val > 0.)
                    val = 0.;
            } else {
                val -= 2;
                if (val < 0.)
                    val = 0.;
            }

            if ( val != 0. )
                allZero = FALSE;
            magnitudesp[ i ] = val;
        }
    } else {
        for ( i = 0; i < size.width(); i++ )
            magnitudesp[ i ] = 0.;
    }

    return allZero;
}

void MonoScope::draw( QPainter *p, const QColor &back )
{
    double *magnitudesp = magnitudes.data();
    double r, g, b, per;

    p->fillRect( 0, 0, size.width(), size.height(), back );
    for ( int i = 1; i < size.width(); i++ ) {
        per = double( magnitudesp[ i ] ) /
              double( size.height() / 4 );
        if (per < 0.0)
            per = -per;
        if (per > 1.0)
            per = 1.0;
        else if (per < 0.0)
            per = 0.0;

        r = startColor.red() + (targetColor.red() -
                                startColor.red()) * (per * per);
        g = startColor.green() + (targetColor.green() -
                                  startColor.green()) * (per * per);
        b = startColor.blue() + (targetColor.blue() -
                                 startColor.blue()) * (per * per);

        if (r > 255.0)
            r = 255.0;
        else if (r < 0.0)
            r = 0;

        if (g > 255.0)
            g = 255.0;
        else if (g < 0.0)
            g = 0;

        if (b > 255.0)
            b = 255.0;
        else if (b < 0.0)
            b = 0;

        p->setPen(QColor(int(r), int(g), int(b)));
        p->drawLine( i - 1, size.height() / 2 + magnitudesp[ i - 1 ],
                     i, size.height() / 2 + magnitudesp[ i ] );
    }
}

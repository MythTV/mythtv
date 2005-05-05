/*
	visual.cpp

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for mfe visualizations
	
*/
//  #include "polygon.h"
//  #include "constants.h"

//  #include <qwidget.h>
//  #include <qdialog.h>
//  #include <qmemarray.h>
//  #include <qpixmap.h>
//  #include <qptrlist.h>
//  #include <qstringlist.h>

#include <iostream>
using namespace std;

#include "visual.h"

MfeVisualizer::MfeVisualizer()
{
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer->start(1000 / 20);    //  20 fps
}

void MfeVisualizer::add(uchar *b, unsigned long b_len, unsigned long w, int c, int p)
{
}

void MfeVisualizer::timeout()
{
/*
    VisualNode *node = nodes.first()
    process();

    VisualNode *node = 0;

    if (playing && output()) {
        long synctime = output()->GetAudiotime();
	mutex()->lock();
	VisualNode *prev = 0;
	while ((node = nodes.first()))
	{
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
    if (vis && process)
	stop = vis->process(node);
    if (node)
        delete node;

    if (vis && process) 
    {
        QPainter p(&pixmap);
        if (vis->draw(&p, Qt::black))
            bitBlt(this, 0, 0, &pixmap);
    } 

    if (!playing && stop)
	timer->stop();
*/
}

void MfeVisualizer::paintEvent(QPaintEvent *)
{
    cout << "in MfeVisualizer::paintEvent()" << endl;
    bitBlt(this, 0, 0, &pixmap);
}

void MfeVisualizer::resizeEvent( QResizeEvent *event )
{
    pixmap.resize(event->size());
    pixmap.fill(backgroundColor());
    QWidget::resizeEvent( event );
/*
    if ( vis )
        vis->resize( size() );
*/
}




MfeVisualizer::~MfeVisualizer()
{
}


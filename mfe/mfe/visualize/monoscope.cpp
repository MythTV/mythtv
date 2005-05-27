/*
	monocope.cpp

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A monoscope visualization
	
*/

#include "monoscope.h"

#include <qpainter.h>
#include <qpixmap.h>

#include <iostream>
using namespace std;


MonoScope::MonoScope()
            :MfeVisualizer()
{
    fps = 40;
    rubberband = false;
    falloff = 1.0;

    startColor = Qt::blue;
    targetColor = Qt::red;

    visualization_type = MVT_MONOSCOPE;
}

bool MonoScope::update(QPixmap *pixmap_to_draw_on)
{
    if(nodes.size() < 1)
    {
        return false;  
    }

/*
    std::deque<VisualNode*>::iterator iter;
    int process_at_most = 0;
    for (iter = nodes.begin(); iter != nodes.end(); iter++)
    {
        if (process_at_most < 2)
        {
            process( (*iter) );
        }
        ++process_at_most;
    }
*/

    process(nodes.back());
    nodes.clear();


    QPainter p(pixmap_to_draw_on);
    draw(&p, Qt::black);
    return true;
}

void MonoScope::resize( QSize new_size, QColor)
{
    my_size = new_size;
    my_size = new_size;
    uint os = magnitudes.size();
    magnitudes.resize( my_size.width() * 2 );
    for ( ; os < magnitudes.size(); os++ )
	magnitudes[os] = 0.0;

}


MonoScope::~MonoScope()
{
}


bool MonoScope::process( VisualNode *node )
{
    bool allZero = TRUE;
    int i;  
    long s, indexTo;
    double *magnitudesp = magnitudes.data();
    double val, tmp;

    double index, step = 256.0 / my_size.width();

    if (node) 
    {
        index = 0;
        for ( i = 0; i < my_size.width(); i++) 
        {
            indexTo = (int)(index + step);
            if (indexTo == (int)index)
                indexTo = (int)(index + 1);

            if ( rubberband ) 
            {
                val = magnitudesp[ i ];
                if (val < 0.) 
                {
                    val += falloff;
                    if ( val > 0. )
                    {
                        val = 0.;
                    }
                } 
                else 
                {
                    val -= falloff;
                    if ( val < 0. )
                    {
                        val = 0.;
                    }
                }
            } 
            else
            {
                val = 0.;
            }

            for (s = (int)index; s < indexTo && s < node->length; s++) 
            {
                tmp = ( double( node->left[s] ) +
                        (node->right ? double( node->right[s] ) : 0) *
                        double( my_size.height() / 2 ) ) / 65536.;
                if (tmp > 0)
                {
                    val = (tmp > val) ? tmp : val;
                }
                else
                {
                    val = (tmp < val) ? tmp : val;
                }
            }

            if ( val != 0. )
            {
                allZero = FALSE;
            }
            magnitudesp[ i ] = val;
            index = index + step;
        }
    } 
    else if (rubberband) 
    {
        for ( i = 0; i < my_size.width(); i++) {
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
    } 
    else 
    {
        for ( i = 0; i < my_size.width(); i++ )
            magnitudesp[ i ] = 0.;
    }

    return allZero;
}

bool MonoScope::draw( QPainter *p, const QColor &back )
{
    double *magnitudesp = magnitudes.data();
    double r, g, b, per;

    p->fillRect( 0, 0, my_size.width(), my_size.height(), back );
    for ( int i = 1; i < my_size.width(); i++ ) {
        per = double( magnitudesp[ i ] ) /
              double( my_size.height() / 4 );
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

        //p->setPen(Qt::red);
        p->setPen(QColor(int(r), int(g), int(b)));
        p->drawLine( i - 1, (int)(my_size.height() / 2 + magnitudesp[ i - 1 ]),
                     i, (int)(my_size.height() / 2 + magnitudesp[ i ] ));
    }

    return true;
}


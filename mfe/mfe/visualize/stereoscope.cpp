/*
	stereoscope.cpp

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A stereoscope visualization
	
*/

#include "stereoscope.h"

#include <qpainter.h>
#include <qpixmap.h>

#include <iostream>
using namespace std;


StereoScope::StereoScope()
            :MfeVisualizer()
{
    fps = 40;
    rubberband = false;
    falloff = 1.0;

    startColor = Qt::blue;
    targetColor = Qt::red;
    
    visualization_type = MVT_STEREOSCOPE;
}

bool StereoScope::update(QPixmap *pixmap_to_draw_on)
{
    if(nodes.size() < 1)
    {
        return false;  
    }

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
    nodes.clear();



    QPainter p(pixmap_to_draw_on);
    draw(&p, Qt::black);
    return true;
}

void StereoScope::resize( QSize new_size, QColor)
{
    my_size = new_size;
    my_size = new_size;
    uint os = magnitudes.size();
    magnitudes.resize( my_size.width() * 2 );
    for ( ; os < magnitudes.size(); os++ )
	magnitudes[os] = 0.0;

}


StereoScope::~StereoScope()
{
}


bool StereoScope::process( VisualNode *node )
{
    bool allZero = TRUE;
    int i;
    long s, indexTo;
    double *magnitudesp = magnitudes.data();
    double valL, valR, tmpL, tmpR;
    double index, step = 256.0 / my_size.width();

    if (node)
    {
	    index = 0;
	    for ( i = 0; i < my_size.width(); i++) 
	    {
	        indexTo = (int)(index + step);
            if (indexTo == (int)(index))
            {
                indexTo = (int)(index + 1);
            }

	        if ( rubberband ) 
	        {
		        valL = magnitudesp[ i ];
		        valR = magnitudesp[ i + my_size.width() ];
		        if (valL < 0.) 
		        {
		            valL += falloff;
		            if ( valL > 0. )
			        valL = 0.;
		        } 
		        else 
		        {
		            valL -= falloff;
		            if ( valL < 0. )
		            {
			            valL = 0.;
			        }
		        }
		        if (valR < 0.) 
		        {
		            valR += falloff;
		            if ( valR > 0. )
		            {
			            valR = 0.;
			        }
		        } 
		        else 
		        {
		            valR -= falloff;
		            if ( valR < 0. )
		            {
			            valR = 0.;
			        }
		        }
	        } 
	        else
	        {
		        valL = valR = 0.;
		    }

	        for (s = (int)index; s < indexTo && s < node->length; s++)
	        {
		        tmpL = ( ( node->left ?
			            double( node->left[s] ) : 0.) *
			            double( my_size.height() / 4 ) ) / 32768.;
		        tmpR = ( ( node->right ?
			            double( node->right[s]) : 0.) *
			            double( my_size.height() / 4 ) ) / 32768.;
		        if (tmpL > 0)
		        {
		            valL = (tmpL > valL) ? tmpL : valL;
		        }
		        else
		        {
		            valL = (tmpL < valL) ? tmpL : valL;
		        }
		        if (tmpR > 0)
		        {
		            valR = (tmpR > valR) ? tmpR : valR;
		        }
		        else
		        {
		            valR = (tmpR < valR) ? tmpR : valR;
		        }
	        }

	        if (valL != 0. || valR != 0.)
	        {
		        allZero = FALSE;
		    }

	        magnitudesp[ i ] = valL;
	        magnitudesp[ i + my_size.width() ] = valR;

	        index = index + step;
	    }
    } 
    else if (rubberband) 
    {
	    for ( i = 0; i < my_size.width(); i++)
	    {
	        valL = magnitudesp[ i ];
	        if (valL < 0) 
	        {
		        valL += 2;
		        if (valL > 0.)
		        {
		            valL = 0.;
		        }
	        } 
	        else 
	        {
		        valL -= 2;
		        if (valL < 0.)
		        {
    		        valL = 0.;
    		    }
	        }

	        valR = magnitudesp[ i + my_size.width() ];
	        if (valR < 0.) 
	        {
		        valR += falloff;
		        if (valR > 0.)
		        {
		            valR = 0.;
		        }
	        } 
	        else 
	        {
		        valR -= falloff;
		        if (valR < 0.)
		        {
		            valR = 0.;
		        }
	        }

	        if (valL != 0. || valR != 0.)
	        {
    		    allZero = FALSE;
    		}

	        magnitudesp[ i ] = valL;
	        magnitudesp[ i + my_size.width() ] = valR;
	    }
    } 
    else 
    {
	    for ( i = 0; (unsigned) i < magnitudes.size(); i++ )
	    {
	        magnitudesp[ i ] = 0.;
	    }
    }

    return allZero;
}

bool StereoScope::draw( QPainter *p, const QColor &back )
{

    double *magnitudesp = magnitudes.data();
    double r, g, b, per;

    p->fillRect(0, 0, my_size.width(), my_size.height(), back);
    for ( int i = 1; i < my_size.width(); i++ ) {
	// left
	per = double( magnitudesp[ i ] * 2 ) /
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

	p->setPen( QColor( int(r), int(g), int(b) ) );
        //p->setPen(Qt::red);
	p->drawLine( i - 1, (int)((my_size.height() / 4) + magnitudesp[i - 1]),
		     i, (int)((my_size.height() / 4) + magnitudesp[i]));

	// right
	per = double( magnitudesp[ i + my_size.width() ] * 2 ) /
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

	p->setPen( QColor( int(r), int(g), int(b) ) );
    //    p->setPen(Qt::red);
	p->drawLine( i - 1, (int)((my_size.height() * 3 / 4) +
		     magnitudesp[i + my_size.width() - 1]),
		     i, (int)((my_size.height() * 3 / 4) + 
                     magnitudesp[i + my_size.width()]));
    }

    return true;
}


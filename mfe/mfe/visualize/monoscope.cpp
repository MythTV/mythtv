/*
	monocope.cpp

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A monoscope visualization
	
*/

#include "monoscope.h"
#include "inlines.h"

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


void MonoScope::add(uchar *b, unsigned long b_len, int c, int p)
{
    long len = b_len, cnt;
    short *l = 0, *r = 0;

    len /= c;
    len /= (p / 8);
    
    if (len > 256)
        len = 256;

    cnt = len;

    if (c == 2) 
    {
        l = new short[len];
        r = new short[len];

        if (p == 8)
            stereo16_from_stereopcm8(l, r, b, cnt);
        else if (p == 16)
            stereo16_from_stereopcm16(l, r, (short *) b, cnt);
    } 
    else if (c == 1) 
    {
        l = new short[len];

        if (p == 8)
            mono16_from_monopcm8(l, b, cnt);
        else if (p == 16)
            mono16_from_monopcm16(l, (short *) b, cnt);
    } 
    else
        len = 0;

    nodes.append(new VisualNode(l, r, len, 0));
    
}



bool MonoScope::update(QPixmap *pixmap_to_draw_on)
{
    if(nodes.count() < 1)
    {
        return false;  
    }

    VisualNode *node = NULL;
    while( (node = nodes.first()) )
    {
        if(nodes.count() < 3)
        {
            process(node);
        }
        delete node;
        nodes.removeFirst();
    }


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


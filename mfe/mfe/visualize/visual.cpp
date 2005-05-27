/*
	visual.cpp

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for mfe visualizations
	
*/

#include <iostream>
using namespace std;

#include "visual.h"
#include "inlines.h"

MfeVisualizer::MfeVisualizer()
{
    fps = 20;   //  fallback default
}

void MfeVisualizer::add(uchar *b, unsigned long b_len, int c, int p)
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

    nodes.push_back(new VisualNode(l, r, len, 0));
    //nodes.append(new VisualNode(l, r, len, 0));
    
    
}

void MfeVisualizer::resize( QSize, QColor)
{
    cerr << "in base class MfeVisualizer::resize()" << endl;
}


bool MfeVisualizer::update(QPixmap*)
{
    cerr << "in base class MfeVisualizer::update()" << endl;
    return false;
}

MfeVisualizer::~MfeVisualizer()
{
}


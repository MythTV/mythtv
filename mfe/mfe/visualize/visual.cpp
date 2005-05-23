/*
	visual.cpp

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for mfe visualizations
	
*/

#include <iostream>
using namespace std;

#include "visual.h"

MfeVisualizer::MfeVisualizer()
{
    fps = 20;   //  fallback default
}

void MfeVisualizer::add(uchar*, unsigned long, int, int)
{
    cerr << "in base class MfeVisualizer::add()" << endl;
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


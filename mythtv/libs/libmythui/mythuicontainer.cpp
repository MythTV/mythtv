/*
    mythcontainer.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Object that holds UI widgets in a logical group.
	
*/

#include <iostream>
using namespace std;

#include "mythuicontainer.h"

MythUIContainer::MythUIContainer(MythUIType *parent, const char *name)
                :MythUIType(parent, name)
                
{
    m_Area = QRect(0,0,0,0);
    setDebugColor(QColor(255,0,0));
}

MythUIContainer::~MythUIContainer()
{
}


/*
	mythuitreecolumn.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Holds buttons in a column
	
*/

#include <iostream>
using namespace std;

#include "mythuitreecolumn.h"

MythUITreeColumn::MythUITreeColumn(
                                    QRect l_area, 
                                    int l_id, 
                                    MythUIType *parent, 
                                    const char *name
                                  )
                 :MythUIType(parent, name)
{
    m_Area = l_area;
    m_id = l_id;
    setDebugColor(QColor(0, 255, 255));
}

void MythUITreeColumn::calculateMaxButtons()
{
    cout << "I am a column with an id of "
         << m_id
         << " and I am calculating how many buttons to draw"
         << endl;
}

MythUITreeColumn::~MythUITreeColumn()
{
}


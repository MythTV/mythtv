#ifndef MYTHCONTAINER_H_
#define MYTHCONTAINER_H_
/*
    mythcontainer.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Object that holds UI widgets in a logical group.
	
*/


#include <qstring.h>
#include <qrect.h>

#include "mythuitype.h"

class MythUIContainer : public MythUIType
{
  public:
    MythUIContainer(MythUIType *parent, const char *name);
   ~MythUIContainer();

    void    setArea(QRect area ) { m_Area = area; }
    QRect   getArea(){ return m_Area; }
    QPoint  getPosition() { return m_Area.topLeft(); }
};

#endif

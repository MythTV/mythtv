#ifndef VISUALBASE_H_
#define VISUALBASE_H_
/*
	visualbase.h

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for visualizations.
	
*/

#include <qmutex.h>

class VisualBase
{

  public:

    VisualBase();
    virtual ~VisualBase();

    virtual void add(uchar *b, unsigned long b_len, unsigned long w, int c, int p) = 0;

  protected:
  
    QMutex  data_mutex;
};

#endif

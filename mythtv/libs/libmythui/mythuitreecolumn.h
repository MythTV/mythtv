#ifndef MYTHUITREECOLUMN_H_
#define MYTHUITREECOLUMN_H_
/*
	mythuitreecolumn.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Holds buttons in a column
	
*/

#include "mythuitype.h"

class MythUITreeColumn : public MythUIType
{
  public:
  
    MythUITreeColumn(
                        QRect l_area, 
                        int l_id, 
                        MythUIType *parent, 
                        const char *name
                    );
   ~MythUITreeColumn();

    void calculateMaxButtons();
   
  private:

    int m_id;
       
   
};

#endif

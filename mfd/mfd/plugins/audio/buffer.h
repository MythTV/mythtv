#ifndef BUFFER_H_
#define BUFFER_H_
/*
	buffer.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an audio buffer
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
*/

#include "constants.h"

class Buffer
{
  
  public:
  
    Buffer();
    ~Buffer();
    static unsigned long size(){return globalBlockSize;}

  //    NB not private

    unsigned char *data;
    unsigned long nbytes;
    unsigned long rate;

};


#endif // __buffer_h


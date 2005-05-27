#ifndef TRANSCODER_H_
#define TRANSCODER_H_
/*
	transcoder.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the universal transcoder
*/

#include <iostream>
using namespace std; 

#include "httpserver.h"

class Transcoder: public MFDHttpPlugin
{

  public:

    Transcoder(MFD *owner, int identity);
    ~Transcoder();

};

#endif  // transcoder_h_

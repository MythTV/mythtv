/*
	buffer.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	an audio buffer
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
*/

#include "buffer.h"

Buffer::Buffer()
{
	data = new unsigned char[Buffer::size()];
	nbytes = 0;
	rate = 0;
}



Buffer::~Buffer()
{
    if(data)
    {
        delete [] data;
	    data = 0;
	}
	nbytes = 0;
	rate = 0;
}




// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef   __buffer_h
#define   __buffer_h


class Buffer {
public:
    Buffer()
    {
	data = new unsigned char[Buffer::size()];
	nbytes = 0;
	rate = 0;
    }
    ~Buffer()
    {
	delete data;
	data = 0;
	nbytes = 0;
	rate = 0;
    }

    unsigned char *data;
    unsigned long nbytes;
    unsigned long rate;

    static unsigned long size() { return globalBlockSize; }
};


#endif // __buffer_h


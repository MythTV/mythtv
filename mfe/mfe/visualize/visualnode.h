#ifndef VISUALNODE_H_
#define VISUALNODE_H_
/*
	visualnode.h

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Class for storing visualization data
	
*/

class VisualNode
{
public:

    VisualNode(short *l, short *r, unsigned long n, unsigned long o);
    ~VisualNode();

    short *left, *right;
    long length, offset;
    
};



#endif

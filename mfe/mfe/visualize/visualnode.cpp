/*
	visualnode.cpp

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Class for storing visualization data
	
*/

#include "visualnode.h"

VisualNode::VisualNode(short *l, short *r, unsigned long n, unsigned long o)
 	       : left(l), right(r), length(n), offset(o)
{
    // left and right are allocated and then passed to this class
	// the code that allocated left and right should give up all ownership
}

VisualNode::~VisualNode()
{
    delete [] left;
    delete [] right;
}


/*
	mythtreenode.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An object for building trees of things (mostly metadata/content)
*/


#include "mythtreenode.h"

MythTreeNode::MythTreeNode(const QString& a_string)
{

    m_string = a_string;

    //
    //  Set my type as unknown
    //
    
    m_type = TNT_unknown;
}

MythTreeNode::~MythTreeNode()
{
}



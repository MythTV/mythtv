/*
	lean and mean DAAP-lib
	output stream class for taged data

	Copyright (c) deleet 2003, Alexander Oberdoerster & Johannes Zander
	http://deleet.de/projekte/daap/
	
	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.
	
	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "tagoutput.h"


	//////
	// tag output
	
	TagOutput& TagOutput::operator << ( const Tag& tag ) {
		operator << ( tag.type );
		openTags.push_back( msg.size()); // remember position of length field
		operator << ( (u32)0 );          // reserve 4 byte for the length field
		return( *this );
	}

	TagOutput& TagOutput::operator << ( u16 x ) {
		msg.push_back(( x>>8 )&0xff );
		msg.push_back(( x    )&0xff );
		return( *this );
	}
		
	TagOutput& TagOutput::operator << ( u32 x ) {
		msg.push_back(( x>>24 )&0xff );
		msg.push_back(( x>>16 )&0xff );
		msg.push_back(( x>>8  )&0xff );
		msg.push_back(( x     )&0xff );
		return( *this );
	}
	
	TagOutput& TagOutput::closeTag() {
		assert( !openTags.empty());
		updateInt32( openTags.back(), msg.size()-openTags.back() - 4 ); // length of tag (header exluded)
		openTags.pop_back();
		return( *this );
	}

	// assume the chunk is already in UTF-8
	TagOutput& TagOutput::outUnchanged( const Chunk& c ) {
		for( u32 i=0; i<c.size(); ++i )
			msg.push_back( c[i] );
		return( *this );
	}

	// assume the chunk is in iso-latin1 and convert it to UTF-8
	TagOutput& TagOutput::outUTF8( const Chunk& c ) {
		for( u32 i=0; i<c.size(); ++i )
			if( c[i]<0x80 )
				msg.push_back( c[i] );
			else {
				msg.push_back( 0xc0 | ( c[i] >> 6 ));
				msg.push_back( 0x80 | ( c[i] & 0x3f ));
			}
		return( *this );
	}

/*
	lean and mean DAAP-lib
	input stream class for taged data

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

#include "taginput.h"


	//////
	// tag input

	Tag::TagType TagInput::peekTagType() const {
		return( !isCorrupted() && leftInMsg()>=4
			? readInt32( pos )
			: 0 );
	}
		
	TagInput& TagInput::operator >> ( Tag& x ) {
		if( !isCorrupted(( tagEnd.empty() ? msg.size()-pos : leftInTag()) < 8 )) {
			x.type = readInt32( pos );
			u32 size = readInt32( pos+4 );
			pos += 8;
			tagEnd.push_back( pos + size ); // open a new tag
		}
		return( *this );
	}
		
	TagInput& TagInput::operator >> ( Chunk& x ) {
		if( !isCorrupted()) {
			u32 left = leftInTag();

			x    = Chunk( msg.begin()+pos, left );
			pos += left;
		}
		return( *this );
	}

	TagInput& TagInput::operator >> ( std::string& x ) { // beware: this is an UTF-8 string!
		if( !isCorrupted()) {
			u32 left = leftInTag();
			
			if( left==0 ) {  
				if( !x.empty() )
					x.erase( 0, x.size() - 1 );
			} else {
				x.assign( (const char*)msg.begin()+pos, (const char*)msg.begin()+pos+left );
			}
			
			pos += left;
		}

		return( *this );
	}

	TagInput& TagInput::operator >> ( u8& x ) {
		if( !isCorrupted( leftInTag() < 1 ))
			x = msg[ pos++ ];
		return( *this );
	}
		
	TagInput& TagInput::operator >> ( u16& x ) {
		if( !isCorrupted( leftInTag() < 2 )) {
			x = msg[ pos+0 ]*0x100 + msg[ pos+1 ];
			pos += 2;
		}
		return( *this );
	}

	TagInput& TagInput::operator >> ( u32& x ) {
		if( !isCorrupted( leftInTag() < 4 )) {
			x = readInt32( pos );
			pos += 4;
		}
		return( *this );
	}

	TagInput& TagInput::operator >> ( u64& x ) {
		if( !isCorrupted( leftInTag() < 8 )) {
			x   = (((u64)readInt32( pos )) << 32 ) + readInt32( pos+4 );
			pos += 8;
		}
		return( *this );
	}
	
	void TagInput::skip( u32 amount ) {
		if( !isCorrupted( leftInTag()<amount ))
			pos += amount;
	}
	
	void TagInput::skipTag() {
		Tag temp;

		*this >> temp;
		skipRestOfTag();
		closeTag();
	}

	void TagInput::skipRestOfTag() {
		if( !isCorrupted() && !tagEnd.empty())
			pos += leftInTag();
	}

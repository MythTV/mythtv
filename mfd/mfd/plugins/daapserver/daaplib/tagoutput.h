#ifndef __TAGOUTPUT_DAAP__
	#define __TAGOUTPUT_DAAP__

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
	
	#include "basic.h"
	#include <string>
	#include <vector>
	#include <assert.h>


	//////
	// tag output
	
	class TagOutput {
	public:
		TagOutput() {}
		
		inline explicit TagOutput( Tag::TagType tag ) {
			operator << ( Tag( tag ));
		}
		
		inline const Chunk data() const {
			assert( openTags.empty());
			const u32 size=msg.size(); 
			return( Chunk( size==0 ? 0 : &msg[0], size ));
		}
		
		//////
		// output
		
		TagOutput& operator << ( const Tag& tag );

		inline TagOutput& operator << ( const Chunk& chunk ) {
			msg.insert( msg.end(), chunk.begin(), chunk.end());
			return( *this );
		}

		inline TagOutput& operator << ( u8 x ) {
			msg.push_back( x );
			return( *this );
		}
			
		TagOutput& operator << ( u16 x );

		TagOutput& operator << ( u32 x );
		
		inline TagOutput& operator << ( const u64& x ) {
			return( *this << (u32)(x>>32) << (u32)x );
		}
		
		inline TagOutput& operator << ( s8  x ) { return( operator <<((u8)x )); }
		inline TagOutput& operator << ( s16 x ) { return( operator <<((u16)x )); }
		inline TagOutput& operator << ( s32 x ) { return( operator <<((u32)x )); }
		inline TagOutput& operator << ( const s64& x ) { return( operator <<((u64&)x )); }
		inline TagOutput& operator << ( const Version& x ) { return( *this << x.hi << x.lo ); }

		// strings are assumed to be already in UTF-8 
		// plain ASCII is ok as well, but ISO Latin 1 is not
		inline TagOutput& operator << ( const char* str ) { 
			return( outUnchanged(Chunk((const u8*)str, strlen( str ))));
		}
			
		inline TagOutput& operator << ( const std::string& str ) { 
			return( outUnchanged(Chunk((const u8*)str.data(), str.size())));
		}

/*
		inline TagOutput& operator << ( const char* str ) { // beware: strings are converted to UTF8
			return( outUTF8( Chunk((const u8*)str, strlen( str ))));
		}
			
		inline TagOutput& operator << ( const std::string& str ) { // beware: strings are converted to UTF8
			return( outUTF8( Chunk((const u8*)str.data(), str.size())));
		}
*/			
		inline TagOutput& operator << ( TagOutput& (*func)( TagOutput& )) {
			return( func( *this ));
		}
			
		friend inline TagOutput& end( TagOutput& x ) {
			return( x.closeTag());
		}
		
	protected:
		typedef std::vector<u8>  DataInt8;
		typedef std::vector<u32> StackInt32;
		
		DataInt8   msg;
		StackInt32 openTags;

		TagOutput& closeTag();
		
		inline void updateInt32( u32 pos, u32 val ) {
			msg[pos+0] = ( val>>24 )&0xff;
			msg[pos+1] = ( val>>16 )&0xff;
			msg[pos+2] = ( val>>8  )&0xff;
			msg[pos+3] = ( val     )&0xff;
		}

		TagOutput& outUnchanged( const Chunk& c );
		TagOutput& outUTF8( const Chunk& c );

	private: // these are evil things to do, so we disallow 'em!
		TagOutput( const TagOutput& );
		TagOutput& operator = ( const TagOutput& );
	};
#endif

#ifndef __TAGINPUT_DAAP__
	#define __TAGINPUT_DAAP__

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
	
	#include "basic.h"
	#include <string>
	#include <vector>


	//////
	// tag input

	class TagInput {
	public:
		TagInput( const Chunk& chunk )
			: msg( chunk )
			, pos(0)
			, corrupted( false ) {}
		
		inline bool isCorrupted() const { return( corrupted ); }
		inline bool isFinished() const  { return( isCorrupted() || pos==msg.size()); }
		inline u32  leftInMsg() const   { return( !isCorrupted() ? msg.size() - pos : 0 ); }
		inline u32  leftInTag() const   { return( tagEnd.empty()||isCorrupted() ? 0 : tagEnd.back()-pos ); }
		
		inline const Chunk getRestOfTag() const {
			return( Chunk( msg.begin()+pos, !isCorrupted() ? leftInTag() : 0 ));
		}
			
		Tag::TagType peekTagType() const;
		
		
		//////
		// input
		
		TagInput& operator >> ( Tag& x );
		TagInput& operator >> ( Chunk& x );
		TagInput& operator >> ( std::string& x );
		TagInput& operator >> ( u8& x );
		TagInput& operator >> ( u16& x );
		TagInput& operator >> ( u32& x );
		TagInput& operator >> ( u64& x );
		
		inline TagInput& operator >> ( s8&  x ) { return( operator >>((u8&)x  )); }
		inline TagInput& operator >> ( s16& x ) { return( operator >>((u16&)x )); }
		inline TagInput& operator >> ( s32& x ) { return( operator >>((u32&)x )); }
		inline TagInput& operator >> ( s64& x ) { return( operator >>((u64&)x )); }
		inline TagInput& operator >> ( Version& x ) { return( *this >> x.hi >> x.lo ); }
			
		inline TagInput& operator >> ( TagInput& (*func)( TagInput& )) {
			return( func( *this ));
		}
			
		friend inline TagInput& end( TagInput& x ) {
			return( x.closeTag() );
		}


		//////
		// some stuff to skip bytes

		struct Skip { 
			u32 amount;
			
			Skip( u32 _amount )
				: amount( _amount ) {}
		};
		
		void skip( u32 amount );
		void skipTag();
		void skipRestOfTag();
		
		inline TagInput& operator >> ( const Skip& x ) {
			skip( x.amount );
			return( *this );
		}

		friend inline TagInput& skipTag( TagInput& x ) {
			x.skipTag();
			return( x );
		}

		friend inline TagInput& skipRestOfTag( TagInput& x ) {
			x.skipRestOfTag();
			return( x );
		}

	protected:
		typedef std::vector<u32> StackInt32;
		
		const Chunk& msg;
		u32          pos;
		StackInt32   tagEnd;
 		bool         corrupted;
 		
 		inline bool isCorrupted( bool status ) {
 			return( corrupted |= status );
 		}
 		
 		inline TagInput& closeTag() {
 			if( !isCorrupted( tagEnd.empty() || tagEnd.back()!=pos ))
 				tagEnd.pop_back();
 			return( *this );
 		}
 		
 		inline u32 readInt32( u32 _pos ) const {
 			return( msg[ _pos+0 ]*0x1000000 + msg[ _pos+1 ]*0x10000 + msg[ _pos+2 ]*0x100 + msg[ _pos+3 ]);
 		}

	private: // these are evil things to do, so we disallow 'em!
		TagInput( const TagInput& );
		TagInput& operator = ( const TagInput& );
	};
#endif

#ifndef __BASIC_DAAP__
	#define __BASIC_DAAP__
	
	/*
		lean and mean DAAP-lib
		basic data types

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
	

	#ifdef _MSC_VER // MS's for-scope is plain wrong - fix it!
		#define for if( false ){}else for
	#endif

	
	//////
	// unified signed/unsigned 8/16/32/64 bit types
	
	#if( defined( WIN32 ) || defined( _WIN32 ))
		typedef unsigned __int64 u64;
		typedef unsigned __int32 u32;
		typedef unsigned __int16 u16;
		typedef unsigned __int8  u8;
		
		typedef signed   __int64 s64;
		typedef signed   __int32 s32;
		typedef signed   __int16 s16;
		typedef signed   __int8  s8;
	#elif( defined( __GNUC__ ) || defined( __MACOSX__ ))
		#include <inttypes.h>
		typedef uint64_t u64;
		typedef uint32_t u32;
		typedef uint16_t u16;
		typedef uint8_t  u8;
		
		typedef int64_t  s64;
		typedef int32_t  s32;
		typedef int16_t  s16;
		typedef int8_t   s8;
	#elif
		#error "uh - no idea what the int types could be!"
	#endif

	
	//////
	// memory chunk
	
	class Chunk {
	public:
		Chunk( const u8* _ptr=0, u32 _size=0 )
			: ptr( _ptr )
			, memSize( _size ) {}

		inline u8        operator[] ( u32 pos ) const { return( ptr[pos]);	}
		inline const u8* begin()                const { return( ptr ); }
		inline const u8* end()                  const { return( ptr+memSize ); }
		inline u32       size()                 const { return( memSize ); }

	protected:
		const u8* ptr;
		u32       memSize;
	};
		

	//////
	// daap tag

	struct Tag {
		typedef u32 TagType;
		
		TagType type;
		
		Tag( TagType _type=0 )
			: type( _type ) {}
	};

	
	//////
	// version

	struct Version {
		u16 hi, lo;

		Version( u16 _hi=0, u16 _lo=0 )
			: hi( _hi )
			, lo( _lo ) {}
	};


	//////
	// time

	typedef u32 Time;

#endif

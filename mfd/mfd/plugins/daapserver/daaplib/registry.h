#ifndef __REGISTRY_DAAP__
	#define __REGISTRY_DAAP__

	/*
		lean and mean DAAP-lib
		type registration

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
	#include "tagoutput.h"
	#include <deque>


	#ifdef __GNUC__
		#if( __GNUC__ < 3 || ( __GNUC__ == 3 && __GNUC_MINOR__ < 1 )) // gcc version < 3.1.0 ?
			#include <hash_map>
			#define STL_PREFIX std
		#else
			#include <ext/hash_map>
			#define STL_PREFIX __gnu_cxx
		#endif // GCC_VERSION
	#else
		#include <hash_map>
		#define STL_PREFIX std
	#endif // __GNUC__


	//////
	// tag queue
	
	typedef std::deque<Tag::TagType> TypeQueue;


	//////
	// type registry 
	
	class TypeRegistry {
	public:
		enum VarType {
			VT_INT8=1,   VT_INT16=3, VT_INT32=5,    VT_INT64=7,
			VT_STRING=9, VT_TIME=10, VT_VERSION=11, VT_CONTAINER=12
		};
		
		
		static inline Chunk getDictionary() { return( container.data()); }
		static bool         addNameToQueue( const char* str, TypeQueue& queue );
		static Tag::TagType extractMnemonic( TypeQueue& queue );


	protected:
		struct Var {
			Tag::TagType mnemonic;
			const char*  name;
			VarType      type;
		};
		
		typedef STL_PREFIX::hash_map<const char*, const Var*> VarMap;

		struct Init { Init(); };
		friend struct TypeRegistry::Init;
	

		static TagOutput container;
		static Init      init;
		static Var       list[];
		static VarMap    varMap;


		static void registerVar( const Var* var );
	
	private: // these are evil things to do, so we disallow 'em!
		TypeRegistry(); // this class is pure static, no need to instantiate!
	};
#endif

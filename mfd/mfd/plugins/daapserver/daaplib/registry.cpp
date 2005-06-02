/*
	lean and mean DAAP-lib
	type registration

	Copyright (c) deleet 2003, Alexander Oberdoerster & Johannes Zander
	http://deleet.de/projekte/daap/daaplib/
	
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

#include "registry.h"

//////
// type registry 

TagOutput            TypeRegistry::container;
TypeRegistry::VarMap TypeRegistry::varMap;
TypeRegistry::Init   TypeRegistry::init;
TypeRegistry::Var    TypeRegistry::list[] = {

	{ 'miid', "dmap.itemid", VT_INT32 },

	{ 'minm', "dmap.itemname", VT_STRING },
	{ 'mikd', "dmap.itemkind", VT_INT8 },
	{ 'mper', "dmap.persistentid", VT_INT64 },
	{ 'mcon', "dmap.container", VT_CONTAINER },
	{ 'mcti', "dmap.containeritemid", VT_INT32 },
	{ 'mpco', "dmap.parentcontainerid", VT_INT32 },
	{ 'mstt', "dmap.status", VT_INT32 },
	{ 'msts', "dmap.statusstring", VT_STRING },
	{ 'mimc', "dmap.itemcount", VT_INT32 },
	{ 'mctc', "dmap.containercount", VT_INT32 },
	{ 'mrco', "dmap.returnedcount", VT_INT32 },
	{ 'mtco', "dmap.specifiedtotalcount", VT_INT32 },
	{ 'mlcl', "dmap.listing", VT_CONTAINER },
	{ 'mlit', "dmap.listingitem", VT_CONTAINER },
	{ 'mbcl', "dmap.bag", VT_CONTAINER },
	{ 'mdcl', "dmap.dictionary", VT_CONTAINER },
	
	{ 'msrv', "dmap.serverinforesponse", VT_CONTAINER },
	{ 'msau', "dmap.authenticationmethod", VT_INT8 },
	{ 'mslr', "dmap.loginrequired", VT_INT8 },
	{ 'mpro', "dmap.protocolversion", VT_VERSION },
	{ 'msal', "dmap.supportsautologout", VT_INT8 },
	{ 'msup', "dmap.supportsupdate", VT_INT8 },
	{ 'mspi', "dmap.supportspersistentids", VT_INT8 },
	{ 'msex', "dmap.supportsextensions", VT_INT8 },
	{ 'msbr', "dmap.supportsbrowse", VT_INT8 },
	{ 'msqy', "dmap.supportsquery", VT_INT8 },
	{ 'msix', "dmap.supportsindex", VT_INT8 },
	{ 'msrs', "dmap.supportsresolve", VT_INT8 },
	{ 'mstm', "dmap.timeoutinterval", VT_INT32 },
	{ 'msdc', "dmap.databasescount", VT_INT32 },
	{ 'msas', "dmap.authenticationschemes",  VT_INT32 },
	{ 'mlog', "dmap.loginresponse", VT_CONTAINER },
	{ 'mlid', "dmap.sessionid", VT_INT32 },
	
	{ 'mupd', "dmap.updateresponse", VT_CONTAINER },
	{ 'musr', "dmap.serverrevision", VT_INT32 },
	{ 'muty', "dmap.updatetype", VT_INT8 },
	{ 'mudl', "dmap.deletedidlisting", VT_CONTAINER },
	
	{ 'mccr', "dmap.contentcodesresponse", VT_CONTAINER },
	{ 'mcnm', "dmap.contentcodesnumber", VT_INT32 },
	{ 'mcna', "dmap.contentcodesname", VT_STRING },
	{ 'mcty', "dmap.contentcodestype", VT_INT16 },
	
	{ 'apro', "daap.protocolversion", VT_VERSION },
	
	{ 'avdb', "daap.serverdatabases", VT_CONTAINER },
	
	{ 'abro', "daap.databasebrowse", VT_CONTAINER },
	{ 'abal', "daap.browsealbumlisting", VT_CONTAINER },
	{ 'abar', "daap.browseartistlisting", VT_CONTAINER },
	{ 'abcp', "daap.browsecomposerlisting", VT_CONTAINER },
	{ 'abgn', "daap.browsegenrelisting", VT_CONTAINER },
	
	{ 'adbs', "daap.databasesongs", VT_CONTAINER },
	{ 'agrp', "daap.songgrouping", VT_STRING },
	{ 'asal', "daap.songalbum", VT_STRING },
	{ 'asar', "daap.songartist", VT_STRING },
	{ 'asbt', "daap.songbeatsperminute", VT_INT16 },
	{ 'asbr', "daap.songbitrate", VT_INT16 },
	{ 'ascm', "daap.songcomment", VT_STRING },
	{ 'ascd', "daap.songcodectype", VT_INT32 },
	{ 'ascs', "daap.songcodecsubtype", VT_INT32 },
	{ 'asco', "daap.songcompilation", VT_INT8 },
	{ 'ascp', "daap.songcomposer", VT_STRING },
	{ 'asda', "daap.songdateadded", VT_TIME },
	{ 'asdm', "daap.songdatemodified", VT_TIME },
	{ 'asdc', "daap.songdisccount", VT_INT16 },
	{ 'asdn', "daap.songdiscnumber", VT_INT16 },
	{ 'asdb', "daap.songdisabled", VT_INT8 },
	{ 'aseq', "daap.songeqpreset", VT_STRING },
	{ 'asfm', "daap.songformat", VT_STRING },
	{ 'asgn', "daap.songgenre", VT_STRING },
	{ 'asdt', "daap.songdescription", VT_STRING },
	{ 'asrv', "daap.songrelativevolume", VT_INT8 },
	{ 'assr', "daap.songsamplerate", VT_INT32 },
	{ 'assz', "daap.songsize", VT_INT32 },
	{ 'asst', "daap.songstarttime", VT_INT32 },
	{ 'assp', "daap.songstoptime", VT_INT32 },
	{ 'astm', "daap.songtime", VT_INT32 },
	{ 'astc', "daap.songtrackcount", VT_INT16 },
	{ 'astn', "daap.songtracknumber", VT_INT16 },
	{ 'asur', "daap.songuserrating", VT_INT8 },
	{ 'asyr', "daap.songyear", VT_INT16 },
	{ 'asdk', "daap.songdatakind", VT_INT8 },
	{ 'asul', "daap.songdataurl", VT_STRING },
	
	{ 'aply', "daap.databaseplaylists", VT_CONTAINER },
	{ 'abpl', "daap.baseplaylist", VT_INT8 },
	{ 'apso', "daap.playlistsongs", VT_CONTAINER },
	{ 'arsv', "daap.resolve", VT_CONTAINER },
	{ 'arif', "daap.resolveinfo", VT_CONTAINER },
	
	{ 'aeNV', "com.apple.itunes.norm-volume", VT_INT32 },
	{ 'aeSP', "com.apple.itunes.smart-playlist", VT_INT8 },
	{ 'aeSV', "com.apple.itunes.music-sharing-version", VT_INT32},
	{ 'aeGI', "com.apple.itunes.itms-genreid", VT_INT32},
	{ 'aeCI', "com.apple.itunes.itms-composerid", VT_INT32},
	{ 'aePI', "com.apple.itunes.itms-playlistid", VT_INT32},
	{ 'aeAI', "com.apple.itunes.itms-artistid", VT_INT32},
	{ 'aeSI', "com.apple.itunes.itms-songid", VT_INT32},

	//
	//  This type was added for myth <--> myth to enable exchange of myth
	//  digests (oddly calculated md5 checksums). It is not part of
	//  "official" daap and should almost certainly never be exchanged with
	//  a non mfd
	//

	{ 'mypi', "myth.persistentiddigest", VT_STRING },

    //
    //  Another myth <---> myth only tag, this one indicating that a
    //  playlist is ripable
    //
    
    { 'mypr', "myth.playlistripable", VT_INT8 }, 
    
    //
    //  And another one, indicating that a playlist is currently being
    //  ripped.
    //
    
    { 'mypb', "myth.playlistbeingripped", VT_INT8 },

};


void TypeRegistry::registerVar( const Var* var ) {
	varMap[var->name] = var;

    //
    //  Don't send the code that is mfd specific
    //    

    if(var->mnemonic != 'mypi' &&
       var->mnemonic != 'mypr' &&
       var->mnemonic != 'mypb')
    {
	    container << Tag( 'mdcl' ) <<
		    Tag( 'mcnm' ) << var->mnemonic  << end <<
		    Tag( 'mcna' ) << var->name      << end <<
		    Tag( 'mcty' ) << (u16)var->type << end <<
	    end;
    }
}


TypeRegistry::Init::Init() {
	TypeRegistry::container << Tag( 'mccr' ) <<
		Tag( 'mstt' ) << (u32)0xC8 << end;

		for( u32 i=0; i<sizeof(TypeRegistry::list)/sizeof(TypeRegistry::list[0]); ++i )
		{
			registerVar( TypeRegistry::list + i );
        }

	TypeRegistry::container << end;
}


bool TypeRegistry::addNameToQueue( const char* str, TypeQueue& queue ) {
	VarMap::const_iterator var = varMap.find( str );
	
	if( var!=varMap.end()) {
		queue.push_back( (*var).second->mnemonic );
		return( true );
	} else
		return( false );
}


Tag::TagType TypeRegistry::extractMnemonic( TypeQueue& queue ) {
	if( queue.empty())
		return( 0 );
	else {
		Tag::TagType ret = queue.front();
		queue.pop_front();
		return( ret );
	}
}


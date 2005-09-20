#ifndef SERVER_TYPES_H_
#define SERVER_TYPES_H_
/*
	server_types.h

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	enum for server types

*/

enum DaapServerType {
    DAAP_SERVER_UNKNOWN = 0,
    DAAP_SERVER_ITUNES4X,
    DAAP_SERVER_ITUNES41,
    DAAP_SERVER_ITUNES42,
    DAAP_SERVER_ITUNES43,
    DAAP_SERVER_ITUNES45,
    DAAP_SERVER_ITUNES46,
    DAAP_SERVER_ITUNES47,
    DAAP_SERVER_ITUNES48,
    DAAP_SERVER_ITUNES49,
    DAAP_SERVER_ITUNES5X,
    DAAP_SERVER_ITUNES50,
    DAAP_SERVER_MYTH
};
    
#endif

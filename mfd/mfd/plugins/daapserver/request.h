#ifndef REQUEST_H_
#define REQUEST_H_
/*
	request.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Very closely based on:
	
	daapd 0.2.1, a server for the DAA protocol
	(c) deleet 2003, Alexander Oberdoerster
	
	Headers for daap requests.
*/

#include <qstring.h>
#include <qstringlist.h>

#include "./daaplib/basic.h"

enum DaapClientType {
    DAAP_CLIENT_UNKNOWN = 0,
    DAAP_CLIENT_ITUNES4X,
    DAAP_CLIENT_ITUNES41,
    DAAP_CLIENT_ITUNES42,
    DAAP_CLIENT_ITUNES45,
    DAAP_CLIENT_ITUNES46,
    DAAP_CLIENT_ITUNES47,
    DAAP_CLIENT_ITUNES48,
    DAAP_CLIENT_ITUNES49,
    DAAP_CLIENT_MFDDAAPCLIENT
};
    
enum DaapRequestType {
	DAAP_REQUEST_NOREQUEST = 0,  
	DAAP_REQUEST_SERVINFO,
	DAAP_REQUEST_CONTCODES,
	DAAP_REQUEST_LOGIN,
	DAAP_REQUEST_LOGOUT,
	DAAP_REQUEST_UPDATE,
	DAAP_REQUEST_DATABASES,
	DAAP_REQUEST_RESOLVE,
	DAAP_REQUEST_BROWSE
};

class DaapRequest
{
    
  public:  
    
    DaapRequest();
    ~DaapRequest();

    void            parseRawMetaContentCodes();

    //
    //  Sets
    //
    
    void            setRequestType(DaapRequestType x){request_type = x;}
    void            setClientType(DaapClientType x){client_type = x;}
    void            setSessionId(u32 x){session_id = x;}
    void            setDatabaseVersion(u32 x){database_version = x;}
    void            setDatabaseDelta(u32 x){database_delta = x;}
    void            setContentType(const QString &x){content_type = x;}
    void            setRawMetaContentCodes(const QStringList &x){raw_meta_content_codes = x;}

    void            setFilter(const QString &x);
    void            setQuery(const QString &x);
    void            setIndex(const QString &x);
        
    //
    //  Gets
    //
    
    DaapRequestType getRequestType(){return request_type;}
    DaapClientType  getClientType(){return client_type;}
    u32             getSessionId(){return session_id;}
    u32             getDatabaseVersion(){return database_version;}
    u32             getDatabaseDelta(){return database_delta;}
    QString         getContentType(){return content_type;}
    QStringList     getRawMetaContentCodes(){return raw_meta_content_codes;}
    u64             getParsedMetaContentCodes(){return parsed_meta_content_codes;}

  private:
  
    DaapRequestType   request_type;
    DaapClientType    client_type;
    u32               session_id;
    u32               database_version;
    u32               database_delta;
    QString           content_type;
    QStringList       raw_meta_content_codes;
    u64               parsed_meta_content_codes;        

    //
    //  We can set and get these, but don't know how they work
    //

    QString filter;
    QString query;
    QString index;           

};


#endif

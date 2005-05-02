#ifndef MDCAPREQUEST_H_
#define MDCAPREQUEST_H_
/*
	mdcaprequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for mdcap requests
*/

class HttpInRequest;

//
//  enum for the basic types of requests an mdcap client can make
//

enum MdcapRequestType {
	MDCAP_REQUEST_NOREQUEST = 0,  
	MDCAP_REQUEST_SERVINFO,
	MDCAP_REQUEST_LOGIN,
	MDCAP_REQUEST_UPDATE,
	MDCAP_REQUEST_CONTAINERS,
	MDCAP_REQUEST_LOGOUT,
	MDCAP_REQUEST_COMMIT_LIST,
//	MDCAP_REQUEST_COMMIT_ITEM,
    MDCAP_REQUEST_REMOVE_LIST,
	MDCAP_REQUEST_SCREWEDUP
};



class MdcapRequest
{
    
  public:  
    
    MdcapRequest();
    ~MdcapRequest();

    void parsePath(HttpInRequest *http_request);
    
    //
    //  Sets
    //
    
    void setRequestType(MdcapRequestType a_type){mdcap_request_type = a_type;}
    
    //
    //  Gets
    //
    
    MdcapRequestType getRequestType(){return mdcap_request_type;}
    int getContainerId(){return container_id;}
    int getListId(){return list_id;}
    int getGeneration(){return generation;}
    int getDelta(){return delta;}

    bool itemRequest(){return item_request;}
    bool listRequest(){return list_request;}

  private:
  
    MdcapRequestType    mdcap_request_type;

    int container_id;
    int list_id;
    int generation;
    int delta;

    bool item_request;
    bool list_request;
};


#endif

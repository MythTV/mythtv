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
	MDCAP_REQUEST_LOGOUT
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

  private:
  
    MdcapRequestType    mdcap_request_type;

};


#endif

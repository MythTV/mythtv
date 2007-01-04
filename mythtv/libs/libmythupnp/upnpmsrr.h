#ifndef UPnpMSRR_H_
#define UPnpMSRR_H_

#include <qdom.h>
#include <qdatetime.h> 

#include "httpserver.h"
#include "mythcontext.h"
#include "eventing.h"
              
class UPnpMSRR;
                          
typedef enum 
{
    MSRR_Unknown                = 0,
    MSRR_IsAuthorized           = 1,
    MSRR_RegisterDevice         = 2,
    MSRR_IsValidated            = 3

} UPnpMSRRMethod;

//////////////////////////////////////////////////////////////////////////////
//
// UPnpMSRR Class Definition
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPnpMSRR : public Eventing
{
    private:

        UPnpMSRRMethod       GetMethod              ( const QString &sURI  );

        void            HandleIsAuthorized         ( HTTPRequest *pRequest );
        void            HandleRegisterDevice       ( HTTPRequest *pRequest );
        void            HandleIsValidated          ( HTTPRequest *pRequest );

    public:
                 UPnpMSRR(); 
        virtual ~UPnpMSRR();

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif

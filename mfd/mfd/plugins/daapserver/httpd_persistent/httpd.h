/*
** Copyright (c) 2002  Hughes Technologies Pty Ltd.  All rights
** reserved.
**
** Copyright (c) 2003  deleet, Alexander Oberdoerster
**
** Terms under which this software may be used or copied are
** provided in the  specific license associated with this product.
**
** Hughes Technologies disclaims all warranties with regard to this
** software, including all implied warranties of merchantability and
** fitness, in no event shall Hughes Technologies be liable for any
** special, indirect or consequential damages or any damages whatsoever
** resulting from loss of use, data or profits, whether in an action of
** contract, negligence or other tortious action, arising out of or in
** connection with the use or performance of this software.
**
**
** $Id$
**
*/

/*
**  libhttpd Header File
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef LIB_HTTPD_H

#define LIB_HTTPD_H 1

#if !defined(__ANSI_PROTO)
#if defined(_WIN32) || defined(__STDC__) || defined(__cplusplus)
#  define __ANSI_PROTO(x)       x
#else
#  define __ANSI_PROTO(x)       ()
#endif
#endif

#ifdef __cplusplus
// extern "C" {
#endif


/***********************************************************************
** Macro Definitions
*/


#define	HTTP_PORT 		80
#define HTTP_MAX_LEN		10240
#define HTTP_MAX_URL		1024
#define HTTP_MAX_HEADERS	1024
#define HTTP_MAX_AUTH		128
#define	HTTP_IP_ADDR_LEN	17
#define	HTTP_TIME_STRING_LEN	40
#define	HTTP_READ_BUF_LEN	4096
#define	HTTP_ANY_ADDR		NULL

#define	HTTP_GET		1
#define	HTTP_POST		2

#define	HTTP_TRUE		1
#define HTTP_FALSE		0

#define	HTTP_FILE		1
#define HTTP_C_FUNCT		2
#define HTTP_MEMBER_FUNCT	3
#define HTTP_STATIC		4
#define HTTP_WILDCARD		5
#define HTTP_C_WILDCARD		6

#define HTTP_403_RESPONSE "<HTML><HEAD><TITLE>403 Permission Denied</TITLE></HEAD>\n<BODY><H1>Access to the request URL was denied!</H1>\n"
#define HTTP_404_RESPONSE "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>The request URL was not found!</H1>\n</BODY></HTML>\n"
#define HTTP_501_RESPONSE "<HTML><HEAD><TITLE>501 Not Implemented</TITLE></HEAD>\n<BODY><H1>Method Not Implemented</H1>\n"
#define HTTP_505_RESPONSE "<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>HTTP Version Not Supported</H1>\n"


#define httpdRequestMethod(s) 		s->request.method
#define httpdRequestPath(s)		s->request.path
#define httpdRequestContentType(s)	s->request.contentType
#define httpdRequestContentLength(s)	s->request.contentLength
#define httpdRequestAcceptEncoding(s)	s->request.acceptEncoding
#define httpdRequestAcceptLanguage(s)	s->request.acceptLanguage
#define httpdRequestAccept(s)		s->request.accept
#define httpdRequestUserAgent(s)	s->request.userAgent
#define httpdRequestClose(s)		s->request.close
#define httpdRequestVersion(s)		s->request.version
#define httpdRequestDaapVersion(s)	s->request.daapVersion
#define httpdRequestContentRange(s)	s->request.contentRange

#define HTTP_ACL_PERMIT		1
#define HTTP_ACL_DENY		2



extern char 	LIBHTTPD_VERSION[],
		LIBHTTPD_VENDOR[];

/***********************************************************************
** Type Definitions
*/

typedef	struct {
	int	method,
		contentLength,
		authLength,
		close,
		contentRange;
	double	daapVersion,
		version;
	char	path[HTTP_MAX_URL],
		userAgent[HTTP_MAX_URL],
		referer[HTTP_MAX_URL],
		ifModified[HTTP_MAX_URL],
		contentType[HTTP_MAX_URL],
		acceptEncoding[HTTP_MAX_URL],
		acceptLanguage[HTTP_MAX_URL],
		accept[HTTP_MAX_URL],
		authUser[HTTP_MAX_AUTH],
		authPassword[HTTP_MAX_AUTH];
} httpReq;


typedef struct _httpd_var{
	char	*name,
		*value;
	struct	_httpd_var 	*nextValue,
				*nextVariable;
} httpVar;

typedef struct _httpd_content{
	char	*name;
	int	type,
		indexFlag;
	void	(*function)(void *);
	char	*data,
		*path;
	int	(*preload)(void *);
	struct	_httpd_content 	*next;
} httpContent;

typedef struct {
	int		responseLength;
	httpContent	*content;
	char		headersSent,
			headers[HTTP_MAX_HEADERS],
			response[HTTP_MAX_URL],
			contentType[HTTP_MAX_URL];
} httpRes;


typedef struct _httpd_dir{
	char	*name;
	struct	_httpd_dir *children,
			*next;
	struct	_httpd_content *entries;
} httpDir;


typedef struct ip_acl_s{
        int     addr;
        char    len,
                action;
        struct  ip_acl_s *next;
} httpAcl;

class Clients; 

struct httpd {
	int	port,
		serverSock,
		clientSock,
		readBufRemain,
		startTime;
	char	clientAddr[HTTP_IP_ADDR_LEN],
		fileBasePath[HTTP_MAX_URL],
		readBuf[HTTP_READ_BUF_LEN + 1],
		*host,
		*readBufPtr;
	Clients	*clients;
	httpReq	request;
	httpRes response;
	httpVar	*variables;
	httpDir	*content;
	httpAcl	*defaultAcl;
	FILE	*accessLog,
		*errorLog;
	void	(*siteFunction)(httpd *server, void *data);
	void 	*callbackData;
}; 



/***********************************************************************
** Function Prototypes
*/


int httpdAddCContent __ANSI_PROTO((httpd*,char*,char*,int,int(*)(httpd *),void(*)(httpd *)));
int httpdAddFileContent __ANSI_PROTO((httpd*,char*,char*,int,int(*)(httpd *),char*));
int httpdAddStaticContent __ANSI_PROTO((httpd*,char*,char*,int,int(*)(httpd *),char*));
int httpdAddWildcardContent __ANSI_PROTO((httpd*,char*,int(*)(httpd *),char*));
int httpdAddCWildcardContent __ANSI_PROTO((httpd*,char*,int(*)(httpd *),void(*)(httpd *)));
void httpdAddCSiteContent __ANSI_PROTO((httpd*,void(*)(httpd *,void *),void*));
int httpdAddVariable __ANSI_PROTO((httpd*,char*, char*));
int httpdGetConnection __ANSI_PROTO((httpd*, struct timeval*));
int httpdReadRequest __ANSI_PROTO((httpd*));
int httpdCheckAcl __ANSI_PROTO((httpd*, httpAcl*));

char *httpdRequestMethodName __ANSI_PROTO((httpd*));
char *httpdUrlEncode __ANSI_PROTO((char *));

void httpdAddHeader __ANSI_PROTO((httpd*, char*));
void httpdSetContentType __ANSI_PROTO((httpd*, char*));
void httpdSetResponse __ANSI_PROTO((httpd*, char*));
void httpdSuspendRequest __ANSI_PROTO((httpd*));
void httpdEndRequest __ANSI_PROTO((httpd*));

httpd *httpdCreate __ANSI_PROTO((char*, int));
void httpdFreeVariables __ANSI_PROTO((httpd*));
void httpdDumpVariables __ANSI_PROTO((httpd*));
void httpdWrite __ANSI_PROTO((httpd*, char*, int));
void httpdOutput __ANSI_PROTO((httpd*, char*));
void httpdPrintf __ANSI_PROTO((httpd*, char*, ...));
void httpdSendFile __ANSI_PROTO((httpd *, char *, int));
void httpdProcessRequest __ANSI_PROTO((httpd*));
void httpdSend204 __ANSI_PROTO((httpd*));
void httpdSend400 __ANSI_PROTO((httpd*));
void httpdSend403 __ANSI_PROTO((httpd*));
void httpdSendHeaders __ANSI_PROTO((httpd*, int, int, int));
void httpdSetFileBase __ANSI_PROTO((httpd*, char*));
void httpdSetCookie __ANSI_PROTO((httpd*, char*, char*));

void httpdSetErrorLog __ANSI_PROTO((httpd*, FILE*));
void httpdSetAccessLog __ANSI_PROTO((httpd*, FILE*));
void httpdSetDefaultAcl __ANSI_PROTO((httpd*, httpAcl*));

httpVar	*httpdGetVariableByName __ANSI_PROTO((httpd*, char*));
httpVar	*httpdGetVariableByPrefix __ANSI_PROTO((httpd*, char*));
httpVar	*httpdGetVariableByPrefixedName __ANSI_PROTO((httpd*, char*, char*));
httpVar *httpdGetNextVariableByPrefix __ANSI_PROTO((httpVar*, char*));

httpAcl *httpdAddAcl __ANSI_PROTO((httpd*, httpAcl*, char*, int));

int httpdAuthenticate __ANSI_PROTO((httpd*, char*));
int httpdForceAuthenticate __ANSI_PROTO((httpd*, char*));

int httpdSelectLoop( httpd *server, struct timeval& timeout_ );

/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
//}
#endif /* __cplusplus */
#endif /* file inclusion */


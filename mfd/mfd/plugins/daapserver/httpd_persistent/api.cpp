/*
** Copyright (c) 2002  Hughes Technologies Pty Ltd.  All rights
** reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <unistd.h> 
#include <sys/file.h>
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h>
#include <sys/socket.h> 
#include <netdb.h>
#endif

#ifdef __sgi__
#include <snprintf.h>
#endif

#include "config.h"
#include "select.h"
#include "httpd.h"
#include "httpd_priv.h"

#ifdef HAVE_STDARG_H
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif



char *httpdUrlEncode( 
        char    *str )
{
        char    *new_;
        char    *cp;

        new_ = (char *)_httpd_escape(str);
	if (new_ == NULL)
	{
		return(NULL);
	}
        cp = new_;
        while(*cp)
        {
                if (*cp == ' ')
                        *cp = '+';
                cp++;
        }
	return(new_);
}



char *httpdRequestMethodName(
	httpd	*server)
{
	static	char	tmpBuf[255];

	switch(server->request.method)
	{
		case HTTP_GET: return("GET");
		case HTTP_POST: return("POST");
		default: 
			snprintf(tmpBuf,255,"Invalid method '%d'", 
				server->request.method);
			return(tmpBuf);
	}
}


httpVar *httpdGetVariableByName(
	httpd	*server,
	char	*name)
{
	httpVar	*curVar;

	curVar = server->variables;
	while(curVar)
	{
		if (strcmp(curVar->name, name) == 0)
			return(curVar);
		curVar = curVar->nextVariable;
	}
	return(NULL);
}



httpVar *httpdGetVariableByPrefix(
	httpd	*server,
	char	*prefix)
{
	httpVar	*curVar;

	if (prefix == NULL)
		return(server->variables);
	curVar = server->variables;
	while(curVar)
	{
		if (strncmp(curVar->name, prefix, strlen(prefix)) == 0)
			return(curVar);
		curVar = curVar->nextVariable;
	}
	return(NULL);
}


httpVar *httpdGetVariableByPrefixedName(
	httpd	*server,
	char	*prefix,
	char	*name)
{
	httpVar	*curVar;
	int	prefixLen;

	if (prefix == NULL)
		return(server->variables);
	curVar = server->variables;
	prefixLen = strlen(prefix);
	while(curVar)
	{
		if (strncmp(curVar->name, prefix, prefixLen) == 0 &&
			strcmp(curVar->name + prefixLen, name) == 0)
		{
			return(curVar);
		}
		curVar = curVar->nextVariable;
	}
	return(NULL);
}


httpVar *httpdGetNextVariableByPrefix(
	httpVar	*curVar,
	char	*prefix)
{
	if(curVar)
		curVar = curVar->nextVariable;
	while(curVar)
	{
		if (strncmp(curVar->name, prefix, strlen(prefix)) == 0)
			return(curVar);
		curVar = curVar->nextVariable;
	}
	return(NULL);
}


int httpdAddVariable(
	httpd	*server,
	char	*name,
	char	*value)
{
	httpVar *curVar, *lastVar, *newVar;

	while(*name == ' ' || *name == '\t')
		name++;
	newVar = (httpVar*)malloc(sizeof(httpVar));
	bzero(newVar, sizeof(httpVar));
	newVar->name = strdup(name);
	newVar->value = strdup(value);
	lastVar = NULL;
	curVar = server->variables;
	while(curVar)
	{
		if (strcmp(curVar->name, name) != 0)
		{
			lastVar = curVar;
			curVar = curVar->nextVariable;
			continue;
		}
		while(curVar)
		{
			lastVar = curVar;
			curVar = curVar->nextValue;
		}
		lastVar->nextValue = newVar;
		return(0);
	}
	if (lastVar)
		lastVar->nextVariable = newVar;
	else
		server->variables = newVar;
	return(0);
}

httpd *httpdCreate(
	char	*host,
	int	port)
{
	httpd	*new_;
	int	sock,
		opt;
        struct  sockaddr_in     addr;

	/*
	** Create the handle and setup it's basic config
	*/
	new_ = (httpd*)malloc(sizeof(httpd));
	if (new_ == NULL)
		return(NULL);
	bzero(new_, sizeof(httpd));
	new_->port = port;
	if (host == HTTP_ANY_ADDR)
		new_->host = HTTP_ANY_ADDR;
	else
		new_->host = strdup(host);
	new_->content = (httpDir*)malloc(sizeof(httpDir));
	bzero(new_->content,sizeof(httpDir));
	new_->content->name = strdup("");

	/*
	** Setup the socket
	*/
#ifdef _WIN32
	{ 
	WORD 	wVersionRequested;
	WSADATA wsaData;
	int 	err;

	wVersionRequested = MAKEWORD( 2, 2 );

	err = WSAStartup( wVersionRequested, &wsaData );
	
	/* Found a usable winsock dll? */
	if( err != 0 ) 
	   return NULL;

	/* 
	** Confirm that the WinSock DLL supports 2.2.
	** Note that if the DLL supports versions greater 
	** than 2.2 in addition to 2.2, it will still return
	** 2.2 in wVersion since that is the version we
	** requested.
	*/

	if( LOBYTE( wsaData.wVersion ) != 2 || 
	    HIBYTE( wsaData.wVersion ) != 2 ) {

		/* 
		** Tell the user that we could not find a usable
		** WinSock DLL.
		*/
		WSACleanup( );
		return NULL;
	}

	/* The WinSock DLL is acceptable. Proceed. */
 	}
#endif

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock  < 0)
	{
		free(new_);
		return(NULL);
	}
#	ifdef SO_REUSEADDR
	opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt,sizeof(int));
#	endif
	new_->serverSock = sock;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	if (new_->host == HTTP_ANY_ADDR)
	{
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		addr.sin_addr.s_addr = inet_addr(new_->host);
	}
	addr.sin_port = htons((u_short)new_->port);
	if (_httpd_setnonblocking(sock) < 0)
	{
		close(sock);
		free(new_);
		return(NULL);
	}
	if (bind(sock,(struct sockaddr *)&addr,sizeof(addr)) <0)
	{
		close(sock);
		free(new_);
		return(NULL);
	}
	listen(sock, 128);
	new_->startTime = time(NULL);
	new_->clients = new Clients();
	return(new_);
}

	
void httpdDestroy(
	httpd	*server)
{
	if (server == NULL)
		return;
	if (server->host)
		free(server->host);
	free(server);
}

int httpdReadRequest(
	httpd	*server)
{
	static	char	buf[HTTP_MAX_LEN];
	int	count,
		inHeaders;
	char	*cp, *cp2;
	
	/*
	** Setup for a standard response
	*/
	strcpy(server->response.headers,
		"Server: Hughes Technologies Embedded Server (persistent patch)\r\n"); 
	strcpy(server->response.contentType, "text/html");
	strcpy(server->response.response,"200 OK\r\n");
	server->response.headersSent = 0;
	server->request.contentRange=0;

	/*
	** Read the request
	*/
	count = 0;
	inHeaders = 1;
	while(server->clients->readLine((const int) server->clientSock, (char*) buf, (int)HTTP_MAX_LEN) > 0)
	{
		count++;

		/*
		** Special case for the first line.  Scan the request
		** method and path etc
		*/
		if (count == 1)
		{
			/*
			** First line.  Scan the request info
			*/
			cp = cp2 = buf;
			while(isalpha(*cp2))
				cp2++;
			*cp2 = 0; 
			if (strncasecmp(cp,"GET",3) == 0)
				server->request.method = HTTP_GET;
			if (server->request.method == 0)
			{
				_httpd_send501(server);
				_httpd_writeErrorLog(server,LEVEL_ERROR, "Invalid method received:");
				_httpd_writeErrorLog(server,LEVEL_ERROR, cp);				return(-1);
			}
			cp = cp2+1;
			while(*cp == ' ')
				cp++;
			cp2 = cp;
			while(*cp2 != ' ' && *cp2 != 0)
				cp2++;
			*cp2 = 0;
			strncpy(server->request.path,cp,HTTP_MAX_URL);
			_httpd_sanitiseUrl(server->request.path);
			cp = cp2+1;
			while(*cp == ' ' && *cp != 0)
				cp++;
			cp2 = cp;
			while(*cp2 != '/' && *cp2 != 0)
				cp2++;
			*cp2 = 0;
			server->request.version = 1.0;
			if (strcasecmp(cp,"HTTP") == 0) {
				cp = cp2+1;
				if ( (server->request.version = strtod(cp, NULL)) == 0)
					server->request.version = 1.0;
			}
			if (server->request.version > 1.1)
			{
				_httpd_send505(server);
				_httpd_writeErrorLog(server,LEVEL_ERROR, "Invalid httpd version received");
				return(-1);
			}
			continue;
		}

		/*
		** Process the headers
		*/
		if (inHeaders)
		{
			if (*buf == 0)
			{
				/*
				** End of headers.  Continue if there's
				** data to read
				*/
				if (server->request.contentLength == 0)
					break;
				inHeaders = 0;
				break;
			}
			if (strncasecmp(buf,"Cookie: ",7) == 0)
			{
				char	*var,
					*val,
					*end;

				var = index(buf,':');
				while(var)
				{
					var++;
					val = index(var, '=');
					*val = 0;
					val++;
					end = index(val,';');
					if(end)
						*end = 0;
					httpdAddVariable(server, var, val);
					var = end;
				}
			}
			if (strncasecmp(buf,"Authorization: ",15) == 0)
			{
				cp = index(buf,':') + 2;
				if (strncmp(cp,"Basic ", 6) != 0)
				{
					/* Unknown auth method */
				}
				else
				{
					char 	authBuf[100];

					cp = index(cp,' ') + 1;
					_httpd_decode(cp, authBuf, 100);
					server->request.authLength = 
						strlen(authBuf);
					cp = index(authBuf,':');
					if (cp)
					{
						*cp = 0;
						strncpy(
						   server->request.authPassword,
						   cp+1, HTTP_MAX_AUTH);
					}
					strncpy(server->request.authUser, 
						authBuf, HTTP_MAX_AUTH);
				}
			}
			if (strncasecmp(buf,"Connection: ",12) == 0)
			{
				cp = index(buf,':') + 1;
				while( isspace( *cp ) ) 
					cp++;
				
				if( strncasecmp(cp,"Close",5) == 0)
				{
					server->request.close = true;
				}
			}
			if (strncasecmp(buf,"Referer: ",9) == 0)
			{
				cp = index(buf,':') + 2;
				if(cp)
				{
					strncpy(server->request.referer,cp,
						HTTP_MAX_URL);
				}
			}
			if (strncasecmp(buf,"If-Modified-Since: ",19) == 0)
			{
				cp = index(buf,':') + 2;
				if(cp)
				{
					strncpy(server->request.ifModified,cp,
						HTTP_MAX_URL);
					cp = index(server->request.ifModified,
						';');
					if (cp)
						*cp = 0;
				}
			}
			if (strncasecmp(buf,"Content-Type: ",14) == 0)
			{
				cp = index(buf,':') + 2;
				if(cp)
				{
					strncpy(server->request.contentType,cp,
						HTTP_MAX_URL);
				}
			}
			if (strncasecmp(buf,"Content-Length: ",16) == 0)
			{
				cp = index(buf,':') + 2;
				if(cp)
					server->request.contentLength=atoi(cp);
			}
			if (strncasecmp(buf,"Accept: ",8) == 0)
			{
				cp = index(buf,':') + 2;
				if(cp)
				{
					strncpy(server->request.accept,cp,
						HTTP_MAX_URL);
				}
			}
			if (strncasecmp(buf,"Accept-Encoding: ",17) == 0)
			{
				cp = index(buf,':') + 2;
				if(cp)
				{
					strncpy(server->request.acceptEncoding,cp,
						HTTP_MAX_URL);
				}
			}
			if (strncasecmp(buf,"Accept-Language: ",17) == 0)
			{
				cp = index(buf,':') + 2;
				if(cp)
				{
					strncpy(server->request.acceptLanguage,cp,
						HTTP_MAX_URL);
				}
			}
			if (strncasecmp(buf,"User-Agent: ",12) == 0)
			{
				cp = index(buf,':') + 2;
				if(cp)
				{
					strncpy(server->request.userAgent,cp,
						HTTP_MAX_URL);
				}
			}
			if (strncasecmp(buf,"Client-DAAP-Version: ",21) == 0)
			{
				cp = index(buf,':') + 2;
				if(cp)
					server->request.daapVersion=strtol(cp,NULL,10);
			}
			if (strncasecmp(buf,"range: ",7) == 0)
			{
				// we're not parsing the 'range:' request,
				// instead we assume a very specific (but the most common) format:
				// range: bytes=532543-
				cp = index(buf,'=') + 1;
				if(cp)
					server->request.contentRange=atoi(cp);
			} 
			continue;
		}
	}
	
	/*
	** Process and POST data
	*/

	/*
	 * FIX THIS
	 * We only support GET at the moment and we assume we don't 
	 * get any other methods with content, because that would
	 * get quite hairy with HTTP 1.1 pipelining.
	 *
	 * bottom line: pipelined commands with content throw us off 
	 * the tracks.
	 *
	if (server->request.contentLength > 0)
	{
		bzero(buf, HTTP_MAX_LEN);
		
		server->clients->readBuf( server->clientSock, buf, server->request.contentLength);
		_httpd_storeData(server, buf);
		
	}
	*/

	/*
	** Process any URL data
	*/
	cp = index(server->request.path,'?');
	if (cp != NULL)
	{
		*cp = 0;
		cp++;
		_httpd_storeData(server, cp);
	}
	return(0);
}


void httpdSuspendRequest(
	httpd	*server)
{
	_httpd_freeVariables(server->variables);
	server->variables = NULL;
	bzero(&server->request, sizeof(server->request));
}


void httpdEndRequest(
	httpd	*server)
{
	_httpd_freeVariables(server->variables);
	server->variables = NULL;
//	shutdown(server->clientSock,2);
//	close(server->clientSock);
//	server->clients->erase(server->clientSock);
	server->clients->finish(server->clientSock);
	bzero(&server->request, sizeof(server->request));
}


void httpdFreeVariables(
        httpd   *server)
{
        _httpd_freeVariables(server->variables);
}



void httpdDumpVariables(
	httpd	*server)
{
	httpVar	*curVar,
		*curVal;

	curVar = server->variables;
	while(curVar)
	{
		printf("Variable '%s'\n", curVar->name);
		curVal = curVar;
		while(curVal)
		{
			printf("\t= '%s'\n",curVal->value);
			curVal = curVal->nextValue;
		}
		curVar = curVar->nextVariable;
	}
}

void httpdSetFileBase(
	httpd	*server,
	char	*path)
{
	strncpy(server->fileBasePath, path, HTTP_MAX_URL);
}


int httpdAddFileContent(
	httpd	*server,
	char	*dir,
	char	*name,
	int	indexFlag,
	int	(*preload)(httpd *server),
	char	*path)
{
	httpDir	*dirPtr;
	httpContent *newEntry;

	dirPtr = _httpd_findContentDir(server, dir, HTTP_TRUE);
	newEntry =  (httpContent*)malloc(sizeof(httpContent));
	if (newEntry == NULL)
		return(-1);
	bzero(newEntry,sizeof(httpContent));
	newEntry->name = strdup(name);
	newEntry->type = HTTP_FILE;
	newEntry->indexFlag = indexFlag;
	newEntry->preload = (int(*)(void*))preload;
	newEntry->next = dirPtr->entries;
	dirPtr->entries = newEntry;
	if (*path == '/')
	{
		/* Absolute path */
		newEntry->path = strdup(path);
	}
	else
	{
		/* Path relative to base path */
		newEntry->path = (char*) malloc(strlen(server->fileBasePath) +
			strlen(path) + 2);
		snprintf(newEntry->path, HTTP_MAX_URL, "%s/%s",
			server->fileBasePath, path);
	}
	return(0);
}



int httpdAddWildcardContent(
	httpd	*server,
	char	*dir,
	int     indexflag,
	int	(*preload)(httpd *server),
	char	*path)
{
    indexflag = indexflag;  //-Wall -W
	httpDir	*dirPtr;
	httpContent *newEntry;

	dirPtr = _httpd_findContentDir(server, dir, HTTP_TRUE);
	newEntry = (httpContent *) malloc(sizeof(httpContent));
	if (newEntry == NULL)
		return(-1);
	bzero(newEntry,sizeof(httpContent));
	newEntry->name = NULL;
	newEntry->type = HTTP_WILDCARD;
	newEntry->indexFlag = HTTP_FALSE;
	newEntry->preload = (int(*)(void*)) preload;
	newEntry->next = dirPtr->entries;
	dirPtr->entries = newEntry;
	if (*path == '/')
	{
		/* Absolute path */
		newEntry->path = strdup(path);
	}
	else
	{
		/* Path relative to base path */
		newEntry->path = (char*)malloc(strlen(server->fileBasePath) +
			strlen(path) + 2);
		snprintf(newEntry->path, HTTP_MAX_URL, "%s/%s",
			server->fileBasePath, path);
	}
	return(0);
}




int httpdAddCContent(
	httpd	*server,
	char	*dir,
	char	*name,
	int     indexFlag,
	int	(*preload)(httpd *server),
	void	(*function)(httpd *server))
{
	httpDir	*dirPtr;
	httpContent *newEntry;

	dirPtr = _httpd_findContentDir(server, dir, HTTP_TRUE);
	newEntry = (httpContent *) malloc(sizeof(httpContent));
	if (newEntry == NULL)
		return(-1);
	bzero(newEntry,sizeof(httpContent));
	newEntry->name = strdup(name);
	newEntry->type = HTTP_C_FUNCT;
	newEntry->indexFlag = indexFlag; 
	newEntry->function = (void(*)(void*))function;
	newEntry->preload = (int(*)(void*))preload;
	newEntry->next = dirPtr->entries;
	dirPtr->entries = newEntry;
	return(0);
}


int httpdAddCWildcardContent(
	httpd	*server,
	char	*dir,
	int	(*preload)(httpd *server),
	void	(*function)(httpd *server))
{
	httpDir	*dirPtr;
	httpContent *newEntry;

	dirPtr = _httpd_findContentDir(server, dir, HTTP_TRUE);
	newEntry = (httpContent*) malloc(sizeof(httpContent));
	if (newEntry == NULL)
		return(-1);
	bzero(newEntry,sizeof(httpContent));
	newEntry->name = NULL;
	newEntry->type = HTTP_C_WILDCARD;
	newEntry->indexFlag = HTTP_TRUE;
	newEntry->function = (void(*)(void*))function;
	newEntry->preload = (int(*)(void*))preload;
	newEntry->next = dirPtr->entries;
	dirPtr->entries = newEntry;
	return(0);
}


void httpdAddCSiteContent(
	httpd	*server,
	void	(*function)(httpd *server, void *data),
	void	*data)
{
	server->siteFunction = function;
	server->callbackData = data;
}


int httpdAddStaticContent(
	httpd	*server,
	char	*dir,
	char	*name,
	int	indexFlag,
	int	(*preload)(httpd *server),
	char	*data)
{
	httpDir	*dirPtr;
	httpContent *newEntry;

	dirPtr = _httpd_findContentDir(server, dir, HTTP_TRUE);
	newEntry = (httpContent *)malloc(sizeof(httpContent));
	if (newEntry == NULL)
		return(-1);
	bzero(newEntry,sizeof(httpContent));
	newEntry->name = strdup(name);
	newEntry->type = HTTP_STATIC;
	newEntry->indexFlag = indexFlag;
	newEntry->data = data;
	newEntry->preload = (int(*)(void*))preload;
	newEntry->next = dirPtr->entries;
	dirPtr->entries = newEntry;
	return(0);
}

void httpdSend204(
	httpd	*server)
{
	_httpd_send204(server);
}

void httpdSend400(
	httpd	*server)
{
	_httpd_send400(server);
}

void httpdSend403(
	httpd	*server)
{
	_httpd_send403(server);
}

void httpdSendHeaders(
	httpd	*server,
	int 	len)
{
	_httpd_sendHeaders(server, len, 0, 0);
}

void httpdSetResponse(
	httpd	*server,
	char	*msg)
{
	strncpy(server->response.response, msg, HTTP_MAX_URL);
}

void httpdSetContentType(
	httpd	*server,
	char	*type)
{
	strcpy(server->response.contentType, type);
}


void httpdAddHeader(
	httpd	*server,
	char	*msg)
{
	strcat(server->response.headers,msg);
	if (msg[strlen(msg) - 1] != '\n')
		strcat(server->response.headers,"\015\n");
}

void httpdSetCookie(
	httpd	*server,
	char	*name,
	char	*value)
{
	char	buf[HTTP_MAX_URL];

	snprintf(buf,HTTP_MAX_URL, "Set-Cookie: %s=%s; path=/;", name, value);
	httpdAddHeader(server,buf);
}

void httpdWrite(
	httpd	*server,
	char	*msg,
	int 	len)
{
	server->response.responseLength += len;
	httpdSendHeaders(server, len);
	server->clients->doWrite( server->clientSock, msg, len);
}

void httpdOutput(
	httpd	*server,
	char	*msg)
{
	char	buf[HTTP_MAX_LEN],
		varName[80],
		*src,
		*dest;
	int	count;

	src = msg;
	dest = buf;
	count = 0;
	while(*src && count < HTTP_MAX_LEN)
	{
		if (*src == '$')
		{
			char	*cp,
				*tmp;
			int	count2;
			httpVar	*curVar;

			tmp = src + 1;
			cp = varName;
			count2 = 0;
			while(*tmp&&(isalnum(*tmp)||*tmp == '_')&&count2 < 80)
			{
				*cp++ = *tmp++;
				count2++;
			}
			*cp = 0;
			curVar = httpdGetVariableByName(server,varName);
			if (curVar)
			{
				strcpy(dest, curVar->value);
				dest = dest + strlen(dest);
				count += strlen(dest);
			}
			else
			{
				*dest++ = '$';
				strcpy(dest, varName);
				dest += strlen(varName);
				count += 1 + strlen(varName);
			}
			src = src + strlen(varName) + 1;
			continue;
		}
		*dest++ = *src++;
		count++;
	}	
	*dest = 0;
	server->response.responseLength += strlen(buf);
	if (server->response.headersSent == 0)
		httpdSendHeaders(server, strlen(buf));
	server->clients->doWrite( server->clientSock, buf, strlen(buf));
}

void httpdSendFile( 
	httpd 	*server,
	char 	*path,
	int	skip = 0)
{
	_httpd_sendFile(server, path, skip);
}


#ifdef HAVE_STDARG_H
void httpdPrintf(httpd *server, char *fmt, ...)
{
#else
void httpdPrintf(va_alist)
        va_dcl
{
        httpd		*server;
        char		*fmt;
#endif
        va_list         args;
	char		buf[HTTP_MAX_LEN];

#ifdef HAVE_STDARG_H
        va_start(args, fmt);
#else
        va_start(args);
        server = (httpd *) va_arg(args, httpd * );
        fmt = (char *) va_arg(args, char *);
#endif
	if (server->response.headersSent == 0)
		httpdSendHeaders(server, strlen(buf));
	vsnprintf(buf, HTTP_MAX_LEN, fmt, args);
	server->response.responseLength += strlen(buf);
	server->clients->doWrite( server->clientSock, buf, strlen(buf));
}




void httpdProcessRequest(
	httpd	*server)
{
	char	dirName[HTTP_MAX_URL],
		entryName[HTTP_MAX_URL],
		*cp;
	httpDir	*dir;
	httpContent *entry;
	
	server->response.responseLength = 0;
	strncpy(dirName, httpdRequestPath(server), HTTP_MAX_URL);
	cp = rindex(dirName, '/');
	if (cp == NULL)
	{
		printf("Invalid request path '%s'\n",dirName);
		return;
	}
	strncpy(entryName, cp + 1, HTTP_MAX_URL);
	if (cp != dirName)
		*cp = 0;
	else
		*(cp+1) = 0;

	if( server->siteFunction != NULL) {
		(server->siteFunction)(server, server->callbackData);
		_httpd_writeAccessLog(server);
		return;
	}
	
	dir = _httpd_findContentDir(server, dirName, HTTP_FALSE);
	if (dir == NULL)
	{
		_httpd_send404(server);
		_httpd_writeAccessLog(server);
		return;
	}
	entry = _httpd_findContentEntry(server, dir, entryName);
	if (entry == NULL)
	{
		_httpd_send404(server);
		_httpd_writeAccessLog(server);
		return;
	}
	if (entry->preload)
	{
		if ((entry->preload)(server) < 0)
		{
			_httpd_writeAccessLog(server);
			return;
		}
	}
	switch(entry->type)
	{
		case HTTP_C_FUNCT:
		case HTTP_C_WILDCARD:
			(entry->function)(server);
			break;

		case HTTP_STATIC:
			_httpd_sendStatic(server, entry->data);
			break;

		case HTTP_FILE:
			_httpd_sendFile(server, entry->path, server->request.contentRange);
			break;

		case HTTP_WILDCARD:
			if (_httpd_sendDirectoryEntry(server,entry,entryName)<0)
			{
				_httpd_send404(server);
			}
			break;
	}
	_httpd_writeAccessLog(server);
}

void httpdSetAccessLog(
	httpd	*server,
	FILE	*fp)
{
	server->accessLog = fp;
}

void httpdSetErrorLog(
	httpd	*server,
	FILE	*fp)
{
	server->errorLog = fp;
}

int httpdAuthenticate(
	httpd	*server,
	char	*realm)
{
	char	buffer[255];

	if (server->request.authLength == 0)
	{
		httpdSetResponse(server, "401 Please Authenticate\015\n");
		snprintf(buffer,sizeof(buffer), 
			"WWW-Authenticate: Basic realm=\"%s\"\015\n", realm);
		httpdAddHeader(server, buffer);
		httpdOutput(server,"\n");
	}
	return server->request.authLength;
}


int httpdForceAuthenticate(
	httpd	*server,
	char	*realm)
{
	char	buffer[255];

	httpdSetResponse(server, "401 Please Authenticate\015\n");
	snprintf(buffer,sizeof(buffer), 
		"WWW-Authenticate: Basic realm=\"%s\"\015\n", realm);
	httpdAddHeader(server, buffer);
	httpdOutput(server,"\n");
	return server->request.authLength;
}

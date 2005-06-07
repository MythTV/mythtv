#ifndef DAAPINPUT_H_
#define DAAPINPUT_H_
/*
	daapinput.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Magical little class that makes remote daap resources look like local
	direct access files
*/

#include <qvaluevector.h>
#include <qiodevice.h>
#include <qsocketdevice.h>
#include <qurl.h>
#include <qstring.h>
#include <qdict.h>

#include "mfd_plugin.h"
#include "httpheader.h"

#include "../daapclient/server_types.h"

class DaapInput: public QIODevice
{

  public:

    DaapInput(
                MFDServicePlugin*, 
                QUrl, 
                DaapServerType l_daap_server_type = DAAP_SERVER_MYTH, 
                int l_payload_max_size = 262400
             );
    ~DaapInput();

    bool              open(int mode );
    Q_LONG            readBlock( char *data, long unsigned int maxlen );
    Q_ULONG           size() const;
    unsigned long int at() const;
    bool              at(long unsigned int );
    bool              reallySeek(long unsigned int );
    void              close();
    bool              isOpen() const;
    int               status();
    void              eatThroughHeadersAndGetToPayload();
    int               readLine(int *parse_point, char *parsing_buffer, char *raw_response, int raw_length);    
    bool              isDirectAccess(){return true;}
    void              log(const QString &log_message, int verbosity);
    void              warning(const QString &error_message);
    bool              fillSomePayload();

    //
    //  Things we have to define but don't ever use
    //

    virtual Q_LONG  writeBlock( const char*, long unsigned int){return 0;}
    virtual void    flush(){;}
    virtual int     getch(){ return 0;}
    virtual int     putch(int){return 0;}
    virtual int     ungetch(int){return 0;}

        
  private:
  
    QUrl                my_url;
    QString             my_host;
    int                 my_port;
    QString             my_path_and_query;
    bool                all_is_well;
    QSocketDevice       *socket_to_daap_server;
    bool                connected;
    QDict<HttpHeader>   headers;
    std::deque<char>    payload;
    uint                payload_size;
    uint                total_possible_range;
    uint                range_begin;
    uint                range_end;
    int                 payload_bytes_read;
    uint                fake_seek_position;
    MFDServicePlugin    *parent;
    int                 connection_count;
    DaapServerType      daap_server_type;
    int                 payload_max_size;
};

#endif  // daapinput_h_

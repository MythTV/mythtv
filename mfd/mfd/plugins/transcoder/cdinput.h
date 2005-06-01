#ifndef CDINPUT_H_
#define CDINPUT_H_
/*
	cdinput.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Access CD audio data (with cdparanoia) via something that inherits from
	QIODevice
	
*/

#include <qiodevice.h>
#include <qurl.h>

#include <cdaudio.h>
extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}



class CdInput: public QIODevice
{

  public:

    CdInput(QUrl the_url);
    ~CdInput();

    bool              open(int mode=IO_ReadOnly);
    void              close();
    Q_ULONG           size() const;
    Q_LONG            readBlock( char *data, long unsigned int maxlen );

    //
    //  Things we have to define but don't ever use
    //

    virtual Q_LONG  writeBlock( const char*, long unsigned int){return 0;}
    virtual void    flush(){;}
    virtual int     getch(){ return 0;}
    virtual int     putch(int){return 0;}
    virtual int     ungetch(int){return 0;}

/*

    unsigned long int at() const;
    bool              at(long unsigned int );
    bool              isOpen() const;
    int               status();
    void              eatThroughHeadersAndGetToPayload();
    int               readLine(int *parse_point, char *parsing_buffer, char *raw_response, int raw_length);    
    bool              isDirectAccess(){return true;}
    void              log(const QString &log_message, int verbosity);
    void              warning(const QString &error_message);


*/        
  private:
  
    QUrl                url;
    cdrom_drive        *device;
    cdrom_paranoia     *paranoia;
    long int            start;
    long int            curpos;
    long int            end;
/*
    QString             my_host;
    int                 my_port;
    QString             my_path_and_query;
    bool                all_is_well;
    QSocketDevice       *socket_to_daap_server;
    bool                connected;
    QDict<HttpHeader>   headers;
    QValueVector<char>  payload;
    uint                payload_size;
    uint                total_possible_range;
    uint                range_begin;
    uint                range_end;
    int                 payload_bytes_read;
    uint                fake_seek_position;
    MFDServicePlugin    *parent;
    int                 connection_count;
    DaapServerType      daap_server_type;
*/
};

#endif  // cdinput_h_

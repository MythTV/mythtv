#ifndef DAAPINPUT_H_
#define DAAPINPUT_H_
/*
	daapinput.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Magical little class that makes remote daap resources look like local
	direct access files
*/

#include <qsocketdevice.h>
#include <qurl.h>
#include <qstring.h>

class DaapInput: public QSocketDevice
{

  public:

    DaapInput(QUrl);
    ~DaapInput();

    bool    open( int mode );
    int     readBlock( char *data, uint maxlen );

    //bool    isDirectAccess();

    uint    size();
    int     at();
    bool    at( int );
    void    close();
    bool    isOpen();
    int     status();
        
  private:
  
    QUrl    my_url;
    QString my_host;
    int     my_port;
    bool    connected;
    bool    all_is_well;

};

#endif  // daapinput_h_

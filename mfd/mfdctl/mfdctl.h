#ifndef MFDCTL_H_
#define MFDCTL_H_
/*
	mfdctl.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for a tiny little program to
	start, stop, restart, reload the mfd

*/

#include <iostream>
using namespace std;

#include <qobject.h>
#include <qsocket.h>

class MFDCTL : public QObject
{

  Q_OBJECT

  public:
  
    MFDCTL(int port_number, int logging_verbosity, QString host_name, QString command);
    ~MFDCTL();
    
    bool keepRunning(){return keep_running;}
    bool anyProblems(){return problems;}

  public slots:
  
    void socketConnected();
    void socketError(int what_error); 
    void readFromServer();
           
  private:
  
    int     port;
    int     verbosity;
    QString host;
    QString mode;
    QSocket *my_client_socket;
    bool    keep_running;
    bool    problems;
};


#endif  // mfdctl_h_


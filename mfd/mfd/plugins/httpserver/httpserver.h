#ifndef HTTPSERVER_H_
#define HTTPSERVER_H_
/*
	httpserver.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the http server
*/

#include <iostream>
using namespace std; 

#include "mfd_plugin.h"

class ClientHttpServer: public MFDHttpPlugin
{

  public:

    ClientHttpServer(MFD *owner, int identity);
    ~ClientHttpServer();

    void    run();
    void    readFromMFD();
    void    addMHttpServer(QString server_address, uint server_port, QString service_name);
    void    removeMHttpServer(QString server_address, uint server_port, QString service_name);
    void    handleIncoming(HttpRequest *http_request, int client_id);
    
    //
    //  Stuff that builds the HTML
    //

    void    addTopHTML(HttpRequest *http_request);
    void    startCoreTable(HttpRequest *http_request);
    void    listMFDs(HttpRequest *http_request);
    void    listSections(HttpRequest *http_request, const QString &branch_one);
    void    showCurrentSection(HttpRequest *http_request, const QString &branch_one, const QString &branch_two);
    void    showPlaylists(HttpRequest *http_request);
    void    showTracks(HttpRequest *http_request);
    void    endCoreTable(HttpRequest *http_request);
    void    addBottomHTML(HttpRequest *http_request);     

    //
    //  Stuff to do stuff (well that's helpful)
    //
    
    void    playTrack(int which_container, int which_track);
    void    playPlaylist(int which_container, int which_playlist);
    void    stopAudio();
        
  private:
  
    QString         my_hostname;
    QSocketDevice   *client_socket_to_mfd;
    QSocketDevice   *client_socket_to_audio;
    int             core_table_columns;
    MetadataServer  *metadata_server;
    
};

#endif  // httpserver_h_

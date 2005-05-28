#ifndef TRANSCODER_H_
#define TRANSCODER_H_
/*
	transcoder.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the universal transcoder
*/

#include <iostream>
using namespace std; 

#include "httpserver.h"
#include "../../mdserver.h"

class QDir;

class Transcoder: public MFDHttpPlugin
{

  public:

    Transcoder(MFD *owner, int identity);
    ~Transcoder();

    void    run();
    bool    initialize();
    void    handleIncoming(HttpInRequest *httpin_request, int client_id);
    void    handleStatusRequest(HttpInRequest *httpin_request);
    void    handleStartJobRequest(HttpInRequest *httpin_request);
    int     bumpStatusGeneration();

  private:
  
    MetadataServer *metadata_server;
    QMutex          status_generation_mutex;
    int             status_generation;
    QDir           *top_level_directory;
    QDir           *music_destination_directory;
};

#endif  // transcoder_h_

#ifndef CLIENTTRACKER_H_
#define CLIENTTRACKER_H_
/*
	clienttracker.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for a tiny class to keep track of which clients are on which
	generation of data
*/

class ClientTracker
{

  public:

    ClientTracker(int l_client_id, int l_generation);
    ~ClientTracker();

    int     getClientId();
    int     getGeneration();
    void    setGeneration(int new_generation); 
    
  private:
  
    int client_id;
    int generation;
};

#endif  // clienttracker_h_

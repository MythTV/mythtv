#ifndef PLAYLISTENTRY_H_
#define PLAYLISTENTRY_H_
/*
	playlist_entry.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	little thing that holds name, id, (etc?) of an entry on a playlist

*/

#include <qstring.h>

class PlaylistEntry
{

  public:
  
    PlaylistEntry();
    PlaylistEntry(int an_id, const QString &a_name, bool is_another_playlist = false);
    ~PlaylistEntry();
  
    int     getId(){return id;}
    void    setId(int an_id){ id=an_id; }
    
    QString getName(){return name;}
    void    setName(const QString &a_name){ name=a_name;}
    
    bool    isAnotherPlaylist(){ return is_playlist_reference; }
  
  private:
  
    int     id;
    QString name;
    bool    is_playlist_reference;
};

#endif

#ifndef MUSICDATA_H_
#define MUSICDATA_H_

//#include <musicmetadata.h>
#include <mythexp.h>

#include "playlistcontainer.h"

class PlaylistContainer;
class AllMusic;
class AllStream;

//----------------------------------------------------------------------------

class MusicData : public QObject
{
  Q_OBJECT

  public:

    MusicData();
    ~MusicData();

  public slots:
    void reloadMusic(void);

  public:
    //QString             musicDir;
    PlaylistContainer  *all_playlists;
    AllMusic           *all_music;
    AllStream          *all_streams;
    bool                initialized;
};

// This global variable contains the MusicData instance for the application
extern MPUBLIC MusicData *gMusicData;

#endif

#ifndef MDCONTAINER_H_
#define MDCONTAINER_H_
/*
	mdcontainer.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for metadata container and the thread(s) that fill(s) it

*/

#include <qstring.h>
#include <qsqldatabase.h>
#include <qintdict.h>
#include <qptrlist.h>
#include <qthread.h>
#include <qwaitcondition.h>

#include "metadata.h"


class MFD;


enum MetadataCollectionContentType
{
    MCCT_unknown = 0,
    MCCT_audio,
    MCCT_video
};

enum MetadataCollectionLocationType
{
    MCLT_host = 0,
    MCLT_lan,
    MCLT_net
};

class MetadataContainer
{
    //
    //  A base class for objects that hold metadata
    //


  public:
  
    MetadataContainer(
                        MFD *l_parent,
                        int l_unique_identifier,
                        MetadataCollectionContentType  l_content_type,
                        MetadataCollectionLocationType l_location_type
                     );

    virtual ~MetadataContainer();

    void                log(const QString &log_message, int verbosity);
    void                warning(const QString &warning_message);
    //virtual bool        tryToUpdate();
    int                 getIdentifier(){return unique_identifier;}
    bool                isAudio();
    bool                isVideo();
    bool                isLocal();
    uint                getMetadataCount();
    uint                getPlaylistCount();

    Metadata*           getMetadata(int item_id);
    QIntDict<Metadata>* getMetadata(){return current_metadata;}

    Playlist*           getPlaylist(int pl_id);
    QIntDict<Playlist>* getPlaylists(){return current_playlists;}

    void                dataSwap(QIntDict<Metadata>* new_metadata, QIntDict<Playlist>* new_playlists);
 
  protected:
  
    MFD *parent;
    int unique_identifier;
    
    MetadataCollectionContentType  content_type;
    MetadataCollectionLocationType location_type;

    QIntDict<Metadata>   *current_metadata;
    QIntDict<Playlist>   *current_playlists;
};

/*

class MetadataMythDBContainer: public MetadataContainer
{
    //
    //  A container that works off myth DB tables
    //
    
  public:
  
    MetadataMythDBContainer(
                            MFD *l_parent,
                            int l_unique_identifier,
                            MetadataCollectionContentType  l_content_type,
                            MetadataCollectionLocationType l_location_type,
                            QSqlDatabase *l_db
                           );

    ~MetadataMythDBContainer();
    
    //bool    tryToUpdate();
    
  private:
  
    QSqlDatabase    *db;
    // MetadataMonitor *metadata_monitor;

};
                            
*/


#endif

#ifndef DEVICEWATCHER_H_
#define DEVICEWATCHER_H_
/*
	devicewatcher.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for object to watch a given device
*/

#include <iostream>
using namespace std; 

#include <qstring.h>
#include "../../mdserver.h"

class DiscWatcher;
 
enum MCDDeviceType
{
    M_CD_DEVICE_TYPE_UNKNOWN = 0,
    M_CD_DEVICE_TYPE_NOTHING,
    M_CD_DEVICE_TYPE_R,
    M_CD_DEVICE_TYPE_RW
};

enum MDVDDeviceType
{
    M_DVD_DEVICE_TYPE_UNKNOWN = 0,
    M_DVD_DEVICE_TYPE_NOTHING,
    M_DVD_DEVICE_TYPE_R,
    M_DVD_DEVICE_TYPE_RW
};


class DeviceWatcher
{

  public:

    DeviceWatcher(
                    DiscWatcher *owner, 
                    const QString &device, 
                    MetadataServer *the_md_server,
                    MFD* the_mfd
                 );
    ~DeviceWatcher();

    void checkDeviceType();
    void check();
    void removeAllMetadata();
    void updateMetadata();
    void updateAudioCDMetadata();

    void log(const QString &log_message, int verbosity);
    void warning(const QString &warn_message);
    
  private:
  
    MFD             *my_mfd;
    DiscWatcher     *parent;
    QString         device_to_watch;
    MCDDeviceType   cd_device_type;
    MDVDDeviceType  dvd_device_type;

    MetadataServer    *metadata_server;
    MetadataContainer *current_metadata_container;
};

#endif  // devicewatcher_h_

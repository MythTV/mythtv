/*
	cdinput.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Access CD audio data (with cdparanoia) via something that inherits from
	QIODevice
	
*/

#include <iostream>
using namespace std;

#include "cdinput.h"


CdInput::CdInput(QUrl the_url)
          :QIODevice()
{
    url = the_url;
    device = NULL;
    paranoia = NULL;
    curpos = 0;
    start = 0;
    end = 0;   
}



bool CdInput::open(int)
{
    //
    //  Parse the url into useable bits
    //

    QString devicename = url.dirPath();
    QString tracknum_string = url.fileName().section('.', 0, 0);
    bool ok = false;
    int tracknum = tracknum_string.toInt(&ok);
    if(!ok)
    {
        cerr << "cdinput.o: could not convert \""
             << tracknum_string
             << "\" into a number to find track"
             << endl;
        return false;
    }

    //
    //  Use cdda interface to identify the device
    //

    device = cdda_identify(devicename.ascii(), 0, NULL);
    if(!device)
    {
        cerr << "cdinput.o: could not identify this device: \""
             << devicename.ascii()
             << "\"" << endl;
        return false;
    }

    //
    //  Try and open the device
    //
    
    if (cdda_open(device))
    {
        cerr << "cdinput.o: could not open this device: \""
             << devicename.ascii()
             << "\"" << endl;
        cdda_close(device);
        return false;
    }

    cdda_verbose_set(device, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

    start = cdda_track_firstsector(device, tracknum);
    end = cdda_track_lastsector(device, tracknum);
    
    if (start > end || end == start)
    {
        cerr << "cdinput.o: no sectors in track?" << endl;
        cdda_close(device);
        return false;
    }
    
    paranoia = paranoia_init(device);
    paranoia_modeset(paranoia, PARANOIA_MODE_OVERLAP);
    paranoia_seek(paranoia, start, SEEK_SET);

    curpos = start;

    return true;
}

Q_LONG CdInput::readBlock( char *data, long unsigned int maxlen )
{
    if(maxlen < 2352)
    {
        cerr << "cdinput.o: something called readBlock() with a maxlen " << endl
             << "less than 2352. This is not a smart thing to do, as " << endl
             << "reading less than one sector of an audio cd (ie. 2352 " << endl
             << "bytes) is quite pointless." << endl
             << endl;
    }
    
    long unsigned int amount_to_copy = CD_FRAMESIZE_RAW;
    if(maxlen < amount_to_copy)
    {
        amount_to_copy = maxlen;
    }
    
    curpos++;
    if (curpos <= end)
    {
        int16_t *cdbuffer;
        
        //cdbuffer = paranoia_read(paranoia, paranoia_cb);
        cdbuffer = paranoia_read(paranoia, NULL);

        memcpy(data, (char *)cdbuffer, amount_to_copy);
    }
    else
    {
        return 0;
    }
    
    return amount_to_copy;
}



Q_ULONG CdInput::size() const
{
    return (end - start) * CD_FRAMESIZE_RAW;
    return 0;
    //   return total_possible_range;
}


void CdInput::close()
{
    if(paranoia)
    {
        paranoia_free(paranoia);
    }
    if(device)
    {
        cdda_close(device);
    }
    device = NULL;
    paranoia = NULL;
    curpos = 0;
    start = 0;
    end = 0;
}

CdInput::~CdInput()
{
    close();
}


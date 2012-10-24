// -*- Mode: c++ -*-
/*
 *  Copyright (C) Daniel Kristjansson 2010
 *
 *  Copyright notice is in asirecorder.cpp of the MythTV project.
 */

#ifndef _ASI_RECORDER_H_
#define _ASI_RECORDER_H_

// MythTV includes
#include "dtvrecorder.h"

class ASIStreamHandler;
class RecordingProfile;
class ASIChannel;
class QString;
class TVRec;

/** \class ASIRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle streams from ASI drivers.
 *
 *  This has been written and tested with the DVEO ASI hardware and drivers.
 *  Those particular drivers do not come with udev rules so you will need
 *  to create the following rule in /etc/udev/rules.d/99-asi.rules
\verbatim
SUBSYSTEM=="asi", OWNER="mythtv", GROUP="video", MODE="0660", RUN+="/etc/udev/asi.sh" OPTIONS+="last_rule"
\endverbatim
Also, create a /etc/udev/asi.sh file with the following contents:
\htmlonly
<pre>
#!/bin/sh

for VAL in buffers bufsize dev granularity mode null_packets timestamps transport uevent clock_source; do
  chown mythtv /sys/devices/&#42;/&#42;/&#42;/dvbm/&#42;/asi&#42;/$VAL ;
  chgrp video /sys/devices/&#42;/&#42;/&#42;/dvbm/&#42;/asi&#42;/$VAL ;
  chmod g+rw /sys/devices/&#42;/&#42;/&#42;/dvbm/&#42;/asi&#42;/$VAL ;
  chmod g-x /sys/devices/&#42;/&#42;/&#42;/dvbm/&#42;/asi&#42;/$VAL ;
  ls -l /sys/devices/&#42;/&#42;/&#42;/dvbm/&#42;/asi&#42;/$VAL ;
done
</pre>
\endhtmlonly
 *
 * Be sure to mark asi.sh as executable with "chmod a+x /etc/udev/asi.sh"
 * udev will silently ignore it if it is not marked.
 *
 * This of course assumes you want MythTV to have access to all asi
 * device, if this is not the case, you will need to write a more
 * specific rule and modify the asi.sh to only grant permission
 * on a specific asi revice
 *
 *  \sa DTVRecorder
 */
class ASIRecorder : public DTVRecorder
{
  public:
    ASIRecorder(TVRec *rec, ASIChannel *channel);

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);
    using DTVRecorder::SetOption;
    void SetOption(const QString &name, int value);

    void run(void);

    bool Open(void);
    bool IsOpen(void) const;
    void Close(void);

  private:
    ASIChannel       *m_channel;
    ASIStreamHandler *m_stream_handler;
    bool              m_record_mpts;
};

#endif // _ASI_RECORDER_H_

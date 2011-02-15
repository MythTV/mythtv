#ifndef SCHEDULEDRECORDING_H
#define SCHEDULEDRECORDING_H

#include "mythtvexp.h"

class MTV_PUBLIC ScheduledRecording
{
  public:
    ScheduledRecording();
    ~ScheduledRecording();

    static void signalChange(int recordid);
    // Use -1 for recordid when all recordids are potentially
    // affected, such as when the program table is updated.
    // Use 0 for recordid when a reschdule isn't specific to a single
    // recordid, such as when a recording type priority is changed.
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */


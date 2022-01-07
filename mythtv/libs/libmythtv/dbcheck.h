#ifndef DBCHECK_H_
#define DBCHECK_H_

#include "mythtvexp.h"
#include "mythdbcheck.h"

// Call after establishing the first db connection.
MTV_PUBLIC bool InitializeMythSchema(void);

// Call after initializing the main db connection.
MTV_PUBLIC bool UpgradeTVDatabaseSchema(bool upgradeAllowed = false,
                                        bool upgradeIfNoUI  = false,
                                        bool informSystemd  = false);

MTV_PUBLIC DBUpdates getRecordingExtenderDbInfo (int version);
#endif

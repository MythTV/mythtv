#ifndef DBCHECK_H_
#define DBCHECK_H_

#include "mythtvexp.h"

// Call after establishing the first db connection.
MTV_PUBLIC bool InitializeMythSchema(void);

// Call after initializing the main db connection.
MTV_PUBLIC bool UpgradeTVDatabaseSchema(const bool upgradeAllowed = false,
                                     const bool upgradeIfNoUI  = false);

#endif


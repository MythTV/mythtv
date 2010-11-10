#ifndef DBCHECK_H_
#define DBCHECK_H_

#include "mythexp.h"

// Call after establishing the first db connection.
MPUBLIC bool InitializeMythSchema(void);

// Call after initialing the main db connection.
MPUBLIC bool UpgradeTVDatabaseSchema(const bool upgradeAllowed = false,
                                     const bool upgradeIfNoUI  = false);

#endif


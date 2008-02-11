#ifndef DBCHECK_H_
#define DBCHECK_H_

#include "mythexp.h"

// Used to compare the TV Database schema version against the expected version
MPUBLIC int CompareTVDatabaseSchemaVersion(void);
// Call after initialing the main db connection.
MPUBLIC bool UpgradeTVDatabaseSchema(void);

#endif


#ifndef MYTHDIGEST_H_
#define MYTHDIGEST_H_
/*
	mythdigest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for a little object to calculate persistent content id's
	(checksums)

*/

#include <inttypes.h>
        
#include <qstring.h>

class MythDigest
{

  public:

    MythDigest(const QString &a_file_name);
    ~MythDigest();

    QString calculate();

  private:
  
    QString file_name;
};

#endif

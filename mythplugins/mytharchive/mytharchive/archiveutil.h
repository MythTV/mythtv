/*
    archiveutil.h

    some shared functions and types
*/

#ifndef ARCHIVEUTIL_H_
#define ARCHIVEUTIL_H_

#include <qstring.h>

class ProgramInfo;

typedef enum
{
    AD_DVD_SL = 0,
    AD_DVD_DL = 1,
    AD_DVD_RW = 2,
    AD_FILE   = 3
} ARCHIVEDESTINATION;

typedef struct ArchiveDestination
{
    ARCHIVEDESTINATION type;
    QString name;
    QString description;
    long long freeSpace;
}_ArchiveDestination;

extern struct ArchiveDestination ArchiveDestinations[];
extern int ArchiveDestinationsCount;


QString formatSize(long long sizeKB, int prec = 2);
QString getTempDirectory();
void checkTempDirectory();
bool extractDetailsFromFilename(const QString &inFile,
                                QString &chanID, QString &startTime);
ProgramInfo *getProgramInfoForFile(const QString &inFile);

#endif

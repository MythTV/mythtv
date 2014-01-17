/*
    archiveutil.h

    some shared functions and types
*/

#ifndef ARCHIVEUTIL_H_
#define ARCHIVEUTIL_H_

#include <stdint.h>
#include <QString>
#include <QMetaType>

class ProgramInfo;

typedef enum
{
    AD_DVD_SL = 0,
    AD_DVD_DL = 1,
    AD_DVD_RW = 2,
    AD_FILE   = 3
} ARCHIVEDESTINATION;

Q_DECLARE_METATYPE (ARCHIVEDESTINATION);

typedef struct ArchiveDestination
{
    ARCHIVEDESTINATION type;
    const char *name;
    const char *description;
    int64_t freeSpace;
}_ArchiveDestination;

extern struct ArchiveDestination ArchiveDestinations[];
extern int ArchiveDestinationsCount;

typedef struct
{
    QString name;
    QString description;
    float bitrate;
} EncoderProfile;

typedef struct ThumbImage
{
    QString caption;
    QString filename;
    qint64  frame;
} ThumbImage;

typedef struct
{
    int     id;
    QString type;
    QString title;
    QString subtitle;
    QString description;
    QString startDate;
    QString startTime;
    QString filename;
    int64_t size;
    int64_t newsize;
    int duration;
    int cutDuration;
    EncoderProfile *encoderProfile;
    QString fileCodec;
    QString videoCodec;
    int videoWidth, videoHeight;
    bool hasCutlist;
    bool useCutlist;
    bool editedDetails;
    QList<ThumbImage*> thumbList;
} ArchiveItem;

QString formatSize(int64_t sizeKB, int prec = 2);
QString getTempDirectory(bool showError = false);
void checkTempDirectory();
bool extractDetailsFromFilename(const QString &inFile,
                                QString &chanID, QString &startTime);
ProgramInfo *getProgramInfoForFile(const QString &inFile);
bool getFileDetails(ArchiveItem *a);
void recalcItemSize(ArchiveItem *item);
QString getBaseName(const QString &filename);
void showWarningDialog(const QString &msg);

Q_DECLARE_METATYPE(EncoderProfile *)
Q_DECLARE_METATYPE(ArchiveItem *)

#endif

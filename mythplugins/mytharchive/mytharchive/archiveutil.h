/*
    archiveutil.h

    some shared functions and types
*/

#ifndef ARCHIVEUTIL_H_
#define ARCHIVEUTIL_H_

#include <cstdint>
#include <QString>
#include <QMetaType>

class ProgramInfo;

enum ARCHIVEDESTINATION : std::uint8_t
{
    AD_DVD_SL = 0,
    AD_DVD_DL = 1,
    AD_DVD_RW = 2,
    AD_FILE   = 3
};

Q_DECLARE_METATYPE (ARCHIVEDESTINATION);

struct ArchiveDestination
{
    ARCHIVEDESTINATION type;
    const char *name;
    const char *description;
    int64_t freeSpace;
};

extern std::vector<ArchiveDestination> ArchiveDestinations;

struct EncoderProfile
{
    QString name;
    QString description;
    float bitrate { 0.0F };
};

struct ThumbImage
{
    QString caption;
    QString filename;
    qint64  frame { 0 };
};

struct ArchiveItem
{
    int     id                     { 0 };
    QString type;
    QString title;
    QString subtitle;
    QString description;
    QString startDate;
    QString startTime;
    QString filename;
    int64_t size                   { 0 };
    int64_t newsize                { 0 };
    int duration                   { 0 };
    int cutDuration                { 0 };
    EncoderProfile *encoderProfile { nullptr };
    QString fileCodec;
    QString videoCodec;
    int videoWidth                 { 0 };
    int videoHeight                { 0 };
    bool hasCutlist                { false };
    bool useCutlist                { false };
    bool editedDetails             { false };
    QList<ThumbImage*> thumbList;
};

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

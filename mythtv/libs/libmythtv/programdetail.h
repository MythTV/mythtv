#ifndef _PROGRAM_DETAIL_H_
#define _PROGRAM_DETAIL_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QString>
#include <QDateTime>

// MythTV headers
#include "mythexp.h"

class MPUBLIC ProgramDetail
{
  public:
    QString   channame;
    QString   title;
    QString   subtitle;
    QDateTime startTime;
    QDateTime endTime;
};
typedef vector<ProgramDetail> ProgramDetailList;

MPUBLIC bool GetProgramDetailList(
    QDateTime         &nextRecordingStart,
    bool              *hasConflicts = NULL,
    ProgramDetailList *list = NULL);

#endif // _PROGRAM_DETAIL_H_

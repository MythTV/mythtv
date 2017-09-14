#ifndef RECSTATUS_H_
#define RECSTATUS_H_

#include <QtCore>
#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "programtypes.h"

class SERVICE_PUBLIC RecStatus : public QObject
{
  Q_OBJECT

  public:
    Q_ENUMS(Type)

    enum Type {
        Pending = -15,
        Failing = -14,
        //OtherRecording = -13, (obsolete)
        //OtherTuning = -12, (obsolete)
        MissedFuture = -11,
        Tuning = -10,
        Failed = -9,
        TunerBusy = -8,
        LowDiskSpace = -7,
        Cancelled = -6,
        Missed = -5,
        Aborted = -4,
        Recorded = -3,
        Recording = -2,
        WillRecord = -1,
        Unknown = 0,
        DontRecord = 1,
        PreviousRecording = 2,
        CurrentRecording = 3,
        EarlierShowing = 4,
        TooManyRecordings = 5,
        NotListed = 6,
        Conflict = 7,
        LaterShowing = 8,
        Repeat = 9,
        Inactive = 10,
        NeverRecord = 11,
        Offline = 12
        //OtherShowing = 13 (obsolete)
    }; // note stored in int8_t in ProgramInfo

    static QString toUIState(Type);
    static QString toString(Type, uint id);
    static QString toString(Type, const QString &name);
    static QString toString(Type, RecordingType type = kNotRecording);
    static QString toDescription(Type, RecordingType,
                                 const QDateTime &recstartts);
    public:

        static inline void InitializeCustomTypes();

        explicit RecStatus(QObject *parent = 0) : QObject(parent) {}
        explicit RecStatus(const RecStatus & /* src */)					 {}
};

Q_DECLARE_METATYPE( RecStatus )
Q_DECLARE_METATYPE( RecStatus*)

inline void RecStatus::InitializeCustomTypes()
{
    qRegisterMetaType< RecStatus  >();
    qRegisterMetaType< RecStatus* >();
}

#endif

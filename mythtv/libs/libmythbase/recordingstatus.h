#ifndef RECORDING_STATUS_H_
#define RECORDING_STATUS_H_

#include <QString>
#include <QDateTime>

#include "mythbaseexp.h"
#include "programtypes.h"

class MBASE_PUBLIC RecStatus : public QObject
{
  Q_OBJECT

  public:
    // NOLINTNEXTLINE(performance-enum-size)
    enum Type  {
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
    Q_ENUM(Type)

    static QString toUIState(RecStatus::Type recstatus);
    static QString toString(RecStatus::Type recstatus, uint id);
    static QString toString(RecStatus::Type recstatus, const QString &name);
    static QString toString(RecStatus::Type recstatus, RecordingType type = kNotRecording);
    static QString toDescription(Type recstatus, RecordingType rectype,
                                 const QDateTime &recstartts);
    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE explicit RecStatus(QObject *parent = nullptr) : QObject(parent) {}

    private:
        Q_DISABLE_COPY(RecStatus)
};

inline void RecStatus::InitializeCustomTypes()
{
    qRegisterMetaType< RecStatus* >();
}

#endif // RECORDING_STATUS_H_

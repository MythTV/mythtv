// -*- Mode: c++ -*-

#ifndef STREAM_HANDLER_H
#define STREAM_HANDLER_H

#include <utility>
#include <vector>

// Qt headers
#include <QWaitCondition>
#include <QString>
#include <QMutex>
#include <QMap>
#include <QRecursiveMutex>

// MythTV headers
#include "libmythbase/mthread.h"
#include "libmythbase/mythdate.h"

#include "DeviceReadBuffer.h" // for ReaderPausedCB
#include "mpeg/mpegstreamdata.h" // for PIDPriority

class ThreadedFileWriter;

//#define DEBUG_PID_FILTERS

class PIDInfo
{
  public:
    PIDInfo() = default;
    explicit PIDInfo(uint pid) : m_pid(pid) {}
    PIDInfo(uint pid, uint stream_type, int pes_type) :
        m_pid(pid), m_streamType(stream_type), m_pesType(pes_type) {;}
    virtual ~PIDInfo() {;}

    virtual bool Open(const QString &/*dev*/, bool /*use_section_reader*/)
        { return false; }
    virtual bool Close(const QString &/*dev*/) { return false; }
    bool IsOpen(void) const { return m_filterFd >= 0; }

    uint        m_pid        { UINT_MAX };
    int         m_filterFd   { -1 };  ///< Input filter file descriptor
    uint        m_streamType {  0 };  ///< StreamID
    int         m_pesType    { -1 };  ///< PESStreamID
};
// Please do not change this to hash or other unordered container.
// HDHRStreamHandler::UpdateFilters() relies on the forward
// iterator returning these in order of ascending pid number.
using PIDInfoMap = QMap<uint,PIDInfo*>;

// locking order
// _pid_lock -> _listener_lock
// _add_rm_lock -> _listener_lock
//              -> _start_stop_lock

class StreamHandler : protected MThread, public DeviceReaderCB
{
  public:
    virtual void AddListener(MPEGStreamData *data,
                             bool allow_section_reader = false,
                             bool needs_buffering      = false,
                             const QString& output_file= QString());
    virtual void RemoveListener(MPEGStreamData *data);
    bool IsRunning(void) const;
    bool HasError(void) const { return m_bError; }

    /// Called with _listener_lock locked just after adding new output file.
    virtual bool AddNamedOutputFile(const QString &filename);
    /// Called with _listener_lock locked just before removing old output file.
    virtual void RemoveNamedOutputFile(const QString &filename);

  protected:
    explicit StreamHandler(QString device, int inputid)
        : MThread("StreamHandler"), m_device(std::move(device)), m_inputId(inputid) {}
    ~StreamHandler() override;

    void Start(void);
    void Stop(void);

    void SetRunning(bool running,
                    bool using_buffering,
                    bool using_section_reader);

    bool AddPIDFilter(PIDInfo *info);
    bool RemovePIDFilter(uint pid);
    bool RemoveAllPIDFilters(void);

    void UpdateListeningForEIT(void);
    bool UpdateFiltersFromStreamData(void);
    virtual bool UpdateFilters(void) { return true; }
    virtual void CycleFiltersByPriority() {}

    PIDPriority GetPIDPriority(uint pid) const;

    // DeviceReaderCB
    void ReaderPaused(int fd) override { (void) fd; } // DeviceReaderCB
    void PriorityEvent(int fd) override { (void) fd; } // DeviceReaderCB

    virtual PIDInfo *CreatePIDInfo(uint pid, uint stream_type, int pes_type)
        { return new PIDInfo(pid, stream_type, pes_type); }

  protected:
    /// Write out a copy of the raw MPTS
    void WriteMPTS(const unsigned char * buffer, uint len);
    /// At minimum this sets _running_desired, this may also send
    /// signals to anything that might be blocking the run() loop.
    /// \note: The _start_stop_lock must be held when this is called.
    virtual void SetRunningDesired(bool desired);

  protected:
    QString             m_device;
    int                 m_inputId;
    bool                m_needsBuffering        {false};
    bool                m_allowSectionReader    {false};

    QMutex              m_addRmLock;

    mutable QMutex      m_startStopLock;
    volatile bool       m_runningDesired        {false};

    // not to be confused with the other four header files that define
    // m_error to be a QString.  Somehow v4l2encstreamhandler.cpp
    // blends these into a single class.
    volatile bool       m_bError                {false};
    bool                m_running               {false};
    bool                m_restarting            {false};
    bool                m_usingBuffering        {false};
    bool                m_usingSectionReader    {false};
    QWaitCondition      m_runningStateChanged;

    mutable QRecursiveMutex m_pidLock;
    std::vector<uint>   m_eitPids;
    PIDInfoMap          m_pidInfo;
    uint                m_openPidFilters        {0};
    bool                m_filtersChanged        {false};
    MythTimer           m_cycleTimer;

    ThreadedFileWriter *m_mptsTfw               {nullptr};
    QSet<QString>       m_mptsFiles;
    QString             m_mptsBaseFile;
    QMutex              m_mptsLock;

    using StreamDataList = QHash<MPEGStreamData*,QString>;
    mutable QRecursiveMutex m_listenerLock;
    StreamDataList      m_streamDataList;
};

#endif // STREAM_HANDLER_H

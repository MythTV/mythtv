// -*- Mode: c++ -*-

#ifndef _STREAM_HANDLER_H_
#define _STREAM_HANDLER_H_

#include <vector>
using namespace std;

#include <QWaitCondition>
#include <QString>
#include <QMutex>
#include <QMap>

#include "DeviceReadBuffer.h" // for ReaderPausedCB
#include "mpegstreamdata.h" // for PIDPriority
#include "mthread.h"
#include "mythdate.h"

class ThreadedFileWriter;

//#define DEBUG_PID_FILTERS

class PIDInfo
{
  public:
    PIDInfo() :
        _pid(0xffffffff), filter_fd(-1), streamType(0), pesType(-1) {;}
    explicit PIDInfo(uint pid) :
        _pid(pid),        filter_fd(-1), streamType(0), pesType(-1) {;}
    PIDInfo(uint pid, uint stream_type, int pes_type) :
        _pid(pid),                       filter_fd(-1),
        streamType(stream_type),         pesType(pes_type) {;}
    virtual ~PIDInfo() {;}

    virtual bool Open(const QString &/*dev*/, bool /*use_section_reader*/)
        { return false; }
    virtual bool Close(const QString &/*dev*/) { return false; }
    bool IsOpen(void) const { return filter_fd >= 0; }

    uint        _pid;
    int         filter_fd;         ///< Input filter file descriptor
    uint        streamType;        ///< StreamID
    int         pesType;           ///< PESStreamID
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
                             QString output_file       = QString());
    virtual void RemoveListener(MPEGStreamData *data);
    bool IsRunning(void) const;
    bool HasError(void) const { return m_bError; }

    /// Called with _listener_lock locked just after adding new output file.
    virtual bool AddNamedOutputFile(const QString &filename);
    /// Called with _listener_lock locked just before removing old output file.
    virtual void RemoveNamedOutputFile(const QString &filename);

  protected:
    explicit StreamHandler(const QString &device, int inputid)
        : MThread("StreamHandler"), m_device(device), m_inputid(inputid) {}
    ~StreamHandler();

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
    void WriteMPTS(unsigned char * buffer, uint len);
    /// At minimum this sets _running_desired, this may also send
    /// signals to anything that might be blocking the run() loop.
    /// \note: The _start_stop_lock must be held when this is called.
    virtual void SetRunningDesired(bool desired);

  protected:
    QString             m_device;
    int                 m_inputid;
    bool                m_needs_buffering       {false};
    bool                m_allow_section_reader  {false};

    QMutex              m_add_rm_lock;

    mutable QMutex      m_start_stop_lock;
    volatile bool       m_running_desired       {false};

    // not to be confused with the other four header files that define
    // m_error to be a QString.  Somehow v4l2encstreamhandler.cpp
    // blends these into a single class.
    volatile bool       m_bError                {false};
    bool                m_running               {false};
    bool                m_restarting            {false};
    bool                m_using_buffering       {false};
    bool                m_using_section_reader  {false};
    QWaitCondition      m_running_state_changed;

    mutable QMutex      m_pid_lock              {QMutex::Recursive};
    vector<uint>        m_eit_pids;
    PIDInfoMap          m_pid_info;
    uint                m_open_pid_filters      {0};
    MythTimer           m_cycle_timer;

    ThreadedFileWriter *m_mpts_tfw              {nullptr};
    QSet<QString>       m_mpts_files;
    QString             m_mpts_base_file;
    QMutex              m_mpts_lock;

    using StreamDataList = QMap<MPEGStreamData*,QString>;
    mutable QMutex      m_listener_lock         {QMutex::Recursive};
    StreamDataList      m_stream_data_list;
};

#endif // _STREAM_HANDLER_H_

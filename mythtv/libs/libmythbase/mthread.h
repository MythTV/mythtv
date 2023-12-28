#ifndef MYTH_THREAD_H
#define MYTH_THREAD_H

#include <climits>

#include <QRunnable>
#include <QThread>

#include "mythbaseexp.h"
#include "mythchrono.h"

class MThreadInternal;
class MThread;

/// Use this to determine if you are in the named thread.
bool MBASE_PUBLIC is_current_thread(MThread *thread);
/// Use this to determine if you are in the named thread.
bool MBASE_PUBLIC is_current_thread(QThread *thread);
/// Use this to determine if you are in the named thread.
bool MBASE_PUBLIC is_current_thread(MThread &thread);

/** This is a wrapper around QThread that does several additional things.
 *
 *  \ingroup mthreadpool
 *
 *  First it requires that you set the thread's name which is used for
 *  debugging.
 *
 *  It adds RunProlog() and RunEpilog() which are called automatically
 *  when you don't override run(), but must be called explicitly if
 *  you do. These take care of setting up logging and take care of
 *  dealing with QSqlConnections which are per-thread variables.
 *
 *  It also adds some sanity checking to the destructor so a thread
 *  can not be deleted while it still running.
 *
 *  Optionally it can be given a QRunnable to run in MThread::run()
 *  instead of calling QThread::run() (which starts an event loop.)
 *  When you override MThread::run() or use a QRunnable you are
 *  responsible for stopping the thread, MThread::exit() will not
 *  work.
 *
 *  Warning: Do not statically initialize MThreads. C++ itself
 *  doesn't allow you to specify the order of static initializations.
 *  See: http://www.parashift.com/c++-faq-lite/ctors.html#faq-10.15
 *
 */
class MBASE_PUBLIC MThread
{
    friend class MThreadInternal;
  public:
    /// \brief Standard constructor
    explicit MThread(const QString &objectName);
    /// \brief Use this constructor if you want the default run() method to
    ///        run the QRunnable's run() method instead of entering the Qt
    ///        event loop. Unlike MThreadPool, MThread will not delete a
    ///        runnable with the autoDelete property set.
    explicit MThread(const QString &objectName, QRunnable *runnable);
    virtual ~MThread();

    MThread(const MThread &) = delete;            // not copyable
    MThread &operator=(const MThread &) = delete; // not copyable

    /// \brief Sets up a thread, call this if you reimplement run().
    void RunProlog(void);
    /// \brief Cleans up a thread's resources, call this if you reimplement
    ///        run().
    void RunEpilog(void);

    /// \brief Returns the thread, this will always return the same pointer
    ///        no matter how often you restart the thread.
    QThread *qthread(void);

    void setObjectName(const QString &name);
    QString objectName(void) const;

    void setPriority(QThread::Priority priority);
    QThread::Priority priority(void) const;

    bool isFinished(void) const;
    bool isRunning(void) const;

    void setStackSize(uint stackSize);
    uint stackSize(void) const;

    /// \brief Use this to exit from the thread if you are using a Qt event loop
    void exit(int retcode = 0);
    /// \brief Tell MThread to start running the thread in the near future.
    void start(QThread::Priority p = QThread::InheritPriority);
    /// \brief Kill a thread unsafely.
    ///
    /// This should never be called on a thread while it holds a mutex
    /// or semaphore, since those locks will never be unlocked. Use
    /// the static setTerminationEnabled(true) to tell MThread when
    /// it is safe to terminate the thread and setTerminationEnabled(false)
    /// to tell it that termination is not safe again.
    void terminate(void);
    void quit(void); ///< calls exit(0)

    /// This is to be called on startup in those few threads that
    /// haven't been ported to MThread.
    static void ThreadSetup(const QString& name);
    /// This is to be called on exit in those few threads that
    /// haven't been ported to MThread.
    static void ThreadCleanup(void);

  public:
    /// \brief Wait for the MThread to exit, with a maximum timeout
    /// \param time Maximum time to wait for MThread to exit, in ms
    bool wait(std::chrono::milliseconds time = std::chrono::milliseconds::max());

    /// \brief This will print out all the running threads, call exit(1) on
    ///        each and then wait up to 5 seconds total for all the threads
    ///        to exit.
    static void Cleanup(void);
    static void GetAllThreadNames(QStringList &list);
    static void GetAllRunningThreadNames(QStringList &list);

  protected:
    /// \brief Runs the Qt event loop unless we have a QRunnable,
    ///        in which case we run the runnable run instead.
    /// \note  If you override this method you must call RunProlog
    ///        before you do any work and RunEpilog before you exit
    ///        the run method.
    virtual void run(void);
    /// Enters the qt event loop. call exit or quit to exit thread
    int exec(void);

    static void setTerminationEnabled(bool enabled = true);
    static void usleep(std::chrono::microseconds time);
    template<typename R, typename P>
    static typename std::enable_if_t<std::chrono::treat_as_floating_point<R>::value, void>
    usleep(std::chrono::duration<R,P> time) { usleep(duration_cast<std::chrono::microseconds>(time)); }; // cppcheck-suppress [missingReturn,usleepCalled]

    MThreadInternal *m_thread {nullptr};
    QRunnable *m_runnable     {nullptr};
    bool m_prologExecuted     {true};
    bool m_epilogExecuted     {true};
};

#endif // MYTH_THREAD_H

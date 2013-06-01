// POSIX header
#include <signal.h> // for SIGXXX

// Qt headers
#include <QStringList>
#include <QRegExp>

// MythTV headers
#include "mythsystemlegacy.h"
#include "mythsystem.h"
#include "exitcodes.h"

// temporary debugging headers
#include <iostream>
using namespace std;


class MythSystemLegacyWrapper : public MythSystem
{
  public:
    static MythSystemLegacyWrapper *Create(
        const QStringList &args,
        uint flags,
        QString startPath,
        Priority cpuPriority,
        Priority diskPriority)
    {
        if (args.empty())
            return NULL;

        QString program = args[0];
        QStringList other_args = args.mid(1);

        MythSystemLegacy *legacy =
            new MythSystemLegacy(args.join(" "), flags);

        if (!startPath.isEmpty())
            legacy->SetDirectory(startPath);

        MythSystemLegacyWrapper *wrapper =
            new MythSystemLegacyWrapper(legacy, flags);

        // TODO implement cpuPriority and diskPriority
        return wrapper;
    }

    ~MythSystemLegacyWrapper(void)
    {
        Wait(0);
    }

    virtual uint GetFlags(void) const MOVERRIDE
    {
        return m_flags;
    }

    /// Returns the starting path of the program
    virtual QString GetStartingPath(void) const MOVERRIDE
    {
        return m_legacy->GetDirectory();
    }

    /// Return the CPU Priority of the program
    virtual Priority GetCPUPriority(void) const MOVERRIDE
    {
        return kNormalPriority;
    }

    /// Return the Disk Priority of the program
    virtual Priority GetDiskPriority(void) const MOVERRIDE
    {
        return kNormalPriority;
    }

    /// Blocks until child process is collected or timeout reached.
    /// Returns true if program has exited and has been collected.
    /// WARNING if program returns 142 then we will forever
    ///         think it is running even though it is not.
    /// WARNING The legacy timeout is in seconds not milliseconds,
    ///         timeout will be rounded.
    virtual bool Wait(uint timeout_ms) MOVERRIDE
    {
        timeout_ms = (timeout_ms >= 1000) ? timeout_ms + 500 :
            ((timeout_ms == 0) ? 0 : 1000);
        uint legacy_wait_ret = m_legacy->Wait(timeout_ms / 1000);
        if (GENERIC_EXIT_RUNNING == legacy_wait_ret)
            return false;
        return true;
    }

    /// Returns the standard input stream for the program
    /// if the kMSStdIn flag was passed to the constructor.
    virtual QIODevice *GetStandardInputStream(void) MOVERRIDE
    {
        return NULL;
    }

    /// Returns the standard output stream for the program
    /// if the kMSStdOut flag was passed to the constructor.
    virtual QIODevice *GetStandardOutputStream(void) MOVERRIDE
    {
        return NULL;
    }

    /// Returns the standard error stream for the program
    /// if the kMSStdErr flag was passed to the constructor.
    virtual QIODevice *GetStandardErrorStream(void) MOVERRIDE
    {
        return NULL;
    }

    /// Sends the selected signal to the program
    virtual void Signal(MythSignal sig) MOVERRIDE
    {
        m_legacy->Signal(sig);
    }

    /** \brief returns the exit code, if any, that the program returned.
     *
     *  Returns -1 if the program exited without exit code.
     *  Returns -2 if the program has not yet been collected.
     *  Returns an exit code 0..255 if the program exited with exit code.
     */
    virtual int GetExitCode(void) const MOVERRIDE
    {
        // FIXME doesn't actually know why program exited.
        //       if program returns 142 then we will forever
        //       think it is running even though it is not.
        int status = m_legacy->GetStatus();
        if (GENERIC_EXIT_RUNNING == status)
            return -2;
        return status;
    }

    /** \brief returns the signal, if any, that killed the program.
     *
     *  If the program was killed by a signal this returns the signal
     *  that killed the program or signal unknown if it was not one of
     *  the common signals. If the program is still running or if the
     *  program did not exit due to a signal this returns kSignalNone.
     *
     *  Note: Platform agnostic code should only rely on kSignalNone
     *        vs non-kSignalNone values, since querying the actual
     *        signal is not possible on many platforms.
     *
     *  TODO: Should we just return a tristate: killed by signal,
     *        still running, exited normally?
     *  TODO: Should we just eliminate this entirely, i.e. a
     *        limited interface is just telling us if GetExitCode()
     *        will return -2 or -1, or 0..255, so it is redundant.
     */
    virtual MythSignal GetSignal(void) const MOVERRIDE
    {
        return kSignalNone;
/*
        int status = m_legacy->GetStatus();
        if (!WIFSIGNALED(status))
            return kSignalNone;
        int posix_signal = -1; // sentinel, POSIX signals are all positive
#ifdef WSTOPSIG
        posix_signal = WSTOPSIG(status);
#endif
        switch (posix_signal)
        {
            case SIGHUP: return kSignalHangup;
            case SIGINT: return kSignalInterrupt;
            case SIGCONT: return kSignalContinue;
            case SIGQUIT: return kSignalQuit;
            case SIGSEGV: return kSignalSegfault;
            case SIGKILL: return kSignalKill;
            case SIGUSR1: return kSignalUser1;
            case SIGUSR2: return kSignalUser2;
            case SIGTERM: return kSignalTerm;
            case SIGSTOP: return kSignalStop;
            default: return kSignalUnknown;
        }
*/
    }

  private:
    MythSystemLegacyWrapper(MythSystemLegacy *legacy, uint flags) :
        m_legacy(legacy), m_flags(flags)
    {
        m_legacy->Run();
    }

  private:
    MythSystemLegacy *m_legacy;
    uint m_flags;
};

MythSystem *MythSystem::Create(
    const QStringList &args,
    uint flags,
    QString startPath,
    Priority cpuPriority,
    Priority diskPriority)
{
    return MythSystemLegacyWrapper::Create(
        args, flags, startPath, cpuPriority, diskPriority);
}

MythSystem *MythSystem::Create(
    QString args,
    uint flags,
    QString startPath,
    Priority cpuPriority,
    Priority diskPriority)
{
    return MythSystem::Create(
        args.split(QRegExp("\\s+")), flags, startPath,
        cpuPriority, diskPriority);
}

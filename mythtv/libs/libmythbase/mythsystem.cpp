/* -*- Mode: c++ -*-
 *  Class MythSystem
 *
 *  Copyright (C) Daniel Kristjansson 2013
 *  Copyright (C) Gavin Hurlbut 2012
 *  Copyright (C) Issac Richards 2008
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

// C++ headers
#include <cstdint>
#include <csignal> // for SIGXXX

// Qt headers
#include <QByteArray>
#include <QIODevice>
#include <QStringList>
#include <utility>

// MythTV headers
#include "mythsystemlegacy.h"
#include "mythsystem.h"
#include "exitcodes.h"

class MythSystemLegacyWrapper : public MythSystem
{
  public:
    static MythSystemLegacyWrapper *Create(
        const QStringList &args,
        uint flags,
        const QString& startPath,
        Priority /*cpuPriority*/,
        Priority /*diskPriority*/)
    {
        if (args.empty())
            return nullptr;

        auto *legacy = new MythSystemLegacy(args.join(" "), flags);

        if (!startPath.isEmpty())
            legacy->SetDirectory(startPath);

        uint ac = kMSAutoCleanup | kMSRunBackground;
        if ((ac & flags) == ac)
        {
            legacy->Run();
            return nullptr;
        }

        auto *wrapper = new MythSystemLegacyWrapper(legacy, flags);

        // TODO implement cpuPriority and diskPriority
        return wrapper;
    }

    ~MythSystemLegacyWrapper(void) override
    {
        MythSystemLegacyWrapper::Wait(0ms);
    }

    uint GetFlags(void) const override // MythSystem
    {
        return m_flags;
    }

    /// Returns the starting path of the program
    QString GetStartingPath(void) const override // MythSystem
    {
        return m_legacy->GetDirectory();
    }

    /// Return the CPU Priority of the program
    Priority GetCPUPriority(void) const override // MythSystem
    {
        return kNormalPriority;
    }

    /// Return the Disk Priority of the program
    Priority GetDiskPriority(void) const override // MythSystem
    {
        return kNormalPriority;
    }

    /// Blocks until child process is collected or timeout reached.
    /// Returns true if program has exited and has been collected.
    /// WARNING if program returns 142 then we will forever
    ///         think it is running even though it is not.
    /// WARNING The legacy timeout is in seconds not milliseconds,
    ///         timeout will be rounded.
    bool Wait(std::chrono::milliseconds timeout) override // MythSystem
    {
        if (timeout >= 1s)
            timeout = timeout + 500ms;
        else if (timeout == 0ms)
            timeout = 0ms;
        else
            timeout = 1s;
        uint legacy_wait_ret = m_legacy->Wait(duration_cast<std::chrono::seconds>(timeout));
        return GENERIC_EXIT_RUNNING != legacy_wait_ret;
    }

    /// Returns the standard input stream for the program
    /// if the kMSStdIn flag was passed to the constructor.
    /// Note: This is not safe!
    QIODevice *GetStandardInputStream(void) override // MythSystem
    {
        if (!(kMSStdIn & m_flags))
            return nullptr;

        if (!m_legacy->GetBuffer(0)->isOpen() &&
            !m_legacy->GetBuffer(0)->open(QIODevice::WriteOnly))
        {
            return nullptr;
        }

        return m_legacy->GetBuffer(0);
    }

    /// Returns the standard output stream for the program
    /// if the kMSStdOut flag was passed to the constructor.
    QIODevice *GetStandardOutputStream(void) override // MythSystem
    {
        if (!(kMSStdOut & m_flags))
            return nullptr;

        Wait(0ms); // legacy getbuffer is not thread-safe, so wait

        if (!m_legacy->GetBuffer(1)->isOpen() &&
            !m_legacy->GetBuffer(1)->open(QIODevice::ReadOnly))
        {
            return nullptr;
        }

        return m_legacy->GetBuffer(1);
    }

    /// Returns the standard error stream for the program
    /// if the kMSStdErr flag was passed to the constructor.
    QIODevice *GetStandardErrorStream(void) override // MythSystem
    {
        if (!(kMSStdErr & m_flags))
            return nullptr;

        Wait(0ms); // legacy getbuffer is not thread-safe, so wait

        if (!m_legacy->GetBuffer(2)->isOpen() &&
            !m_legacy->GetBuffer(2)->open(QIODevice::ReadOnly))
        {
            return nullptr;
        }

        return m_legacy->GetBuffer(2);
    }

    /// Sends the selected signal to the program
    void Signal(MythSignal sig) override // MythSystem
    {
        m_legacy->Signal(sig);
    }

    /** \brief returns the exit code, if any, that the program returned.
     *
     *  Returns -1 if the program exited without exit code.
     *  Returns -2 if the program has not yet been collected.
     *  Returns an exit code 0..255 if the program exited with exit code.
     */
    int GetExitCode(void) const override // MythSystem
    {
        // FIXME doesn't actually know why program exited.
        //       if program returns 142 then we will forever
        //       think it is running even though it is not.
        int status = m_legacy->GetStatus();
        if (GENERIC_EXIT_RUNNING == status)
            return -2;
        if (GENERIC_EXIT_KILLED == status)
            return -1;
        return status;
    }

  private:
    MythSystemLegacyWrapper(MythSystemLegacy *legacy, uint flags) :
        m_legacy(legacy), m_flags(flags)
    {
        m_legacy->Run();
    }

  private:
    QScopedPointer<MythSystemLegacy> m_legacy;
    uint m_flags;
};

MythSystem *MythSystem::Create(
    const QStringList &args,
    uint flags,
    const QString& startPath,
    Priority cpuPriority,
    Priority diskPriority)
{
    return MythSystemLegacyWrapper::Create(
        args, flags, startPath, cpuPriority, diskPriority);
}

MythSystem *MythSystem::Create(
    const QString& args,
    uint flags,
    const QString& startPath,
    Priority cpuPriority,
    Priority diskPriority)
{
    return MythSystem::Create(
        args.simplified().split(' '), flags, startPath,
        cpuPriority, diskPriority);
}

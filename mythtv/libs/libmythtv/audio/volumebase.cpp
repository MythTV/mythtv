#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include <QString>
#include <QMutex>
#include <QMutexLocker>

#include "libmythbase/mthread.h"
#include "libmythbase/mythcorecontext.h"

#include "volumebase.h"


namespace {
class VolumeWriteBackThread : public MThread
{
    VolumeWriteBackThread() : MThread("VolumeWriteBack") { }

  public:
    // Singleton
    static VolumeWriteBackThread *Instance()
    {
        QMutexLocker lock(&s_mutex);
        static auto *s_instance = new VolumeWriteBackThread;
        return s_instance;
    }

    VolumeWriteBackThread(const VolumeWriteBackThread &) = delete;
    VolumeWriteBackThread & operator =(const VolumeWriteBackThread &) = delete;

    void SetVolume(int value)
    {
        QMutexLocker lock(&m_mutex);

        if (m_volume == value)
            return;
        m_volume = value;

        switch (m_state)
        {
        case kRunning:
            break;
        case kFinished:
            wait();
            [[fallthrough]];
        case kStopped:
            m_state = kRunning;
            start();
            break;
        }
    }

  protected:
    void run(void) override // MThread
    {
        m_state = kRunning;
        RunProlog();

        static constexpr std::chrono::milliseconds holdoff { 500ms }; // min ms between Db writes
        QString controlLabel = gCoreContext->GetSetting("MixerControl", "PCM");
        controlLabel += "MixerVolume";

        QMutexLocker lock(&m_mutex);
        while (gCoreContext && !gCoreContext->IsExiting())
        {
            int volume = m_volume;
            lock.unlock();

            // Update the dbase with the new volume
            gCoreContext->SaveSetting(controlLabel, volume);

            // Ignore further volume changes for the holdoff period
            setTerminationEnabled(true);
            usleep(holdoff); // cppcheck-suppress usleepCalled
            setTerminationEnabled(false);

            lock.relock();
            if (volume == m_volume)
                break;
        }

        m_state = kFinished;
        RunEpilog();
    }

  private:
    static QMutex s_mutex;
    QMutex mutable m_mutex;
    enum : std::uint8_t { kStopped, kRunning, kFinished } m_state {kStopped};
    int m_volume {-1};
};

QMutex VolumeWriteBackThread::s_mutex;
} // namespace


VolumeBase::VolumeBase()
{
    m_internalVol = gCoreContext->GetBoolSetting("MythControlsVolume", true);
    m_swvol = m_swvolSetting =
        (gCoreContext->GetSetting("MixerDevice", "default").toLower() == "software");
}

bool VolumeBase::SWVolume(void) const
{
    return m_swvol;
}

void VolumeBase::SWVolume(bool set)
{
    if (m_swvolSetting)
        return;
    m_swvol = set;
}

uint VolumeBase::GetCurrentVolume(void) const
{
    return m_volume;
}

void VolumeBase::SetCurrentVolume(int value)
{
    m_volume = std::clamp(value, 0, 100);
    UpdateVolume();
    
    // Throttle Db writes
    VolumeWriteBackThread::Instance()->SetVolume(m_volume);
}

void VolumeBase::AdjustCurrentVolume(int change)
{
    SetCurrentVolume(m_volume + change);
}

MuteState VolumeBase::SetMuteState(MuteState mstate)
{
    m_currentMuteState = mstate;
    UpdateVolume();
    return m_currentMuteState;
}

void VolumeBase::ToggleMute(void)
{
    bool is_muted = GetMuteState() == kMuteAll;
    SetMuteState((is_muted) ? kMuteOff : kMuteAll);
}

MuteState VolumeBase::GetMuteState(void) const
{
    return m_currentMuteState;
}

MuteState VolumeBase::NextMuteState(MuteState cur)
{
    MuteState next = cur;

    switch (cur)
    {
       case kMuteOff:
           next = kMuteLeft;
           break;
       case kMuteLeft:
           next = kMuteRight;
           break;
       case kMuteRight:
           next = kMuteAll;
           break;
       case kMuteAll:
           next = kMuteOff;
           break;
    }

    return (next);
}

void VolumeBase::UpdateVolume(void)
{
    int new_volume = m_volume;
    if (m_currentMuteState == kMuteAll)
    {
        new_volume = 0;
    }

    if (m_swvol)
    {
        SetSWVolume(new_volume, false);
        return;
    }
    
    for (int i = 0; i < m_channels; i++)
    {
        SetVolumeChannel(i, new_volume);
    }
    
    // Individual channel muting is handled in GetAudioData,
    // this code demonstrates the old method.
    // if (m_currentMuteState == kMuteLeft)
    // {
    //     SetVolumeChannel(0, 0);
    // }
    // else if (m_currentMuteState == kMuteRight)
    // {
    //     SetVolumeChannel(1, 0);
    // }
}

void VolumeBase::SyncVolume(void)
{
    // Read the volume from the audio driver and setup our internal state to match
    if (m_swvol)
        m_volume = GetSWVolume();
    else
        m_volume = GetVolumeChannel(0);
}

void VolumeBase::SetChannels(int new_channels)
{
    m_channels = new_channels;
}

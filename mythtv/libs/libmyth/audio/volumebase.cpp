#include <cstdio>
#include <cstdlib>

#include <algorithm>
using namespace std;

#include <QString>
#include <QMutex>
#include <QMutexLocker>

#include "volumebase.h"
#include "mythcorecontext.h"
#include "mthread.h"


namespace {
class VolumeWriteBackThread : public MThread
{
    VolumeWriteBackThread(const VolumeWriteBackThread &);
    VolumeWriteBackThread & operator =(const VolumeWriteBackThread &);
    VolumeWriteBackThread() : MThread("VolumeWriteBack") 
    { }

  public:
    // Singleton
    static VolumeWriteBackThread *Instance()
    {
        QMutexLocker lock(&s_mutex);
        static VolumeWriteBackThread *s_instance = new VolumeWriteBackThread;
        return s_instance;
    }

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
            [[clang::fallthrough]];
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

        const int holdoff = 500; // min ms between Db writes
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
            msleep(holdoff);
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
    enum { kStopped, kRunning, kFinished } m_state {kStopped};
    int m_volume {-1};
};

QMutex VolumeWriteBackThread::s_mutex;
} // namespace


VolumeBase::VolumeBase() :
    volume(80), current_mute_state(kMuteOff), channels(0)
{
    internal_vol = gCoreContext->GetBoolSetting("MythControlsVolume", true);
    swvol = swvol_setting =
        (gCoreContext->GetSetting("MixerDevice", "default").toLower() == "software");
}

bool VolumeBase::SWVolume(void) const
{
    return swvol;
}

void VolumeBase::SWVolume(bool set)
{
    if (swvol_setting)
        return;
    swvol = set;
}

uint VolumeBase::GetCurrentVolume(void) const
{
    return volume;
}

void VolumeBase::SetCurrentVolume(int value)
{
    volume = max(min(value, 100), 0);
    UpdateVolume();
    
    // Throttle Db writes
    VolumeWriteBackThread::Instance()->SetVolume(volume);
}

void VolumeBase::AdjustCurrentVolume(int change)
{
    SetCurrentVolume(volume + change);
}

MuteState VolumeBase::SetMuteState(MuteState mstate)
{
    current_mute_state = mstate;
    UpdateVolume();
    return current_mute_state;
}

void VolumeBase::ToggleMute(void)
{
    bool is_muted = GetMuteState() == kMuteAll;
    SetMuteState((is_muted) ? kMuteOff : kMuteAll);
}

MuteState VolumeBase::GetMuteState(void) const
{
    return current_mute_state;
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
    int new_volume = volume;
    if (current_mute_state == kMuteAll)
    {
        new_volume = 0;
    }

    if (swvol)
    {
        SetSWVolume(new_volume, false);
        return;
    }
    
    for (int i = 0; i < channels; i++)
    {
        SetVolumeChannel(i, new_volume);
    }
    
    // Individual channel muting is handled in GetAudioData,
    // this code demonstrates the old method.
    // if (current_mute_state == kMuteLeft)
    // {
    //     SetVolumeChannel(0, 0);
    // }
    // else if (current_mute_state == kMuteRight)
    // {
    //     SetVolumeChannel(1, 0);
    // }
}

void VolumeBase::SyncVolume(void)
{
    // Read the volume from the audio driver and setup our internal state to match
    if (swvol)
        volume = GetSWVolume();
    else
        volume = GetVolumeChannel(0);
}

void VolumeBase::SetChannels(int new_channels)
{
    channels = new_channels;
}

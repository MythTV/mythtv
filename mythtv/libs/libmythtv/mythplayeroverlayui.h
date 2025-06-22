#ifndef MYTHPLAYEROVERLAYUI_H
#define MYTHPLAYEROVERLAYUI_H

// MythTV
#include "mythplayeruibase.h"

class MTV_PUBLIC MythPlayerOverlayUI : public MythPlayerUIBase
{
    Q_OBJECT

  signals:
    void OverlayStateChanged(MythOverlayState OverlayState);

  public slots:
    void BrowsingChanged(bool Browsing);
    void EditingChanged(bool Editing);

  public:
    MythPlayerOverlayUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerOverlayUI() override = default;

    virtual void UpdateSliderInfo(osdInfo& Info, bool PaddedFields = false);

    OSD* GetOSD()    { return &m_osd; }
    void LockOSD()   { m_osdLock.lock(); }
    void UnlockOSD() { m_osdLock.unlock(); }

  protected slots:
    void UpdateOSDMessage (const QString& Message);
    void UpdateOSDMessage (const QString& Message, OSDTimeout Timeout);
    void SetOSDStatus     (const QString &Title, OSDTimeout Timeout);
    void UpdateOSDStatus  (osdInfo &Info, int Type, OSDTimeout Timeout);
    void UpdateOSDStatus  (const QString& Title, const QString& Desc,
                           const QString& Value, int Type, const QString& Units,
                           int Position, OSDTimeout Timeout);
    void ChangeOSDPositionUpdates(bool Enable);
    void UpdateOSDPosition();

  protected:
    virtual std::chrono::milliseconds GetMillisecondsPlayed(bool HonorCutList);
    virtual std::chrono::milliseconds GetTotalMilliseconds(bool HonorCutList) const;
    std::chrono::seconds GetSecondsPlayed(bool HonorCutList);
    std::chrono::seconds GetTotalSeconds(bool HonorCutList) const;

    OSD    m_osd;
    QRecursiveMutex m_osdLock;
    bool   m_browsing  { false };
    bool   m_editing   { false };
    // Set in the decoder thread
    bool   m_reinitOsd { false };

  private:
    Q_DISABLE_COPY(MythPlayerOverlayUI)
    QTimer m_positionUpdateTimer;
};

#endif

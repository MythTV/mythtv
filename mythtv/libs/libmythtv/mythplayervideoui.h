#ifndef MYTHPLAYERVIDEOUI_H
#define MYTHPLAYERVIDEOUI_H

// MythTV
#include "DetectLetterbox.h"
#include "mythinteropgpu.h"
#include "mythplayercaptionsui.h"

class DecoderCallback
{
  public:
    using Callback = void (*)(void*, void*, void*);
    DecoderCallback() = default;
    DecoderCallback(QString Debug, Callback Function, QAtomicInt *Ready,
                    void *Opaque1, void *Opaque2, void *Opaque3)
      : m_debug(std::move(Debug)),
        m_function(Function),
        m_ready(Ready),
        m_opaque1(Opaque1),
        m_opaque2(Opaque2),
        m_opaque3(Opaque3)
    {
    }

    QString m_debug;
    Callback m_function { nullptr };
    QAtomicInt *m_ready { nullptr };
    void* m_opaque1     { nullptr };
    void* m_opaque2     { nullptr };
    void* m_opaque3     { nullptr };
};

class MTV_PUBLIC MythPlayerVideoUI : public MythPlayerCaptionsUI
{
    Q_OBJECT

  signals:
    void RefreshVideoState();
    void VideoColourStateChanged(const MythVideoColourState& ColourState);

  public:
    MythPlayerVideoUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerVideoUI() override = default;

    const MythInteropGPU::InteropMap& GetInteropTypes() const;
    void HandleDecoderCallback(const QString& Debug, DecoderCallback::Callback Function,
                               void* Opaque1, void* Opaque2);

    void CheckAspectRatio(MythVideoFrame* Frame);

  public slots:
    void ProcessCallbacks();
    void SupportedAttributesChanged(PictureAttributeSupported Supported);
    void PictureAttributeChanged(PictureAttribute Attribute, int Value);
    void PictureAttributesUpdated(const std::map<PictureAttribute,int>& Values);

  protected slots:
    void ReinitOSD();
    void ToggleAdjustFill(AdjustFillMode Mode = kAdjustFill_Toggle);

  protected:
    bool InitVideo() override;

    DetectLetterbox m_detectLetterBox;

  private:
    Q_DISABLE_COPY(MythPlayerVideoUI)

    QMutex                   m_decoderCallbackLock;
    QVector<DecoderCallback> m_decoderCallbacks;
    MythVideoColourState     m_colourState;
    MythInteropGPU::InteropMap m_interopTypes;
};

#endif

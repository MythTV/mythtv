#ifndef MYTHPLAYERVIDEOUI_H
#define MYTHPLAYERVIDEOUI_H

// MythTV
#include "mythplayeraudioui.h"

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

class MTV_PUBLIC MythPlayerVideoUI : public MythPlayerAudioUI
{
    Q_OBJECT

  public:
    MythPlayerVideoUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerVideoUI() override = default;

    void HandleDecoderCallback(const QString& Debug, DecoderCallback::Callback Function,
                               void* Opaque1, void* Opaque2);

  public slots:
    void ProcessCallbacks();

  protected slots:
    void WindowResized(const QSize& /*Size*/);

  protected:
    bool InitVideo() override;

  private:
    Q_DISABLE_COPY(MythPlayerVideoUI)

    QMutex  m_decoderCallbackLock;
    QVector<DecoderCallback> m_decoderCallbacks;
};

#endif

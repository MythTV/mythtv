#ifndef AUDIOPULSEHANDLER_H
#define AUDIOPULSEHANDLER_H

#include <QThread>
#include <pulse/pulseaudio.h>

class PulseHandler
{
  public:
    enum PulseAction
    {
        kPulseSuspend = 0,
        kPulseResume,
        kPulseCleanup,
    };

    static bool Suspend(enum PulseAction action);
    static PulseHandler *g_pulseHandler;
    static bool          g_pulseHandlerActive;

   ~PulseHandler(void);
    bool Valid(void);

    pa_context_state m_ctx_state;
    pa_context      *m_ctx;
    int              m_pending_operations;

  private:
    PulseHandler(void);
    bool Init(void);
    bool SuspendInternal(bool suspend);

    pa_mainloop     *m_loop;
    bool             m_initialised;
    bool             m_valid;
    QThread         *m_thread;
};

#endif // AUDIOPULSEHANDLER_H

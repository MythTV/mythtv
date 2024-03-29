#include <QMutexLocker>
#include <QString>
#include <QMutex>
#include <QElapsedTimer>

#include <unistd.h> // for usleep

#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mthread.h"

#include "audiopulsehandler.h"

#define LOC QString("Pulse: ")

static inline bool IS_READY(pa_context_state arg)
{
    return ((PA_CONTEXT_READY      == arg) ||
            (PA_CONTEXT_FAILED     == arg) ||
            (PA_CONTEXT_TERMINATED == arg));
}

static QString state_to_string(pa_context_state state)
{
    QString ret = "Unknown";
    switch (state)
    {
        case PA_CONTEXT_UNCONNECTED:  ret = "Unconnected";  break;
        case PA_CONTEXT_CONNECTING:   ret = "Connecting";   break;
        case PA_CONTEXT_AUTHORIZING:  ret = "Authorizing";  break;
        case PA_CONTEXT_SETTING_NAME: ret = "Setting Name"; break;
        case PA_CONTEXT_READY:        ret = "Ready!";       break;
        case PA_CONTEXT_FAILED:       ret = "Failed";       break;
        case PA_CONTEXT_TERMINATED:   ret = "Terminated";   break;
    }
    return ret;
}

PulseHandler* PulseHandler::g_pulseHandler = nullptr;
bool          PulseHandler::g_pulseHandlerActive = false;

bool PulseHandler::Suspend(enum PulseAction action)
{
    // global lock around all access to our global singleton
    static QMutex s_globalLock;
    QMutexLocker locker(&s_globalLock);

    // cleanup the PulseAudio server connection if requested
    if (kPulseCleanup == action)
    {
        if (g_pulseHandler)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Cleaning up PulseHandler");
            delete g_pulseHandler;
            g_pulseHandler = nullptr;
        }
        return true;
    }

    static int s_iPulseRunning = -1;
    static QElapsedTimer s_time;
    static auto s_ePulseAction = PulseAction(-1);

    // Use the last result of IsPulseAudioRunning if within time
    if (s_time.isValid() && !s_time.hasExpired(30000))
    {
        if (!s_iPulseRunning)
            return false;

        // If the last action is repeated then do nothing
        if (action == s_ePulseAction)
            return true;
    }
    // NB IsPulseAudioRunning calls myth_system and can take up to 100mS
    else if (IsPulseAudioRunning())
    {
        s_iPulseRunning = 1;
        s_time.start();
    }
    else
    {
        // do nothing if PulseAudio is not currently running
        LOG(VB_AUDIO, LOG_INFO, LOC + "PulseAudio not running");
        s_iPulseRunning = 0;
        s_time.start();
        return false;
    }

    // make sure any pre-existing handler is still valid
    if (g_pulseHandler && !g_pulseHandler->Valid())
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + "PulseHandler invalidated. Deleting.");
        delete g_pulseHandler;
        g_pulseHandler = nullptr;
    }

    // create our handler
    if (!g_pulseHandler)
    {
        auto* handler = new PulseHandler();
        if (handler)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + "Created PulseHandler object");
            g_pulseHandler = handler;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to create PulseHandler object");
            return false;
        }
    }

    // enable processing of incoming callbacks
    g_pulseHandlerActive = true;
    bool result = g_pulseHandler->SuspendInternal(kPulseSuspend == action);
    // disable processing of incoming callbacks in case we delete/recreate our
    // instance due to a termination or other failure
    g_pulseHandlerActive = false;
    s_ePulseAction = action;
    return result;
}

static void StatusCallback(pa_context *ctx, void *userdata)
{
    // ignore any status updates while we're inactive, we can update
    // directly as needed
    if (!ctx || !PulseHandler::g_pulseHandlerActive)
        return;

    // validate the callback
    auto *handler = static_cast<PulseHandler*>(userdata);
    if (!handler)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Callback: no handler.");
        return;
    }

    if (handler->m_ctx != ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Callback: handler/context mismatch.");
        return;
    }

    if (handler != PulseHandler::g_pulseHandler)
    {
        LOG(VB_GENERAL, LOG_ERR,
                "Callback: returned handler is not the global handler.");
        return;
    }

    // update our status
    pa_context_state state = pa_context_get_state(ctx);
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Callback: State changed %1->%2")
        .arg(state_to_string(handler->m_ctxState), state_to_string(state)));
    handler->m_ctxState = state;
}

static void OperationCallback(pa_context *ctx, int success, void *userdata)
{
    if (!ctx)
        return;

    // ignore late updates but flag them as they may be an issue
    if (!PulseHandler::g_pulseHandlerActive)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Received a late/unexpected operation callback. Ignoring.");
        return;
    }

    // validate the callback
    auto *handler = static_cast<PulseHandler*>(userdata);
    if (!handler)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Operation: no handler.");
        return;
    }

    if (handler->m_ctx != ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Operation: handler/context mismatch.");
        return;
    }

    if (handler != PulseHandler::g_pulseHandler)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Operation: returned handler is not the global handler.");
        return;
    }

    // update the context
    handler->m_pendingOperations--;
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Operation: success %1 remaining %2")
            .arg(success).arg(handler->m_pendingOperations));
}

PulseHandler::~PulseHandler(void)
{
    // TODO - do we need to drain the context??

    LOG(VB_AUDIO, LOG_INFO, LOC + "Destroying PulseAudio handler");

    // is this correct?
    if (m_ctx)
    {
        pa_context_disconnect(m_ctx);
        pa_context_unref(m_ctx);
    }

    if (m_loop)
    {
        pa_signal_done();
        pa_mainloop_free(m_loop);
    }
}

bool PulseHandler::Valid(void)
{
    if (m_initialised && m_valid)
    {
        m_ctxState = pa_context_get_state(m_ctx);
        return PA_CONTEXT_READY == m_ctxState;
    }
    return false;
}

bool PulseHandler::Init(void)
{
    if (m_initialised)
        return m_valid;
    m_initialised = true;

    // Initialse our connection to the server
    m_loop = pa_mainloop_new();
    if (!m_loop)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get PulseAudio mainloop");
        return m_valid;
    }

    pa_mainloop_api *api = pa_mainloop_get_api(m_loop);
    if (!api)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get PulseAudio api");
        return m_valid;
    }

    if (pa_signal_init(api) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise signaling");
        return m_valid;
    }

    const char *client = "mythtv";
    m_ctx = pa_context_new(api, client);
    if (!m_ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create context");
        return m_valid;
    }

    // remember which thread created this object for later sanity debugging
    m_thread = QThread::currentThread();

    // we set the callback, connect and then run the main loop 'by hand'
    // until we've successfully connected (or not)
    pa_context_set_state_callback(m_ctx, StatusCallback, this);
    pa_context_connect(m_ctx, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr);
    int ret = 0;
    int tries = 0;
    while ((tries++ < 100) && !IS_READY(m_ctxState))
    {
        pa_mainloop_iterate(m_loop, 0, &ret);
        usleep(10000);
    }

    if (PA_CONTEXT_READY != m_ctxState)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Context not ready after 1000ms");
        return m_valid;
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + "Initialised handler");
    m_valid = true;
    return m_valid;
}

bool PulseHandler::SuspendInternal(bool suspend)
{
    // set everything up...
    if (!Init())
        return false;

    // just in case it all goes pete tong
    if (!is_current_thread(m_thread))
        LOG(VB_AUDIO, LOG_WARNING, LOC +
            "PulseHandler called from a different thread");

    QString action = suspend ? "suspend" : "resume";
    // don't bother to suspend a networked server
    if (!pa_context_is_local(m_ctx))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "PulseAudio server is remote. No need to " + action);
        return false;
    }

    // create and dispatch 2 operations to suspend or resume all current sinks
    // and all current sources
    m_pendingOperations = 2;
    pa_operation *operation_sink =
        pa_context_suspend_sink_by_index(
            m_ctx, PA_INVALID_INDEX, static_cast<int>(suspend), OperationCallback, this);
    pa_operation_unref(operation_sink);

    pa_operation *operation_source =
        pa_context_suspend_source_by_index(
            m_ctx, PA_INVALID_INDEX, static_cast<int>(suspend), OperationCallback, this);
    pa_operation_unref(operation_source);

    // run the loop manually and wait for the callbacks
    int count = 0;
    int ret = 0;
    while (m_pendingOperations && count++ < 100)
    {
        pa_mainloop_iterate(m_loop, 0, &ret);
        usleep(10000);
    }

    // a failure isn't necessarily disastrous
    if (m_pendingOperations)
    {
        m_pendingOperations = 0;
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to " + action);
        return false;
    }

    // rejoice
    LOG(VB_GENERAL, LOG_INFO, LOC + "PulseAudio " + action + " OK");
    return true;
}

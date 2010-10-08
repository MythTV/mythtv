#include "mythverbose.h"
#include "util.h"
#include "audiopulsehandler.h"

#define LOC QString("Pulse: ")
#define ERR QString("Pulse err: ")
#define WAR QString("Pulse Warning: ")

#define IS_READY(arg) ((PA_CONTEXT_READY      == arg) || \
                       (PA_CONTEXT_FAILED     == arg) || \
                       (PA_CONTEXT_TERMINATED == arg))

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

PulseHandler* PulseHandler::g_pulseHandler = NULL;
bool          PulseHandler::g_pulseHandlerActive = false;

bool PulseHandler::Suspend(enum PulseAction action)
{
    // global lock around all access to our global singleton
    static QMutex global_lock;
    QMutexLocker locker(&global_lock);

    // cleanup the PulseAudio server connection if requested
    if ((kPulseCleanup == action) && g_pulseHandler)
    {
        VERBOSE(VB_GENERAL, LOC + "Cleaning up PulseHandler");
        delete g_pulseHandler;
        g_pulseHandler = NULL;
        return true;
    }

    // do nothing if PulseAudio is not currently running
    if (!IsPulseAudioRunning())
    {
        VERBOSE(VB_AUDIO, LOC + "PulseAudio not running");
        return false;
    }

    // make sure any pre-existing handler is still valid
    if (g_pulseHandler && !g_pulseHandler->Valid())
    {
        VERBOSE(VB_AUDIO, LOC + "PulseHandler invalidated. Deleting.");
        delete g_pulseHandler;
        g_pulseHandler = NULL;
    }

    // create our handler
    if (!g_pulseHandler)
    {
        PulseHandler* handler = new PulseHandler();
        if (handler)
        {
            VERBOSE(VB_AUDIO, LOC + "Created PulseHandler object");
            g_pulseHandler = handler;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, ERR + "Failed to create PulseHandler object");
            return false;
        }
    }

    bool result;
    // enable processing of incoming callbacks
    g_pulseHandlerActive = true;
    result = g_pulseHandler->SuspendInternal(kPulseSuspend == action);
    // disable processing of incoming callbacks in case we delete/recreate our
    // instance due to a termination or other failure
    g_pulseHandlerActive = false;
    return result;
}

static void StatusCallback(pa_context *ctx, void *userdata)
{
    // ignore any status updates while we're inactive, we can update
    // directly as needed
    if (!ctx || !PulseHandler::g_pulseHandlerActive)
        return;

    // validate the callback
    PulseHandler *handler = static_cast<PulseHandler*>(userdata);
    if (!handler)
    {
        VERBOSE(VB_IMPORTANT, ERR + "Callback: no handler.");
        return;
    }

    if (handler->m_ctx != ctx)
    {
        VERBOSE(VB_IMPORTANT, ERR + "Callback: handler/context mismatch.");
        return;
    }

    if (handler != PulseHandler::g_pulseHandler)
    {
        VERBOSE(VB_IMPORTANT, ERR +
                "Callback: returned handler is not the global handler.");
        return;
    }

    // update our status
    pa_context_state state = pa_context_get_state(ctx);
    VERBOSE(VB_AUDIO, LOC + QString("Callback: State changed %1->%2")
            .arg(state_to_string(handler->m_ctx_state))
            .arg(state_to_string(state)));
    handler->m_ctx_state = state;
}

static void OperationCallback(pa_context *ctx, int success, void *userdata)
{
    if (!ctx)
        return;

    // ignore late updates but flag them as they may be an issue
    if (!PulseHandler::g_pulseHandlerActive)
    {
        VERBOSE(VB_IMPORTANT, WAR + "Received a late/unexpected operation "
                                    "callback. Ignoring.");
        return;
    }

    // validate the callback
    PulseHandler *handler = static_cast<PulseHandler*>(userdata);
    if (!handler)
    {
        VERBOSE(VB_IMPORTANT, ERR + "Operation: no handler.");
        return;
    }

    if (handler->m_ctx != ctx)
    {
        VERBOSE(VB_IMPORTANT, ERR + "Operation: handler/context mismatch.");
        return;
    }

    if (handler != PulseHandler::g_pulseHandler)
    {
        VERBOSE(VB_IMPORTANT, ERR +
                "Operation: returned handler is not the global handler.");
        return;
    }

    // update the context
    handler->m_pending_operations--;
    VERBOSE(VB_AUDIO, LOC + QString("Operation: success %1 remaining %2")
            .arg(success).arg(handler->m_pending_operations));
}

PulseHandler::PulseHandler(void)
  : m_ctx_state(PA_CONTEXT_UNCONNECTED), m_ctx(NULL), m_pending_operations(0),
    m_loop(NULL), m_initialised(false), m_valid(false),
    m_thread(NULL)
{
}

PulseHandler::~PulseHandler(void)
{
    // TODO - do we need to drain the context??

    VERBOSE(VB_AUDIO, LOC + "Destroying PulseAudio handler");

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
        m_ctx_state = pa_context_get_state(m_ctx);
        return PA_CONTEXT_READY == m_ctx_state;
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
        VERBOSE(VB_IMPORTANT, ERR + "Failed to get PulseAudio mainloop");
        return m_valid;
    }

    pa_mainloop_api *api = pa_mainloop_get_api(m_loop);
    if (!api)
    {
        VERBOSE(VB_IMPORTANT, ERR + "Failed to get PulseAudio api");
        return m_valid;
    }

    if (pa_signal_init(api) != 0)
    {
        VERBOSE(VB_IMPORTANT, ERR + "Failed to initialise signaling");
        return m_valid;
    }

    const char *client = "mythtv";
    m_ctx = pa_context_new(api, client);
    if (!m_ctx)
    {
        VERBOSE(VB_IMPORTANT, ERR + "Failed to create context");
        return m_valid;
    }

    // remember which thread created this object for later sanity debugging
    m_thread = QThread::currentThread();

    // we set the callback, connect and then run the main loop 'by hand'
    // until we've successfully connected (or not)
    pa_context_set_state_callback(m_ctx, StatusCallback, this);
    pa_context_connect(m_ctx, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);
    int ret = 0;
    int tries = 0;
    while ((tries++ < 100) && !IS_READY(m_ctx_state))
    {
        pa_mainloop_iterate(m_loop, 0, &ret);
        usleep(10000);
    }

    if (PA_CONTEXT_READY != m_ctx_state)
    {
        VERBOSE(VB_IMPORTANT, ERR + "Context not ready after 1000ms");
        return m_valid;
    }

    VERBOSE(VB_AUDIO, LOC + "Initialised handler");
    m_valid = true;
    return m_valid;
}

bool PulseHandler::SuspendInternal(bool suspend)
{
    // set everything up...
    if (!Init())
        return false;

    // just in case it all goes pete tong
    if (QThread::currentThread() != m_thread)
        VERBOSE(VB_AUDIO, WAR + "PulseHandler called from a different thread");

    QString action = suspend ? "suspend" : "resume";
    // don't bother to suspend a networked server
    if (!pa_context_is_local(m_ctx))
    {
        VERBOSE(VB_IMPORTANT, ERR +
                "PulseAudio server is remote. No need to " + action);
        return false;
    }

    // create and dispatch 2 operations to suspend or resume all current sinks
    // and all current sources
    m_pending_operations = 2;
    pa_operation *operation_sink =
        pa_context_suspend_sink_by_index(
            m_ctx, PA_INVALID_INDEX, suspend, OperationCallback, this);
    pa_operation_unref(operation_sink);

    pa_operation *operation_source =
        pa_context_suspend_source_by_index(
            m_ctx, PA_INVALID_INDEX, suspend, OperationCallback, this);
    pa_operation_unref(operation_source);

    // run the loop manually and wait for the callbacks
    int count = 0;
    int ret = 0;
    while (m_pending_operations && count++ < 100)
    {
        pa_mainloop_iterate(m_loop, 0, &ret);
        usleep(10000);
    }

    // a failure isn't necessarily disastrous
    if (m_pending_operations)
    {
        m_pending_operations = 0;
        VERBOSE(VB_IMPORTANT, ERR + "Failed to " + action);
        return false;
    }

    // rejoice
    VERBOSE(VB_GENERAL, LOC + "PulseAudio " + action + " OK");
    return true;
}

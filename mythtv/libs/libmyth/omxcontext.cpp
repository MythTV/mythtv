#include "omxcontext.h"
using namespace omxcontext;

#include <cstddef>
#include <cassert>

#include <OMX_Core.h>
#ifdef USING_BROADCOM
#include <bcm_host.h>
#endif

#include <QRegExp>
#include <QAtomicInt>

#include "mythlogging.h"

#define LOC QString("OMX:%1 ").arg(Id())
#define LOCA QString("OMX: ")
#define LOCB(c) QString("OMX:%1 ").arg((c).Id())


// Stringize a macro
#define _STR(s) #s
#define STR(s) _STR(s)

// Compile time assertion
#undef STATIC_ASSERT
#ifdef __GNUC__
# define STATIC_ASSERT(e) extern char _dummy[(e) ? 1 : -1] __attribute__((unused))
#else
# define STATIC_ASSERT(e) extern char _dummy[(e) ? 1 : -1]
#endif

#ifdef USING_BROADCOM
// NB The Broadcom BCB2835 driver is compiled with OMX_SKIP64BIT defined.
// If OMX_SKIP64BIT is not defined, OMX_TICKS becomes a 64-bit type which
// alters the size & layout of OMX_BUFFERHEADERTYPE
STATIC_ASSERT(offsetof(OMX_BUFFERHEADERTYPE, nTimeStamp) == 13 * 4);
STATIC_ASSERT(offsetof(OMX_BUFFERHEADERTYPE, nFlags) == 15 * 4);
STATIC_ASSERT(sizeof(OMX_BUFFERHEADERTYPE) == 18 * 4);
#endif // USING_BROADCOM


/*
 * Types
 */
namespace {

#ifdef USING_BROADCOM
struct BroadcomLib
{
    BroadcomLib()
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOCA + "bcm_host_init...");
        bcm_host_init();
    }
    ~BroadcomLib()
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOCA + "bcm_host_deinit...");
        bcm_host_deinit();
    }
};
#endif

} // namespace


/*
 * Module data
 */
static QAtomicInt s_iRefcnt;
static int s_ref;


/**
 * OMXComponent
 */
// static
bool OMXComponent::IncrRef()
{
    static bool s_bOK = false;

    if (s_iRefcnt.fetchAndAddOrdered(1) == 0)
    {
#ifdef USING_BROADCOM
        // BUG: Calling bcm_host_init/bcm_host_deinit more than once causes
        // occasional InsufficientResources errors
        static BroadcomLib s_bBcmInit;
#endif

        LOG(VB_GENERAL, LOG_DEBUG, LOCA + "OMX_Init...");
        OMX_ERRORTYPE e = OMX_Init();
        if (e == OMX_ErrorNone)
        {
            s_bOK = true;
            LOG(VB_GENERAL, LOG_DEBUG, LOCA + "OMX_Init done");
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOCA + QString("OMX_Init error %1")
                .arg(Error2String(e)));
            s_bOK = false;
        }
    }
    return s_bOK;
}

//static
void OMXComponent::DecrRef()
{
    if (s_iRefcnt.fetchAndAddOrdered(-1) == 1)
    {
#ifdef USING_BELLAGIO
        // version 0.9.3 throws SEGVs after repeated init/deinit
        s_iRefcnt.fetchAndAddOrdered(1);
#else
        LOG(VB_GENERAL, LOG_DEBUG, LOCA + "OMX_DeInit...");
        OMX_ERRORTYPE e = OMX_Deinit();
        if (e == OMX_ErrorNone)
            LOG(VB_GENERAL, LOG_DEBUG, LOCA + "OMX_DeInit done");
        else
            LOG(VB_GENERAL, LOG_ERR, LOCA + QString("OMX_Deinit error %1")
                .arg(Error2String(e)));
#endif
    }
}

OMXComponent::OMXComponent(const QString &name, OMXComponentCtx &cb) :
    m_ref(s_ref++), m_cb(cb)
{
    if (!IncrRef())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid OpenMAX context");
        return;
    }

    if (name.isEmpty())
        return;

    OMX_ERRORTYPE e = GetComponent(this, QRegExp(name + "$", Qt::CaseInsensitive));
    if (e != OMX_ErrorNone)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Can't find component: " + name);

        // List available components
        LOG(VB_GENERAL, LOG_NOTICE, "Components available:");
        EnumComponents(QRegExp(), LOG_NOTICE, VB_GENERAL);
        return;
    }
    m_state = OMX_StateLoaded;

    char buf[128];
    OMX_VERSIONTYPE ver, spec;
    OMX_UUIDTYPE uuid;
    if (OMX_GetComponentVersion(Handle(), buf, &ver, &spec, &uuid) != OMX_ErrorNone)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString(
            "Attached %1 ver=%2.%3.%4.%5 spec=%6.%7.%8.%9")
        .arg(buf)
        .arg(ver.s.nVersionMajor).arg(ver.s.nVersionMinor)
        .arg(ver.s.nRevision).arg(ver.s.nStep)
        .arg(spec.s.nVersionMajor).arg(spec.s.nVersionMinor)
        .arg(spec.s.nRevision).arg(spec.s.nStep));
}

// virtual
OMXComponent::~OMXComponent()
{
    Shutdown();

    if (m_handle)
    {
        OMX_ERRORTYPE e = OMX_FreeHandle(m_handle);
        m_handle = nullptr;
        m_state = OMX_StateInvalid;
        if (e != OMX_ErrorNone)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("OMX_FreeHandle error %1")
                .arg(Error2String(e)));
    }

    DecrRef();
}

void OMXComponent::Shutdown()
{
    if (!m_handle)
        return;

    if (m_state > OMX_StateIdle)
        SetState(OMX_StateIdle, 50);

    if (m_state > OMX_StateLoaded)
    {
        OMXComponentCB<OMXComponent> cb(this, &OMXComponent::ReleaseBuffers);
        SetState(OMX_StateLoaded, 50, &cb);
    }

    for (unsigned port = 0; port < Ports(); ++port)
    {
        OMX_ERRORTYPE e = OMX_ErrorNone;
        if (PortDef(port).eDir == OMX_DirOutput)
            e = OMX_SetupTunnel(Handle(), Base() + port, nullptr, 0);
        else
            e = OMX_SetupTunnel(nullptr, 0, Handle(), Base() + port);
        if (e != OMX_ErrorNone)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString(
                "OMX_SetupTunnel shutdown error %1").arg(Error2String(e)));
    }
}

OMX_ERRORTYPE OMXComponent::ReleaseBuffers()
{
    m_cb.ReleaseBuffers(*this);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXComponent::Init(OMX_INDEXTYPE type)
{
    if (!m_handle)
        return OMX_ErrorInvalidComponent;

    if (m_state != OMX_StateLoaded)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " in incorrect state");
        return OMX_ErrorInvalidState;
    }

    switch( type)
    {
      case OMX_IndexParamVideoInit:
      case OMX_IndexParamAudioInit:
      case OMX_IndexParamImageInit:
      case OMX_IndexParamOtherInit:
        break;
      default:
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " invalid type");
        return OMX_ErrorBadParameter;
    }

    // Get port base index and count
    OMX_PORT_PARAM_TYPE port;
    OMX_DATA_INIT(port);
    OMX_ERRORTYPE e = OMX_GetParameter(m_handle, type, &port);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + QString(" error %1")
            .arg(Error2String(e)));
        return e;
    }

    m_base = port.nStartPortNumber;
    if (port.nPorts < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " found no ports");
        return OMX_ErrorPortsNotCompatible;
    }

    m_portdefs.resize(port.nPorts);

    // Read the default port definitions
    for (size_t i = 0; i < port.nPorts; ++i)
    {
        e = GetPortDef(i);
        if (OMX_ErrorNone != e)
        {
            m_portdefs.clear();
            return e;
        }
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXComponent::GetPortDef(unsigned index)
{
    if (!m_handle)
        return OMX_ErrorInvalidComponent;

    if ((int)index >= m_portdefs.size())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + QString(" invalid index %1")
            .arg(index));
        return OMX_ErrorBadParameter;
    }

    OMX_PARAM_PORTDEFINITIONTYPE &def = m_portdefs[index];
    OMX_DATA_INIT(def);
    def.nPortIndex = m_base + index;
    OMX_ERRORTYPE e = OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition,
                                        &def);
    if (e != OMX_ErrorNone)
        LOG(VB_GENERAL, LOG_ERR, LOC + QString(
                "Get IndexParamPortDefinition error %1").arg(Error2String(e)));
    return e;
}

const OMX_PARAM_PORTDEFINITIONTYPE &OMXComponent::PortDef(unsigned index) const
{
    assert((int)index < m_portdefs.size());
    return m_portdefs[index];
}

OMX_PARAM_PORTDEFINITIONTYPE &OMXComponent::PortDef(unsigned index)
{
    assert((int)index < m_portdefs.size());
    return m_portdefs[index];
}

void OMXComponent::ShowPortDef(unsigned index, LogLevel_t level, uint64_t mask) const
{
    const OMX_PARAM_PORTDEFINITIONTYPE &def = PortDef(index);

    LOG(mask, level, LOC + QString(
            "Port %1: %2, bufs=%3(%4) bufsize=%5@%9 %8, %6, %7")
        .arg(def.nPortIndex)
        .arg(def.eDir == OMX_DirInput ? "input" : "output")
        .arg(def.nBufferCountActual).arg(def.nBufferCountMin)
        .arg(def.nBufferSize)
        .arg(def.bEnabled ? "enabled" : "disabled")
        .arg(def.bPopulated ? "populated" : "unpopulated")
        .arg(def.bBuffersContiguous ? "contiguous" : "discontiguous")
        .arg(def.nBufferAlignment) );

    switch (def.eDomain)
    {
    case OMX_PortDomainVideo:
        LOG(mask, level, LOC + QString(
                "Port %1: video, w=%2 h=%3 stride=%4 sliceH=%5 bps=%6"
                " fps=%7 compress=%8 enc=%9")
            .arg(def.nPortIndex)
            .arg(def.format.video.nFrameWidth)
            .arg(def.format.video.nFrameHeight)
            .arg(def.format.video.nStride)
            .arg(def.format.video.nSliceHeight)
            .arg(def.format.video.nBitrate)
            .arg(def.format.video.xFramerate / 65536.0)
            .arg(Coding2String(def.format.video.eCompressionFormat))
            .arg(Format2String(def.format.video.eColorFormat)) );
        break;
    case OMX_PortDomainAudio:
        LOG(mask, level, LOC + QString(
                "Port %1: audio, encoding=%2")
            .arg(def.nPortIndex)
            .arg(Coding2String(def.format.audio.eEncoding)) );
        break;
    case OMX_PortDomainImage:
        LOG(mask, level, LOC + QString(
                "Port %1: image, w=%2 h=%3 stride=%4 sliceH=%5"
                " compress=%6 enc=%7")
            .arg(def.nPortIndex)
            .arg(def.format.image.nFrameWidth)
            .arg(def.format.image.nFrameHeight)
            .arg(def.format.image.nStride)
            .arg(def.format.image.nSliceHeight)
            .arg(Coding2String(def.format.image.eCompressionFormat))
            .arg(Format2String(def.format.image.eColorFormat)) );
        break;
    case OMX_PortDomainOther:
        LOG(mask, level, LOC + QString("Port %1: other")
            .arg(def.nPortIndex) );
        break;
    default:
        LOG(mask, level, LOC + QString("Port %1: domain 0x%2")
            .arg(def.nPortIndex).arg(def.eDomain,0,16) );
        break;
    }
}

static inline void ShowFormat(const OMXComponent &cmpnt, LogLevel_t level,
    uint64_t mask, const OMX_VIDEO_PARAM_PORTFORMATTYPE &fmt)
{
    LOG(mask, level, LOCB(cmpnt) + QString(
            "Port %1 #%2: compress=%3 colour=%4")
        .arg(fmt.nPortIndex).arg(fmt.nIndex)
        .arg(Coding2String(fmt.eCompressionFormat))
        .arg(Format2String(fmt.eColorFormat)) );
}

static inline void ShowFormat(const OMXComponent &cmpnt, LogLevel_t level,
    uint64_t mask, const OMX_IMAGE_PARAM_PORTFORMATTYPE &fmt)
{
    LOG(mask, level, LOCB(cmpnt) + QString(
            "Port %1 #%2: compress=%3 colour=%4")
        .arg(fmt.nPortIndex).arg(fmt.nIndex)
        .arg(Coding2String(fmt.eCompressionFormat))
        .arg(Format2String(fmt.eColorFormat)) );
}

static inline void ShowFormat(const OMXComponent &cmpnt, LogLevel_t level,
    uint64_t mask, const OMX_AUDIO_PARAM_PORTFORMATTYPE &fmt)
{
    LOG(mask, level, LOCB(cmpnt) + QString(
            "Port %1 #%2: encoding=%3")
        .arg(fmt.nPortIndex).arg(fmt.nIndex)
        .arg(Coding2String(fmt.eEncoding)) );
}

static inline void ShowFormat(const OMXComponent &cmpnt, LogLevel_t level,
    uint64_t mask, const OMX_OTHER_PARAM_PORTFORMATTYPE &fmt)
{
    LOG(mask, level, LOCB(cmpnt) + QString(
            "Port %1 #%2: encoding=%3")
        .arg(fmt.nPortIndex).arg(fmt.nIndex)
        .arg(Other2String(fmt.eFormat)) );
}

template<typename T, OMX_INDEXTYPE type>
void ShowFormats(const OMXComponent &cmpnt, unsigned n, LogLevel_t level, uint64_t mask)
{
    if  (!VERBOSE_LEVEL_CHECK(mask, level))
        return;

    T fmt {};
    OMX_DATA_INIT(fmt);
    fmt.nPortIndex = cmpnt.Base() + n;

    OMX_ERRORTYPE e = OMX_ErrorNone;
    for (OMX_U32 index = 0; e == OMX_ErrorNone && index < 100; ++index)
    {
        fmt.nIndex = index;
        e = OMX_GetParameter(cmpnt.Handle(), type, &fmt);
        switch (e)
        {
        case OMX_ErrorNone:
            ShowFormat(cmpnt, level, mask, fmt);
            break;
        case OMX_ErrorNoMore:
            break;
        default:
            LOG(mask, LOG_ERR, LOCB(cmpnt) + QString(
                    "GetParameter PortFormat error %1")
                .arg(Error2String(e)));
            break;
        }
    }
}

void OMXComponent::ShowFormats(unsigned index, LogLevel_t level, uint64_t mask) const
{
    if (PortDef(index).eDomain == OMX_PortDomainVideo)
        ::ShowFormats<OMX_VIDEO_PARAM_PORTFORMATTYPE, OMX_IndexParamVideoPortFormat>(
            *this, index, level, mask);
    else if (PortDef(index).eDomain == OMX_PortDomainImage)
        ::ShowFormats<OMX_IMAGE_PARAM_PORTFORMATTYPE, OMX_IndexParamImagePortFormat>(
            *this, index, level, mask);
    else if (PortDef(index).eDomain == OMX_PortDomainAudio)
        ::ShowFormats<OMX_AUDIO_PARAM_PORTFORMATTYPE, OMX_IndexParamAudioPortFormat>(
            *this, index, level, mask);
    else if (PortDef(index).eDomain == OMX_PortDomainOther)
        ::ShowFormats<OMX_OTHER_PARAM_PORTFORMATTYPE, OMX_IndexParamOtherPortFormat>(
            *this, index, level, mask);
    else
        LOG(mask, LOG_ERR, LOC + QString("Unknown port domain 0x%1")
            .arg(PortDef(index).eDomain,0,16));
}

OMX_ERRORTYPE OMXComponent::SetState(OMX_STATETYPE state, int ms, OMXComponentAbstractCB *cb)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("SetState %1")
        .arg(State2String(state)));

    if (!m_handle)
        return OMX_ErrorInvalidComponent;

    QMutexLocker lock(&m_state_lock);
    if (m_state == state)
        return OMX_ErrorSameState;
    lock.unlock();

    OMX_ERRORTYPE e = SendCommand(OMX_CommandStateSet, state, nullptr, ms, cb);
    if (e != OMX_ErrorNone)
        return e;

    lock.relock();
    if (m_state != state)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString(
                "SetState %1 not set").arg(State2String(state)));
        return OMX_ErrorNotReady;
    }

    return OMX_ErrorNone;
}

OMX_STATETYPE OMXComponent::GetState()
{
    QMutexLocker lock(&m_state_lock);
    if (m_handle)
        OMX_GetState(m_handle, &m_state);
    return m_state;
}

OMX_ERRORTYPE OMXComponent::SendCommand(OMX_COMMANDTYPE cmd,
    OMX_U32 nParam, void *pCmdData, int ms, OMXComponentAbstractCB *cb)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + __func__ + QString(" %1 %2 begin")
        .arg(Command2String(cmd)).arg(nParam) );

    if (!m_handle)
        return OMX_ErrorInvalidComponent;

    while (m_state_sema.tryAcquire())
        ;
    (void)LastError();

    OMX_ERRORTYPE e = OMX_SendCommand(m_handle, cmd, nParam, pCmdData);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + QString(" %1 %2")
            .arg(Command2String(cmd)).arg(Error2String(e)) );
        return e;
    }

    // Check for immediate errors
    e = LastError();
    if (e != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + __func__ + QString(" %1 %2 %3")
            .arg(Command2String(cmd)).arg(nParam).arg(Error2String(e)) );
        return e;
    }

    // Callback
    if (cb)
    {
        e = cb->Action(this);
        if (e != OMX_ErrorNone)
            return e;
    }

    // Await command completion
    if (!m_state_sema.tryAcquire(1, ms))
    {
        if (ms == 0)
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + __func__ + QString(" %1 %2 pending")
                .arg(Command2String(cmd)).arg(nParam) );
        else
            LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + QString(" %1 %2 timed out")
                .arg(Command2String(cmd)).arg(nParam) );
        return OMX_ErrorTimeout;
    }

    e = LastError();
    if (e != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + __func__ + QString(" %1 %2 final %3")
            .arg(Command2String(cmd)).arg(nParam).arg(Error2String(e)) );
        return e;
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + __func__ + QString(" %1 %2 end")
        .arg(Command2String(cmd)).arg(nParam) );
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXComponent::WaitComplete(int ms)
{
    if (!m_state_sema.tryAcquire(1, ms))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + __func__ + " Timed out");
        return OMX_ErrorTimeout;
    }

    OMX_ERRORTYPE e = LastError();
    if (e != OMX_ErrorNone)
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " " + Error2String(e));
    return e;
}

OMX_ERRORTYPE OMXComponent::LastError()
{
    QMutexLocker lock(&m_state_lock);
    OMX_ERRORTYPE e = m_error;
    m_error = OMX_ErrorNone;
    return e;
}

/*
 * Get an OpenMAX component by name
 */
// static
OMX_ERRORTYPE OMXComponent::GetComponent(OMXComponent *cmpnt, const QRegExp &re,
    LogLevel_t level, uint64_t mask)
{
    if (!cmpnt && !VERBOSE_LEVEL_CHECK(mask, level))
        return OMX_ErrorNone;

    char buf[128];

    OMX_ERRORTYPE e = OMX_ErrorNone;
    for (int i = 0; e == OMX_ErrorNone; ++i)
    {
        e = OMX_ComponentNameEnum(buf, sizeof buf, i);
        switch (e)
        {
          case OMX_ErrorNone:
            if (!QString(buf).contains(re))
                break;

            if (cmpnt)
            {
                OMX_CALLBACKTYPE cb = {&OMXComponent::EventCB,
                    &OMXComponent::EmptyBufferCB, &OMXComponent::FillBufferCB};
                return OMX_GetHandle(&cmpnt->m_handle, buf, cmpnt, &cb);
            }

            LOG(mask, level, LOCA + QString("%1"). arg(buf));
            break;

          case OMX_ErrorNoMore:
            break;
          default:
            break;
        }
    }
    return e;
}

/*
 * OpenMAX callbacks
 * CAUTION!! Context undefined
*/
//static
OMX_ERRORTYPE OMXComponent::EventCB(
    OMX_IN OMX_HANDLETYPE /*pHandle*/,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_EVENTTYPE eEvent,
    OMX_IN OMX_U32 nData1,
    OMX_IN OMX_U32 nData2,
    OMX_IN OMX_PTR pEventData)
{
    OMXComponent *omx = reinterpret_cast<OMXComponent* >(pAppData);
    return omx->m_cb.Event(*omx, eEvent, nData1, nData2, pEventData);
}

//static
OMX_ERRORTYPE OMXComponent::EmptyBufferCB(
    OMX_IN OMX_HANDLETYPE /*pHandle*/,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_BUFFERHEADERTYPE *hdr)
{
    OMXComponent *omx = reinterpret_cast<OMXComponent* >(pAppData);
    return omx->m_cb.EmptyBufferDone(*omx, hdr);
}

//static
OMX_ERRORTYPE OMXComponent::FillBufferCB(
    OMX_IN OMX_HANDLETYPE /*pHandle*/,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_BUFFERHEADERTYPE *hdr)
{
    OMXComponent *omx = reinterpret_cast<OMXComponent* >(pAppData);
    return omx->m_cb.FillBufferDone(*omx, hdr);
}


/**
 * OMXComponentCtx
 */
OMX_ERRORTYPE OMXComponentCtx::Event( OMXComponent &cmpnt,
    OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2, OMX_PTR /*pEventData*/)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOCB(cmpnt) + QString("%1 0x%2 0x%3")
        .arg(Event2String(eEvent)).arg(nData1,0,16).arg(nData2,0,16) );

    switch(eEvent)
    {
      case OMX_EventCmdComplete:
        if (!cmpnt.m_state_lock.tryLock(500))
        {
            LOG(VB_GENERAL, LOG_CRIT, LOCB(cmpnt) + QString(
                    "EventCmdComplete %1 deadlock")
                .arg(Command2String(OMX_COMMANDTYPE(nData1))));
            break;
        }
        switch (nData1)
        {
          case OMX_CommandStateSet:
            cmpnt.m_state = OMX_STATETYPE(nData2);
            cmpnt.m_error = OMX_ErrorNone;
            break;
          case OMX_CommandFlush:
          case OMX_CommandPortDisable:
          case OMX_CommandPortEnable:
          case OMX_CommandMarkBuffer:
            // nData2 = port index
            cmpnt.m_error = OMX_ErrorNone;
            break;
          default:
            LOG(VB_GENERAL, LOG_WARNING, LOCB(cmpnt) + QString(
                    "EventCmdComplete unknown %1")
                .arg(Command2String(OMX_COMMANDTYPE(nData1))));
            break;
        }
        cmpnt.m_state_lock.unlock();
        cmpnt.m_state_sema.tryAcquire();
        cmpnt.m_state_sema.release();
        break;

      case OMX_EventError:
        if (!cmpnt.m_state_lock.tryLock(500))
        {
            LOG(VB_GENERAL, LOG_CRIT, LOCB(cmpnt) + QString(
                    "EventError %1 deadlock")
                .arg(Error2String(OMX_ERRORTYPE(nData1))));
            break;
        }
        switch (nData1)
        {
          case OMX_ErrorInvalidState:
            LOG(VB_GENERAL, LOG_ERR, LOCB(cmpnt) + "EventError: InvalidState");
            cmpnt.m_state = OMX_StateInvalid;
            break;
          case OMX_ErrorResourcesPreempted:
            LOG(VB_GENERAL, LOG_ERR, LOCB(cmpnt) + "EventError: ResourcesPreempted");
            cmpnt.m_state = OMX_StateIdle;
            break;
          case OMX_ErrorResourcesLost:
            LOG(VB_GENERAL, LOG_ERR, LOCB(cmpnt) + "EventError: ResourcesLost");
            cmpnt.m_state = OMX_StateLoaded;
            break;
          case OMX_ErrorUnsupportedSetting:
            LOG(VB_PLAYBACK, LOG_ERR, LOCB(cmpnt) + "EventError: UnsupportedSetting");
            break;
          default:
            LOG(VB_GENERAL, LOG_ERR, LOCB(cmpnt) + QString("EventError: %1")
                .arg(Error2String(OMX_ERRORTYPE(nData1))));
            break;
        }
        cmpnt.m_error = OMX_ERRORTYPE(nData1);
        cmpnt.m_state_lock.unlock();
        cmpnt.m_state_sema.tryAcquire();
        cmpnt.m_state_sema.release();
        break;

      case OMX_EventMark:
        // pEventData = linked data
        break;

      case OMX_EventPortSettingsChanged:
        LOG(VB_GENERAL, LOG_NOTICE, LOCB(cmpnt) + QString(
                "EventPortSettingsChanged port %1")
            .arg(nData1));
        break;

      case OMX_EventBufferFlag:
#if 0
        LOG(VB_PLAYBACK, LOG_DEBUG, LOCB(cmpnt) + QString(
                "EventBufferFlag: port=%1 flags=%2")
            .arg(nData1).arg(HeaderFlags(nData2)) );
#endif
        break;

      case OMX_EventResourcesAcquired:
      case OMX_EventComponentResumed:
      case OMX_EventDynamicResourcesAvailable:
      case OMX_EventPortFormatDetected:
      //case OMX_EventParamOrConfigChanged:
        break;

      default:
        break;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXComponentCtx::EmptyBufferDone(OMXComponent &cmpnt,
    OMX_BUFFERHEADERTYPE *hdr)
{
    LOG(VB_GENERAL, LOG_NOTICE, LOCB(cmpnt) + __func__ + QString(" 0x%1")
        .arg(quintptr(hdr),0,16));
    assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
    assert(hdr->nVersion.nVersion == OMX_VERSION);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXComponentCtx::FillBufferDone(OMXComponent &cmpnt,
    OMX_BUFFERHEADERTYPE *hdr)
{
    LOG(VB_GENERAL, LOG_NOTICE, LOCB(cmpnt) + __func__ + QString(" 0x%1")
        .arg(quintptr(hdr),0,16));
    assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
    assert(hdr->nVersion.nVersion == OMX_VERSION);
    return OMX_ErrorNone;
}


namespace omxcontext {

#define CASE2STR(f) case f: return STR(f)

const char *Coding2String(OMX_VIDEO_CODINGTYPE eCompressionFormat)
{
    switch(eCompressionFormat)
    {
        CASE2STR(OMX_VIDEO_CodingUnused);
        CASE2STR(OMX_VIDEO_CodingAutoDetect);
        CASE2STR(OMX_VIDEO_CodingMPEG2);
        CASE2STR(OMX_VIDEO_CodingH263);
        CASE2STR(OMX_VIDEO_CodingMPEG4);
        CASE2STR(OMX_VIDEO_CodingWMV);
        CASE2STR(OMX_VIDEO_CodingRV);
        CASE2STR(OMX_VIDEO_CodingAVC);
        CASE2STR(OMX_VIDEO_CodingMJPEG);
#ifdef USING_BROADCOM
        CASE2STR(OMX_VIDEO_CodingVP6);
        CASE2STR(OMX_VIDEO_CodingVP7);
        CASE2STR(OMX_VIDEO_CodingVP8);
        CASE2STR(OMX_VIDEO_CodingYUV);
        CASE2STR(OMX_VIDEO_CodingSorenson);
        CASE2STR(OMX_VIDEO_CodingTheora);
        CASE2STR(OMX_VIDEO_CodingMVC);
#endif
        default:
            // Quiet compiler warning.
            break;
    }
    static char s_buf[32];
    return strcpy(s_buf, qPrintable(QString("VIDEO_Coding 0x%1")
                                    .arg(eCompressionFormat,0,16)));
}

const char *Coding2String(OMX_IMAGE_CODINGTYPE eCompressionFormat)
{
    switch(eCompressionFormat)
    {
        CASE2STR(OMX_IMAGE_CodingUnused);
        CASE2STR(OMX_IMAGE_CodingAutoDetect);
        CASE2STR(OMX_IMAGE_CodingJPEG);
        CASE2STR(OMX_IMAGE_CodingJPEG2K);
        CASE2STR(OMX_IMAGE_CodingEXIF);
        CASE2STR(OMX_IMAGE_CodingTIFF);
        CASE2STR(OMX_IMAGE_CodingGIF);
        CASE2STR(OMX_IMAGE_CodingPNG);
        CASE2STR(OMX_IMAGE_CodingLZW);
        CASE2STR(OMX_IMAGE_CodingBMP);
#ifdef USING_BROADCOM
        CASE2STR(OMX_IMAGE_CodingTGA);
        CASE2STR(OMX_IMAGE_CodingPPM);
#endif
        default:
            // Quiet compiler warning.
            break;
    }
    static char s_buf[32];
    return strcpy(s_buf, qPrintable(QString("IMAGE_Coding 0x%1")
                                    .arg(eCompressionFormat,0,16)));
}

const char *Coding2String(OMX_AUDIO_CODINGTYPE eEncoding)
{
    switch(eEncoding)
    {
        CASE2STR(OMX_AUDIO_CodingUnused);
        CASE2STR(OMX_AUDIO_CodingAutoDetect);
        CASE2STR(OMX_AUDIO_CodingPCM);
        CASE2STR(OMX_AUDIO_CodingADPCM);
        CASE2STR(OMX_AUDIO_CodingAMR);
        CASE2STR(OMX_AUDIO_CodingGSMFR);
        CASE2STR(OMX_AUDIO_CodingGSMEFR);
        CASE2STR(OMX_AUDIO_CodingGSMHR);
        CASE2STR(OMX_AUDIO_CodingPDCFR);
        CASE2STR(OMX_AUDIO_CodingPDCEFR);
        CASE2STR(OMX_AUDIO_CodingPDCHR);
        CASE2STR(OMX_AUDIO_CodingTDMAFR);
        CASE2STR(OMX_AUDIO_CodingTDMAEFR);
        CASE2STR(OMX_AUDIO_CodingQCELP8);
        CASE2STR(OMX_AUDIO_CodingQCELP13);
        CASE2STR(OMX_AUDIO_CodingEVRC);
        CASE2STR(OMX_AUDIO_CodingSMV);
        CASE2STR(OMX_AUDIO_CodingG711);
        CASE2STR(OMX_AUDIO_CodingG723);
        CASE2STR(OMX_AUDIO_CodingG726);
        CASE2STR(OMX_AUDIO_CodingG729);
        CASE2STR(OMX_AUDIO_CodingAAC);
        CASE2STR(OMX_AUDIO_CodingMP3);
        CASE2STR(OMX_AUDIO_CodingSBC);
        CASE2STR(OMX_AUDIO_CodingVORBIS);
        CASE2STR(OMX_AUDIO_CodingWMA);
        CASE2STR(OMX_AUDIO_CodingRA);
        CASE2STR(OMX_AUDIO_CodingMIDI);
#ifdef USING_BROADCOM
        CASE2STR(OMX_AUDIO_CodingFLAC);
        CASE2STR(OMX_AUDIO_CodingDDP); // Any variant of Dolby Digital Plus
        CASE2STR(OMX_AUDIO_CodingDTS);
        CASE2STR(OMX_AUDIO_CodingWMAPRO);
        CASE2STR(OMX_AUDIO_CodingATRAC3);
        CASE2STR(OMX_AUDIO_CodingATRACX);
        CASE2STR(OMX_AUDIO_CodingATRACAAL);
#endif
        default:
            // Quiet compiler warning.
            break;
    }
    static char s_buf[32];
    return strcpy(s_buf, qPrintable(QString("AUDIO_Coding 0x%1")
                                    .arg(eEncoding,0,16)));
}

const char *Other2String(OMX_OTHER_FORMATTYPE eFormat)
{
    switch(eFormat)
    {
        CASE2STR(OMX_OTHER_FormatTime);
        CASE2STR(OMX_OTHER_FormatPower);
        CASE2STR(OMX_OTHER_FormatStats);
        CASE2STR(OMX_OTHER_FormatBinary);
#ifdef USING_BROADCOM
        CASE2STR(OMX_OTHER_FormatText);
        CASE2STR(OMX_OTHER_FormatTextSKM2);
        CASE2STR(OMX_OTHER_FormatText3GP5);
#endif
        default:
            // Quiet compiler warning.
            break;
    }
    static char s_buf[32];
    return strcpy(s_buf, qPrintable(QString("Other format 0x%1")
                                    .arg(eFormat,0,16)));
}

const char *Format2String(OMX_COLOR_FORMATTYPE eColorFormat)
{
    switch(eColorFormat)
    {
        CASE2STR(OMX_COLOR_FormatUnused);
        CASE2STR(OMX_COLOR_FormatMonochrome);
        CASE2STR(OMX_COLOR_Format8bitRGB332);
        CASE2STR(OMX_COLOR_Format12bitRGB444);
        CASE2STR(OMX_COLOR_Format16bitARGB4444);
        CASE2STR(OMX_COLOR_Format16bitARGB1555);
        CASE2STR(OMX_COLOR_Format16bitRGB565);
        CASE2STR(OMX_COLOR_Format16bitBGR565);
        CASE2STR(OMX_COLOR_Format18bitRGB666);
        CASE2STR(OMX_COLOR_Format18bitARGB1665);
        CASE2STR(OMX_COLOR_Format19bitARGB1666);
        CASE2STR(OMX_COLOR_Format24bitRGB888);
        CASE2STR(OMX_COLOR_Format24bitBGR888);
        CASE2STR(OMX_COLOR_Format24bitARGB1887);
        CASE2STR(OMX_COLOR_Format25bitARGB1888);
        CASE2STR(OMX_COLOR_Format32bitBGRA8888);
        CASE2STR(OMX_COLOR_Format32bitARGB8888);
        CASE2STR(OMX_COLOR_FormatYUV411Planar);
        CASE2STR(OMX_COLOR_FormatYUV411PackedPlanar);
        CASE2STR(OMX_COLOR_FormatYUV420Planar);
        CASE2STR(OMX_COLOR_FormatYUV420PackedPlanar);
        CASE2STR(OMX_COLOR_FormatYUV420SemiPlanar);
        CASE2STR(OMX_COLOR_FormatYUV422Planar);
        CASE2STR(OMX_COLOR_FormatYUV422PackedPlanar);
        CASE2STR(OMX_COLOR_FormatYUV422SemiPlanar);
        CASE2STR(OMX_COLOR_FormatYCbYCr);
        CASE2STR(OMX_COLOR_FormatYCrYCb);
        CASE2STR(OMX_COLOR_FormatCbYCrY);
        CASE2STR(OMX_COLOR_FormatCrYCbY);
        CASE2STR(OMX_COLOR_FormatYUV444Interleaved);
        CASE2STR(OMX_COLOR_FormatRawBayer8bit);
        CASE2STR(OMX_COLOR_FormatRawBayer10bit);
        CASE2STR(OMX_COLOR_FormatRawBayer8bitcompressed);
        CASE2STR(OMX_COLOR_FormatL2);
        CASE2STR(OMX_COLOR_FormatL4);
        CASE2STR(OMX_COLOR_FormatL8);
        CASE2STR(OMX_COLOR_FormatL16);
        CASE2STR(OMX_COLOR_FormatL24);
        CASE2STR(OMX_COLOR_FormatL32);
        CASE2STR(OMX_COLOR_FormatYUV420PackedSemiPlanar);
        CASE2STR(OMX_COLOR_FormatYUV422PackedSemiPlanar);
        CASE2STR(OMX_COLOR_Format18BitBGR666);
        CASE2STR(OMX_COLOR_Format24BitARGB6666);
        CASE2STR(OMX_COLOR_Format24BitABGR6666);
        CASE2STR(OMX_COLOR_FormatKhronosExtensions);
        CASE2STR(OMX_COLOR_FormatVendorStartUnused);
#ifdef USING_BROADCOM
        CASE2STR(OMX_COLOR_Format32bitABGR8888);
        CASE2STR(OMX_COLOR_Format8bitPalette);
        CASE2STR(OMX_COLOR_FormatYUVUV128);
        CASE2STR(OMX_COLOR_FormatRawBayer12bit);
        CASE2STR(OMX_COLOR_FormatBRCMEGL);
        CASE2STR(OMX_COLOR_FormatBRCMOpaque);
        CASE2STR(OMX_COLOR_FormatYVU420PackedPlanar);
        CASE2STR(OMX_COLOR_FormatYVU420PackedSemiPlanar);
#endif
        default:
            // Quiet compiler warning.
            break;
    }
    static char s_buf[32];
    return strcpy(s_buf, qPrintable(QString("COLOR_Format 0x%1")
                                    .arg(eColorFormat,0,16)));
}

const char *Event2String(OMX_EVENTTYPE eEvent)
{
    switch(eEvent)
    {
        CASE2STR(OMX_EventCmdComplete);
        CASE2STR(OMX_EventError);
        CASE2STR(OMX_EventMark);
        CASE2STR(OMX_EventPortSettingsChanged);
        CASE2STR(OMX_EventBufferFlag);
        CASE2STR(OMX_EventResourcesAcquired);
        CASE2STR(OMX_EventComponentResumed);
        CASE2STR(OMX_EventDynamicResourcesAvailable);
        CASE2STR(OMX_EventPortFormatDetected);
#ifdef USING_BROADCOM
        CASE2STR(OMX_EventParamOrConfigChanged);
#endif
        default:
            // Quiet compiler warning.
            break;
    }
    static char s_buf[32];
    return strcpy(s_buf, qPrintable(QString("Event 0x%1").arg(eEvent,0,16)));
}

const char *Error2String(OMX_ERRORTYPE eError)
{
    switch (eError)
    {
        CASE2STR(OMX_ErrorNone);
        CASE2STR(OMX_ErrorInsufficientResources);
        CASE2STR(OMX_ErrorUndefined);
        CASE2STR(OMX_ErrorInvalidComponentName);
        CASE2STR(OMX_ErrorComponentNotFound);
        CASE2STR(OMX_ErrorInvalidComponent);
        CASE2STR(OMX_ErrorBadParameter);
        CASE2STR(OMX_ErrorNotImplemented);
        CASE2STR(OMX_ErrorUnderflow);
        CASE2STR(OMX_ErrorOverflow);
        CASE2STR(OMX_ErrorHardware);
        CASE2STR(OMX_ErrorInvalidState);
        CASE2STR(OMX_ErrorStreamCorrupt);
        CASE2STR(OMX_ErrorPortsNotCompatible);
        CASE2STR(OMX_ErrorResourcesLost);
        CASE2STR(OMX_ErrorNoMore);
        CASE2STR(OMX_ErrorVersionMismatch);
        CASE2STR(OMX_ErrorNotReady);
        CASE2STR(OMX_ErrorTimeout);
        CASE2STR(OMX_ErrorSameState);
        CASE2STR(OMX_ErrorResourcesPreempted);
        CASE2STR(OMX_ErrorPortUnresponsiveDuringAllocation);
        CASE2STR(OMX_ErrorPortUnresponsiveDuringDeallocation);
        CASE2STR(OMX_ErrorPortUnresponsiveDuringStop);
        CASE2STR(OMX_ErrorIncorrectStateTransition);
        CASE2STR(OMX_ErrorIncorrectStateOperation);
        CASE2STR(OMX_ErrorUnsupportedSetting);
        CASE2STR(OMX_ErrorUnsupportedIndex);
        CASE2STR(OMX_ErrorBadPortIndex);
        CASE2STR(OMX_ErrorPortUnpopulated);
        CASE2STR(OMX_ErrorComponentSuspended);
        CASE2STR(OMX_ErrorDynamicResourcesUnavailable);
        CASE2STR(OMX_ErrorMbErrorsInFrame);
        CASE2STR(OMX_ErrorFormatNotDetected);
        CASE2STR(OMX_ErrorContentPipeOpenFailed);
        CASE2STR(OMX_ErrorContentPipeCreationFailed);
        CASE2STR(OMX_ErrorSeperateTablesUsed);
        CASE2STR(OMX_ErrorTunnelingUnsupported);
#ifdef USING_BROADCOM
        CASE2STR(OMX_ErrorDiskFull);
        CASE2STR(OMX_ErrorMaxFileSize);
        CASE2STR(OMX_ErrorDrmUnauthorised);
        CASE2STR(OMX_ErrorDrmExpired);
        CASE2STR(OMX_ErrorDrmGeneral);
#endif
        default:
            // Quiet compiler warning.
            break;
    }
    static char s_buf[32];
    return strcpy(s_buf, qPrintable(QString("Error 0x%1").arg(eError,0,16)));
}

const char *State2String(OMX_STATETYPE eState)
{
    switch (eState)
    {
        CASE2STR(OMX_StateInvalid);
        CASE2STR(OMX_StateLoaded);
        CASE2STR(OMX_StateIdle);
        CASE2STR(OMX_StateExecuting);
        CASE2STR(OMX_StatePause);
        CASE2STR(OMX_StateWaitForResources);
        default:
            // Quiet compiler warning.
            break;
    }
    static char s_buf[32];
    return strcpy(s_buf, qPrintable(QString("State 0x%1").arg(eState,0,16)));
}

const char *Command2String(OMX_COMMANDTYPE cmd)
{
    switch (cmd)
    {
        CASE2STR(OMX_CommandStateSet);
        CASE2STR(OMX_CommandFlush);
        CASE2STR(OMX_CommandPortDisable);
        CASE2STR(OMX_CommandPortEnable);
        CASE2STR(OMX_CommandMarkBuffer);
        default:
            // Quiet compiler warning.
            break;
    }
    static char s_buf[32];
    return strcpy(s_buf, qPrintable(QString("Command 0x%1").arg(cmd,0,16)));
}

#define FLAG2LIST(_f,_n,_l)\
    do {\
        if ((_n) & OMX_BUFFERFLAG_##_f) {\
            (_n) &= ~OMX_BUFFERFLAG_##_f;\
            (_l) << STR(_f);\
        }\
    } while(false)

QString HeaderFlags(OMX_U32 nFlags)
{
    QStringList flags;

    FLAG2LIST(EOS,nFlags,flags);
    FLAG2LIST(STARTTIME,nFlags,flags);
    FLAG2LIST(DECODEONLY,nFlags,flags);
    FLAG2LIST(DATACORRUPT,nFlags,flags);
    FLAG2LIST(ENDOFFRAME,nFlags,flags);
    FLAG2LIST(SYNCFRAME,nFlags,flags);
    FLAG2LIST(EXTRADATA,nFlags,flags);
    FLAG2LIST(CODECCONFIG,nFlags,flags);

    if (nFlags)
        flags << QString("0x%1").arg(nFlags,0,16);

    return QString("<") + flags.join(",") + ">";
}

#ifdef USING_BROADCOM
const char *Interlace2String(OMX_INTERLACETYPE eMode)
{
    switch (eMode)
    {
        CASE2STR(OMX_InterlaceProgressive);
        CASE2STR(OMX_InterlaceFieldSingleUpperFirst);
        CASE2STR(OMX_InterlaceFieldSingleLowerFirst);
        CASE2STR(OMX_InterlaceFieldsInterleavedUpperFirst);
        CASE2STR(OMX_InterlaceFieldsInterleavedLowerFirst);
        CASE2STR(OMX_InterlaceMixed);
        default:
            // Quiet compiler warning.
            break;
    }
    static char buf[32];
    return strcpy(buf, qPrintable(QString("Interlace 0x%1").arg(eMode,0,16)));
}
#endif // USING_BROADCOM

const char *Filter2String(OMX_IMAGEFILTERTYPE eType)
{
    switch (eType)
    {
        CASE2STR(OMX_ImageFilterNone);
        CASE2STR(OMX_ImageFilterNoise);
        CASE2STR(OMX_ImageFilterEmboss);
        CASE2STR(OMX_ImageFilterNegative);
        CASE2STR(OMX_ImageFilterSketch);
        CASE2STR(OMX_ImageFilterOilPaint);
        CASE2STR(OMX_ImageFilterHatch);
        CASE2STR(OMX_ImageFilterGpen);
        CASE2STR(OMX_ImageFilterAntialias);
        CASE2STR(OMX_ImageFilterDeRing);
        CASE2STR(OMX_ImageFilterSolarize);
#ifdef USING_BROADCOM
        CASE2STR(OMX_ImageFilterWatercolor);
        CASE2STR(OMX_ImageFilterPastel);
        CASE2STR(OMX_ImageFilterSharpen);
        CASE2STR(OMX_ImageFilterFilm);
        CASE2STR(OMX_ImageFilterBlur);
        CASE2STR(OMX_ImageFilterSaturation);
        CASE2STR(OMX_ImageFilterDeInterlaceLineDouble);
        CASE2STR(OMX_ImageFilterDeInterlaceAdvanced);
        CASE2STR(OMX_ImageFilterColourSwap);
        CASE2STR(OMX_ImageFilterWashedOut);
        CASE2STR(OMX_ImageFilterColourPoint);
        CASE2STR(OMX_ImageFilterPosterise);
        CASE2STR(OMX_ImageFilterColourBalance);
        CASE2STR(OMX_ImageFilterCartoon);
        CASE2STR(OMX_ImageFilterAnaglyph);
        CASE2STR(OMX_ImageFilterDeInterlaceFast);
#endif //def USING_BROADCOM
        default:
            // Quiet compiler warning.
            break;
    }
    static char s_buf[32];
    return strcpy(s_buf, qPrintable(QString("FilterType 0x%1").arg(eType,0,16)));
}

} // namespace omxcontext
// EOF

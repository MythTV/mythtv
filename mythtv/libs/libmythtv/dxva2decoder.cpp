#include <QString>
#include <QLibrary>

#include "mythlogging.h"
#include "initguid.h"
#include "mythrender_d3d9.h"
#include "dxva2decoder.h"

static const GUID IID_IDirectXVideoDecoderService =
{
    0xfc51a551, 0xd5e7, 0x11d9, {0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02}
};

static inline QString toString(const GUID& guid)
{
    return QString("%1-%2-%3-%4%5-%6%7%8%9%10%11")
            .arg(guid.Data1, 8, 16, QLatin1Char('0'))
            .arg(guid.Data2, 4, 16, QLatin1Char('0'))
            .arg(guid.Data3, 4, 16, QLatin1Char('0'))
            .arg(guid.Data4[0], 2, 16, QLatin1Char('0'))
            .arg(guid.Data4[1], 2, 16, QLatin1Char('0'))
            .arg(guid.Data4[2], 2, 16, QLatin1Char('0'))
            .arg(guid.Data4[3], 2, 16, QLatin1Char('0'))
            .arg(guid.Data4[4], 2, 16, QLatin1Char('0'))
            .arg(guid.Data4[5], 2, 16, QLatin1Char('0'))
            .arg(guid.Data4[6], 2, 16, QLatin1Char('0'))
            .arg(guid.Data4[7], 2, 16, QLatin1Char('0')).toUpper();
}

#define LOC QString("DXVA2: ")
#define ERR QString("DXVA2 Error: ")

DEFINE_GUID(DXVA2_ModeH264_A,       0x1b81be64, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_B,       0x1b81be65, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_C,       0x1b81be66, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_D,       0x1b81be67, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_E,       0x1b81be68, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_F,       0x1b81be69, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);

DEFINE_GUID(DXVA2_ModeWMV8_A,       0x1b81be80, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeWMV8_B,       0x1b81be81, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);

DEFINE_GUID(DXVA2_ModeWMV9_A,       0x1b81be90, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeWMV9_B,       0x1b81be91, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeWMV9_C,       0x1b81be94, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);

DEFINE_GUID(DXVA2_ModeVC1_A,        0x1b81beA0, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeVC1_B,        0x1b81beA1, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeVC1_C,        0x1b81beA2, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeVC1_D,        0x1b81beA3, 0xa0c7, 0x11d3, 0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);

DEFINE_GUID(DXVA2_ModeMPEG2_MoComp, 0xe6a9f44b, 0x61b0, 0x4563, 0x9e,0xa4,0x63,0xd2,0xa3,0xc6,0xfe,0x66);
DEFINE_GUID(DXVA2_ModeMPEG2_IDCT,   0xbf22ad00, 0x03ea, 0x4690, 0x80,0x77,0x47,0x33,0x46,0x20,0x9b,0x7e);
DEFINE_GUID(DXVA2_ModeMPEG2_VLD,    0xee27417f, 0x5e28, 0x4e65, 0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9);

#define DXVA2_ModeWMV8_PostProc     DXVA2_ModeWMV8_A
#define DXVA2_ModeWMV8_MoComp       DXVA2_ModeWMV8_B

#define DXVA2_ModeWMV9_PostProc     DXVA2_ModeWMV9_A
#define DXVA2_ModeWMV9_MoComp       DXVA2_ModeWMV9_B
#define DXVA2_ModeWMV9_IDCT         DXVA2_ModeWMV9_C

#define DXVA2_ModeVC1_PostProc      DXVA2_ModeVC1_A
#define DXVA2_ModeVC1_MoComp        DXVA2_ModeVC1_B
#define DXVA2_ModeVC1_IDCT          DXVA2_ModeVC1_C
#define DXVA2_ModeVC1_VLD           DXVA2_ModeVC1_D

#define DXVA2_ModeH264_MoComp_NoFGT DXVA2_ModeH264_A
#define DXVA2_ModeH264_MoComp_FGT   DXVA2_ModeH264_B
#define DXVA2_ModeH264_IDCT_NoFGT   DXVA2_ModeH264_C
#define DXVA2_ModeH264_IDCT_FGT     DXVA2_ModeH264_D
#define DXVA2_ModeH264_VLD_NoFGT    DXVA2_ModeH264_E
#define DXVA2_ModeH264_VLD_FGT      DXVA2_ModeH264_F

DEFINE_GUID(DXVA2_Intel_ModeH264_A, 0x604F8E64, 0x4951, 0x4c54, 0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);
DEFINE_GUID(DXVA2_Intel_ModeH264_C, 0x604F8E66, 0x4951, 0x4c54, 0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);
DEFINE_GUID(DXVA2_Intel_ModeH264_E, 0x604F8E68, 0x4951, 0x4c54, 0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);
DEFINE_GUID(DXVA2_Intel_ModeVC1_E , 0xBCC5DB6D, 0xA2B6, 0x4AF0, 0xAC,0xE4,0xAD,0xB1,0xF7,0x87,0xBC,0x89);

typedef struct {
    const QString name;
    const GUID   *guid;
    MythCodecID   codec;
} dxva2_mode;

static const dxva2_mode dxva2_modes[] =
{
    {"MPEG2 VLD",                  &DXVA2_ModeMPEG2_VLD,    kCodec_MPEG2_DXVA2},
    {"MPEG2 MoComp",               &DXVA2_ModeMPEG2_MoComp, kCodec_NONE},
    {"MPEG2 IDCT",                 &DXVA2_ModeMPEG2_IDCT,   kCodec_NONE},

    {"H.264 VLD, FGT",             &DXVA2_ModeH264_F,       kCodec_H264_DXVA2},
    {"H.264 VLD, no FGT",          &DXVA2_ModeH264_E,       kCodec_H264_DXVA2},
    {"H.264 IDCT, FGT",            &DXVA2_ModeH264_D,       kCodec_NONE},
    {"H.264 IDCT, no FGT",         &DXVA2_ModeH264_C,       kCodec_NONE},
    {"H.264 MoComp, FGT",          &DXVA2_ModeH264_B,       kCodec_NONE},
    {"H.264 MoComp, no FGT",       &DXVA2_ModeH264_A,       kCodec_NONE},

    {"WMV8 MoComp",                &DXVA2_ModeWMV8_B,       kCodec_NONE},
    {"WMV8 post processing",       &DXVA2_ModeWMV8_A,       kCodec_NONE},

    {"WMV9 IDCT",                  &DXVA2_ModeWMV9_C,       kCodec_NONE},
    {"WMV9 MoComp",                &DXVA2_ModeWMV9_B,       kCodec_NONE},
    {"WMV9 post processing",       &DXVA2_ModeWMV9_A,       kCodec_NONE},

    {"VC-1 VLD",                   &DXVA2_ModeVC1_D,        kCodec_VC1_DXVA2},
    {"VC-1 VLD",                   &DXVA2_ModeVC1_D,        kCodec_WMV3_DXVA2},
    {"VC-1 IDCT",                  &DXVA2_ModeVC1_C,        kCodec_NONE},
    {"VC-1 MoComp",                &DXVA2_ModeVC1_B,        kCodec_NONE},
    {"VC-1 post processing",       &DXVA2_ModeVC1_A,        kCodec_NONE},

    {"Intel H.264 VLD, no FGT",    &DXVA2_Intel_ModeH264_E, kCodec_H264_DXVA2},
    {"Intel H.264 IDCT, no FGT",   &DXVA2_Intel_ModeH264_C, kCodec_NONE},
    {"Intel H.264 MoComp, no FGT", &DXVA2_Intel_ModeH264_A, kCodec_NONE},
    {"Intel VC-1 VLD",             &DXVA2_Intel_ModeVC1_E,  kCodec_VC1_DXVA2},

    {"", NULL, kCodec_NONE}
};

#define CREATE_CHECK(arg1, arg2) \
  if (ok) \
  { \
      ok = arg1; \
      if (!ok) \
          LOG(VB_GENERAL, LOG_ERR, LOC + arg2); \
  }

DXVA2Decoder::DXVA2Decoder(uint num_bufs, MythCodecID codec_id,
                           uint width, uint height)
  : m_deviceManager(NULL), m_device(NULL), m_service(NULL),
    m_codec_id(codec_id), m_width(width),  m_height(height)
{
    memset(&m_format, 0, sizeof(DXVA2_VideoDesc));
    memset(&m_context, 0, sizeof(dxva_context));
    memset(&m_config, 0, sizeof(DXVA2_ConfigPictureDecode));
    m_context.cfg = &m_config;
    m_context.surface_count = num_bufs;
    m_context.surface = new IDirect3DSurface9*[num_bufs];
    for (uint i = 0; i < num_bufs; i++)
        m_context.surface[i] = NULL;
}

DXVA2Decoder::~DXVA2Decoder(void)
{
    DestroyDecoder();
    DestroySurfaces();
    DestroyVideoService();
}

bool DXVA2Decoder::Init(MythRenderD3D9* render)
{
    bool ok = true;
    CREATE_CHECK(m_width > 0,  "Invalid width.")
    CREATE_CHECK(m_height > 0, "Invalid height.")
    CREATE_CHECK(CreateVideoService(render), "Failed to create video service.")
    CREATE_CHECK(GetInputOutput(), "Failed to find input/output combination.")
    InitFormat();
    CREATE_CHECK(GetDecoderConfig(), "Failed to find a raw input bitstream.")
    CREATE_CHECK(CreateSurfaces(), "Failed to create surfaces.")
    CREATE_CHECK(CreateDecoder(), "Failed to create decoder.")
    return ok;
}

typedef HRESULT (__stdcall *DXVA2CreateVideoServicePtr)(IDirect3DDevice9* pDD,
                                                        REFIID riid,
                                                        void** ppService);

bool DXVA2Decoder::CreateVideoService(MythRenderD3D9* render)
{
    // Loads the DXVA2 library
    DXVA2CreateVideoServicePtr create_video_service =
        (DXVA2CreateVideoServicePtr)MythRenderD3D9::ResolveAddress("DXVA2", "DXVA2CreateVideoService");
    if (!create_video_service)
        return false;

    m_deviceManager = render->GetDeviceManager();
    if (!m_deviceManager)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get device manager.");
        return false;
    }

    HRESULT hr = IDirect3DDeviceManager9_OpenDeviceHandle(m_deviceManager, &m_device);
    if (FAILED(hr))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get device handle.");
        return false;
    }

    hr = IDirect3DDeviceManager9_GetVideoService(m_deviceManager,
                                                 m_device,
                                                 IID_IDirectXVideoDecoderService,
                                                 (void**)&m_service);
    if (FAILED(hr))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get video service.");
        return false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Created DXVA2 video service.");
    return true;
}

void DXVA2Decoder::DestroyVideoService(void)
{
    if (m_device)
        IDirect3DDeviceManager9_CloseDeviceHandle(m_deviceManager, m_device);
    if (m_service)
        IDirectXVideoDecoderService_Release(m_service);
    m_deviceManager = NULL;
    m_device  = NULL;
    m_service = NULL;
}

bool DXVA2Decoder::GetInputOutput(void)
{
    if (!m_service)
        return false;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Looking for support for %1")
                                        .arg(toString(m_codec_id)));
    uint input_count;
    GUID *input_list;
    m_format.Format = D3DFMT_UNKNOWN;
    IDirectXVideoDecoderService_GetDecoderDeviceGuids(
        m_service, &input_count, &input_list);

    for (const dxva2_mode* mode = dxva2_modes;
        !mode->name.isEmpty() && m_format.Format == D3DFMT_UNKNOWN;
         mode++)
    {
        if (mode->codec != m_codec_id)
            continue;
        for (uint j = 0; j < input_count; j++)
        {
            if (IsEqualGUID(input_list[j], *mode->guid))
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Testing %1")
                                           .arg(mode->name));
                if (TestTarget(input_list[j]))
                    break;
            }
        }
    }

    return m_format.Format != D3DFMT_UNKNOWN;
}

bool DXVA2Decoder::TestTarget(const GUID &guid)
{
    if (!m_service)
        return false;
    uint       output_count = 0;
    D3DFORMAT *output_list  = NULL;
    IDirectXVideoDecoderService_GetDecoderRenderTargets(
        m_service, guid, &output_count, &output_list);
    for(uint i = 0; i < output_count; i++)
    {
        if(output_list[i] == MAKEFOURCC('Y','V','1','2') ||
           output_list[i] == MAKEFOURCC('N','V','1','2'))
        {
            m_input         = guid;
            m_format.Format = output_list[i];
            return true;
        }
    }
    return false;
}

void DXVA2Decoder::InitFormat(void)
{
    m_format.SampleWidth                 = m_width;
    m_format.SampleHeight                = m_height;
    m_format.InputSampleFreq.Numerator   = 0;
    m_format.InputSampleFreq.Denominator = 0;
    m_format.OutputFrameFreq             = m_format.InputSampleFreq;
    m_format.UABProtectionLevel          = false;
    m_format.Reserved                    = 0;
}

bool DXVA2Decoder::GetDecoderConfig(void)
{
    if (!m_service)
        return false;

    uint                       cfg_count = 0;
    DXVA2_ConfigPictureDecode *cfg_list  = NULL;
    IDirectXVideoDecoderService_GetDecoderConfigurations(
        m_service, m_input, &m_format, NULL, &cfg_count, &cfg_list);

    DXVA2_ConfigPictureDecode config = {};
    uint bitstream = 1;
    for (uint i = 0; i < cfg_count; i++)
    {
        // select first available
        if (config.ConfigBitstreamRaw == 0 &&
            cfg_list[i].ConfigBitstreamRaw != 0)
        {
            config = cfg_list[i];
        }
        // overide with preferred if found
        if (config.ConfigBitstreamRaw != bitstream &&
            cfg_list[i].ConfigBitstreamRaw == bitstream)
        {
            config = cfg_list[i];
        }
    }
    //CoTaskMemFree(cfg_list);
    if(!config.ConfigBitstreamRaw)
        return false;
    m_config = config;
    //*const_cast<DXVA2_ConfigPictureDecode*>(m_context.cfg) = config;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Found bitstream type %1")
        .arg(m_context.cfg->ConfigBitstreamRaw));
    return true;
}

bool DXVA2Decoder::CreateSurfaces(void)
{
    if (!m_service || !m_context.surface)
        return false;

    HRESULT hr = IDirectXVideoDecoderService_CreateSurface(
        m_service, (m_width + 15) & ~15, (m_height + 15) & ~15,
        m_context.surface_count - 1, m_format.Format,
        D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget,
        m_context.surface, NULL);

    if (FAILED(hr))
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created %1 decoder surfaces.")
                                         .arg(m_context.surface_count));

    return true;
}

void DXVA2Decoder::DestroySurfaces(void)
{
    for (uint i = 0; i < m_context.surface_count; i++)
    {
        if (m_context.surface[i])
        {
            m_context.surface[i]->Release();
            m_context.surface[i] = NULL;
        }
    }
    m_context.surface = NULL;
    m_context.surface_count = 0;
}

bool DXVA2Decoder::CreateDecoder(void)
{
    if (!m_service || !m_context.surface)
        return false;

    HRESULT hr = IDirectXVideoDecoderService_CreateVideoDecoder(
        m_service, m_input, &m_format,
        const_cast<DXVA2_ConfigPictureDecode*>(m_context.cfg),
        m_context.surface, m_context.surface_count, &m_context.decoder);

    if (FAILED(hr))
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Created decoder: %1->%2x%3 (%4 surfaces)")
            .arg(toString(m_codec_id)).arg(m_width).arg(m_height)
            .arg(m_context.surface_count));
    return true;
}

void DXVA2Decoder::DestroyDecoder(void)
{
    if (m_context.decoder)
        IDirectXVideoDecoder_Release(m_context.decoder);
    m_context.decoder = NULL;
}

void* DXVA2Decoder::GetSurface(uint num)
{
    if (num < 0 || num >= m_context.surface_count)
        return NULL;
    return (void*)m_context.surface[num];
}

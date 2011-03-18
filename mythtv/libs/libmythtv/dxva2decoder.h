#ifndef DXVA2DECODER_H
#define DXVA2DECODER_H

#include <windows.h>
#include <d3d9.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/dxva2.h"
}

#include "mythcodecid.h"
#include "dxva2api.h"

class DXVA2Decoder
{
  public:
    DXVA2Decoder(uint num_bufs, MythCodecID codec_id,
                 uint width, uint height);
   ~DXVA2Decoder(void);
    bool Init(MythRenderD3D9* render);
    bool CreateVideoService(MythRenderD3D9* render);
    void DestroyVideoService(void);
    bool GetInputOutput(void);
    void InitFormat(void);
    bool TestTarget(const GUID &guid);
    bool GetDecoderConfig(void);
    bool CreateSurfaces(void);
    void DestroySurfaces(void);
    bool CreateDecoder(void);
    void DestroyDecoder(void);
    void* GetSurface(uint num);

    IDirect3DDeviceManager9     *m_deviceManager;
    HANDLE                       m_device;
    IDirectXVideoDecoderService *m_service;
    struct dxva_context          m_context;
    DXVA2_ConfigPictureDecode    m_config;
    MythCodecID                  m_codec_id;
    GUID                         m_input;
    DXVA2_VideoDesc              m_format;
    uint                         m_width;
    uint                         m_height;
};

#endif // DXVA2DECODER_H

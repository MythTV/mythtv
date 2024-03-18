//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 MythTV Developers <mythtv-dev@mythtv.org>
//
// This is part of MythTV (https://www.mythtv.org)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////


#ifndef MYTHCODECONTEXT_H
#define MYTHCODECONTEXT_H

// Qt
#include <QRecursiveMutex>
#include <QStringList>
#include <QAtomicInt>

// MythTV
#include "libmythtv/mythcodecid.h"
#include "libmythtv/mythframe.h"
#include "libmythtv/mythinteropgpu.h"
#include "libmythtv/mythtvexp.h"

#include "decoderbase.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

using CreateHWDecoder = int (*)(AVCodecContext *Context);

class MythPlayerUI;
class MythVideoProfile;

class MTV_PUBLIC MythCodecContext
{
  public:
    enum CodecProfile : std::uint8_t
    {
        NoProfile = 0,
        MPEG1,
        MPEG2,
        MPEG2Simple,
        MPEG2Main,
        MPEG2422,
        MPEG2High,
        MPEG2Spatial,
        MPEG2SNR,
        MPEG4,
        MPEG4Simple,
        MPEG4SimpleScaleable,
        MPEG4Core,
        MPEG4Main,
        MPEG4NBit,
        MPEG4ScaleableTexture,
        MPEG4SimpleFace,
        MPEG4BasicAnimated,
        MPEG4Hybrid,
        MPEG4AdvancedRT,
        MPEG4CoreScaleable,
        MPEG4AdvancedCoding,
        MPEG4AdvancedCore,
        MPEG4AdvancedScaleableTexture,
        MPEG4SimpleStudio,
        MPEG4AdvancedSimple,
        H263,
        H264,
        H264Baseline,
        H264ConstrainedBaseline,
        H264Main,
        H264MainExtended,
        H264High,
        H264High10,
        H264Extended,
        H264High422,
        H264High444,
        H264ConstrainedHigh,
        HEVC,
        HEVCMain,
        HEVCMain10,
        HEVCMainStill,
        HEVCRext,
        HEVCMain10HDR,
        HEVCMain10HDRPlus,
        VC1,
        VC1Simple,
        VC1Main,
        VC1Complex,
        VC1Advanced,
        VP8,
        VP9,
        VP9_0,
        VP9_1,
        VP9_2,
        VP9_3,
        VP9_2HDR,
        VP9_2HDRPlus,
        VP9_3HDR,
        VP9_3HDRPlus,
        AV1,
        AV1Main,
        AV1High,
        AV1Professional,
        MJPEG
    };

    explicit MythCodecContext(DecoderBase *Parent, MythCodecID CodecID);
    virtual ~MythCodecContext() = default;

    static MythCodecContext* CreateContext (DecoderBase *Parent, MythCodecID Codec);
    static void GetDecoders                (RenderOptions &Opts, bool Reinit = false);
    static QStringList GetDecoderDescription(void);
    static MythCodecID FindDecoder         (const QString &Decoder, AVStream *Stream,
                                            AVCodecContext **Context, const AVCodec **Codec);
    static int  GetBuffer                  (struct AVCodecContext *Context, AVFrame *Frame, int Flags);
    static bool GetBuffer2                 (struct AVCodecContext *Context, MythVideoFrame *Frame,
                                            AVFrame *AvFrame, int Flags);
    static int  InitialiseDecoder          (AVCodecContext *Context, CreateHWDecoder Callback, const QString &Debug);
    static int  InitialiseDecoder2         (AVCodecContext *Context, CreateHWDecoder Callback, const QString &Debug);
    static void ReleaseBuffer              (void *Opaque, uint8_t *Data);
    static void FramesContextFinished      (AVHWFramesContext *Context);
    static void DeviceContextFinished      (AVHWDeviceContext *Context);
    static void CreateDecoderCallback      (void *Wait, void *Context, void *Callback);
    static MythPlayerUI* GetPlayerUI       (AVCodecContext* Context);
    static bool FrameTypeIsSupported       (AVCodecContext* Context, VideoFrameType Format);
    static AVBufferRef* CreateDevice       (AVHWDeviceType Type, MythInteropGPU* Interop, const QString& Device = QString());
    static bool IsUnsupportedProfile       (AVCodecContext *Context);
    static QString GetProfileDescription   (CodecProfile Profile, QSize Size,
                                            VideoFrameType Format = FMT_NONE, uint ColorDepth = 0);
    static CodecProfile FFmpegToMythProfile(AVCodecID CodecID, int Profile);

    virtual void   InitVideoCodec          (AVCodecContext *Context,
                                            bool SelectedStream, bool &DirectRendering);
    virtual int    HwDecoderInit           (AVCodecContext */*Context*/) { return 0; }
    virtual bool   RetrieveFrame           (AVCodecContext */*Context*/, MythVideoFrame */*Frame*/, AVFrame */*AvFrame*/) { return false; }
    virtual int    FilteredReceiveFrame    (AVCodecContext *Context, AVFrame *Frame);
    virtual void   SetDeinterlacing        (AVCodecContext */*Context*/, MythVideoProfile */*Profile*/, bool /*DoubleRate*/) {}
    virtual void   PostProcessFrame        (AVCodecContext */*Context*/, MythVideoFrame */*Frame*/) {}
    virtual bool   IsDeinterlacing         (bool &/*DoubleRate*/, bool /*StreamChange*/ = false) { return false; }
    virtual void   SetDecoderOptions       (AVCodecContext */*Context*/, const AVCodec */*Codec*/) { }
    virtual bool   DecoderWillResetOnFlush (void) { return false; }
    virtual bool   DecoderWillResetOnAspect(void) { return false; }
    virtual bool   DecoderNeedsReset       (AVCodecContext */*Context*/) { return m_resetRequired; }

  protected:
    virtual bool   RetrieveHWFrame         (MythVideoFrame* Frame, AVFrame* AvFrame);
    static void    DestroyInterop          (MythInteropGPU* Interop);
    static void    NewHardwareFramesContext(void);
    static QAtomicInt s_hwFramesContextCount;

    DecoderBase* m_parent        { nullptr     };
    MythCodecID  m_codecID       { kCodec_NONE };
    bool         m_resetRequired { false       };
};

#endif

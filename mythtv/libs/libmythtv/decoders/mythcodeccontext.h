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
#include <QMutex>
#include <QStringList>

// MythTV
#include "mythtvexp.h"
#include "mythcodecid.h"
#include "mythframe.h"

typedef int (*CreateHWDecoder)(AVCodecContext *Context);

class MythOpenGLInterop;
class VideoDisplayProfile;

class MTV_PUBLIC MythCodecContext
{
  public:
    explicit MythCodecContext(MythCodecID CodecID);
    virtual ~MythCodecContext() = default;

    static MythCodecContext* CreateContext (MythCodecID Codec);
    static int  GetBuffer                  (struct AVCodecContext *Context, AVFrame *Frame, int Flags);
    static int  GetBuffer2                 (struct AVCodecContext *Context, AVFrame *Frame, int Flags);
    static int  GetBuffer3                 (struct AVCodecContext *Context, AVFrame *Frame, int Flags);
    static int  InitialiseDecoder          (AVCodecContext *Context, CreateHWDecoder Callback, const QString &Debug);
    static int  InitialiseDecoder2         (AVCodecContext *Context, CreateHWDecoder Callback, const QString &Debug);
    static void ReleaseBuffer              (void *Opaque, uint8_t *Data);
    static void FramesContextFinished      (AVHWFramesContext *Context);
    static void DeviceContextFinished      (AVHWDeviceContext *Context);
    static void DestroyInteropCallback     (void *Wait, void *Interop, void*);
    static void CreateDecoderCallback      (void *Wait, void *Context, void *Callback);
    static AVBufferRef* CreateDevice       (AVHWDeviceType Type, const QString &Device = QString());

    virtual int    HwDecoderInit           (AVCodecContext*) { return 0; }
    virtual int    FilteredReceiveFrame    (AVCodecContext *Context, AVFrame *Frame);
    virtual void   SetDeinterlacing        (AVCodecContext*, VideoDisplayProfile*, bool) {}
    virtual void   PostProcessFrame        (AVCodecContext*, VideoFrame*) {}
    virtual bool   IsDeinterlacing         (bool &) { return false; }

  protected:
    static void DestroyInterop             (MythOpenGLInterop *Interop);
    MythCodecID m_codecID;
};

#endif // MYTHCODECCONTEXT_H

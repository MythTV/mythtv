//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-19 MythTV Developers <mythtv-dev@mythtv.org>
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

#include "mythcorecontext.h"
#include "mythlogging.h"
#ifdef USING_VAAPI
#include "mythvaapicontext.h"
#endif
#ifdef USING_VDPAU
#include "mythvdpaucontext.h"
#endif
#ifdef USING_NVDEC
#include "mythnvdeccontext.h"
#endif
#ifdef USING_VTB
#include "mythvtbcontext.h"
#endif
#ifdef USING_MEDIACODEC
#include "mythmediacodeccontext.h"
#endif
#include "mythcodeccontext.h"

#define LOC QString("MythCodecContext: ")

MythCodecContext::MythCodecContext()
{
}

// static
MythCodecContext *MythCodecContext::CreateContext(MythCodecID codec)
{
    MythCodecContext *mctx = nullptr;
#ifdef USING_VAAPI
    if (codec_is_vaapi(codec) || codec_is_vaapi_dec(codec))
        mctx = new MythVAAPIContext(codec);
#endif
#ifdef USING_VDPAU
    if (codec_is_vdpau_hw(codec) || codec_is_vdpau_hw(codec))
        mctx = new MythVDPAUContext(codec);
#endif
#ifdef USING_NVDEC
    if (codec_is_nvdec_dec(codec) || codec_is_nvdec(codec))
        mctx = new MythNVDECContext(codec);
#endif
#ifdef USING_VTB
    if (codec_is_vtb_dec(codec) || codec_is_vtb(codec))
        mctx = new MythVTBContext(codec);
#endif
#ifdef USING_MEDIACODEC
    if (codec_is_mediacodec(codec) || codec_is_mediacodec_dec(codec))
        mctx = new MythMediaCodecContext(codec);
#endif
    Q_UNUSED(codec);

    if (!mctx)
        mctx = new MythCodecContext();
    return mctx;
}

/*! \brief Retrieve and process/filter AVFrame
 * \note This default implementation implements no processing/filtering
*/
int MythCodecContext::FilteredReceiveFrame(AVCodecContext *Context, AVFrame *Frame)
{
    return avcodec_receive_frame(Context, Frame);
}

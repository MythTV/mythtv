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

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "nvdeccontext.h"
#include "videooutbase.h"
#include "mythplayer.h"
#include "mythhwcontext.h"

extern "C" {
    #include "libavutil/pixfmt.h"
    #include "libavutil/hwcontext.h"
    #include "libavutil/opt.h"
    #include "libavcodec/avcodec.h"
}

#define LOC QString("NVDEC: ")

int NvdecContext::HwDecoderInit(AVCodecContext *ctx)
{
    AVBufferRef *context = MythHWContext::CreateDevice(AV_HWDEVICE_TYPE_CUDA,
                                                       gCoreContext->GetSetting("NVDECDevice"));
    if (context)
    {
        ctx->hw_device_ctx = context;
        SetDeinterlace(ctx);
        return 0;
    }
    return -1;
}


bool NvdecContext::isValidDeinterlacer(QString filtername)
{
    return filtername.startsWith("nvdec");
}

QStringList NvdecContext::GetDeinterlacers(void)
{
    return MythCodecContext::GetDeinterlacers("nvdec-dec");
}

int NvdecContext::SetDeinterlace(AVCodecContext *ctx)
{
    QMutexLocker lock(&contextLock);
    bool dropSecondFld = false;
    int ret = 0;
    QString mode = GetDeinterlaceMode(dropSecondFld);
    if (mode.isEmpty())
    {
        ret = av_opt_set(ctx->priv_data,"deint","weave",0);
        if (ret == 0)
            LOG(VB_GENERAL, LOG_INFO, LOC +
                "Disabled hardware decoder based deinterlacer.");
        return ret;
    }
    ret = av_opt_set(ctx->priv_data,"deint",mode.toLocal8Bit(),0);
    // ret = av_opt_set(ctx,"deint",mode.toLocal8Bit(),AV_OPT_SEARCH_CHILDREN);
    if (ret == 0)
        ret = av_opt_set_int(ctx->priv_data,"drop_second_field",(int)dropSecondFld,0);
        // ret = av_opt_set_int(ctx,"drop_second_field",(int)dropSecondFld,AV_OPT_SEARCH_CHILDREN);
    if (ret == 0)
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Enabled hardware decoder based deinterlace '%1' mode: <%2>.")
                .arg(deinterlacername).arg(mode));

    return ret;
}

QString NvdecContext::GetDeinterlaceMode(bool &dropSecondFld)
{
    // example mode - weave
    // example deinterlacername - nvdecdoublerateweave

    dropSecondFld=true;
    QString mode;
    if (!isValidDeinterlacer(deinterlacername))
        return mode;
    mode = deinterlacername;
    mode.remove(0,5); //remove "nvdec"
    if (mode.startsWith("doublerate"))
    {
        dropSecondFld=false;
        mode.remove(0,10);  // remove "doublerate"
    }
    return mode;
}

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


#ifndef NVDECCONTEXT_H
#define NVDECCONTEXT_H

#include "mythtvexp.h"
#include "mythcodecid.h"
#include "mythcodeccontext.h"

extern "C" {
    #include "libavcodec/avcodec.h"
}

class MTV_PUBLIC NvdecContext : public MythCodecContext
{
  public:
    NvdecContext(void) = default;
    static MythCodecID GetBestSupportedCodec(AVCodec **ppCodec,
                                             const QString &decoder,
                                             uint stream_type,
                                             AVPixelFormat &pix_fmt);
    int HwDecoderInit(AVCodecContext *ctx) override; // MythCodecContext
    bool isValidDeinterlacer(QString /*name*/ ) override; // MythCodecContext
    QStringList GetDeinterlacers(void) override; // MythCodecContext

  protected:
    int SetDeinterlace(AVCodecContext *ctx);
    QString GetDeinterlaceMode(bool &dropSecondFld);


};

#endif // NVDECCONTEXT_H

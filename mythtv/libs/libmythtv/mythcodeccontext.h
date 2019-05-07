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

struct AVCodecContext;
struct AVFrame;
struct AVStream;
struct AVFilterContext;
struct AVFilterGraph;
struct AVBufferRef;
class MythPlayer;

#include "mythtvexp.h"
#include "mythcodecid.h"

class MTV_PUBLIC MythCodecContext
{
  public:
    MythCodecContext(void);
    virtual ~MythCodecContext() = default;
    static MythCodecContext* createMythCodecContext(MythCodecID codec);
    virtual int HwDecoderInit(AVCodecContext * /*ctx*/) { return 0; }
    void setStream(AVStream *initStream) { stream = initStream; }
    virtual int FilteredReceiveFrame(AVCodecContext *ctx, AVFrame *frame);
    static QStringList GetDeinterlacers(const QString& decodername);
    static bool isCodecDeinterlacer(const QString& decodername);
    virtual QStringList GetDeinterlacers(void) { return QStringList(); }
    virtual QString GetDeinterlaceFilter() { return QString(); }
    void setPlayer(MythPlayer *tPlayer) { player = tPlayer; }
    bool setDeinterlacer(bool enable, QString name = QString());
    bool isDeinterlacing(void) { return filter_graph != nullptr;}
    QString getDeinterlacerName(void) { return deinterlacername; }
    bool BestDeint(void);
    bool FallbackDeint(void);
    bool getDoubleRate(void) { return doublerate; }
    QString GetFallbackDeint(void);

  protected:
    virtual bool isValidDeinterlacer(QString /*name*/) { return false; }
    virtual int InitDeinterlaceFilter(AVCodecContext *ctx, AVFrame *frame);
    AVStream* stream;
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
    bool filtersInitialized;
    AVBufferRef *hw_frames_ctx;
    MythPlayer *player;
    int64_t priorPts[2];
    int64_t ptsUsed;
    int width;
    int height;
    QString deinterlacername;
    QMutex contextLock;
    bool doublerate;
};

#endif // MYTHCODECCONTEXT_H

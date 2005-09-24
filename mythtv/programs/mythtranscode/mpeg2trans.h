#ifndef MPEG2TRANS_H
#define MPEG2TRANS_H

#include "avformat.h"

#include "mythcontext.h"
#include "programinfo.h"

class MPEG2trans
{
    public:
        MPEG2trans(ProgramInfo *pginfo, int jobID);
        ~MPEG2trans();

        int DoTranscode(QString &infile, QString &tmpfile, bool useCutlist);
        int BuildKeyframeIndex(QString &file, QMap <long long, long long> &posMap);

    private:
        bool InitAV(QString &inputfile);
        bool InitOutput(QString &outfile);

        AVPacketList *ReadGop(int index, int64_t *min_dts, int64_t *min_pts, int64_t *end_pts); 
        void FreeGop(AVPacketList *pkts);
        void FlushGop(AVPacketList *pkts, int64_t delta, int64_t start, int64_t end);

        bool chkTranscodeDB;

        ProgramInfo *m_pginfo;
        int m_jobID;

        AVFormatContext *inputFC;
        AVFormatContext *outputFC;
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */

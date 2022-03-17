#include <algorithm>
#include <utility>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/stringutil.h"

#include "recordinginfo.h"
#include "recordingquality.h"

static void merge_overlapping(RecordingGaps &gaps);
static double score_gaps(const RecordingInfo& /*ri*/, const RecordingGaps& /*gaps*/);
static QDateTime get_start(const RecordingInfo& /*ri*/);
static QDateTime get_end(const RecordingInfo& /*ri*/);

RecordingQuality::RecordingQuality(const RecordingInfo *ri,
                                   RecordingGaps rg)
                 : m_recordingGaps(std::move(rg))
{
    if (!ri)
        return;

    std::stable_sort(m_recordingGaps.begin(), m_recordingGaps.end());
    merge_overlapping(m_recordingGaps);

    m_overallScore = score_gaps(*ri, m_recordingGaps);

    LOG(VB_RECORD, LOG_INFO,
        QString("RecordingQuality() score(%3)").arg(m_overallScore));
}

RecordingQuality::RecordingQuality(
    const RecordingInfo *ri, RecordingGaps rg,
    const QDateTime &first, const QDateTime &latest) :
    m_recordingGaps(std::move(rg))
{
    if (!ri)
        return;

    m_programKey = ri->MakeUniqueKey();

    // trim start
    QDateTime start = get_start(*ri);
    while (!m_recordingGaps.empty() &&
           m_recordingGaps.first().GetStart() < start)
    {
        RecordingGap &firstGap = m_recordingGaps.first();
        if (start < firstGap.GetEnd())
            firstGap = RecordingGap(start, firstGap.GetEnd());
        else
            m_recordingGaps.pop_front();
    }

    // trim end
    QDateTime end = get_end(*ri);
    while (!m_recordingGaps.empty() &&
           m_recordingGaps.back().GetEnd() > end)
    {
        RecordingGap &back = m_recordingGaps.back();
        if (back.GetStart() < end)
            back = RecordingGap(back.GetStart(), end);
        else
            m_recordingGaps.pop_back();
    }

    // account for late start
    int start_gap = (first.isValid()) ? start.secsTo(first) : 0;
    if (start_gap >  gCoreContext->GetNumSetting("MaxStartGap", 15))
        m_recordingGaps.push_front(RecordingGap(start, first));

    // account for missing end
    int end_gap = (latest.isValid()) ? latest.secsTo(end) : 0;
    if (end_gap > gCoreContext->GetNumSetting("MaxEndGap", 15))
        m_recordingGaps.push_back(RecordingGap(latest, end));

    std::stable_sort(m_recordingGaps.begin(), m_recordingGaps.end());
    merge_overlapping(m_recordingGaps);

    m_overallScore = score_gaps(*ri, m_recordingGaps);

    LOG(VB_RECORD, LOG_INFO,
        QString("RecordingQuality() start(%1) end(%2) score(%3)")
        .arg(MythDate::toString(start, MythDate::ISODate),
             MythDate::toString(end, MythDate::ISODate),
             QString::number(m_overallScore)));
}

void RecordingQuality::AddTSStatistics(
    int continuity_error_count, int packet_count)
{
    m_continuityErrorCount = continuity_error_count;
    m_packetCount = packet_count;
    if (!m_packetCount)
        return;

    double er = double(m_continuityErrorCount) / double(m_packetCount);
    if (er >= 0.01)
        m_overallScore = std::max(m_overallScore * 0.60, 0.0);
    else if (er >= 0.001)
        m_overallScore = std::max(m_overallScore * 0.80, 0.0);
    else if (er >= 0.0001)
        m_overallScore = std::max(m_overallScore * 0.90, 0.0);

    if (er >= 0.01)
        m_overallScore = std::min(m_overallScore, 0.5);
}

bool RecordingQuality::IsDamaged(void) const
{
    return (m_overallScore * 100) <
        gCoreContext->GetNumSetting("MinimumRecordingQuality", 95);
}

QString RecordingQuality::toStringXML(void) const
{
    QString str =
        QString(R"(<RecordingQuality overall_score="%1" key="%2")")
        .arg(m_overallScore).arg(m_programKey);

    if (m_packetCount)
    {
        str += QString(R"( continuity_error_count="%1" packet_count="%2")")
            .arg(m_continuityErrorCount).arg(m_packetCount);
    }

    if (m_recordingGaps.empty())
        return str + " />";

    str += ">\n";

    auto add_gap = [](const QString& s, const auto & gap) {
        return s + StringUtil::indentSpaces(1) +
            QString("<Gap start=\"%1\" end=\"%2\" duration=\"%3\" />\n")
            .arg(gap.GetStart().toString(Qt::ISODate))
            .arg(gap.GetEnd().toString(Qt::ISODate))
            .arg(gap.GetStart().secsTo(gap.GetEnd()));
    };
    str = std::accumulate(m_recordingGaps.cbegin(),m_recordingGaps.cend(),
                          str, add_gap);

    return str + "</RecordingQuality>";
}

static void merge_overlapping(RecordingGaps &gaps)
{
    if (gaps.empty())
        return;

    RecordingGaps::iterator it = gaps.begin();
    RecordingGaps::iterator next = it; ++next;
    while (next != gaps.end())
    {
        if ((*it).GetEnd() >= (*next).GetStart())
        {
            (*it) = RecordingGap((*it).GetStart(), (*next).GetEnd());
            next = gaps.erase(next);
        }
        else
        {
            it = next;
            ++next;
        }
    }
}

static double score_gaps(const RecordingInfo &ri, const RecordingGaps &gaps)
{
    RecordingGaps::const_iterator it = gaps.begin();
    if (it == gaps.end())
        return 1.0;

    QDateTime start = get_start(ri);

    double program_length = start.secsTo(get_end(ri));
    if (program_length < 1.0)
        return 0.0;

    double score = 1.0;
    for (; it != gaps.end(); ++it)
    {
        double gap_start  = start.secsTo((*it).GetStart());
        double gap_end    = start.secsTo((*it).GetEnd());
        double gap_length = gap_end - gap_start;
        double rel_start  = gap_start / program_length;
        double rel_end    = gap_end / program_length;
        double rel_center = (rel_start + rel_end) * 0.5;
        double rel_length = rel_end - rel_start;

        /*
        LOG(VB_GENERAL, LOG_INFO,
            QString("%1 gap(%2,%3,%4) rel(%5,%6,%7)")
            .arg((*it).toString())
            .arg(gap_start).arg(gap_end).arg(gap_length)
            .arg(rel_start).arg(rel_end).arg(rel_length));
        */

        if (rel_center >= 0.9 || rel_end >= 0.95)
            rel_length *= 4;

        if (rel_center < 0.1)
            rel_length *= 2;

        if (gap_length > 5) // 5 secs
            rel_length *= 1.5;

        if (gap_length > 120) // 2 minutes
            rel_length *= 5;

        // NOTE: many more scoring adjustments could be made here
        // and we may want to tune this differently depending on
        // program length.

        score -= rel_length;
    }

    return (score > 0.0) ? score : 0.0;
}

static QDateTime get_start(const RecordingInfo &ri)
{
    if (ri.GetDesiredStartTime().isValid())
    {
        return (ri.GetScheduledStartTime() > ri.GetDesiredStartTime()) ?
            ri.GetScheduledStartTime() : ri.GetDesiredStartTime();
    }
    return ri.GetScheduledStartTime();
}

static QDateTime get_end(const RecordingInfo &ri)
{
    if (ri.GetDesiredEndTime().isValid())
    {
        return (ri.GetScheduledEndTime() < ri.GetDesiredEndTime()) ?
            ri.GetScheduledEndTime() : ri.GetDesiredEndTime();
    }
    return ri.GetScheduledEndTime();
}

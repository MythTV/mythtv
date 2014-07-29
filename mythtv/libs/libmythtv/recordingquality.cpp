#include <algorithm>
using namespace std;

#include "recordingquality.h"
#include "mythcorecontext.h"
#include "recordinginfo.h"
#include "mythmiscutil.h"
#include "mythlogging.h"

static void merge_overlapping(RecordingGaps &gaps);
static double score_gaps(const RecordingInfo&, const RecordingGaps&);
static QDateTime get_start(const RecordingInfo&);
static QDateTime get_end(const RecordingInfo&);

RecordingQuality::RecordingQuality(const RecordingInfo *ri,
                                   const RecordingGaps &rg)
                 : m_continuity_error_count(0), m_packet_count(0)
{
    if (!ri)
        return;

    stable_sort(m_recording_gaps.begin(), m_recording_gaps.end());
    merge_overlapping(m_recording_gaps);

    m_overall_score = score_gaps(*ri, m_recording_gaps);

    LOG(VB_RECORD, LOG_INFO,
        QString("RecordingQuality() score(%3)").arg(m_overall_score));
}

RecordingQuality::RecordingQuality(
    const RecordingInfo *ri, const RecordingGaps &rg,
    const QDateTime &first, const QDateTime &latest) :
    m_continuity_error_count(0), m_packet_count(0),
    m_overall_score(1.0), m_recording_gaps(rg)
{
    if (!ri)
        return;

    m_program_key = ri->MakeUniqueKey();

    // trim start
    QDateTime start = get_start(*ri);
    while (!m_recording_gaps.empty() &&
           m_recording_gaps.first().GetStart() < start)
    {
        RecordingGap &first = m_recording_gaps.first();
        if (start < first.GetEnd())
            first = RecordingGap(start, first.GetEnd());
        else
            m_recording_gaps.pop_front();
    }

    // trim end
    QDateTime end = get_end(*ri);
    while (!m_recording_gaps.empty() &&
           m_recording_gaps.back().GetEnd() > end)
    {
        RecordingGap &back = m_recording_gaps.back();
        if (back.GetStart() < end)
            back = RecordingGap(back.GetStart(), end);
        else
            m_recording_gaps.pop_back();
    }

    // account for late start
    int start_gap = (first.isValid()) ? start.secsTo(first) : 0;
    if (start_gap > 15)
        m_recording_gaps.push_front(RecordingGap(start, first));

    // account for missing end
    int end_gap = (latest.isValid()) ? latest.secsTo(end) : 0;
    if (end_gap > 15)
        m_recording_gaps.push_back(RecordingGap(latest, end));

    stable_sort(m_recording_gaps.begin(), m_recording_gaps.end());
    merge_overlapping(m_recording_gaps);

    m_overall_score = score_gaps(*ri, m_recording_gaps);

    LOG(VB_RECORD, LOG_INFO,
        QString("RecordingQuality() start(%1) end(%2) score(%3)")
        .arg(MythDate::toString(start, MythDate::ISODate))
        .arg(MythDate::toString(end, MythDate::ISODate))
        .arg(m_overall_score));
}

void RecordingQuality::AddTSStatistics(
    int continuity_error_count, int packet_count)
{
    m_continuity_error_count = continuity_error_count;
    m_packet_count = packet_count;
    if (!m_packet_count)
        return;

    double er = double(m_continuity_error_count) / double(m_packet_count);
    if (er >= 0.01)
        m_overall_score = max(m_overall_score * 0.60, 0.0);
    else if (er >= 0.001)
        m_overall_score = max(m_overall_score * 0.80, 0.0);
    else if (er >= 0.0001)
        m_overall_score = max(m_overall_score * 0.90, 0.0);

    if (er >= 0.01)
        m_overall_score = min(m_overall_score, 0.5);
}

bool RecordingQuality::IsDamaged(void) const
{
    return (m_overall_score * 100) <
        gCoreContext->GetNumSetting("MinimumRecordingQuality", 95);
}

QString RecordingQuality::toStringXML(void) const
{
    QString str =
        QString("<RecordingQuality overall_score=\"%1\" key=\"%2\"")
        .arg(m_overall_score).arg(m_program_key);

    if (m_packet_count)
    {
        str += QString(" countinuity_error_count=\"%1\" packet_count=\"%2\"")
            .arg(m_continuity_error_count).arg(m_packet_count);
    }

    if (m_recording_gaps.empty())
        return str + " />";

    str += ">\n";

    RecordingGaps::const_iterator it = m_recording_gaps.begin();
    for (; it != m_recording_gaps.end(); ++it)
    {
        str += xml_indent(1) +
            QString("<Gap start=\"%1\" end=\"%2\" duration=\"%3\" />\n")
            .arg((*it).GetStart().toString(Qt::ISODate))
            .arg((*it).GetEnd().toString(Qt::ISODate))
            .arg((*it).GetStart().secsTo((*it).GetEnd()));
    }

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
    else
    {
        return ri.GetScheduledStartTime();
    }
}

static QDateTime get_end(const RecordingInfo &ri)
{
    if (ri.GetDesiredEndTime().isValid())
    {
        return (ri.GetScheduledEndTime() < ri.GetDesiredEndTime()) ?
            ri.GetScheduledEndTime() : ri.GetDesiredEndTime();
    }
    else
    {
        return ri.GetScheduledEndTime();
    }
}

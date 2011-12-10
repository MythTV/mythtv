#include <QList>
#include <QMap>
#include <QString>

#include "flagresults.h"
#include "mythlogging.h"

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

FlagFindingsMap findingsMap;

void findingsAdd(int value, QString description);

void FindingsInitialize(void)
{
#undef _FINDINGDEFS_H
#define _IMPLEMENT_FINDINGS
#include "findingdefs.h"
}

void findingsAdd(int value, QString description)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("FindingsMap: Inserting %1:%2")
        .arg(value) .arg(description));
    findingsMap.insert(value, description);
}

QString FlagFindings::toString(void)
{
    QString finding = QString("Unknown finding (%1)").arg(m_type);
    if (findingsMap.contains((int)m_type))
        finding = findingsMap.value((int)m_type);

    QString str =
        QString("Findings: Frame: %1, Offset: %2.%3, Duration: %4.%5\n")
        .arg(m_timestamp)
        .arg(m_offset / 1000000)
        .arg((int)(m_offset % 1000000), 6, 10, QChar('0'))
        .arg(m_duration / 1000000)
        .arg((int)(m_duration % 1000000), 6, 10, QChar('0'));

    str += QString("    %1: %2").arg(finding).arg(m_value);
    return str;
}

QString FlagFindings::toGnuplot(void)
{
    int64_t timestamp = m_timestamp * m_frameDuration + m_offset;
    QString str = QString("%1 %2 %3") .arg((double)(timestamp) / 1000000.0)
            .arg((double)(timestamp + m_duration - 1) / 1000000.0)
            .arg(m_type + 1);
    return str;
}

void FlagFindings::SetTiming(int64_t timestamp, int duration, int frameDuration,
                             int offset)
{
    m_timestamp = timestamp;
    m_duration = duration;
    m_frameDuration = frameDuration;
    m_offset = offset;
}

QString FlagResults::toString(void)
{
    FlagResults::iterator it;
    QString str("");

    for (it = begin(); it != end(); ++it)
    {
        FlagFindings *finding = *it;
        str += QString("%1\n").arg(finding->toString());
    }
    str += QString("\n");
    return str;
}

QString FlagResults::toGnuplot(void)
{
    FlagResults::iterator it;
    QString str("");

    for (it = begin(); it != end(); ++it)
    {
        FlagFindings *finding = *it;
        str += QString("%1\n") .arg(finding->toGnuplot());
    }
    return str;
}

FlagResults *FlagResults::Create(ResultsMap *map, int64_t timestamp)
{
    FlagResults *results;
    if (map->contains(timestamp))
        results = map->value(timestamp);
    else
    {
        results = new FlagResults;
        map->insertMulti(timestamp, results);
    }

    return results;
}

QString ResultsMap::toString(QString title)
{
    ResultsMap::iterator it;
    QString str = QString("-----------------------------\n%1\n\n").arg(title);

    for (it = begin(); it != end(); ++it)
    {
        FlagResults *results = it.value();
        str += results->toString();
    }
    return str;
}

QString ResultsMap::toGnuplot(void)
{
    ResultsMap::iterator it;
    QString str("");

    for (it = begin(); it != end(); ++it)
    {
        FlagResults *results = it.value();
        str += results->toGnuplot();
    }
    return str;
}

ResultsMap *ResultsMap::Compress(int frameDuration)
{
    if (frameDuration == 0)
        return NULL;

    ResultsMap *newMap = new ResultsMap;
    ResultsMap::iterator it;
    for (it = begin(); it != end(); ++it)
    {
        FlagResults *oldResults = it.value();
        FlagResults::iterator it2 = oldResults->begin();
        if (it2 == oldResults->end())
            continue;

        FlagFindings *oldFinding = *it2;

        int remainDuration = oldFinding->m_duration;
        int64_t timestamp = oldFinding->m_timestamp / frameDuration;
        int offset = oldFinding->m_timestamp - (timestamp * frameDuration);
        int duration = MIN(remainDuration, frameDuration - offset);

        do {
            for (it2 = oldResults->begin(); it2 != oldResults->end(); ++it2)
            {
                FlagResults *results = FlagResults::Create(newMap, timestamp);
                oldFinding = *it2;

                FlagFindings *finding = new FlagFindings(oldFinding);
                finding->SetTiming(timestamp, duration, frameDuration, offset);
                results->append(finding);
            }

            remainDuration -= duration;
            duration = MIN(frameDuration, remainDuration);
            offset = 0;
            timestamp++;
        } while (remainDuration);
    }
    return newMap;
}

int64_t ResultsMap::GetDuration(void)
{
    ResultsMap::iterator it = begin();
    if (it == end())
        return 0;

    FlagResults *results = it.value();
    FlagResults::iterator it2 = results->begin();
    if (it2 == results->end())
        return 0;

    FlagFindings *finding = *it2;
    return finding->m_frameDuration;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

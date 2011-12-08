#include <QList>
#include <QMap>
#include <QString>

#include "flagresults.h"
#include "mythlogging.h"

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

    QString str = QString("%1: %2").arg(finding).arg(m_value);
    return str;
}

QString FlagResults::toString(void)
{
    FlagFindingsList::iterator it;

    QString str = QString("Findings at PTS: %1, Duration: %2\n")
        .arg(m_timestamp) .arg(m_duration);

    for (it = m_findings->begin(); it != m_findings->end(); ++it)
    {
        FlagFindings *finding = *it;
        str += QString("    %1\n").arg(finding->toString());
    }
    str += QString("\n");
    return str;
}

QString ResultsList::toString(QString title)
{
    ResultsList::iterator it;
    QString str = QString("-----------------------------\n%1\n\n").arg(title);

    for (it = begin(); it != end(); ++it)
    {
        FlagResults *results = *it;
        str += results->toString();
    }
    return str;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

// C++ includes
#include <iostream>
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QDomDocument>

// libmyth* includes
#include "exitcodes.h"
#include "mythlogging.h"

// Local includes
#include "markuputils.h"

static int GetMarkupList(const MythUtilCommandLineParser &cmdline,
                         const QString &type)
{
    ProgramInfo pginfo;
    if (!GetProgramInfo(cmdline, pginfo))
        return GENERIC_EXIT_NO_RECORDING_DATA;

    frm_dir_map_t cutlist;
    frm_dir_map_t::const_iterator it;
    QString result;

    if (type == "cutlist")
        pginfo.QueryCutList(cutlist);
    else
        pginfo.QueryCommBreakList(cutlist);

    uint64_t lastStart = 0;
    for (it = cutlist.begin(); it != cutlist.end(); ++it)
    {
        if ((*it == MARK_COMM_START) ||
            (*it == MARK_CUT_START))
        {
            if (!result.isEmpty())
                result += ",";
            lastStart = it.key();
            result += QString("%1-").arg(lastStart);
        }
        else
        {
            if (result.isEmpty())
                result += "0-";
            result += QString("%1").arg(it.key());
        }
    }

    if (result.endsWith('-'))
    {
        uint64_t lastFrame = pginfo.QueryLastFrameInPosMap() + 60;
        if (lastFrame > lastStart)
            result += QString("%1").arg(lastFrame);
    }

    if (type == "cutlist")
        cout << QString("Cutlist: %1\n").arg(result).toLocal8Bit().constData();
    else
    {
        cout << QString("Commercial Skip List: %1\n")
            .arg(result).toLocal8Bit().constData();
    }

    return GENERIC_EXIT_OK;
}

static int SetMarkupList(const MythUtilCommandLineParser &cmdline,
                         const QString &type, QString newList)
{
    ProgramInfo pginfo;
    if (!GetProgramInfo(cmdline, pginfo))
        return GENERIC_EXIT_NO_RECORDING_DATA;

    bool isCutlist = (type == "cutlist");
    frm_dir_map_t markuplist;

    newList.replace(QRegExp(" "), "");

    QStringList tokens = newList.split(",", QString::SkipEmptyParts);

    if (newList.isEmpty())
        newList = "(EMPTY)";

    for (int i = 0; i < tokens.size(); i++)
    {
        QStringList cutpair = tokens[i].split("-", QString::SkipEmptyParts);
        if (isCutlist)
        {
            markuplist[cutpair[0].toInt()] = MARK_CUT_START;
            markuplist[cutpair[1].toInt()] = MARK_CUT_END;
        }
        else
        {
            markuplist[cutpair[0].toInt()] = MARK_COMM_START;
            markuplist[cutpair[1].toInt()] = MARK_COMM_END;
        }
    }

    if (isCutlist)
    {
        pginfo.SaveCutList(markuplist);
        cout << QString("Cutlist set to: %1\n")
            .arg(newList).toLocal8Bit().constData();
        LOG(VB_GENERAL, LOG_NOTICE, QString("Cutlist set to: %1").arg(newList));
    }
    else
    {
        pginfo.SaveCommBreakList(markuplist);
        cout << QString("Commercial Skip List set to: %1\n")
            .arg(newList).toLocal8Bit().constData();
        LOG(VB_GENERAL, LOG_NOTICE, QString("Commercial Skip List set to: %1").arg(newList));
    }

    return GENERIC_EXIT_OK;
}

static int GetCutList(const MythUtilCommandLineParser &cmdline)
{
    return GetMarkupList(cmdline, "cutlist");
}

static int SetCutList(const MythUtilCommandLineParser &cmdline)
{
    return SetMarkupList(cmdline, QString("cutlist"),
                         cmdline.toString("setcutlist"));
}

static int ClearCutList(const MythUtilCommandLineParser &cmdline)
{
    return SetMarkupList(cmdline, QString("cutlist"), QString(""));
}

static int CopySkipListToCutList(const MythUtilCommandLineParser &cmdline)
{
    ProgramInfo pginfo;
    if (!GetProgramInfo(cmdline, pginfo))
        return GENERIC_EXIT_NO_RECORDING_DATA;

    frm_dir_map_t cutlist;
    frm_dir_map_t::const_iterator it;

    pginfo.QueryCommBreakList(cutlist);
    for (it = cutlist.begin(); it != cutlist.end(); ++it)
        if (*it == MARK_COMM_START)
            cutlist[it.key()] = MARK_CUT_START;
        else
            cutlist[it.key()] = MARK_CUT_END;
    pginfo.SaveCutList(cutlist);

    cout << "Commercial Skip List copied to Cutlist\n";
    LOG(VB_GENERAL, LOG_NOTICE, "Commercial Skip List copied to Cutlist");

    return GENERIC_EXIT_OK;
}

static int GetSkipList(const MythUtilCommandLineParser &cmdline)
{
    return GetMarkupList(cmdline, "skiplist");
}

static int SetSkipList(const MythUtilCommandLineParser &cmdline)
{
    return SetMarkupList(cmdline, QString("skiplist"),
                         cmdline.toString("setskiplist"));
}

static int ClearSkipList(const MythUtilCommandLineParser &cmdline)
{
    return SetMarkupList(cmdline, QString("skiplist"), QString(""));
}

static int ClearSeekTable(const MythUtilCommandLineParser &cmdline)
{
    ProgramInfo pginfo;
    if (!GetProgramInfo(cmdline, pginfo))
        return GENERIC_EXIT_NO_RECORDING_DATA;

    cout << "Clearing Seek Table\n";
    LOG(VB_GENERAL, LOG_NOTICE, pginfo.IsVideo() ?
        QString("Clearing Seek Table for Video %1").arg(pginfo.GetPathname()) :
        QString("Clearing Seek Table for Channel ID %1 @ %2")
                .arg(pginfo.GetChanID())
                .arg(pginfo.GetScheduledStartTime().toString()));
    pginfo.ClearPositionMap(MARK_GOP_BYFRAME);
    pginfo.ClearPositionMap(MARK_GOP_START);
    pginfo.ClearPositionMap(MARK_KEYFRAME);
    pginfo.ClearPositionMap(MARK_DURATION_MS);
    pginfo.ClearMarkupFlag(MARK_DURATION_MS);
    pginfo.ClearMarkupFlag(MARK_TOTAL_FRAMES);

    return GENERIC_EXIT_OK;
}

static int GetMarkup(const MythUtilCommandLineParser &cmdline)
{
    ProgramInfo pginfo;
    if (!GetProgramInfo(cmdline, pginfo))
        return GENERIC_EXIT_NO_RECORDING_DATA;

    QString filename = cmdline.toString("getmarkup");
    if (filename.isEmpty())
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --getmarkup filename\n");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QVector<ProgramInfo::MarkupEntry> mapMark, mapSeek;
    pginfo.QueryMarkup(mapMark, mapSeek);
    QFile outfile(filename);
    if (!outfile.open(QIODevice::WriteOnly))
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
            QString("Couldn't open output file %1\n").arg(filename));
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QTextStream stream(&outfile);
    QDomDocument xml;
    QDomProcessingInstruction processing =
        xml.createProcessingInstruction("xml",
                                        "version='1.0' encoding='utf-8'");
    xml.appendChild(processing);
    QDomElement root = xml.createElement("metadata");
    xml.appendChild(root);
    QDomElement item = xml.createElement("item");
    root.appendChild(item);
    QDomElement markup = xml.createElement("markup");
    item.appendChild(markup);
    for (int i = 0; i < mapMark.size(); ++i)
    {
        ProgramInfo::MarkupEntry &entry = mapMark[i];
        QDomElement child = xml.createElement("mark");
        child.setAttribute("type", entry.type);
        child.setAttribute("frame", (qulonglong)entry.frame);
        if (!entry.isDataNull)
            child.setAttribute("data", (qulonglong)entry.data);
        markup.appendChild(child);
    }
    for (int i = 0; i < mapSeek.size(); ++i)
    {
        ProgramInfo::MarkupEntry &entry = mapSeek[i];
        QDomElement child = xml.createElement("seek");
        child.setAttribute("type", entry.type);
        child.setAttribute("frame", (qulonglong)entry.frame);
        if (!entry.isDataNull)
            child.setAttribute("data", (qulonglong)entry.data);
        markup.appendChild(child);
    }

    stream << xml.toString(2);
    outfile.close();
    return GENERIC_EXIT_OK;
}

static int SetMarkup(const MythUtilCommandLineParser &cmdline)
{
    ProgramInfo pginfo;
    if (!GetProgramInfo(cmdline, pginfo))
        return GENERIC_EXIT_NO_RECORDING_DATA;

    QString filename = cmdline.toString("setmarkup");
    if (filename.isEmpty())
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --setmarkup filename\n");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QVector<ProgramInfo::MarkupEntry> mapMark, mapSeek;
    QFile infile(filename);
    if (!infile.open(QIODevice::ReadOnly))
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
            QString("Couldn't open input file %1\n").arg(filename));
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QDomDocument xml;
    if (!xml.setContent(&infile))
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
            QString("Failed to read valid XML from file %1\n").arg(filename));
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QDomElement metadata = xml.documentElement();
    if (metadata.tagName() != "metadata")
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
            QString("Expected top-level 'metadata' element "
                    "in file %1\n").arg(filename));
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QDomNode item = metadata.firstChild();
    if (!item.isElement() || item.toElement().tagName() != "item")
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
            QString("Expected 'item' element within 'metadata' element "
                    "in file %1\n").arg(filename));
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QDomNode markup = item.firstChild();
    if (!markup.isElement() || markup.toElement().tagName() != "markup")
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
            QString("Expected 'markup' element within 'item' element "
                    "in file %1\n").arg(filename));
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    for (QDomNode n = markup.firstChild(); !n.isNull(); n = n.nextSibling())
    {
        if (n.isElement())
        {
            QDomElement e = n.toElement();
            QString tagName = e.tagName();
            bool isMark;
            if (tagName == "mark")
                isMark = true;
            else if (tagName == "seek")
                isMark = false;
            else
            {
                LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
                    QString("Weird tag '%1', expected 'mark' or 'seek'\n")
                    .arg(tagName));
                continue;
            }
            int type = e.attribute("type").toInt();
            uint64_t frame = e.attribute("frame").toULongLong();
            QString dataString = e.attribute("data");
            uint64_t data = dataString.toULongLong();
            bool isDataNull = dataString.isNull();
            ProgramInfo::MarkupEntry entry(type, frame,
                                           data, isDataNull);
            if (isMark)
                mapMark.append(entry);
            else
                mapSeek.append(entry);
        }
    }
    pginfo.SaveMarkup(mapMark, mapSeek);

    return GENERIC_EXIT_OK;
}

void registerMarkupUtils(UtilMap &utilMap)
{
    utilMap["gencutlist"]             = &CopySkipListToCutList;
    utilMap["getcutlist"]             = &GetCutList;
    utilMap["setcutlist"]             = &SetCutList;
    utilMap["clearcutlist"]           = &ClearCutList;
    utilMap["getskiplist"]            = &GetSkipList;
    utilMap["setskiplist"]            = &SetSkipList;
    utilMap["clearskiplist"]          = &ClearSkipList;
    utilMap["clearseektable"]         = &ClearSeekTable;
    utilMap["getmarkup"]              = &GetMarkup;
    utilMap["setmarkup"]              = &SetMarkup;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

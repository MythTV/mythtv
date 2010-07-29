/*
  playlistfile (.pls) parser
  Eskil Heyn Olsen, 2005, distributed under the GPL as part of mythtv.

  Update July 2010 updated for Qt4 (Paul Harrison)

*/

// c
//#include <assert.h>
//#include "iostream"

// qt
#include <QPair>
#include <QList>
#include <QMap>

// mythtv
#include <mythverbose.h>

// mythmusic
#include "pls.h"


using namespace std;

class CfgReader
{
  public:
    CfgReader()
    {
    }
    ~CfgReader()
    {
    }

    typedef QPair<QString,QString> KeyValue;
    typedef QList<KeyValue> KeyValueList;
    typedef QMap<QString, KeyValueList> ConfigMap;

    void parse(const char *d, int l)
    {
        const char *ptr = d;
        int line = 1;
        bool done = l <= 0;

        QString current_section = "";
        KeyValueList keyvals;

        while(!done)
        {
            switch(*ptr)
            {
                case '\0':
                    done = true;
                    break;
                case '#':
                {
                    char *end = strchr(ptr, '\n');
                    if (!end) done = true;
                    ptr = end;
                    break;
                }
                case '\n':
                    ptr ++;
                    line ++;
                    break;
                case '[':
                {
                    ptr ++;
                    const char *nl = strchr(ptr, '\n');
                    const char *end = strchr(ptr, ']');

                    if (!nl) nl = d + l;

                    if (!end || nl < end)
                    {
                        VERBOSE(VB_IMPORTANT, QString("CfgReader:: Badly formatted section, line %1").arg(line));
                        done = true;
                    }

                    if (current_section.length() > 0)
                    {
                        cfg[current_section] = keyvals;
                        keyvals = KeyValueList();
                    }

                    current_section = std::string(ptr, end - ptr).c_str();
                    if (current_section.length() == 0)
                    {
                        VERBOSE(VB_IMPORTANT, QString("CfgReader:: Badly formatted section, line %1").arg(line));
                        done = true;
                    }
                    ptr = end + 1;
                    break;
                }
                default:
                {
                    if (current_section.length() > 0)
                    {
                        const char *eq = strchr(ptr, '=');
                        const char *nl = strchr(ptr, '\n');

                        if (!nl) nl = d + l;

                        if (!eq || nl < eq) 
                        {
                            VERBOSE(VB_IMPORTANT, QString("CfgReader:: Badly formatted line %1").arg(line));
                            done = true;
                        }
                        else
                        {
                            QString key = string(ptr, eq - ptr).c_str();
                            QString val = string(eq + 1, nl - eq - 1).c_str();
                            keyvals.push_back(KeyValue(key, val));
                            ptr = nl;
                        }
                    }
                    else
                    {
                        VERBOSE(VB_IMPORTANT, QString("CfgReader:: Badly formatted line %1").arg(line));
                        done = true;
                    }
                    break;
                }
            }

            if (ptr - d == l) 
                done = true;
        }

        if (current_section.length() > 0)
            cfg[current_section] = keyvals;
    }

    QList<QString> getSections(void)
    {
        QList<QString> res;
        for (ConfigMap::iterator it = cfg.begin(); it != cfg.end(); it++)
            res.push_back(it.key());
        return res;
    }

    QList<QString> getKeys(const QString &section)
    {
        KeyValueList keylist = cfg[section];
        QList<QString> res;
        for (KeyValueList::iterator it = keylist.begin(); it != keylist.end(); it++)
            res.push_back((*it).first);
        return res;
    }

    QString getStrVal(const QString &section, const QString &key, const QString &def = "")
    {
        KeyValueList keylist = cfg[section];
        QString res = def;
        for (KeyValueList::iterator it = keylist.begin(); it != keylist.end(); it++)
        {
            if ((*it).first == key) 
            {
                res =(*it).second;
                break;
            }
        }
        return res;
    }

    int getIntVal(const QString &section, const QString &key, int def=0) 
    {
        QString def_str;
        def_str.setNum (def);
        return getStrVal(section, key, def_str).toInt();
    }

  private:
    ConfigMap cfg;
};

/****************************************************************************/

PlayListFile::PlayListFile(void)
{
}

PlayListFile::~PlayListFile(void)
{
    clear();
}

int PlayListFile::parse(PlayListFile *pls, QTextStream *stream)
{
    int parsed = 0;
    QString d = stream->read();
    CfgReader cfg;
    cfg.parse(d.toAscii(), d.length());

    int num_entries = cfg.getIntVal("playlist", "numberofentries", -1);

    // Some pls files have "numberofentries", some has "NumberOfEntries".
    if (num_entries == -1) 
        num_entries = cfg.getIntVal("playlist", "NumberOfEntries", -1);

    for (int n = 1; n <= num_entries; n++)
    {
        PlayListFileEntry *e = new PlayListFileEntry();
        QString t_key = QString("Title%1").arg(n);
        QString f_key = QString("File%1").arg(n);
        QString l_key = QString("Length%1").arg(n);

        e->setFile(cfg.getStrVal("playlist", f_key));
        e->setTitle(cfg.getStrVal("playlist", t_key));
        e->setLength(cfg.getIntVal("playlist", l_key));

        pls->add(e);
        parsed++;
    }

    return parsed;
}



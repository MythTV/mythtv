#include <QDir>

#include <map>
#include <vector>

#include "dbaccess.h"

#include "dirscan.h"

DirectoryHandler::~DirectoryHandler()
{
}

namespace
{
    class ext_lookup
    {
      private:
        typedef std::map<QString, bool> ext_map;
        ext_map m_extensions;
        bool m_list_unknown;

      public:
        ext_lookup(const FileAssociations::ext_ignore_list &ext_disposition,
                   bool list_unknown) : m_list_unknown(list_unknown)
        {
            for (FileAssociations::ext_ignore_list::const_iterator p =
                 ext_disposition.begin(); p != ext_disposition.end(); ++p)
            {
                m_extensions.insert(ext_map::value_type(p->first.toLower(),
                                                        p->second));
            }
        }

        bool extension_ignored(const QString &extension) const
        {
            ext_map::const_iterator p = m_extensions.find(extension.toLower());
            if (p != m_extensions.end())
                return p->second;
            return !m_list_unknown;
        }
    };

    void scan_dir(const QString &start_path, DirectoryHandler *handler,
                  const ext_lookup &ext_settings)
    {
        QDir d(start_path);

        if (!d.exists())
            return;

        QFileInfoList list = d.entryInfoList();
        if (!list.size())
            return;

        QDir dir_tester;

        for (QFileInfoList::iterator p = list.begin(); p != list.end(); ++p)
        {
            if (p->fileName() == "." ||
                p->fileName() == ".." ||
                p->fileName() == "Thumbs.db")
            {
                continue;
            }

            if (!p->isDir() &&
                ext_settings.extension_ignored(p->extension(false))) continue;

            bool add_as_file = true;

            if (p->isDir())
            {
                add_as_file = false;

                dir_tester.setPath(p->absFilePath() + "/VIDEO_TS");
                if (dir_tester.exists())
                {
                    add_as_file = true;
                }
                else
                {
                    DirectoryHandler *dh = handler->newDir(p->fileName(),
                                                           p->absFilePath());
                    scan_dir(p->absFilePath(), dh, ext_settings);
                }
            }

            if (add_as_file)
            {
                handler->handleFile(p->fileName(), p->absFilePath(),
                                    p->extension(false));
            }
        }
    }
}

void ScanVideoDirectory(const QString &start_path, DirectoryHandler *handler,
        const FileAssociations::ext_ignore_list &ext_disposition,
        bool list_unknown_extensions)
{
    ext_lookup extlookup(ext_disposition, list_unknown_extensions);
    scan_dir(start_path, handler, extlookup);
}

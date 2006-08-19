#ifndef DIRSCAN_H_
#define DIRSCAN_H_

class DirectoryHandler
{
  public:
    virtual ~DirectoryHandler();
    virtual DirectoryHandler *newDir(const QString &dir_name,
                                     const QString &fq_dir_name) = 0;
    virtual void handleFile(const QString &file_name,
                            const QString &fq_file_name,
                            const QString &extension) = 0;
};

void ScanVideoDirectory(const QString &start_path, DirectoryHandler *handler,
        const FileAssociations::ext_ignore_list &ext_disposition,
        bool list_unknown_extensions);

#endif // DIRSCAN_H_

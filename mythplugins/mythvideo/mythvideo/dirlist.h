#ifndef DIRLIST_H_
#define DIRLIST_H_

class Dirlist
{
  public:
    Dirlist::Dirlist(QString &directory);
    QValueList<Metadata> Dirlist::GetPlaylist()
    {
        return playlist;
    };
  private:
    Metadata * Dirlist::CheckFile(QString &filename);
    QValueList<Metadata> playlist;
};

#endif

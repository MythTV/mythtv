#ifndef DIRLIST_H_
#define DIRLIST_H_

class Dirlist
{
  public:
    Dirlist::Dirlist(MythContext *context, QString &directory);
    QValueList<Metadata> Dirlist::GetPlaylist()
    {
        return playlist;
    };
  private:
    Metadata * Dirlist::CheckFile(MythContext *context, QString &filename);
    QValueList<Metadata> playlist;
};

#endif

#ifndef PLAYLIST_H_

#include <qvaluelist.h>
#include <qsqldatabase.h>

#include "metadata.h"

void LoadDefaultPlaylist(QSqlDatabase *db, QValueList<Metadata> &playlist);
void SaveDefaultPlaylist(QSqlDatabase *db, QValueList<Metadata> &playlist);

#endif

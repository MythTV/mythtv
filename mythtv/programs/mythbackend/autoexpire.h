#ifndef AUTOEXPIRE_H_
#define AUTOEXPIRE_H_

class ProgramInfo;
class QSqlDatabase;

#include <qmap.h> 
#include <list>
#include <vector>
#include <qmutex.h>
#include <qobject.h>

using namespace std;

class AutoExpire : public QObject
{
  public:
    AutoExpire(bool runthread, bool master, QSqlDatabase *ldb);
   ~AutoExpire();

    void FillExpireList();
    void PrintExpireList();

  protected:
    void RunExpirer(void);
    static void *ExpirerThread(void *param);

  private:
    void ClearExpireList(void);

    void ExpireEpisodesOverMax(void);

    void FillOldestFirst(void);

    vector<ProgramInfo *> expireList;

    QSqlDatabase *db;

    bool threadrunning;
    bool isMaster;
};

#endif

#ifndef AUTOEXPIRE_H_
#define AUTOEXPIRE_H_

class ProgramInfo;
class QSqlDatabase;

#include <qmap.h> 
#include <list>
#include <vector>
#include <qobject.h>

using namespace std;

class AutoExpire : public QObject
{
  public:
    AutoExpire(bool runthread, QSqlDatabase *ldb);
   ~AutoExpire();

    void FillExpireList();
    void PrintExpireList();

  protected:
    void RunExpirer(void);
    static void *ExpirerThread(void *param);

  private:
    void ClearExpireList(void);

    void FillOldestFirst(void);

    vector<ProgramInfo *> expireList;

    QSqlDatabase *db;

    pthread_mutex_t expirerLock;

    bool threadrunning;
};

#endif

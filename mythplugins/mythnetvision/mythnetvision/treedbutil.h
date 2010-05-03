#ifndef TREEDBUTIL_H_
#define TREEDBUTIL_H_

#include <QString>
#include <QDateTime>

#include "grabbermanager.h"
#include "parse.h"

class QString;
class QDateTime;
class NewsSiteItem;

bool findTreeGrabberInDB(const QString &commandline);
GrabberScript* findTreeGrabberByCommand(const QString &url);
GrabberScript::scriptList findAllDBTreeGrabbers(void);
bool insertTreeInDB(GrabberScript *script);
bool insertTreeInDB(const QString &name, const QString &thumbnail,
                const QString &commandline);

bool removeTreeFromDB(GrabberScript *script);
bool removeTreeFromDB(const QString &commandline);

bool markTreeUpdated(GrabberScript *script, QDateTime curTime);
bool needsUpdate(GrabberScript *script, uint updateFreq);
QDateTime lastUpdate(GrabberScript* script);

bool clearTreeItems(const QString &feedtitle);
bool isTreeInUse(const QString &feedtitle);
bool insertTreeArticleInDB(const QString &feedtitle, const QString &path,
                       const QString &paththumb, ResultVideo *item);
QMultiMap<QPair<QString,QString>, ResultVideo*> getTreeArticles(const QString &feedtitle);

#endif

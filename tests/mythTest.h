#ifndef myth_mythTEST_H
#define myth_mythTEST_H

#include <QtCore/QObject>

class mythTest : public QObject
{
Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void someTest();
};

#endif // myth_mythTEST_H

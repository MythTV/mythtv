#ifndef VXML_H_
#define VXML_H_

#include <qurl.h>

#include "rtp.h"


class vxmlVariable
{
  public:
    vxmlVariable(QString N, QString V);             
    vxmlVariable::vxmlVariable(QString N, short *wav, int S);
    virtual ~vxmlVariable() {}; 
    bool isType(QString t) { return (t == Type); };
    QString getName() { return Name; };
    QString getSValue() { return sValue; };
    short  *getSPValue() { return spValue; };
    int     getSPLength() { return spLength; };
    void    delSPValue() { delete spValue; spValue=0; };

  private:
    QString Name;
    QString Type;
    QString sValue;
    short  *spValue;
    int     spLength;
};

class vxmlVarContainer : public QPtrList<vxmlVariable>
{
  public:
    vxmlVarContainer();
    ~vxmlVarContainer();
    vxmlVariable *findFirstVariable(QString T);
    QString findStringVariable(QString N);
    short *findShortPtrVariable(QString N, int &Samples);
    void removeMatching(QString N);

  private:
};


class vxmlParser
{

  public:
    vxmlParser();
    virtual ~vxmlParser();
    void beginVxmlSession(rtp *r, QString cName);
    void endVxmlSession();


  private:
    static void *vxmlThread(void *p);
    void runVxmlSession();
    bool killVxmlThread;
    bool killVxmlSession;
    bool killVxmlPage;
    QString callerName;
    QString vxmlUrl;
    QString httpMethod;
    QString postNamelist;
    void vxmlThreadWorker();
    bool loadVxmlPage(QString Url, QString Method, QString Namelist, QDomDocument &script);
    void Parse(QDomDocument &vxmlPage);
    void parseForm(QDomElement &formElm);
    bool parseField(QDomElement &field);
    void parseFieldType(QString Type, uint &Max, uint &Min);
    void parseRecord(QDomElement &record);
    void parseFilled(QDomElement &filled, bool &reprompt);
    void parseNoInput(QDomElement &noInput, bool &reprompt);
    void parseIfExpression(QDomElement &ifExp, bool &reprompt);
    bool parseIfBlock(QDomElement &ifBlock, QString Condition, bool &reprompt);
    void parsePrompt(QDomElement &parentElm, bool dtmfInterrupts);
    int  parseDurationType(QString t);
    void PlayWav(QString wavFile);
    void PlayWav(short *buffer, int Samples);
    void SaveWav(short *buffer, int Samples);
    void PlayBeep(int freqHz=1000, int volume=7000, int ms=800);
    void PlaySilence(int ms, bool dtmfInterrupts);
    void PlayTTSPrompt(QString prompt, bool dtmfInterrupts);
    int RecordAudio(short *buffer, int Samples, bool dtmfInterrupts);
    void waitUntilFinished(bool dtmfInterrupts);
    bool evaluateExpression(QString Expression);

    pthread_t vxmlthread;
    QWaitCondition *waker;
    QDomDocument vxmlDoc;
    vxmlVarContainer *vxmlVarList;
    QUrl lastUrl;

    rtp *Rtp;
};



#endif

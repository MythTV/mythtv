/*
	vxml.cpp

	(c) 2004 Paul Volkaerts
	Part of the mythTV project
	

This class takes a VXML page passed and interprets the script driving
the necessary action. 

For a good reference for VXML formatting see
            http://www.voxpilot.com/html/tagref/


The VXML page must be wrapped with a <vxml ...>
construct and can contain one of the following elements...

<prompt>Hello</prompt>
<prompt>Hello.<break time="500ms"/>Please leave a message.</prompt>
    
This will play the embedded string "Hello" through a Text-To-Speech 
engine. The prompt will play from start to end before returning control 
to the script. The second example leaves a gap of silence which can be expressed
in seconds (s) or milliseconds (ms).


<prompt><audio src="filename.wav"/></prompt>
    
As above, except the file is played instead of using text to speech. The only 
file format supported is RIFF ".wav" files encoded as Mono 8KHz PCM.


<form>
  <record name="xxx" beep="true" maxtime="30s" dtmfterm="true" type="audio/x-wav">
    <prompt>After the beep leave your message<break time="50ms"/></prompt>
    <filled>
      <prompt>Thanks. <break time="50ms"/></prompt>
      <prompt>You said <audio expr="xxx"/></prompt>
    </filled>
  </record>
</form>

This form records speech according to the parameters above. The optional
prompt is played first. Recording terminates on either DTMF (if option set
to true above), on timeout (30s above) or on call termination. Once recorded
you can issue another optional prompt which can read back the recorded message.
The default (& currently the only) action is to store the message as voicemail. 
The message name ("xxx" above) can be referred to later in the script to replay 
the recorded message back to the caller.


<form>
  <field name="pin" type="digits?length=5" modal="true">
    <prompt>Enter a 5 digit password</prompt>
  </field>
  <noinput count="1">
    I need 5 digits, please try again
    <reprompt/>
  </noinput>
  <filled>
    <if cond="pin == 12345">
      <submit next="secure-menu.vxml" method="get" />
    <elseif cond="pin == 54321">
      Please Try Again
      <reprompt/>
    <else>
      Bye
    </if>
  </filled>
</form>

This forms prompts and waits for a number of DTMF digits. There is a default
timer if no digits are entered. The "modal" parameter determines whether the
prompt can be interrupted or whether it must finish before digits can be 
entered. This is useful, for instance, for gathering a pin number and can be
used for a simple menu. The "noinput" contruct is called if too few digits
are collected and the "filled" construct is called if exactly the right 
number of digits are entered. The number of digits required are defined by
the "type" parameter which takes either digits?length, digits?maxlength, or
digits?minlength; or a combination of the last two seperated by a semicolon.
The "IF" construct allows you to jump to a new VXML page upon completion, to
just say a message, or to say a message then go back to the start of the form.


<disconnect/>

Causes the script to terminate and the call to be cleared. Usually used for
example in an IF block if you have more scriot after that block.

<clear namelist="xxx"/>

Clears a variable set previously. Can be used within an IF or NOINPUT block,
and as an example can delete a recorded message. Note you don't have to clear
a variable before you overwrite it; deletion before overwriting is automatic.

**********************************************************************/


#include <qapplication.h>
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qdom.h>
#include <qimage.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <iostream>

#include <mythtv/mythcontext.h>

using namespace std;

#include "config.h"
#include "vxml.h"
#include "tts.h"


tts *speechEngine;


/**********************************************************************
vxmlParser

This class takes a VXML page passed and interprets the script driving
the necessary action. The VXML page must be wrapped with a <vxml ...>
construct and can contain one of the elements described at the top of
this file.

Note this runs as a background task constantly; because once the TTS
engine is initialised it must continue to be called from the same 
process.

**********************************************************************/


vxmlParser::vxmlParser()
{
    Rtp = 0;
    callerName = "";
    killVxmlThread = false;
    killVxmlSession = false;
    killVxmlPage = false;
    waker = new QWaitCondition();
    pthread_create(&vxmlthread, NULL, vxmlThread, this);
}

vxmlParser::~vxmlParser()
{
    killVxmlThread = true;
    killVxmlSession = true;
    killVxmlPage = true;
    waker->wakeAll();
    pthread_join(vxmlthread, NULL);
    delete waker;
}

void *vxmlParser::vxmlThread(void *p)
{
    vxmlParser *me = (vxmlParser *)p;
    me->vxmlThreadWorker();
    return NULL;
}


void vxmlParser::vxmlThreadWorker()
{
    speechEngine = new tts();

    while (!killVxmlThread)
    {
        waker->wait();
        if (Rtp != 0)
        {
            cout << "Starting VXML Session; caller=" << callerName << endl;
            runVxmlSession();
            Rtp = 0;
        }    
    }
    
    Rtp = 0;
    delete speechEngine;
} 

void vxmlParser::beginVxmlSession(rtp *r, QString cName)
{
    if ((!killVxmlThread) && (Rtp == 0))
    {
        killVxmlPage = false;
        killVxmlSession = false;
        callerName = cName;
        if (callerName.length() == 0)
            callerName = "Unknown";
        Rtp = r;
        waker->wakeAll();
    }
    else
        cerr << "VXML: Cannot process session; thread dead or busy\n";
}

void vxmlParser::endVxmlSession()
{
    killVxmlPage = true;
    killVxmlSession = true;
    while (Rtp != 0)
        usleep(100000);
}

void vxmlParser::runVxmlSession()
{
    QString ttsVoice = "voice_" + gContext->GetSetting("TTSVoice");
    speechEngine->setVoice(ttsVoice);

    vxmlUrl = gContext->GetSetting("DefaultVxmlUrl");
    httpMethod = "get";
    postNamelist = "";
    lastUrl = vxmlUrl;

    if (vxmlUrl == "")
        vxmlUrl = "Default";

    while ((!killVxmlSession) && (vxmlUrl != ""))
    {
        loadVxmlPage(vxmlUrl, httpMethod, postNamelist, vxmlDoc);
        vxmlUrl = "";
        httpMethod = "";
        postNamelist = "";

        Parse(vxmlDoc);
        killVxmlPage = false;
    }
} 


bool vxmlParser::loadVxmlPage(QString strUrl, QString Method, QString Namelist, QDomDocument &script)
{
    QString Content = "";
    QString httpRequest;

    if (strUrl == "Default")
    {
        QString vmPrompt = gContext->GetSetting("DefaultVoicemailPrompt");
        Content = "<vxml version=\"1.0\">"
                  "<form><record name=\"message\" beep=\"true\" maxtime=\"20s\" dtmfterm=\"true\">";
        if (vmPrompt.endsWith(".wav"))
            Content += "  <prompt><audio src=\"" + vmPrompt + "\"/></prompt>";
        else
            Content += "  <prompt>" + vmPrompt + "</prompt>";
        Content += "  <filled><prompt>Thank you</prompt></filled>"
                   "  </record></form>"
                   "  <form><field name=\"choice\" type=\"digits?length=1\" modal=\"true\">"
                   "    <prompt>Press 1 to hear your message replayed</prompt>"
                   "    <prompt>Or press hash or hang up to leave the message</prompt>"
                   "  </field>"
                   "  <noinput>Goodbye</noinput>"
                   "  <filled>"
                   "    <if cond=\"choice == 1\"><prompt>You said <audio expr=\"message\"/></prompt><reprompt/>"
                   "    <else>Message delivered. Goodbye<disconnect></if>"
                   "  </filled></form></vxml>";
        script.setContent(Content);
        return true;
    }

    QUrl Url(lastUrl, strUrl, TRUE);
    lastUrl = Url;
    lastUrl.setQuery("");
    QString Query = Url.query();
    if (Query != "")
    {
        Query.prepend('?');

        // This needs some explaining!  For some reason, the QT parser breaks when it is
        // parsing a XML element that contains a "&", which makes it difficult to embed
        // URLs with queries. So as a bodge until I figure out whats going on, the URL
        // must actually contain a "+" instead, which is mapped to a "&" here; i.e. URLs must 
        // be of the form ...
        //          http://hostname/path?var1=1+var2=2             <--- + replaces & in the VXML source
        //
        Query.replace('+', '&'); 
    }
    if (Method == "get")
        httpRequest = QString("GET %1%2 HTTP/1.0\r\n"
                              "User-Agent: MythPhone/1.0\r\n"
                              "\r\n").arg(Url.path()).arg(Query);
    else
    {
        Namelist.replace('+', '&'); // As above
        httpRequest = QString("POST %1%2 HTTP/1.0\r\n"
                              "User-Agent: MythPhone/1.0\r\n"
                              "Content-Type: application/x-www-form-urlencoded\r\n"
                              "Content-Length: %3\r\n"
                              "\r\n%4").arg(Url.path()).arg(Query).arg(Namelist.length()).arg(Namelist);
    }
    QSocketDevice *httpSock = new QSocketDevice(QSocketDevice::Stream);
    QHostAddress hostIp;
    int port = Url.port();
    if (port == -1)
        port = 80;
    if (hostIp.setAddress(Url.host()))
        hostIp.setAddress("127.0.0.1");
    //cout << "Requesting page from host " << hostIp.toString() << ":" << port << " Method: " << Method << " path " << Url.path() << Query << endl;

    if (httpSock->connect(hostIp, port))
    {
        int bytesAvail;
        if (httpSock->writeBlock(httpRequest, httpRequest.length()) != -1) 
        {
            QString resp = "";
            while ((bytesAvail = httpSock->waitForMore(3000)) != -1)
            {
                char *httpResponse = new char[bytesAvail+1];
                int len = httpSock->readBlock(httpResponse, bytesAvail);
                if (len >= 0)
                {
                    httpResponse[len] = 0;
                    resp += QString(httpResponse);
                    //cout << "Got HTML response\n" << resp << endl;
                    QString firstLine = resp.section('\n', 0);
                    if ((firstLine.contains("200 OK")) && !resp.contains("</vxml>"))
                    {
                        delete httpResponse;
                        continue;
                    }

                    Content = resp.section("\r\n\r\n", 1, 1);
                    script.setContent(Content);
                    //cout << "Got VXML content\n" << Content << endl;
                }
                delete httpResponse;
                break;
            }
        }
        else
            cerr << "Error sending VXML GET to socket\n";
    }
    else
        cout << "Could not connect to VXML host " << Url.host() << ":" << Url.port() << endl;
    httpSock->close();
    delete httpSock;
   
    if (Content != "") 
        return true; 

    Content = "<vxml version=\"1.0\">"
              "  <prompt>There is a technical problem, please report this to the site owner</prompt>"
              " </vxml>";
    script.setContent(Content);
    return false; 

}

void vxmlParser::Parse(QDomDocument &vxmlPage)
{
    QDomElement rootElm = vxmlPage.documentElement();

    // Create a variable space
    vxmlVarList = new vxmlVarContainer;

    if (rootElm.tagName() != "vxml")
    {
        cerr << "Invalid VXML script\n";
        return;
    }

    QDomNode n = rootElm.firstChild();
    while ((!n.isNull()) && (!killVxmlPage))
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "form")
            {
                parseForm(e);
            }
            else if (e.tagName() == "prompt")
            {
                parsePrompt(e, false);
            }
            else if (e.tagName() == "submit")
            {
                vxmlUrl = e.attribute("next");
                postNamelist = e.attribute("namelist");
                httpMethod = e.attribute("method");
                killVxmlPage = true;
            }
            else
                cerr << "Unsupported VXML tag \"" << e.tagName() << "\"\n";
        }
        n = n.nextSibling();
    }


    // Check if there are any variables with recordings in them, and automatically save to file
    short *wav = 0;
    int samples;
    vxmlVariable *variable;
    for (variable=vxmlVarList->first(); variable; variable=vxmlVarList->next())
    {
        if (variable->isType("SHORTPTR"))
        {
            samples = variable->getSPLength();
            wav     = variable->getSPValue();
            SaveWav(wav, samples);
        }
    }


    // Delete the variable space - it doesnt carry between pages
    delete vxmlVarList;
}



void vxmlParser::parsePrompt(QDomElement &prompt, bool dtmfInterrupts)
{
    QDomNode n = prompt.firstChild();
    while ((!n.isNull()) && (!killVxmlPage))
    {
        QDomElement e = n.toElement();
        QDomText t = n.toText();
        if (!e.isNull())
        {
            if (e.tagName() == "break")
            {
                QString strDuration = e.attribute("time");
                if (strDuration) 
                {
                    PlaySilence(parseDurationType(strDuration), dtmfInterrupts);
                }
            }
            else if (e.tagName() == "audio")
            {
                QString srcFile = e.attribute("src");
                if (srcFile) 
                    PlayWav(srcFile);
                QString expression = e.attribute("expr");
                if (expression) 
                {
                    int samples;
                    short *wav = vxmlVarList->findShortPtrVariable(expression, samples);
                    PlayWav(wav, samples);
                }
            }
            else
                cerr << "Unsupported prompt sub-element tag \"" << e.tagName() << "\"\n";
        }
        else if (!t.isNull())
        {
            PlayTTSPrompt(t.data(), dtmfInterrupts);
        }
        else
            cerr << "Unsupported child type for \"prompt\" tag\n";
        n = n.nextSibling();
    }
}

int vxmlParser::parseDurationType(QString t)
{
    int breaktime = 0;
    if (t.contains("ms", false))
        breaktime = 1;
    else if (t.contains("s", false))
        breaktime = 1000;
    return (breaktime * atoi((const char *)t));
}

bool vxmlParser::parseField(QDomElement &field)
{
    QString Name = field.attribute("name");
    QString Type = field.attribute("type");
    QString Modal = field.attribute("modal");

    uint minDigits = 0;
    uint maxDigits = 0;
    parseFieldType(Type, maxDigits, minDigits);

    // Flush any waiting DTMF digits
    Rtp->getDtmf();

    QDomNode n = field.firstChild();
    while ((!n.isNull()) && (!killVxmlPage))
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "prompt")
                parsePrompt(e, Modal == "true");
        }
        n = n.nextSibling();
    }

    // If the prompt got to its natural end then we just quit now; but if it got
    // stopped by a digit then we carry on waiting for max-digits
    QString inDigits = Rtp->getDtmf();
    if ((inDigits.length() > 0) && (inDigits.length() < maxDigits))
    {
        QString newDigits;
        do
        {
            PlaySilence(4000, true); // Default to waiting 4 seconds for more digits
            newDigits = Rtp->getDtmf();
            inDigits += newDigits;
        } while ((inDigits.length() < maxDigits) && (newDigits.length() > 0));
    }

    // If we got the right number of digits create the variable
    if (inDigits.length() >= minDigits)
    {
        vxmlVariable *variable = new vxmlVariable(Name, inDigits);
        vxmlVarList->removeMatching(Name);
        vxmlVarList->append(variable);
        return true;
    }
    return false;
}

void vxmlParser::parseFieldType(QString Type, uint &Max, uint &Min)
{
    Max = Min = 0;
    if (Type.startsWith("digits?length="))
    {
        Type.remove(0,14);
        Max = Min = Type.toUInt();
    }
    else if (Type.startsWith("digits?"))
    {
        int startMin = Type.find("minlength");
        if (startMin >= 0)
        {
            startMin += 10;
            QString minString = Type.mid(startMin);
            Min = (uint)atoi(minString);
        }

        int startMax = Type.find("maxlength");
        if (startMax >= 0)
        {
            startMax += 10;
            QString maxString = Type.mid(startMax);
            Max = (uint)atoi(maxString);
        }
    }
}

void vxmlParser::parseRecord(QDomElement &record)
{
    bool reprompt;
    QString Name = record.attribute("name");
    QString Type = record.attribute("type");
    QString dtmfTerm = record.attribute("dtmfterm");
    QString maxTimeStr = record.attribute("maxtime");
    QString beep = record.attribute("beep");

    int maxTime = parseDurationType(maxTimeStr);
    if (maxTime == 0)
        return;

    QDomNode n = record.firstChild();
    while ((!n.isNull()) && (!killVxmlPage))
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "prompt")
                parsePrompt(e, false);
            else if (e.tagName() == "filled")
            {
                if (beep == "true")
                    PlayBeep();
                short *recBuffer = new short [maxTime*8];  // 8khz PCM
                int recSize = RecordAudio(recBuffer, maxTime*8, dtmfTerm == "true");

                // Store the recording in a variable for now
                vxmlVariable *variable = new vxmlVariable(Name, recBuffer, recSize);
                vxmlVarList->removeMatching(Name);
                vxmlVarList->append(variable);

                parseFilled(e, reprompt);
            }
        }
        n = n.nextSibling();
    }

}

void vxmlParser::parseNoInput(QDomElement &noInput, bool &reprompt)
{
    QDomNode n = noInput.firstChild();
    while ((!n.isNull()) && (!killVxmlPage))
    {
        QDomElement e = n.toElement();
        QDomText t = n.toText();
        if (!e.isNull())
        {
            if (e.tagName() == "submit")
            {
                vxmlUrl = e.attribute("next");
                postNamelist = e.attribute("namelist");
                httpMethod = e.attribute("method");
                killVxmlPage = true;
            }
            else if (e.tagName() == "disconnect")
                killVxmlPage = true;
            else if (e.tagName() == "clear")
                vxmlVarList->removeMatching(e.attribute("namelist"));
            else if (e.tagName() == "reprompt")
                reprompt = true;
            else
                cerr << "Unsupported prompt sub-element tag \"" << e.tagName() << "\"\n";
        }
        else if (!t.isNull())
        {
            PlayTTSPrompt(t.data(), false);
        }
        else
            cerr << "Unsupported child type for \"prompt\" tag\n";
        n = n.nextSibling();
    }
}

void vxmlParser::parseFilled(QDomElement &filled, bool &reprompt)
{
    QDomNode n = filled.firstChild();
    while ((!n.isNull()) && (!killVxmlPage))
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "prompt")
                parsePrompt(e, false);
            else if (e.tagName() == "if")
                parseIfExpression(e, reprompt);
            else
                cerr << "Unsupported prompt sub-element tag \"" << e.tagName() << "\"\n";
        }
        else
            cerr << "Unsupported child type for \"prompt\" tag\n";
        n = n.nextSibling();
    }
}

void vxmlParser::parseIfExpression(QDomElement &ifExp, bool &reprompt)
{
    QString Cond = ifExp.attribute("cond");
    QDomElement e = ifExp;

    while ((!e.isNull()) && (!killVxmlPage))
    {
        if (parseIfBlock(e, Cond, reprompt))
            break;
        else
        {
            QDomNode n = e.firstChild();

            // Find the next "elseif" of "else"
            while ((!n.isNull()) && (!killVxmlPage))
            {
                e = n.toElement();
                if (!e.isNull())
                {
                    if (e.tagName() == "elseif") 
                    {
                        Cond = e.attribute("cond");
                        break;
                    }
                    else if (e.tagName() == "else")
                    {
                        Cond = "";
                        break;
                    }
                }
                n = n.nextSibling();
            }
            if ((n.isNull()) || (killVxmlPage))
                break;
        }
    } 
}


bool vxmlParser::parseIfBlock(QDomElement &ifBlock, QString Cond, bool &reprompt)
{
    if (evaluateExpression(Cond))
    {
        QDomNode n = ifBlock.firstChild();
        while ((!n.isNull()) && (!killVxmlPage))
        {
            QDomElement e = n.toElement();
            QDomText t = n.toText();
            if (!e.isNull())
            {
                if (e.tagName() == "submit")
                {
                    vxmlUrl = e.attribute("next");
                    postNamelist = e.attribute("namelist");
                    httpMethod = e.attribute("method");
                    killVxmlPage = true;
                }
                else if (e.tagName() == "prompt")
                    parsePrompt(e, false);
                else if (e.tagName() == "disconnect")
                    killVxmlPage = true;
                else if (e.tagName() == "clear")
                    vxmlVarList->removeMatching(e.attribute("namelist"));
                else if (e.tagName() == "reprompt")
                    reprompt = true;
                else if ((e.tagName() == "elseif") || (e.tagName() == "else"))
                    break; // End of this block
                else
                    cerr << "Unsupported prompt sub-element tag \"" << e.tagName() << "\"\n";
            }
            else if (!t.isNull())
            {
                PlayTTSPrompt(t.data(), false);
            }
            n = n.nextSibling();
        }
        return true;
    }
    return false;
}


bool vxmlParser::evaluateExpression(QString Expression)
{
    // Absence of an expression must mean its valid
    if (Expression == "")
    {
        return true;
    }

    // This is a really simple expression evaluator
    // The only expression format supported is variable == "value" or variable != "value"
    int Equals = Expression.find("==");
    int NotEquals = Expression.find("!=");
    int Seperator;

    if (Equals > 0)
        Seperator = Equals;
    else if (NotEquals > 0)
        Seperator = NotEquals;
    else
    {
        cerr << "Invalid IF expression in VXML page\n";
        return false;
    }

    QString varName = Expression.left(Seperator).stripWhiteSpace();
    QString varValue = vxmlVarList->findStringVariable(varName);
    QString value = Expression.mid(Seperator+2, Expression.length()).stripWhiteSpace();

    if (((Equals >= 0) && (varValue == value)) ||
        ((NotEquals >= 0) && (varValue != value)))
    {
        return true;    
    }
    return false;
}


void vxmlParser::parseForm(QDomElement &formElm)
{
    bool reprompt;
    int loopCnt=0;
    do
    {
        reprompt = false;
        loopCnt++;
        QDomNode n = formElm.firstChild();
        bool filled = false;
        while ((!n.isNull()) && (!killVxmlPage))
        {
            QDomElement e = n.toElement();
            if (!e.isNull())
            {
                if (e.tagName() == "record")
                    parseRecord(e);
                else if (e.tagName() == "field")
                    filled = parseField(e);
                else if ((e.tagName() == "filled") && (filled))
                    parseFilled(e, reprompt);
                else if ((e.tagName() == "noinput") && (!filled) && ((e.attribute("count") == 0) || (atoi(e.attribute("count")) == loopCnt)))
                    parseNoInput(e, reprompt);
            }
            n = n.nextSibling();
        }
    } while (reprompt);
}


void vxmlParser::PlayWav(short *buffer, int Samples)
{
    Rtp->Transmit(buffer, Samples);
    waitUntilFinished(false);
}


void vxmlParser::PlayWav(QString wavFile)
{
    wavfile wav;
    wav.load(wavFile);

    Rtp->Transmit(wav.getData(), wav.samples());
    waitUntilFinished(false);
}


void vxmlParser::SaveWav(short *buffer, int Samples)
{
    char *homeDir = getenv("HOME");
    QString fileName = QString(homeDir) + "/.mythtv/MythPhone/Voicemail/" +  
                       QDateTime::currentDateTime().toString() + " " + callerName + ".wav";

    // Check if the file exists & delete it. It should NOT exist, the naming convention should
    // always produce unique filenames for small-scale voicemail systems such as this
    QFile f(fileName);
    if (f.exists())
        f.remove();

    wavfile vmail;
    vmail.load(buffer, Samples);
    vmail.saveToFile(fileName);
}




void vxmlParser::PlayBeep(int freqHz, int volume, int ms)
{
    int Samples = ms * 8; // PCM sampled at 8KHz
    short *beepBuffer = new short[Samples];

    for (int c=0; c<Samples; c++)
        beepBuffer[c] = (short)(sin(c * 2 * M_PI * freqHz / 8000) * volume);

    Rtp->Transmit(beepBuffer, Samples);
    waitUntilFinished(false);
    delete beepBuffer;
}



void vxmlParser::PlayTTSPrompt(QString prompt, bool dtmfInterrupts)
{
    wavfile Wave;
    speechEngine->toWavFile((const char *)prompt, Wave);
    // Debug
    //Wave.print();   
    //Wave.saveToFile("/home/mythtv/test1.wav");

    if (Wave.getData())
    {
        Rtp->Transmit(Wave.getData(), Wave.samples());
        waitUntilFinished(dtmfInterrupts);
    }
}

void vxmlParser::PlaySilence(int ms, bool dtmfInterrupts)
{
    if (ms)
    {
        Rtp->Transmit(ms);
        waitUntilFinished(dtmfInterrupts);
    }
}

int vxmlParser::RecordAudio(short *buffer, int Samples, bool dtmfInterrupts)
{
    if (Samples)
    {
        Rtp->Record(buffer, Samples);
        waitUntilFinished(dtmfInterrupts);
        return Rtp->GetRecordSamples();
    }
    return 0;
}

void vxmlParser::waitUntilFinished(bool dtmfInterrupts)
{
    // Waits until either the RTP stack has finished
    // doing whats its told, a DTMF key is received,
    // or the call terminates
    while ((!killVxmlPage) && (!Rtp->Finished()) && 
           (!dtmfInterrupts || (!Rtp->checkDtmf())))
        usleep(100000);
    if (!Rtp->Finished())
        Rtp->StopTransmitRecord();
}



/////////////////////////////////////////////////////////////////////////////////////
//                              VXML Variable Container
//                              
// This contains the variables that are dynamically created during a VXML webpage.
// Needs a lot of work, since it is only written with very basic VXML pages in mind.
//
/////////////////////////////////////////////////////////////////////////////////////

vxmlVarContainer::vxmlVarContainer():QPtrList<vxmlVariable>()
{
}

vxmlVarContainer::~vxmlVarContainer()
{
    vxmlVariable *p;
    while ((p = first()) != 0)
    {
        if (p->isType("SHORTPTR"))
            p->delSPValue();
        remove();
      	delete p;
    }
}

vxmlVariable *vxmlVarContainer::findFirstVariable(QString T)
{
    vxmlVariable *it;
    for (it=first(); it; it=next())
    {
        if (it->isType(T))
        {
            return it;
        }
    }
    return 0;
}

QString vxmlVarContainer::findStringVariable(QString N)
{
    vxmlVariable *it;
    for (it=first(); it; it=next())
    {
        if (it->isType("STRING"))
        {
            if (it->getName() == N)
                return it->getSValue();
        }
    }
    return "";
}

short *vxmlVarContainer::findShortPtrVariable(QString N, int &Samples)
{
    vxmlVariable *it;
    for (it=first(); it; it=next())
    {
        if (it->isType("SHORTPTR"))
        {
            if (it->getName() == N)
            {
                Samples = it->getSPLength();
                return it->getSPValue();
            }
        }
    }
    return 0;
}

void vxmlVarContainer::removeMatching(QString N)
{
    vxmlVariable *it;
    for (it=first(); it; it=next())
    {
        if (it->getName() == N)
        {
            if (it->isType("SHORTPTR"))
                it->delSPValue();
            remove();
            delete it;
        }
    }
}

vxmlVariable::vxmlVariable(QString N, QString V)
{
    Name = N;
    sValue = V;
    Type = "STRING";
    spValue = 0;
}

vxmlVariable::vxmlVariable(QString N, short *wav, int S)
{
    Name = N;
    spValue = wav;
    spLength = S;
    Type = "SHORTPTR";
}







/*
	sipstack.cpp

	(c) 2004 Paul Volkaerts

  Procedures and classes for building and parsing SIP messages.
	
*/


#include <qapplication.h>
#include <qdatetime.h>
#include <qhostaddress.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netdb.h>

using namespace std;

#include "config.h"
#include "sipstack.h"
#include "md5digest.h"

//////////////////////////////////////////////////////////////////////////////
//                                    SipMsg
//////////////////////////////////////////////////////////////////////////////


SipMsg::SipMsg(QString Method)
{
    thisMethod = Method;
    Msg = "";
    statusCode = 0;
    statusText = "";
    cseqValue = 0;
    cseqMethod = "";
    Expires = -1;
    msgContainsSDP = false;
    callId = 0;
    sdp = 0;
    contactUrl = 0;
    recRouteUrl = 0;
    fromUrl = 0;
    toUrl = 0;
    completeVia = "";
    completeRR = "";
    completeTo = "";
    completeFrom = "";
    viaIp = "";
    viaPort = 0;
}

SipMsg::SipMsg()
{
    thisMethod = "";
    Msg = "";
    statusCode = 0;
    statusText = "";
    cseqValue = 0;
    cseqMethod = "";
    Expires = -1;
    msgContainsSDP = false;
    callId = 0;
    sdp = 0;
    contactUrl = 0;
    recRouteUrl = 0;
    fromUrl = 0;
    toUrl = 0;
    completeVia = "";
    completeRR = "";
    completeTo = "";
    completeFrom = "";
    viaIp = "";
    viaPort = 0;
}

SipMsg::~SipMsg()
{
    if (callId)
        delete callId;
    if (sdp)
        delete sdp;
    if (contactUrl)
        delete contactUrl;
    if (recRouteUrl)
        delete recRouteUrl;
    if (fromUrl)
        delete fromUrl;
    if (toUrl)
        delete toUrl;
}

SipMsg &SipMsg::operator= (SipMsg &rhs)
{
    if (this == &rhs)
        return *this;

    Msg = rhs.Msg;
    thisMethod = rhs.thisMethod;
    statusCode = rhs.statusCode;
    statusText = rhs.statusText;
    if (callId != 0)
        callId = new SipCallId(*rhs.callId);
    cseqValue = rhs.cseqValue;
    cseqMethod = rhs.cseqMethod;
    msgContainsSDP = rhs.msgContainsSDP;

    //
    // TODO --- Should this do more???  What is this fn used for; delete if not used
    //

    // Note: SDP not copied
    sdp = 0;

    return *this;
}
 
void SipMsg::addRequestLine(SipUrl &Url)
{
    Msg = thisMethod + " sip:";
    if (Url.getUser().length() > 0)
        Msg += Url.getUser() + "@";
    Msg += Url.getHost();

    if (Url.getPort() != 5060)
        Msg += ":" + QString::number(Url.getPort()); 
    Msg += " SIP/2.0\r\n";
}

void SipMsg::addStatusLine(int Code)
{
    Msg =  "SIP/2.0 " + QString::number(Code) + " " + StatusPhrase(Code) + "\r\n";
}

void SipMsg::addVia(QString Hostname, int Port)
{
    Msg += "Via: SIP/2.0/UDP " + Hostname + ":" + QString::number(Port) + "\r\n";
}

void SipMsg::addGenericLine(QString Line)
{
    Msg += Line;
}

void SipMsg::addTo(SipUrl &to, QString tag)
{
    Msg += "To: " + to.string();
    if (tag.length() > 0)
        Msg += ";tag=" + tag;
    Msg += "\r\n";
}

void SipMsg::addFrom(SipUrl &from, QString tag)
{
    Msg += "From: " + from.string();
    if (tag.length() > 0)
        Msg += ";tag=" + tag;
    Msg += "\r\n";
}

void SipMsg::addCallId(SipCallId id)
{
    Msg += "Call-ID: " + id.string() + "\r\n";
}

void SipMsg::addCSeq(int c)
{
    Msg += QString("CSeq: ") + QString::number(c) + " " + thisMethod + "\r\n";
}

void SipMsg::addContact(SipUrl contact)
{
    Msg += "Contact: " + contact.string() + "\r\n";
}

void SipMsg::addUserAgent()
{
    Msg += "User-Agent: MythPhone\r\n";
}

void SipMsg::addAllow()
{
    Msg += "Allow: INVITE, ACK, CANCEL, BYE, INFO, NOTIFY\r\n";
}

void SipMsg::addAuthorization(QString authMethod, QString Username, QString Password, QString Realm, QString Nonce, QString Uri, bool Proxy)
{
    // Calculate the Digest key for the response
    HASHHEX HA1;
    HASHHEX HA2 = "";
    HASHHEX Response;
    DigestCalcHA1("md5", Username, Realm, Password, Nonce, "", HA1);
    DigestCalcResponse(HA1, Nonce, "", "", "", thisMethod, Uri, "", HA2, Response);

    if (Proxy)
        Msg += "Proxy-Authorization: " + authMethod;
    else
        Msg += "Authorization: " + authMethod;
    Msg += " username=\"" + Username + "\"";
    Msg += ",realm=\"" + Realm + "\"";
    Msg += ",uri=\"" + Uri + "\"";
    Msg += ",nonce=\"" + Nonce + "\"";
    Msg += QString(",response=\"") + Response + "\"";
    Msg += ",algorithm=md5\r\n";
}

void SipMsg::addProxyAuthorization(QString authMethod, QString Username, QString Password, QString Realm, QString Nonce, QString Uri)
{
    addAuthorization(authMethod, Username, Password, Realm, Nonce, Uri, true);
}

void SipMsg::addExpires(int e)
{
    Msg += "Expires: " + QString::number(e) + "\r\n";
}

void SipMsg::addNullContent()
{
    Msg += "Content-Length: 0\r\n\r\n";
}

void SipMsg::addSDP(SipSdp &sdp)
{
    Msg += QString("Content-Type: application/sdp\r\n"
           "Content-Length: ") + QString::number(sdp.length()) + "\r\n"
           "\r\n"
           + sdp.string();
}

void SipMsg::insertVia(QString Hostname, int Port)
{
    // Find the first Via statement so we can insert ourself
    QStringList::Iterator it;
    for (it=attList.begin(); (it != attList.end()) && (*it != ""); it++)
    {
        if ((*it).find("Via:", 0, false) == 0)
            break;
    }

    // Insert new Via
    QString Via = "Via: SIP/2.0/UDP " + Hostname + ":" + QString::number(Port);
    if ((*it).find("Via:", 0, false) == 0)
        attList.insert(it, Via);
    else
        attList.insert(attList.at(1), Via);

    // And recreate the completed msg
    Msg = attList.join("\r\n");
}

void SipMsg::removeVia()
{
    // Find the first Via statement 
    QStringList::Iterator it;
    for (it=attList.begin(); (it != attList.end()) && (*it != ""); it++)
    {
        if ((*it).find("Via:", 0, false) == 0)
            break;
    }

    // Remove the first Via. It may be on a line on its own (remove line) or may be part of a comma-separated list
    if ((*it).find("Via:", 0, false) == 0)
    {
        int commaPosn;
        if ((commaPosn = (*it).find(',')) != -1)
            (*it).remove(5, commaPosn-4);
        else
            attList.remove(it); // Should we check this is us first?
    }

    // And recreate the completed msg
    Msg = attList.join("\r\n");

    // Now need to re-decode the Via to get the top message
    viaIp = "";
    viaPort = 0;
    for (it=attList.begin(); (it != attList.end()) && (*it != ""); it++)
    {
        if ((*it).find("Via:", 0, false) == 0)
        {
            decodeVia(*it);
            break;
        }
    }
}

QString SipMsg::StatusPhrase(int Code)
{
    switch (Code)
    {
    case 100: return "Trying";
    case 180: return "Ringing";
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 481: return "Call Leg/Transaction Does Not Exist";
    case 486: return "Busy Here";
    case 488: return "Not Acceptable Here";
    }
    return "Dont know";
}


void SipMsg::decode(QString sipString)
{
    Msg = sipString; // Save in case we want to forward

    // Split the attribute lines into a string list for easier access
    attList = QStringList::split("\r\n", sipString, true);

    // Deccode main body of SIP message
    decodeRequestLine(attList[0]);
    QStringList::Iterator it;
    for (it=attList.begin(); (it != attList.end()) && (*it != ""); it++)
        decodeLine(*it);

    // Deccode main body of SIP message
    if (msgContainsSDP) 
    {
        if (sdp != 0)
            delete sdp;
        sdp = new SipSdp("", 0, 0);
        QPtrList<sdpCodec> *codecList = 0; // Tracks the media block we are parsing
        while (it != attList.end())
        {
            codecList = decodeSDPLine(*it, codecList);
            it++;
        }
    }
}

void SipMsg::decodeLine(QString line)
{
    if (line.find("Via:", 0, false) == 0)
        decodeVia(line);
    else if (line.find("To:", 0, false) == 0)
        decodeTo(line);
    else if (line.find("From:", 0, false) == 0)
        decodeFrom(line);
    else if (line.find("Contact:", 0, false) == 0)
        decodeContact(line);
    else if (line.find("Record-Route:", 0, false) == 0)
        decodeRecordRoute(line);
    else if (line.find("Call-ID:", 0, false) == 0)
        decodeCallid(line);
    else if (line.find("CSeq:", 0, false) == 0)
        decodeCseq(line);
    else if (line.find("Expires:", 0, false) == 0)
        decodeExpires(line);
    else if (line.find("Content-Type:", 0, false) == 0)
        decodeContentType(line);
    else if (line.find("WWW-Authenticate:", 0, false) == 0)
        decodeAuthenticate(line);
    else if (line.find("Proxy-Authenticate:", 0, false) == 0)
        decodeAuthenticate(line);
}

void SipMsg::decodeRequestLine(QString line)
{
    QString Token = line.section(' ', 0, 0);
    if ((Token == "INVITE") || (Token == "ACK") || (Token == "BYE") || (Token == "CANCEL") || (Token == "REGISTER"))
        thisMethod = Token;
    else if (Token == "SIP/2.0")
    {
        thisMethod = "STATUS";
        statusCode = (line.section(' ', 1, 1)).toInt();
        statusText = line.section(' ', 2, -1);
    }
    else    
        thisMethod = "UNKNOWN-" + Token;
}

void SipMsg::decodeVia(QString via)
{
    if ((via.find("Via: SIP/2.0/UDP", 0, false) == 0) && (viaIp.length() == 0))
    {
        QString str1 = via.mid(17);
        QString str2 = str1.section(';', 0, 0);
        QString str3 = str2.section(',', 0, 0); // We are only interested in the value of the first one, so ignore multiples per line
        viaIp = str3.section(':', 0, 0);
        QString viaPortStr = str3.section(':', 1, 1);
        viaPort = (viaPortStr.length() != 0) ? viaPortStr.toInt() : 5060;
    }
    completeVia += via + "\r\n";
}

void SipMsg::decodeAuthenticate(QString auth)
{
    authMethod = auth.section(' ', 1, 1);
    QString Params = auth.section(' ', 2);
    while (Params.length() > 0)
    {
        QString thisParam = Params.section(',', 0, 0);
        Params.remove(0, thisParam.length()+1);
        QString temp = Params.stripWhiteSpace();
        Params = temp;

        QString thisParamNoWs = thisParam.stripWhiteSpace();
        QString ParamName  = thisParamNoWs.section('=', 0, 0);
        QString ParamValue = thisParamNoWs.section('=', 1, 1);
        QString ParamValueNoQuotes = (ParamValue.startsWith("\"")) ? ParamValue.section('\"', 1, 1) : ParamValue;

        if (ParamName == "realm")
            authRealm = ParamValueNoQuotes;
        else if (ParamName == "nonce")
            authNonce = ParamValueNoQuotes;
        else if (ParamName == "qop")
        {
            if (ParamValueNoQuotes != "auth")
                cout << "SIP: QOP value not set to AUTH in Challenge\n";
        }
        else
            cout << "SIP: Unknown parameter in -Authenticate; " << ParamName << endl;
    }
}

void SipMsg::decodeFrom(QString from)
{
    if (fromUrl != 0)
        delete fromUrl;
    fromUrl = decodeUrl(from.mid(6)); // Remove "from: " first
    QString temp = from.section(";tag=", 1, 1);
    fromTag = temp.section(";", 0, 0);
    completeFrom = from + "\r\n";
}

void SipMsg::decodeTo(QString to)
{
    if (toUrl != 0)
        delete toUrl;
    toUrl = decodeUrl(to.mid(4)); // Remove "to: " first
    QString temp = to.section(";tag=", 1, 1);
    toTag = temp.section(";", 0, 0);
    completeTo = to + "\r\n";
}

void SipMsg::decodeContact(QString contact)
{
    if (contactUrl != 0)
        delete contactUrl;
    contactUrl = decodeUrl(contact.mid(9)); // Remove "Contact: " first
    QString temp = contact.section(";expires=", 1, 1);
    QString expiresStr = temp.section(";", 0, 0);
    if (expiresStr.length() > 0)
        Expires = expiresStr.toInt();
}

void SipMsg::decodeRecordRoute(QString rr)
{
    if (recRouteUrl != 0)
        delete recRouteUrl;
    recRouteUrl = decodeUrl(rr.mid(14)); // Remove "Record-Route: " first
    completeRR += rr + "\r\n";
}

SipUrl *SipMsg::decodeUrl(QString source)
{
    QString str1, str2, str3, str4, str5, str6, str7, str8, str9, str10;
    int Port = 0;

    // Expect one of 
    //      "abc"<sip:1234@1.1.1.1:5060>;tag=xxxxx
    //      "abc"<sip:1234@1.1.1.1:5060;lr=on>
    //      "abc"<sip:1234@1.1.1.1:5060>
    //      abc <sip:1234@1.1.1.1:5060>
    //      <sip:1234@1.1.1.1:5060>
    //      sip:1234@1.1.1.1:5060
    str3 = str7 = str9 = ""; // Initialise the important ones
    str1 = source.section(';', 0, 0); // Ignore any parameters after ';', if one is present
    if (str1.contains('<'))
    {
        str2 = str1.section('<', 0, 0);
        str3 = (str2.startsWith("\"")) ? str2.section('\"', 1, 1) : str2.stripWhiteSpace(); // str3 is the Display Name
        str4 = str1.section('<', 1, 1);
        str5 = str4.section('>', 0, 0); // sip:user@host:port
    }
    else
        str5 = str1;

    if (str5.startsWith("sip:"))
    {
        str6 = str5.mid(4);
        if (str6.contains('@'))
        {
            str7 = str6.section('@', 0, 0); // user 
            str8 = str6.section('@', 1, 1); // host:port
        }
        else
        {
            str7 = "";
            str8 = str6; // host:port
        }
        str9  = str8.section(':', 0, 0); // host
        str10 = str8.section(':', 1, 1); // port
        Port = (str10.length() > 0) ? str10.toInt() : 5060;
    }

    return new SipUrl(str3, str7, str9, Port);
}

void SipMsg::decodeCseq(QString cseq)
{
    cseqValue = (cseq.section(' ', 1, 1)).toInt();
    cseqMethod = cseq.section(' ', 2, 2);
}

void SipMsg::decodeExpires(QString Exp)
{
    Expires = (Exp.section(' ', 1, 1)).toInt();
}

void SipMsg::decodeCallid(QString callid)
{
    if (callId == 0)
        callId = new SipCallId;
    callId->setValue(callid.section(' ', 1, 1));
}

void SipMsg::decodeContentType(QString cType)
{
    if (cType.section(' ', 1, 1) == "application/sdp")
        msgContainsSDP = true;
}

QPtrList<sdpCodec> *SipMsg::decodeSDPLine(QString sdpLine, QPtrList<sdpCodec> *codecList)
{
    if (sdpLine.startsWith("c="))
        decodeSDPConnection(sdpLine);
    else if (sdpLine.startsWith("m="))
        codecList = decodeSDPMedia(sdpLine);
    else if (sdpLine.startsWith("a="))
        decodeSDPMediaAttribute(sdpLine, codecList);
    return codecList;
}

void SipMsg::decodeSDPConnection(QString c)
{
    if (sdp)
    {
        sdp->setMediaIp(c.section(' ', 2, 2));
    }
}

QPtrList<sdpCodec> *SipMsg::decodeSDPMedia(QString m)
{
    if (sdp)
    {
        int c=0;
        QString s;
        if (m.startsWith("m=audio"))
        {
            sdp->setAudioPort((m.section(' ', 1, 1)).toInt());
            while ((s = m.section(' ', c+3, c+3)) != 0)
            {
                sdp->addAudioCodec(s.toInt(), "");
                c++;
            }
            return (sdp->getAudioCodecList());
        }
        else if (m.startsWith("m=video"))
        {
            sdp->setVideoPort((m.section(' ', 1, 1)).toInt());
            while ((s = m.section(' ', c+3, c+3)) != 0)
            {
                sdp->addVideoCodec(s.toInt(), "", "");
                c++;
            }
            return (sdp->getVideoCodecList());
        }
    }
    return 0;
}

void SipMsg::decodeSDPMediaAttribute(QString a, QPtrList<sdpCodec> *codecList)
{
    if ((codecList != 0) && ((a.startsWith("a=rtpmap:")) || (a.startsWith("a=fmtp:"))))
    {
        QString attrib = a.section(':', 1, 1);
        int payload = (attrib.section(' ', 0, 0)).toInt();

        sdpCodec *c;
        for (c=codecList->first(); c; c=codecList->next())
        {
            if (c->intValue() == payload)
            {
                if (a.startsWith("a=rtpmap:"))
                    c->setName(attrib.section(' ', 1, 1));
                else
                    c->setFormat(attrib.section(' ', 1, 1));
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////////////
//                                    SipUrl
//////////////////////////////////////////////////////////////////////////////

SipUrl::SipUrl(QString url, QString DisplayName)
{
    thisDisplayName = DisplayName;
    QString temp = url;
    if (url.startsWith("sip:"))
        url = temp.mid(4);
    thisUser = url.section('@', 0, 0);
    temp = url.section('@', 1, 1);
    thisHostname = temp.section(':', 0, 0);
    QString PortStr = temp.section(':', 1, 1);
    thisPort = PortStr.length() > 0 ? PortStr.toInt() : 5060;
    HostnameToIpAddr();
    encode();
}

SipUrl::SipUrl(QString dispName, QString User, QString Hostname, int Port)
{
    thisDisplayName = dispName;
    thisUser = User;
    thisHostname = Hostname;
    thisPort = Port;
    HostnameToIpAddr();
    encode();
}

SipUrl::SipUrl(SipUrl *orig)
{
    thisDisplayName = orig->thisDisplayName;
    thisUser = orig->thisUser;
    thisHostname = orig->thisHostname;
    thisPort = orig->thisPort;
    thisUrl = orig->thisUrl;
    thisHostIp = orig->thisHostIp;
}

void SipUrl::HostnameToIpAddr()
{
    if (thisHostname.length() > 0)
    {
        QHostAddress ha;
        ha.setAddress(thisHostname); // See if it is already an IP address
        if (ha.toString() != thisHostname)
        {
            // Need a DNS lookup on the URL
            struct hostent *h;
            h = gethostbyname((const char *)thisHostname);
            if (h != 0)
            {
                ha.setAddress(ntohl(*(long *)h->h_addr));
                thisHostIp = ha.toString();
            }
            else
                thisHostIp = "";
        }
        else
            thisHostIp = thisHostname;
    }
    else
        thisHostIp = "";
}

void SipUrl::encode()
{
    QString PortStr = "";
    PortStr = QString(":") + QString::number(thisPort); // Note; some proxies demand the port to be present even if it is 5060
    if (thisDisplayName.length() > 0)                            
        thisUrl = "\"" + thisDisplayName + "\"" + "<sip:" + thisUser + "@" + thisHostname + PortStr + ">";
    else
        thisUrl = "<sip:" + thisUser + "@" + thisHostname + PortStr + ">";
}

SipUrl::~SipUrl()
{
}



//////////////////////////////////////////////////////////////////////////////
//                                    SipCallId
//////////////////////////////////////////////////////////////////////////////

SipCallId::SipCallId(QString ip)
{
    Generate(ip);
}

SipCallId::~SipCallId()
{
}

void SipCallId::Generate(QString ip)
{
    QString now = (QDateTime::currentDateTime()).toString("hhmmsszzz-ddMMyyyy");
    thisCallid = now + "@" + ip;
}

bool SipCallId::operator== (SipCallId &rhs) 
{
    bool match = (thisCallid.compare(rhs.string()) == 0);
    return match;
}

SipCallId &SipCallId::operator= (SipCallId &rhs)
{
    if (this == &rhs)
        return *this;

    thisCallid = rhs.thisCallid;

    return *this;
}
 



//////////////////////////////////////////////////////////////////////////////
//                                    SipSdp
//////////////////////////////////////////////////////////////////////////////

SipSdp::SipSdp(QString IP, int aPort, int vPort)
{
    audioPort = aPort;
    videoPort = vPort;
    MediaIp = IP;
    thisSdp = "";
}

SipSdp::~SipSdp()
{
    sdpCodec *c;
    while ((c=audioCodec.first()) != 0)
    {
        audioCodec.remove();
        delete c;
    }
    while ((c=videoCodec.first()) != 0)
    {
        videoCodec.remove();
        delete c;
    }
}

void SipSdp::addAudioCodec(int c, QString descr, QString fmt)
{
    audioCodec.append(new sdpCodec(c, descr, fmt));
}

void SipSdp::addVideoCodec(int c, QString descr, QString fmt)
{
    videoCodec.append(new sdpCodec(c, descr, fmt));
}

void SipSdp::encode()
{
    sdpCodec *c;

    thisSdp = "v=0\r\n"
              "o=Myth-UA 0 0 IN IP4 " + MediaIp + "\r\n"
              "s=SIP Call\r\n"
              "c=IN IP4 " + MediaIp + "\r\n"
              "t=0 0\r\n";

    if ((audioPort != 0) && (audioCodec.count()>0))
    {
        thisSdp += QString("m=audio ") + QString::number(audioPort) + " RTP/AVP";
        for (c=audioCodec.first(); c; c=audioCodec.next())
           thisSdp += " " + QString::number(c->intValue());
        thisSdp += "\r\n";
        for (c=audioCodec.first(); c; c=audioCodec.next())
           thisSdp += QString("a=rtpmap:") + QString::number(c->intValue()) + " " + c->strValue() + "\r\n";
        for (c=audioCodec.first(); c; c=audioCodec.next())
           if (c->fmtValue() != "")
               thisSdp += "a=fmtp:" + QString::number(c->intValue()) + " " + c->fmtValue() + "\r\n";
        thisSdp += "a=ptime:20\r\n";
    }

    if ((videoPort != 0) && (videoCodec.count()>0))
    {
        thisSdp += QString("m=video ") + QString::number(videoPort) + " RTP/AVP";
        for (c=videoCodec.first(); c; c=videoCodec.next())
           thisSdp += " " + QString::number(c->intValue());
        thisSdp += "\r\n";
        for (c=videoCodec.first(); c; c=videoCodec.next())
           thisSdp += QString("a=rtpmap:") + QString::number(c->intValue()) + " " + c->strValue() + "\r\n";
        for (c=videoCodec.first(); c; c=videoCodec.next())
           if (c->fmtValue() != "")
               thisSdp += "a=fmtp:" + QString::number(c->intValue()) + " " + c->fmtValue() + "\r\n";
    }

}





/*
	sipstack.h

	(c) 2004 Paul Volkaerts

  Procedures and classes for building and parsing SIP messages.
	
*/

#ifndef SIPSTACK_H_
#define SIPSTACK_H_

// Forward declarations
class SipSdp;
class SipXpidf;
class SipUrl;
class SipCallId;
class sdpCodec;


//////////////////////////////////////////////////////////////////////////////
//                                    SipMsg
//////////////////////////////////////////////////////////////////////////////


class SipMsg
{
public:
    SipMsg(QString Method);
    SipMsg();
    ~SipMsg();
    void addRequestLine(SipUrl &Url);
    void addStatusLine(int Code);
    void addVia(QString Hostname, int Port);
    void addTo(SipUrl &to, QString tag="", QString epid="");
    void addFrom(SipUrl &from, QString tag="", QString epid="");
    void addViaCopy(QString Via)    { addGenericLine(Via); }
    void addToCopy(QString To, QString Tag="");
    void addFromCopy(QString From)  { addGenericLine(From); }
    void addRRCopy(QString RR)      { addGenericLine(RR); }
    void addCallId(SipCallId id);
    void addCSeq(int c);
    void addContact(SipUrl contact, QString Methods="");
    void addUserAgent(QString ua="MythPhone");     
    void addAllow();
    void addEvent(QString Event);
    void addSubState(QString State, int Expires);
    void addAuthorization(QString authMethod, QString Username, QString Password, QString realm, QString nonce, QString uri, bool Proxy=false);
    void addProxyAuthorization(QString authMethod, QString Username, QString Password, QString realm, QString nonce, QString uri);
    void addExpires(int e);
    void addTimestamp(int t);
    void addNullContent();
    void addContent(QString contentType, QString contentData);
    void insertVia(QString Hostname, int Port);
    void removeVia();
    QString StatusPhrase(int Code);
    void decode(QString sipString);
    QString string() { return Msg; }
    QString getMethod() { return thisMethod; }
    int getCSeqValue() { return cseqValue; }
    QString getCSeqMethod() { return cseqMethod; }
    int getExpires() { return Expires; }
    int getTimestamp() { return Timestamp; }
    int getStatusCode() { return statusCode; }
    QString getReasonPhrase() { return statusText; }
    SipCallId *getCallId() { return callId; }
    SipMsg &operator= (SipMsg &rhs);
    SipSdp *getSdp()         { return sdp; }
    SipXpidf *getXpidf()     { return xpidf; }
    QString getPlainText()   { return PlainTextContent; }
    SipUrl *getContactUrl()  { return contactUrl; }
    SipUrl *getRecRouteUrl() { return recRouteUrl; }
    SipUrl *getFromUrl()     { return fromUrl; }
    SipUrl *getToUrl()       { return toUrl; }
    QString getFromTag()     { return fromTag; }
    QString getFromEpid()    { return fromEpid; }
    QString getToTag()       { return toTag; }
    QString getCompleteTo()  { return completeTo; }
    QString getCompleteFrom(){ return completeFrom; }
    QString getCompleteVia() { return completeVia; }
    QString getCompleteRR()  { return completeRR; }
    QString getViaIp()       { return viaIp; }
    int     getViaPort()     { return viaPort; }
    QString getAuthMethod()  { return authMethod; }
    QString getAuthRealm()   { return authRealm; }
    QString getAuthNonce()   { return authNonce; } 
    void    addGenericLine(QString Line);


private:
    void decodeLine(QString line);
    void decodeRequestLine(QString line);
    void decodeVia(QString via);
    void decodeFrom(QString from);
    void decodeTo(QString to);
    void decodeContact(QString contact);
    void decodeRecordRoute(QString rr);
    void decodeCseq(QString cseq);
    void decodeExpires(QString Exp);
    void decodeTimestamp(QString ts);
    void decodeCallid(QString callid);
    void decodeAuthenticate(QString auth);
    void decodeContentType(QString cType);
    void decodeSdp(QString content);
    void decodeXpidf(QString content);
    void decodePlainText(QString content);
    QPtrList<sdpCodec> *decodeSDPLine(QString sdpLine, QPtrList<sdpCodec> *codecList);
    void decodeSDPConnection(QString c);
    QPtrList<sdpCodec> *decodeSDPMedia(QString m);
    void decodeSDPMediaAttribute(QString a, QPtrList<sdpCodec> *codecList);
    SipUrl *decodeUrl(QString source);

    QString Msg;
    QStringList attList;
    QString thisMethod;
    int statusCode;
    QString statusText;
    SipCallId *callId;
    int cseqValue;
    QString cseqMethod;
    int Expires;
    int Timestamp;
    bool msgContainsSDP;
    bool msgContainsXPIDF;
    bool msgContainsPlainText;
    SipSdp *sdp;
    SipXpidf *xpidf;
    QString PlainTextContent;
    SipUrl *contactUrl;
    SipUrl *recRouteUrl;
    SipUrl *fromUrl;
    SipUrl *toUrl;
    QString fromTag;
    QString toTag;
    QString fromEpid;
    QString completeTo;
    QString completeFrom;
    QString viaIp;
    int viaPort;
    QString completeVia;
    QString completeRR;
    QString authMethod;
    QString authRealm;
    QString authNonce;
};



//////////////////////////////////////////////////////////////////////////////
//                                    SipUrl
//////////////////////////////////////////////////////////////////////////////

class SipUrl
{
public:
    SipUrl(QString url, QString DisplayName);
    SipUrl(QString dispName, QString User, QString Hostname, int Port);
    SipUrl(SipUrl *orig);
    ~SipUrl();
    bool operator==(SipUrl &rhs) { return (rhs.thisUser == thisUser); }
    const QString string() { return thisUrl; }
    QString getDisplay() { return thisDisplayName; }
    QString getUser() { return thisUser; }
    QString getHost() { return thisHostname; }
    QString getHostIp() { return thisHostIp; }
    QString formatReqLineUrl();
    QString formatContactUrl();
    int getPort() { return thisPort; }
    void setHostIp(QString ip) { thisHostIp = ip; }
    void setPort(int p) { thisPort = p; }

private:
    void encode();
    void HostnameToIpAddr();

    QString thisDisplayName;
    QString thisUser;
    QString thisHostname;
    QString thisHostIp;
    int thisPort;
    QString thisUrl;
};


//////////////////////////////////////////////////////////////////////////////
//                                    SipCallId
//////////////////////////////////////////////////////////////////////////////

class SipCallId
{
public:
    SipCallId(QString ip);
    SipCallId() { thisCallid = "";}
    SipCallId(SipCallId &id) { thisCallid = id.string();}
    ~SipCallId();
    void Generate(QString ip);
    void setValue(QString v) { thisCallid = v; }
    const QString string() { return thisCallid; }
    bool operator== (SipCallId &rhs);
    SipCallId &operator= (SipCallId &rhs);

private:
    QString thisCallid;
};

//////////////////////////////////////////////////////////////////////////////
//                                    SipSdp
//////////////////////////////////////////////////////////////////////////////

class sdpCodec
{
public:
    sdpCodec(int v, QString s, QString f="") { c=v; name=s; format=f; }
    ~sdpCodec() {};
    int intValue() {return c;}
    QString strValue() {return name;}
    QString fmtValue() {return format;}
    void setName(QString n) { name=n; }
    void setFormat(QString f) { format=f; }
private:
    int c;
    QString name;
    QString format;
};

class SipSdp
{
public:
    SipSdp(QString IP, int aPort, int vPort);
    ~SipSdp();
    void addAudioCodec(int c, QString descr, QString fmt="");
    void addVideoCodec(int c, QString descr, QString fmt="");
    void encode();
    const QString string() { return thisSdp; }
    int length()     { return thisSdp.length(); }
    QPtrList<sdpCodec> *getAudioCodecList() { return &audioCodec; }
    QPtrList<sdpCodec> *getVideoCodecList() { return &videoCodec; }
    QString getMediaIP() { return MediaIp; }
    void setMediaIp(QString ip) { MediaIp = ip; }
    void setAudioPort(int p) { audioPort=p; }
    void setVideoPort(int p) { videoPort=p; }
    int getAudioPort() { return audioPort; }
    int getVideoPort() { return videoPort; }

private:
    QString thisSdp;
    QPtrList<sdpCodec> audioCodec;
    QPtrList<sdpCodec> videoCodec;
    int audioPort, videoPort;
    QString MediaIp;
};


//////////////////////////////////////////////////////////////////////////////
//                                    SipXpidf
//////////////////////////////////////////////////////////////////////////////

class SipXpidf
{
public:
    SipXpidf(SipUrl &Url);
    SipXpidf();
    ~SipXpidf() {};
    void setUserHost(QString u, QString h) { user = u; host = h; };
    void setStatus(QString status, QString substatus="") { sipStatus = status; sipSubstatus = substatus; };
    void setSubStatus(QString substatus) { sipSubstatus = substatus; };
    QString encode();
    QString getUser()      { return user; };
    QString getHost()      { return host; };
    QString getStatus()    { return sipStatus; };
    QString getSubstatus() { return sipSubstatus; };

private:
    QString user;
    QString host;
    QString sipStatus;
    QString sipSubstatus;
};


#endif



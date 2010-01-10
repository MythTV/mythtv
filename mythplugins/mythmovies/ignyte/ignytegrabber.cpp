
#include "ignytegrabber.h"
 
IgnyteGrabber::IgnyteGrabber(QString zip, QString radius,
                             QApplication *callingApp) :
    waitForSoap(new QTimer(this)),
    ms(new MythSoap(this)),
    app(callingApp)
{
    //this feels clumsy to me - should we store it in a file instead?
    QString fields ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" "
            "xmlns:tns=\"http://www.ignyte.com/whatsshowing\" xmlns:xs=\"http://www.w3.org"
            "/2001/XMLSchema\">\n"
            "<soap:Body>\n"
            "<tns:GetTheatersAndMovies>\n"
            "<tns:zipCode>"  + zip + "</tns:zipCode>\n"
            "<tns:radius>" + radius + "</tns:radius>\n"
            "</tns:GetTheatersAndMovies>\n"
            "</soap:Body>\n"
            "</soap:Envelope>\n");
    QString server("www.ignyte.com");
    QString path("/webservices/ignyte.whatsshowing.webservice/moviefunctions.asmx");
    QString soapAction("http://www.ignyte.com/whatsshowing/GetTheatersAndMovies");
    ms->doSoapRequest(server, path, soapAction, fields);

    connect(waitForSoap, SIGNAL(timeout()), this, SLOT(checkHttp()));
    waitForSoap->setSingleShot(false);
    waitForSoap->start(0);
}

void IgnyteGrabber::checkHttp()
{
    if (ms->isDone())
    {
        if (ms->hasError())
        {
            cerr << "Data Source Error" << endl;
        }
        else
        {
            outputData(ms->getResponseData().data());
        }
        waitForSoap->stop();
        app->exit();
    }
}

void IgnyteGrabber::outputData(QString data)
{
    int i =  data.indexOf("<GetTheatersAndMoviesResult>");
    data = data.mid(i + 28);
    int x = data.indexOf("</GetTheatersAndMoviesResult>");
    data = data.left(x);
    data = data.remove('\r');
    data = data.remove('\n');
    data = "<?xml version=\"1.0\" encoding=\"utf-8\"?><MovieTimes>" + data + "</MovieTimes>";
    cout << data.toLocal8Bit().constData();
}

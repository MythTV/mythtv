#include "videosource.h"
#include "libmyth/mythwidgets.h"
#include "libmyth/mythcontext.h"

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <qlayout.h>
#include <qfile.h>
#include <iostream>

#ifdef USING_DVB
#include <linux/dvb/frontend.h>
#include "dvbdev.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "videodev_myth.h"

QString VSSetting::whereClause(void) {
    return QString("sourceid = %1").arg(parent.getSourceID());
}

QString VSSetting::setClause(void) {
    return QString("sourceid = %1, %2 = '%3'")
        .arg(parent.getSourceID())
        .arg(getColumn())
        .arg(getValue());
}

QString CCSetting::whereClause(void) {
    return QString("cardid = %1").arg(parent.getCardID());
}

QString CCSetting::setClause(void) {
    return QString("cardid = %1, %2 = '%3'")
        .arg(parent.getCardID())
        .arg(getColumn())
        .arg(getValue());
}

QString CISetting::whereClause(void) {
    return QString("cardinputid = %1").arg(parent.getInputID());
}

QString CISetting::setClause(void) {
    return QString("cardinputid = %1, %2 = '%3'")
        .arg(parent.getInputID())
        .arg(getColumn())
        .arg(getValue());
}

void RegionSelector::fillSelections() {
    clearSelections();

    QString command = QString("tv_grab_uk --configure --list-regions");
    FILE* fp = popen(command.ascii(), "r");

    if (fp == NULL) {
        perror(command.ascii());
        return;
    }

    QFile f;
    f.open(IO_ReadOnly, fp);
    for(QString line ; f.readLine(line, 1024) > 0 ; ) {
        addSelection(line.stripWhiteSpace());
    }

    f.close();
    fclose(fp);
}

void ProviderSelector::fillSelections(const QString& location) {
    QString waitMsg = QString("Fetching providers for %1... Please be patient.")
                             .arg(location);
    VERBOSE(VB_GENERAL, waitMsg);
    
    MythProgressDialog pdlg(waitMsg, 2);

    clearSelections();

    // First let the final character show up...
    qApp->processEvents();    
    
    // Now show our progress dialog.
    pdlg.show();

    QString command = QString("%1 --configure --postalcode %2 --list-providers")
        .arg(grabber)
        .arg(location);

    FILE* fp = popen(command.ascii(), "r");

    if (fp == NULL) {
        pdlg.Close();
        VERBOSE(VB_GENERAL, "Failed to get providers");

        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                            QObject::tr("Failed to get provider information"), 
                            QObject::tr("You probably need to update XMLTV."));
        qApp->processEvents();

        perror(command.ascii());
        return;
    }

    // Update our progress
    pdlg.setProgress(1);

    QFile f;
    f.open(IO_ReadOnly, fp);
    for(QString line ; f.readLine(line, 1024) > 0 ; ) {
        QStringList fields = QStringList::split(":", line.stripWhiteSpace());
        addSelection(fields.last(), fields.first());
    }

    pdlg.setProgress( 2 );
    pdlg.Close();

    f.close();
    fclose(fp);
}

void XMLTV_na_config::save(QSqlDatabase* db) {
    (void)db;

    QString waitMsg(QObject::tr("Please wait while MythTV retrieves the "
                                "list of channels for\nyour provider.  You "
                                "might want to check the output as it\n"
                                "runs by switching to the terminal from "
                                "which you started\nthis program."));
    MythProgressDialog pdlg( waitMsg, 2 );
    VERBOSE(VB_GENERAL, QString("Please wait while we MythTV retrieves the "
                                "list of channelsfor your provider")); 
    pdlg.show(); 

    QString filename = QString("%1/.mythtv/%2.xmltv")
        .arg(QDir::homeDirPath()).arg(parent.getSourceName());
    QString command = QString("tv_grab_na --config-file '%1' --configure --retry-limit %2 --retry-delay %3 --postalcode %4 --provider %5 --auto-new-channels add")
        .arg(filename)
        .arg(2)
        .arg(30)
        .arg(postalcode->getValue().replace(QRegExp(" "), ""))
        .arg(provider->getValue());

    pdlg.setProgress(1);

    int ret = system(command);

    if (ret != 0)
    {
        VERBOSE(VB_GENERAL, command);
        VERBOSE(VB_GENERAL, QString("exited with status %1").arg(ret));
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                  QObject::tr("Failed to get channel "
                                              "information."),
                                  QObject::tr("MythTV was unable to retrieve "
                                              "channel information for your "
                                              "provider.\nPlesae check the "
                                              "terminal window for more "
                                              "information"));
    }

    pdlg.setProgress( 2 );
    pdlg.Close();
}

void XMLTV_uk_config::save(QSqlDatabase* db) {
    (void)db;

    QString waitMsg(QObject::tr("Please wait while MythTV retrieves the "
                                "list of channels for\nyour provider.  You "
                                "might want to check the output as it\n"
                                "runs by switching to the terminal from "
                                "which you started\nthis program."));
    MythProgressDialog pdlg( waitMsg, 2 );
    VERBOSE(VB_GENERAL, QString("Please wait while we MythTV retrieves the "
                                "list of channelsfor your provider"));
    pdlg.show();

    QString filename = QString("%1/.mythtv/%2.xmltv")
        .arg(QDir::homeDirPath()).arg(parent.getSourceName());
    QString command = QString("tv_grab_uk --config-file '%1' --configure --retry-limit %2 --retry-delay %3 --postalcode %4 --provider %5 --auto-new-channels add")
        .arg(filename)
        .arg(2)
        .arg(30)
        .arg(region->getValue())
        .arg(provider->getValue());

    pdlg.setProgress(1);

    int ret = system(command);
    if (ret != 0)
    {
        VERBOSE(VB_GENERAL, command);
        VERBOSE(VB_GENERAL, QString("exited with status %1").arg(ret));
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  QObject::tr("Failed to get channel "
                                              "information."),
                                  QObject::tr("MythTV was unable to retrieve "
                                              "channel information for your "
                                              "provider.\nPlesae check the "
                                              "terminal window for more "
                                              "information"));
    }

    pdlg.setProgress( 2 );
    pdlg.Close();
}

void XMLTV_generic_config::save(QSqlDatabase* db) {
    (void)db;

    QString waitMsg(QObject::tr("Please wait while MythTV retrieves the "
                                "list of channels for\nyour provider.  You "
                                "might want to check the output as it\n"
                                "runs by switching to the terminal from "
                                "which you started\nthis program."));
    MythProgressDialog pdlg( waitMsg, 2 );
    VERBOSE(VB_GENERAL, QString("Please wait while we MythTV retrieves the "
                                "list of channelsfor your provider"));
    pdlg.show();

    QString command;
    if (grabber == "tv_grab_de") {
        command = "tv_grab_de --configure";
    } else {
        QString filename = QString("%1/.mythtv/%2.xmltv")
            .arg(QDir::homeDirPath()).arg(parent.getSourceName());

        command = QString("%1 --config-file '%2' --configure")
            .arg(grabber).arg(filename);
    }

    pdlg.setProgress(1);

    int ret = system(command);
    if (ret != 0)
    {
        VERBOSE(VB_GENERAL, command);
        VERBOSE(VB_GENERAL, QString("exited with status %1").arg(ret));

        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  QObject::tr("Failed to get channel "
                                              "information."),
                                  QObject::tr("MythTV was unable to retrieve "
                                              "channel information for your "
                                              "provider.\nPlesae check the "
                                              "terminal window for more "
                                              "information"));
    }

    if (grabber == "tv_grab_de" || grabber == "tv_grab_sn" || 
        grabber == "tv_grab_fi" || grabber == "tv_grab_es" ||
        grabber == "tv_grab_nl") 
    {
        cerr << "You _MUST_ run 'mythfilldatabase --manual the first time, "
             << "instead\n";
        cerr << "of just 'mythfilldatabase'.  Your grabber does not provide\n";
        cerr << "channel numbers, so you have to set them manually.\n";

        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                  QObject::tr("Warning."),
                                  QObject::tr("You MUST run 'mythfilldatabase "
                                              "--manual the first time,\n "
                                              "instead of just "
                                              "'mythfilldatabase'.\nYour "
                                              "grabber does not provide "
                                              "channel numbers, so you have to "
                                              "set them manually.") );
    }

    pdlg.setProgress( 2 );    
    pdlg.Close();
}

void VideoSource::fillSelections(QSqlDatabase* db,
                                 SelectSetting* setting) {
    QString query = QString("SELECT name, sourceid FROM videosource;");
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection(result.value(0).toString(),
                                  result.value(1).toString());
}

void VideoSource::loadByID(QSqlDatabase* db, int sourceid) {
    id->setValue(sourceid);
    load(db);
}

void CaptureCard::fillSelections(QSqlDatabase* db,
                                 SelectSetting* setting) {
    QString query = QString("SELECT cardtype, videodevice, cardid "
                            "FROM capturecard WHERE hostname = \"%1\";")
                            .arg(gContext->GetHostName());
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection("[ " + result.value(0).toString() + " : " +
                                  result.value(1).toString() + " ]",
                                  result.value(2).toString());
}

void CaptureCard::loadByID(QSqlDatabase* db, int cardid) {
    id->setValue(cardid);
    load(db);
}

CardType::CardType(const CaptureCard& parent)
        : CCSetting(parent, "cardtype") 
{
    setLabel("Card type");
    setHelpText("Change the cardtype to the appropriate type for the capture "
                "card you are configuring.");
    fillSelections(this);
}

void CardType::fillSelections(SelectSetting* setting)
{
    setting->addSelection("Standard V4L capture card", "V4L");
    setting->addSelection("MJPEG capture card (Matrox G200, DC10)", "MJPEG");
    setting->addSelection("MPEG-2 Encoder card (PVR-250, PVR-350)", "MPEG");
    setting->addSelection("pcHDTV ATSC capture card", "HDTV");
    setting->addSelection("Digital Video Broadcast card (DVB)", "DVB");
}

void CardInput::loadByID(QSqlDatabase* db, int inputid) {
    id->setValue(inputid);
    load(db);
}

void CardInput::loadByInput(QSqlDatabase* db, int _cardid, QString _inputname) {
    QString query = QString("SELECT cardinputid FROM cardinput WHERE cardid = %1 AND inputname = '%2';")
        .arg(_cardid)
        .arg(_inputname);
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0) {
        result.next();
        loadByID(db, result.value(0).toInt());
    } else {
        load(db); // new
        cardid->setValue(QString::number(_cardid));
        inputname->setValue(_inputname);
    }
}

void CardInput::save(QSqlDatabase* db) {
    if (sourceid->getValue() == "0")
        // "None" is represented by the lack of a row
        db->exec(QString("DELETE FROM cardinput WHERE cardinputid = %1;").arg(getInputID()));
    else
        ConfigurationWizard::save(db);
}

int CISetting::getInputID(void) const {
    return parent.getInputID();
}

int CCSetting::getCardID(void) const {
    return parent.getCardID();
}

int CaptureCardEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        edit();

    return QDialog::Rejected;
}

void CaptureCardEditor::load(QSqlDatabase* db) {
    clearSelections();
    addSelection("(New capture card)", "0");
    CaptureCard::fillSelections(db, this);
}

int VideoSourceEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        edit();

    return QDialog::Rejected;
}

void VideoSourceEditor::load(QSqlDatabase* db) {
    clearSelections();
    addSelection("(New video source)", "0");
    VideoSource::fillSelections(db, this);
}

int CardInputEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        cardinputs[getValue().toInt()]->exec(db);

    return QDialog::Rejected;
}



void CardInputEditor::load(QSqlDatabase* db) {
    clearSelections();

    // We do this manually because we want custom labels.  If
    // SelectSetting provided a facility to edit the labels, we
    // could use CaptureCard::fillSelections

    QString thequery = QString("SELECT cardid, videodevice, cardtype "
                               "FROM capturecard WHERE hostname = \"%1\";")
                              .arg(gContext->GetHostName());

    QSqlQuery capturecards = db->exec(thequery);
    if (capturecards.isActive() && capturecards.numRowsAffected() > 0)
        while (capturecards.next()) {
            int cardid = capturecards.value(0).toInt();
            QString videodevice(capturecards.value(1).toString());

            QStringList inputs;
            if (capturecards.value(2).toString() != "DVB")
                inputs = VideoDevice::probeInputs(videodevice);
            else
                inputs = QStringList("DVBInput");

            for(QStringList::iterator i = inputs.begin(); i != inputs.end(); ++i) {
                CardInput* cardinput = new CardInput();
                cardinput->loadByInput(db, cardid, *i);
                cardinputs.push_back(cardinput);
                QString index = QString::number(cardinputs.size()-1);

                QString label = QString("%1 (%2) -> %3")
                    .arg("[ " + capturecards.value(2).toString() + 
                         " : " + capturecards.value(1).toString() + 
                         " ]")
                    .arg(*i)
                    .arg(cardinput->getSourceName());
                addSelection(label, index);
            }
        }
}

CardInputEditor::~CardInputEditor() {
    while (cardinputs.size() > 0) {
        delete cardinputs.back();
        cardinputs.pop_back();
    }
}

QStringList VideoDevice::probeInputs(QString device) {
    QStringList ret;

    int videofd = open(device.ascii(), O_RDWR);
    if (videofd < 0) {
        cerr << "Couldn't open " << device << " to probe its inputs.\n";
        return ret;
    }

    bool usingv4l2 = false;

    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));
    if (ioctl(videofd, VIDIOC_QUERYCAP, &vcap) < 0)
         usingv4l2 = false;
    else {
        if (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
            usingv4l2 = true;
    }

    if (usingv4l2) {
        struct v4l2_input vin;
        memset(&vin, 0, sizeof(vin));
        vin.index = 0;

        while (ioctl(videofd, VIDIOC_ENUMINPUT, &vin) >= 0) {
            QString input((char *)vin.name);

            ret += input;

            vin.index++;
        }
    }
    else
    {
        struct video_capability vidcap;
        memset(&vidcap, 0, sizeof(vidcap));
        if (ioctl(videofd, VIDIOCGCAP, &vidcap) != 0) {
            perror("VIDIOCGCAP");
            vidcap.channels = 0;
        }

        for (int i = 0; i < vidcap.channels; i++) {
            struct video_channel test;
            memset(&test, 0, sizeof(test));
            test.channel = i;

            if (ioctl(videofd, VIDIOCGCHAN, &test) != 0) {
                perror("ioctl(VIDIOCGCHAN)");
                continue;
            }

            ret += test.name;
        }
    }

    if (ret.size() == 0)
        ret += "ERROR, No inputs found";

    close(videofd);
    return ret;
}

void DVBConfigurationGroup::probeCard(const QString& cardNumber)
{
#ifdef USING_DVB
    int fd = open(dvbdevice(DVB_DEV_FRONTEND, cardNumber.toInt()), O_RDWR);
    if (fd == -1)
    {
        cardname->setValue(QString("Could not open card #%1!")
                            .arg(cardNumber));
        cardtype->setValue(strerror(errno));
    }
    else
    {
        struct dvb_frontend_info info;

        if (ioctl(fd, FE_GET_INFO, &info) < 0)
        {
            cardname->setValue(QString("Could not get card info for card #%1!")
                                      .arg(cardNumber));
            cardtype->setValue(strerror(errno));
        }
        else
        {
            switch(info.type)
            {
                case FE_QPSK:   cardtype->setValue("DVB-S"); break;
                case FE_QAM:    cardtype->setValue("DVB-C"); break;
                case FE_OFDM:   cardtype->setValue("DVB-T"); break;
            }

            cardname->setValue(info.name);
        }
        close(fd);
    }
#else
    (void)cardNumber;
    cardtype->setValue(QString("Recompile with DVB-Support!"));
#endif
}

void TunerCardInput::fillSelections(const QString& device) {
    clearSelections();

    if (device == QString::null || device == "")
        return;

    QStringList inputs = VideoDevice::probeInputs(device);
    for(QStringList::iterator i = inputs.begin(); i != inputs.end(); ++i)
        addSelection(*i);
}

class DVBSignalMeter: public ProgressSetting, public TransientStorage {
public:
    DVBSignalMeter(int steps): ProgressSetting(steps), TransientStorage() {};
};

class DVBVideoSource: public ComboBoxSetting {
public:
    DVBVideoSource() {
        setLabel("VideoSource");
    };

    void save(QSqlDatabase* db) { (void)db; };
    void load(QSqlDatabase* _db) {
        db = _db;
        fillSelections();
    };

    void fillSelections() {
        clearSelections();
        QSqlQuery query;
        query = db->exec(QString("SELECT videosource.name,videosource.sourceid "
                                 "FROM videosource,capturecard,cardinput "
                                 "WHERE capturecard.cardtype='DVB' "
                                 "AND videosource.sourceid=cardinput.sourceid "
                                 "AND cardinput.cardid=capturecard.cardid"));
        if (!query.isActive())
            MythContext::DBError("VideoSource", query);

        if (query.numRowsAffected() > 0) {
            addSelection("[All VideoSources]", "ALL");
            while(query.next())
                addSelection(query.value(0).toString(),
                              query.value(1).toString());
        } else
            addSelection("[No VideoSources Defined]", "0");
    };

private:
    QSqlDatabase* db;
};

void DVBChannels::fillSelections(const QString& videoSource) 
{
    if (db == NULL)
        return;
    clearSelections();
    QSqlQuery query;
    QString thequery = "SELECT name,chanid FROM channel";
    if (videoSource != "All")
        thequery += QString(" WHERE sourceid='%1'").arg(videoSource);
    query = db->exec(thequery);
    if (!query.isActive()) {
        MythContext::DBError("Selecting Testchannels", query);
        return;
    }

    if (query.numRowsAffected() > 0) {
        while (query.next())
            addSelection(query.value(0).toString(),
                         query.value(1).toString());
    } else
        addSelection("[No Channels Defined]", "0");
}

class DVBLoadTuneButton: public ButtonSetting, public TransientStorage {
public:
    DVBLoadTuneButton() {
        setLabel("Load & Tune");
        setHelpText("Will load the selected channel above into the previous"
                    " screen, and try to tune it. If it fails to tune the"
                    " channel, press back and check the settings.");
    };
};

class DVBTuneOnlyButton: public ButtonSetting, public TransientStorage {
public:
    DVBTuneOnlyButton() {
        setLabel("Tune Only");
        setHelpText("Will ONLY try to tune the previous screen, not alter it."
                    " If it fails to tune, press back and check the settings.");
    };
};

#ifndef USING_DVB
DVBCardVerificationWizard::DVBCardVerificationWizard(int cardNum)
{
    (void)cardNum;
}

DVBCardVerificationWizard::~DVBCardVerificationWizard()
{
}

void DVBCardVerificationWizard::tuneConfigscreen()
{
}

void DVBCardVerificationWizard::tunePredefined()
{
}

#else

DVBCardVerificationWizard::DVBCardVerificationWizard(int cardNum) {
    chan = new DVBChannel(cardNum);

    cid.setValue(0);
    addChild(dvbopts = new DVBSignalChannelOptions(cid));

    VerticalConfigurationGroup* testGroup = new VerticalConfigurationGroup(false,true);
    testGroup->setLabel(QString("Card Verification Wizard (DVB#%1)").arg(cardNum));
    testGroup->setUseLabel(false);

    DVBVideoSource* source;

    sl = new DVBStatusLabel();
    DVBSignalMeter* sn = new DVBSignalMeter(65535);
    DVBSignalMeter* ss = new DVBSignalMeter(65535);
    testGroup->addChild(sl);
    testGroup->addChild(sn);
    testGroup->addChild(ss);

    HorizontalConfigurationGroup* lblGroup = new HorizontalConfigurationGroup(false,true);
#warning FIXME: BER & UB Do not work without reading TS?
    DVBInfoLabel* ber = new DVBInfoLabel("Bit Error Rate");
    DVBInfoLabel* un  = new DVBInfoLabel("Uncorrected Blocks");
    lblGroup->addChild(ber);
    lblGroup->addChild(un);
    testGroup->addChild(lblGroup);

    HorizontalConfigurationGroup* chanGroup = new HorizontalConfigurationGroup(false,true);
    chanGroup->addChild(source = new DVBVideoSource());
    chanGroup->addChild(channels = new DVBChannels());
    testGroup->addChild(chanGroup);

    HorizontalConfigurationGroup* btnGroup = new HorizontalConfigurationGroup(false,true);
    DVBLoadTuneButton* loadtune = new DVBLoadTuneButton();
    DVBTuneOnlyButton* tuneonly = new DVBTuneOnlyButton();
    btnGroup->addChild(loadtune);
    btnGroup->addChild(tuneonly);
    testGroup->addChild(btnGroup);

    sn->setLabel("Signal/Noise");
    ss->setLabel("Signal Strength");

    addChild(testGroup);

    connect(source, SIGNAL(valueChanged(const QString&)),
            channels, SLOT(fillSelections(const QString&)));

    connect(chan, SIGNAL(StatusSignalToNoise(int)), sn, SLOT(setValue(int)));
    connect(chan, SIGNAL(StatusSignalStrength(int)), ss, SLOT(setValue(int)));
    connect(chan, SIGNAL(StatusBitErrorRate(int)), ber, SLOT(set(int)));
    connect(chan, SIGNAL(StatusUncorrectedBlocks(int)), un, SLOT(set(int)));
#warning FIXME: Protect DVB driver access.
    connect(chan, SIGNAL(Status(QString)), sl, SLOT(set(QString)));

    connect(loadtune, SIGNAL(pressed()), this, SLOT(tunePredefined()));
    connect(tuneonly, SIGNAL(pressed()), this, SLOT(tuneConfigscreen()));
}

DVBCardVerificationWizard::~DVBCardVerificationWizard()
{
    delete chan;
}

void DVBCardVerificationWizard::tuneConfigscreen() {
    dvb_channel_t chan_opts;

    if (!chan->Open())
    {
        setLabel("FAILED TO OPEN CARD, CHECK CONSOLE!!");
        return;
    }

    bool parseOK = true;
    switch (chan->GetCardType())
    {
#warning FIXME: Protect DVB driver access.
        case FE_QPSK:
            parseOK = chan->ParseQPSK(
                dvbopts->getFrequency(), dvbopts->getInversion(),
                dvbopts->getSymbolrate(), dvbopts->getFec(),
                dvbopts->getPolarity(),
#warning FIXME: Static Diseqc options.
                QString("0"), QString("0"),
#warning FIXME: Static LNB options.
                QString("11700000"), QString("10600000"),
                QString("9750000"), chan_opts.tuning);
            break;
        case FE_QAM:
            parseOK = chan->ParseQAM(
                dvbopts->getFrequency(), dvbopts->getInversion(),
                dvbopts->getSymbolrate(), dvbopts->getFec(),
                dvbopts->getModulation(), chan_opts.tuning);
            break;
        case FE_OFDM:
            parseOK = chan->ParseOFDM(
                dvbopts->getFrequency(), dvbopts->getInversion(),
                dvbopts->getBandwidth(), dvbopts->getCodeRateHP(),
                dvbopts->getCodeRateLP(), dvbopts->getConstellation(),
                dvbopts->getTransmissionMode(), dvbopts->getGuardInterval(),
                dvbopts->getHierarchy(), chan_opts.tuning);
            break;
    }

    chan->Tune(chan_opts,true);
}

void DVBCardVerificationWizard::tunePredefined() {
    cid.setValue(channels->getValue().toInt());
    dvbopts->load(db);
    tuneConfigscreen();
}
#endif

DVBConfigurationGroup::DVBConfigurationGroup(CaptureCard& a_parent):
  VerticalConfigurationGroup(false,true), parent(a_parent) {
    setUseLabel(false);

    advcfg = new TransButtonSetting();
    advcfg->setLabel("Advanced Configuration");

    DVBCardNum* cardnum = new DVBCardNum(parent);
    cardname = new DVBCardName();
    cardtype = new DVBCardType();

    addChild(cardnum);
    addChild(cardname);
    addChild(cardtype);
    addChild(new DVBAudioDevice(parent));
    addChild(new DVBVbiDevice(parent));
    addChild(new DVBDefaultInput(parent));
    addChild(advcfg);

    connect(cardnum, SIGNAL(valueChanged(const QString&)),
            this, SLOT(probeCard(const QString&)));
    connect(cardnum, SIGNAL(valueChanged(const QString&)),
            &parent, SLOT(setDvbCard(const QString&)));
    connect(advcfg, SIGNAL(pressed()), &parent, SLOT(execDVBConfigMenu()));
    cardnum->setValue(0);
}

void CaptureCard::execDVBConfigMenu() {
    DVBAdvancedConfigMenu acm(*this);
    acm.exec(db);
}

void DVBAdvancedConfigMenu::execCVW() {
    DVBCardVerificationWizard cvw(parent.getDvbCard().toInt());
    cvw.exec(db);
}

void DVBAdvancedConfigMenu::execACW() {
    DVBAdvConfigurationWizard acw(parent);
    acw.exec(db);
}

void DVBAdvancedConfigMenu::execSAT() {
// FIXME: TODO: Satellite Editor.
    //DVBSatelliteWizard sw;
    //sw.exec(db);
}


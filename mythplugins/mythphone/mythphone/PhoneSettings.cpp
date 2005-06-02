/*
	PhoneSettings.cpp 

	(c) 2003 Paul Volkaerts
	
	Part of the mythTV project
	
	methods to set settings for the mythPhone module


*/


#include "mythtv/mythcontext.h"

#include "PhoneSettings.h"
#include <qfile.h>                                                       
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

#include "config.h"

/*
--------------- General WEBCAM Settings ---------------
*/

static HostComboBox *WebcamDevice()
{
    HostComboBox *gc = new HostComboBox("WebcamDevice", true);
    gc->setLabel(QObject::tr("Webcam device"));
    QDir dev("/dev", "video*", QDir::Name, QDir::System);
    gc->fillSelectionsFromDir(dev);
    gc->setHelpText(QObject::tr("Select the device path your webcam is "
		    "using. If you have a capture card it may be using "
		    "/dev/video so try dev/video1"));
    return gc;
};

static HostComboBox *TxResolution()
{
    HostComboBox *gc = new HostComboBox("TxResolution");
    gc->setLabel(QObject::tr("Transmit Resolution"));
    gc->addSelection("176x144");
    gc->addSelection("128x96");
    gc->addSelection("704x576");
    gc->addSelection("352x288");
    gc->setHelpText(QObject::tr("Size of video window to transmit; higher "
                    "resolutions require more bandwidth."));
    return gc;
};

static HostComboBox *CaptureResolution()
{
    HostComboBox *gc = new HostComboBox("CaptureResolution");
    gc->setLabel(QObject::tr("Capture Resolution"));
    gc->addSelection("352x288");
    gc->addSelection("320x240");
    gc->addSelection("176x144");
    gc->addSelection("160x120");
    gc->addSelection("128x96");
    gc->addSelection("704x576");
    gc->addSelection("640x480");
    gc->setHelpText(QObject::tr("Size of video source from your webcam. Choose "
                    "a value compatible with your webcam hardware. Choose "
                    "higher values to digitally pan/zoom before "
                    "transmission."));
    return gc;
};

static HostSpinBox *TransmitFPS()
{
    HostSpinBox *gc = new HostSpinBox("TransmitFPS", 1, 30, 1);
    gc->setLabel(QObject::tr("Transmit Frames/Second"));
    gc->setValue(5);
    gc->setHelpText(QObject::tr("Number of webcam frames/sec to transmit, from "
                    "1 to 30. Higher numbers create better results but use "
                    "more bandwidth."));
    return gc;
};

static HostLineEdit *TransmitBandwidth()
{
    HostLineEdit *gc = new HostLineEdit("TransmitBandwidth");
    gc->setLabel(QObject::tr("Max Transmit Bandwidth Kbps"));
    gc->setValue("256");
    gc->setHelpText(QObject::tr("Enter the upspeed bandwidth of your local WAN in Kbps. "));
    return gc;
};

/*
--------------- General VXML Settings ---------------
*/

static HostSpinBox *TimeToAnswer()
{
    HostSpinBox *gc = new HostSpinBox("TimeToAnswer", 1, 30, 1);
    gc->setLabel(QObject::tr("Time to Answer"));
    gc->setValue(10);
    gc->setHelpText(QObject::tr("The time in seconds a call rings before being "
                    "automatically answered and diverted to a VXML script."));
    return gc;
};

static HostLineEdit *DefaultVxmlUrl()
{
    HostLineEdit *gc = new HostLineEdit("DefaultVxmlUrl");
    gc->setLabel(QObject::tr("Default VXML URL"));
    gc->setValue("http://127.0.0.1/vxml/index.vxml");
    gc->setHelpText(QObject::tr("The URL to retrieve a VXML script which can "
                    "be used to prompt for leaving a voicemail etc. Leave "
                    "blank if you have no HTTP server and a default Voicemail "
                    "script will be used."));
    return gc;
};

static HostLineEdit *DefaultVoicemailPrompt()
{
    HostLineEdit *gc = new HostLineEdit("DefaultVoicemailPrompt");
    gc->setLabel(QObject::tr("Default Voicemail Prompt"));
    gc->setValue(QObject::tr("I am not at home, please leave a message after "
                 "the tone"));
    gc->setHelpText(QObject::tr("Either a text message to be read by the TTS "
                    "engine or the filename of a .wav file to be played to "
                    "callers. Only used where the above setting is blank."));
    return gc;
};

static HostComboBox *TTSVoice()
{
    HostComboBox *gc = new HostComboBox("TTSVoice", true);
    gc->setLabel(QObject::tr("Text to Speech Voice"));
#ifdef FESTIVAL_SUPPORT
    QDir festDir(FESTIVAL_HOME "lib/voices/english/",
                 "[a-z]*;[A-Z]*", QDir::Name, QDir::Dirs);
                 // The name filter is to remove "." and ".." directories
    gc->fillSelectionsFromDir(festDir, false);
#endif
    gc->setHelpText(QObject::tr("Choose a voice to use from the Text To Speech "
                    "library. "));
    return gc;
};
/*
--------------- General SIP Settings ---------------
*/

static HostCheckBox *SipRegisterWithProxy()
{
    HostCheckBox *gc = new HostCheckBox("SipRegisterWithProxy");
    gc->setLabel(QObject::tr("Login to a SIP Server"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Allows you to register with services such as "
                    "Free World Dialup; or with applications like Asterisk. "
                    "Restart mythfrontend if you change this."));
    return gc;
};

static HostLineEdit *SipProxyName()
{
    HostLineEdit *gc = new HostLineEdit("SipProxyName");
    gc->setLabel(QObject::tr("SIP Server DNS Name"));
    gc->setValue("fwd.pulver.com");
    gc->setHelpText(QObject::tr("Name of the Proxy, such as fwd.pulver.com for "
                    "Free World Dialup."));
    return gc;
};

static HostLineEdit *SipProxyAuthName()
{
    HostLineEdit *gc = new HostLineEdit("SipProxyAuthName");
    gc->setLabel(QObject::tr("Sign-in Name"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("Your username for authentication with the "
                    "SIP Server. For FWD this is your FWD number."));
    return gc;
};

static HostLineEdit *SipProxyAuthPassword()
{
    HostLineEdit *gc = new HostLineEdit("SipProxyAuthPassword");
    gc->setLabel(QObject::tr("Password"));
    gc->setValue("");
    gc->setHelpText(QObject::tr("Your password for authentication with the "
                    "SIP Server."));
    return gc;
};

static HostLineEdit *SipLocalPort()
{
    HostLineEdit *gc = new HostLineEdit("SipLocalPort");
    gc->setLabel(QObject::tr("SIP Local Port"));
    gc->setValue("5060");
    gc->setHelpText(QObject::tr("The port on this machine to use. You may need "
                    "to make these different for each Mythfrontend and setup "
                    "your firewall to let this port through."));
    return gc;
};

static HostComboBox *MicrophoneDevice()
{
    HostComboBox *gc = new HostComboBox("MicrophoneDevice", true);
    gc->setLabel(QObject::tr("Microphone device"));
    QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
    gc->addSelection("None");
    gc->fillSelectionsFromDir(dev);
    gc->setHelpText(QObject::tr("Select the device path for your microphone. "
                                "Currently this CANNOT be the same device as used for "
                                "audio output."));
    return gc;
};

static HostSpinBox *PlayoutAudioCall()
{
    HostSpinBox *gc = new HostSpinBox("PlayoutAudioCall", 10, 300, 5);
    gc->setLabel(QObject::tr("Jitter Buffer (Audio Call)"));
    gc->setValue(40);
    gc->setHelpText(QObject::tr("Size of the playout buffer in ms for an audio call. "));
    return gc;
};

static HostSpinBox *PlayoutVideoCall()
{
    HostSpinBox *gc = new HostSpinBox("PlayoutVideoCall", 10, 300, 5);
    gc->setLabel(QObject::tr("Jitter Buffer (Video Call)"));
    gc->setValue(110);
    gc->setHelpText(QObject::tr("Size of the playout buffer in ms for a video call. "
                                "This should be bigger than for an audio call because "
                                "CPU usage causes jitter and to sync video & audio."));
    return gc;
};

static HostLineEdit *MySipName()
{
    HostLineEdit *gc = new HostLineEdit("MySipName");
    gc->setLabel(QObject::tr("My Display Name"));
    gc->setValue(QObject::tr("Me"));
    gc->setHelpText(QObject::tr("My common name to display when I call other "
                    "people. "));
    return gc;
};

/*static HostLineEdit *MySipUser() {
    HostLineEdit *gc = new HostLineEdit("MySipUser");
    gc->setLabel(QObject::tr("My SIP User"));
    gc->setValue("1000");
    gc->setHelpText(QObject::tr("The phone number or username that identifies "
                    "this SIP client. This will be combined with the host "
                    "address to form the SIP URI user@ip-address."));
    return gc;
};*/

static HostLineEdit *CodecPriorityList()
{
    HostLineEdit *gc = new HostLineEdit("CodecPriorityList");
    gc->setLabel(QObject::tr("Codecs Supported"));
    gc->setValue("GSM;G.711u;G.711a");
    gc->setHelpText(QObject::tr("The list of codecs to use, in the preferred "
                    "order separated by semicolon. Supported codecs are "
                    "G.711u, G.711a and GSM."));
    return gc;
};

static HostLineEdit *SipBindInterface()
{
    HostLineEdit *gc = new HostLineEdit("SipBindInterface");
    gc->setLabel(QObject::tr("SIP Network Interface"));
    gc->setValue("eth0");
    gc->setHelpText(QObject::tr("Enter the name of the network to bind to e.g. "
                    "eth0"));
    return gc;
};

static HostComboBox *NatTraversalMethod()
{
    HostComboBox *gc = new HostComboBox("NatTraversalMethod");
    gc->setLabel(QObject::tr("NAT Traversal Method"));
    gc->addSelection("None");
    gc->addSelection("Manual");
    gc->addSelection("Web Server");
    gc->setHelpText(QObject::tr("Method to use for NAT traversal; needs a "
                    "Frontend restart after changing. Choose NONE if you have "
                    "a public IP address, choose MANUAL if your ISP always "
                    "gives you the same public address and manually enter this "
                    "address below. Choose Web Server if you have a dynamic "
                    "NAT address and enter a web address like "
                    "http://checkip.dyndns.org below. "));
    return gc;
};

static HostLineEdit *NatIpAddress()
{
    HostLineEdit *gc = new HostLineEdit("NatIpAddress");
    gc->setLabel(QObject::tr("NAT IP Address"));
    gc->setValue(QObject::tr("http://checkip.dyndns.org"));
    gc->setHelpText(QObject::tr("If you selected MANUAL above, then enter your "
                    "Public IP Address here. If you selected WEB Server above "
                    "then enter your web server URL here."));
    return gc;
};

static HostLineEdit *AudioLocalPort()
{
    HostLineEdit *gc = new HostLineEdit("AudioLocalPort");
    gc->setLabel(QObject::tr("Audio RTP Port"));
    gc->setValue("21232");
    gc->setHelpText(QObject::tr("Enter the port to use for Audio RTP; an even "
                    "number between 1024 and 32767. If you have a firewall you "
                    "should enable UDP through the firewall on this port."));
    return gc;
};

static HostLineEdit *VideoLocalPort()
{
    HostLineEdit *gc = new HostLineEdit("VideoLocalPort");
    gc->setLabel(QObject::tr("Video RTP Port"));
    gc->setValue("21234");
    gc->setHelpText(QObject::tr("Enter the port to use for Video RTP; an even "
                    "number between 1024 and 32767. If you have a firewall you "
                    "should enable UDP through the firewall on this port."));
    return gc;
};

static HostCheckBox *SipAutoanswer()
{
    HostCheckBox *gc = new HostCheckBox("SipAutoanswer");
    gc->setLabel(QObject::tr("Auto-Answer"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("When the MythFrontend is in the MythPhone "
                    "plugin, setting this automatically answers all incoming "
                    "calls. There is no security yet."));
    return gc;
};







MythPhoneSettings::MythPhoneSettings()
{

    VerticalConfigurationGroup* sipProxySet = new VerticalConfigurationGroup(false);
    sipProxySet->setLabel(QObject::tr("SIP Proxy Settings"));
    sipProxySet->addChild(SipRegisterWithProxy());
    sipProxySet->addChild(SipProxyName());
    sipProxySet->addChild(SipProxyAuthName());
    sipProxySet->addChild(SipProxyAuthPassword());
    sipProxySet->addChild(MySipName());
    sipProxySet->addChild(SipAutoanswer());
    addChild(sipProxySet);

    VerticalConfigurationGroup* sipSet = new VerticalConfigurationGroup(false);
    sipSet->setLabel(QObject::tr("IP Settings"));
    sipSet->addChild(SipBindInterface());
    sipSet->addChild(SipLocalPort());
    sipSet->addChild(NatTraversalMethod());
    sipSet->addChild(NatIpAddress());
    sipSet->addChild(AudioLocalPort());
    sipSet->addChild(VideoLocalPort());
    addChild(sipSet);

    VerticalConfigurationGroup* audioSet = new VerticalConfigurationGroup(false);
    audioSet->setLabel(QObject::tr("AUDIO Settings"));
    audioSet->addChild(MicrophoneDevice());
    audioSet->addChild(CodecPriorityList());
    audioSet->addChild(PlayoutAudioCall());
    audioSet->addChild(PlayoutVideoCall());
    addChild(audioSet);

    VerticalConfigurationGroup* webcamSet = new VerticalConfigurationGroup(false);
    webcamSet->setLabel(QObject::tr("WEBCAM Settings"));
    webcamSet->addChild(WebcamDevice());
    webcamSet->addChild(TxResolution());
    webcamSet->addChild(TransmitFPS());
    webcamSet->addChild(TransmitBandwidth());
    webcamSet->addChild(CaptureResolution());
    addChild(webcamSet);
    
    VerticalConfigurationGroup* vxmlSet = new VerticalConfigurationGroup(false);
    vxmlSet->setLabel(QObject::tr("VXML Settings"));
    vxmlSet->addChild(TimeToAnswer());
    vxmlSet->addChild(DefaultVxmlUrl());
    vxmlSet->addChild(DefaultVoicemailPrompt());
    vxmlSet->addChild(TTSVoice());
    addChild(vxmlSet);
}




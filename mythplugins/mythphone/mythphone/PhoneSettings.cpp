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

class WebcamDevice: public GlobalComboBox {
public:
    WebcamDevice():
        GlobalComboBox("WebcamDevice", true) {
        setLabel(QObject::tr("Webcam device"));
        QDir dev("/dev", "video*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        setHelpText(QObject::tr("Select the device path your webcam is "
		    "using. If you have a capture card it may be using "
		    "/dev/video so try dev/video1"));
    }
};

class TxResolution: public GlobalComboBox {
public:
    TxResolution():
        GlobalComboBox("TxResolution") {
        setLabel(QObject::tr("Transmit Resolution"));
        addSelection(QObject::tr("176x144"), "176x144");
        addSelection(QObject::tr("128x96"), "128x96");
        addSelection(QObject::tr("704x576"), "704x576");
        addSelection(QObject::tr("352x288"), "352x288");
        setHelpText(QObject::tr("Size of video window to transmit; higher "
                    "resolutions require more bandwidth."));
    };
};

class CaptureResolution: public GlobalComboBox {
public:
    CaptureResolution():
        GlobalComboBox("CaptureResolution") {
        setLabel(QObject::tr("Capture Resolution"));
        addSelection(QObject::tr("352x288"), "352x288");
        addSelection(QObject::tr("320x240"), "320x240");
        addSelection(QObject::tr("176x144"), "176x144");
        addSelection(QObject::tr("160x120"), "160x120");
        addSelection(QObject::tr("128x96"), "128x96");
        addSelection(QObject::tr("704x576"), "704x576");
        addSelection(QObject::tr("640x480"), "640x480");
        setHelpText(QObject::tr("Size of video source from your webcam. Choose a value compatible with your "
                    "webcam hardware. Choose higher values to digitally pan/zoom before transmission."));
    };
};

class TransmitFPS: public GlobalSpinBox {
public:
    TransmitFPS():
        GlobalSpinBox("TransmitFPS", 1, 30, 1) {
        setLabel(QObject::tr("Transmit Frames/Second"));
        setValue(5);
        setHelpText(QObject::tr("Number of webcam frames/sec to transmit, from 1 to "
                                "30. Higher numbers create better results but use more bandwidth."));
    }
};

/*
--------------- General VXML Settings ---------------
*/

class TimeToAnswer: public GlobalLineEdit {
public:
    TimeToAnswer():
        GlobalLineEdit("TimeToAnswer") {
        setLabel(QObject::tr("Time to Answer"));
        setValue(QObject::tr("10"));
        setHelpText(QObject::tr("The time in seconds a call rings before being "
                                "automatically answered and diverted to a VXML script."));
    }
};

class DefaultVxmlUrl: public GlobalLineEdit {
public:
    DefaultVxmlUrl():
        GlobalLineEdit("DefaultVxmlUrl") {
        setLabel(QObject::tr("Default VXML URL"));
        setValue(QObject::tr("http://127.0.0.1/vxml/index.vxml"));
        setHelpText(QObject::tr("The URL to retrieve a VXML script which can "
                                "be used to prompt for leaving a voicemail etc. Leave blank if you "
                                "have no HTTP server and a default Voicemail script will be used."));
    }
};

class DefaultVoicemailPrompt: public GlobalLineEdit {
public:
    DefaultVoicemailPrompt():
        GlobalLineEdit("DefaultVoicemailPrompt") {
        setLabel(QObject::tr("Default Voicemail Prompt"));
        setValue(QObject::tr("I am not at home, please leave a message after the tone"));
        setHelpText(QObject::tr("Either a text message to be read by the TTS engine or the filename of "
                                "a .wav file to be played to callers. Only used where the above setting is blank."));
    }
};

class TTSVoice: public GlobalComboBox {
public:
    TTSVoice():
        GlobalComboBox("TTSVoice", true) {
        setLabel(QObject::tr("Text to Speech Voice"));
#ifdef FESTIVAL_SUPPORT
        QDir festDir(FESTIVAL_HOME "lib/voices/english/", "[a-z]*;[A-Z]*", QDir::Name, QDir::Dirs);  // The name filter is to remove "." and ".." directories
        fillSelectionsFromDir(festDir, false);
#endif
        setHelpText(QObject::tr("Choose a voice to use from the Text To Speech library. "));
    }
};
/*
--------------- General SIP Settings ---------------
*/

class SipRegisterWithProxy: public GlobalCheckBox {
public:
    SipRegisterWithProxy():
        GlobalCheckBox("SipRegisterWithProxy") {
        setLabel(QObject::tr("Register with a Proxy or Service"));
        setValue(true);
        setHelpText(QObject::tr("Allows you to register with services such as Free World Dialup; "
                    "or with applications like Asterisk. Restart mythfrontend if you change this."));
    };
};

class SipProxyName: public GlobalLineEdit {
public:
    SipProxyName():
        GlobalLineEdit("SipProxyName") {
        setLabel(QObject::tr("Proxy DNS Name"));
        setValue("fwd.pulver.com");
        setHelpText(QObject::tr("Name of the Proxy, such as fwd.pulver.com for Free World Dialup."));
    }
};

class SipProxyAuthName: public GlobalLineEdit {
public:
    SipProxyAuthName():
        GlobalLineEdit("SipProxyAuthName") {
        setLabel(QObject::tr("Authentication Name"));
        setValue("");
        setHelpText(QObject::tr("Your username for authentication with the proxy. For FWD this is your FWD number."));
    }
};

class SipProxyAuthPassword: public GlobalLineEdit {
public:
    SipProxyAuthPassword():
        GlobalLineEdit("SipProxyAuthPassword") {
        setLabel(QObject::tr("Authentication Password"));
        setValue("");
        setHelpText(QObject::tr("Your password for authentication with the proxy."));
    }
};

class SipLocalPort: public GlobalLineEdit {
public:
    SipLocalPort():
        GlobalLineEdit("SipLocalPort") {
        setLabel(QObject::tr("SIP Local Port"));
        setValue(QObject::tr("5060"));
        setHelpText(QObject::tr("The port on this machine to use. You may need to make these different "
                                "for each Mythfrontend and setup your firewall to let this port through."));
    }
};

class MicrophoneDevice: public GlobalComboBox {
public:
    MicrophoneDevice():
        GlobalComboBox("MicrophoneDevice", true) {
        setLabel(QObject::tr("Microphone device"));
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        addSelection("None");
        fillSelectionsFromDir(dev);
        setHelpText(QObject::tr("Select the device path for your microphone. "
		    "Currently this CANNOT be the same device as used for audio output."));
    }
};

class MySipName: public GlobalLineEdit {
public:
    MySipName():
        GlobalLineEdit("MySipName") {
        setLabel(QObject::tr("My Display Name"));
        setValue("Me");
        setHelpText(QObject::tr("My common name to display when I call other people. "));
    }
};

/*class MySipUser: public GlobalLineEdit {
public:
    MySipUser():
        GlobalLineEdit("MySipUser") {
        setLabel(QObject::tr("My SIP User"));
        setValue("1000");
        setHelpText(QObject::tr("The phone number or username that identifies this SIP client. "
                                "This will be combined with the host address to form the SIP URI user@ip-address."));
    }
};*/

class CodecPriorityList: public GlobalLineEdit {
public:
    CodecPriorityList():
        GlobalLineEdit("CodecPriorityList") {
        setLabel(QObject::tr("Codecs Supported"));
        setValue("GSM;G.711u;G.711a");
        setHelpText(QObject::tr("The list of codecs to use, in the preferred order separated by semicolon. "
                                "Supported codecs are G.711u, G.711a and GSM."));
    }
};

class SipBindInterface: public GlobalLineEdit {
public:
    SipBindInterface():
        GlobalLineEdit("SipBindInterface") {
        setLabel(QObject::tr("SIP Network Interface"));
        setValue(QObject::tr("eth0"));
        setHelpText(QObject::tr("Enter the name of the network to bind to e.g. eth0"));
    }
};

class NatTraversalMethod: public GlobalComboBox {
public:
    NatTraversalMethod():
        GlobalComboBox("NatTraversalMethod") {
        setLabel(QObject::tr("NAT Traversal Method"));
        addSelection("None");
        addSelection("Manual");
        addSelection("Web Server");
        setHelpText(QObject::tr("Method to use for NAT traversal; needs a Frontend restart after channging. Choose "
                                "NONE if you have a public IP address, "
                                "choose MANUAL if your ISP always gives you the same public address and manually "
                                "enter this address below. Choose Web Server if you have a dynamic NAT address "
                                "and enter a web address like http://checkip.dyndns.org below. "));
    }
};

class NatIpAddress: public GlobalLineEdit {
public:
    NatIpAddress():
        GlobalLineEdit("NatIpAddress") {
        setLabel(QObject::tr("NAT IP Address"));
        setValue(QObject::tr("http://checkip.dyndns.org"));
        setHelpText(QObject::tr("If you selected MANUAL above, then enter your Public IP Address here. If you "
                                "selected WEB Server above then enter your web server URL here."));
    }
};

class AudioLocalPort: public GlobalLineEdit {
public:
    AudioLocalPort():
        GlobalLineEdit("AudioLocalPort") {
        setLabel(QObject::tr("Audio RTP Port"));
        setValue("21232");
        setHelpText(QObject::tr("Enter the port to use for Audio RTP; an even number between 1024 and 32767. If you have a firewall "
                                "you should enable UDP through the firewall on this port."));
    }
};

class VideoLocalPort: public GlobalLineEdit {
public:
    VideoLocalPort():
        GlobalLineEdit("VideoLocalPort") {
        setLabel(QObject::tr("Video RTP Port"));
        setValue("21234");
        setHelpText(QObject::tr("Enter the port to use for Video RTP; an even number between 1024 and 32767. If you have a firewall "
                                "you should enable UDP through the firewall on this port."));
    }
};

class SipAutoanswer: public GlobalCheckBox {
public:
    SipAutoanswer():
        GlobalCheckBox("SipAutoanswer") {
        setLabel(QObject::tr("Auto-Answer"));
        setValue(false);
        setHelpText(QObject::tr("When the MythFrontend is in the MythPhone plugin, "
                    "setting this automatically answers all incoming calls. There is no security yet."));
    };
};







MythPhoneSettings::MythPhoneSettings()
{

    VerticalConfigurationGroup* sipProxySet = new VerticalConfigurationGroup(false);
    sipProxySet->setLabel(QObject::tr("SIP Proxy Settings"));
    sipProxySet->addChild(new SipRegisterWithProxy());
    sipProxySet->addChild(new SipProxyName());
    sipProxySet->addChild(new SipProxyAuthName());
    sipProxySet->addChild(new SipProxyAuthPassword());
    sipProxySet->addChild(new MySipName());
    //sipProxySet->addChild(new MySipUser());
    addChild(sipProxySet);

    VerticalConfigurationGroup* sipSet = new VerticalConfigurationGroup(false);
    sipSet->setLabel(QObject::tr("SIP Settings"));
    sipSet->addChild(new CodecPriorityList());
    sipSet->addChild(new SipBindInterface());
    sipSet->addChild(new SipLocalPort());
    sipSet->addChild(new NatTraversalMethod());
    sipSet->addChild(new NatIpAddress());
    sipSet->addChild(new AudioLocalPort());
    sipSet->addChild(new VideoLocalPort());
    addChild(sipSet);

    VerticalConfigurationGroup* vxmlSet = new VerticalConfigurationGroup(false);
    vxmlSet->setLabel(QObject::tr("VXML Settings"));
    vxmlSet->addChild(new SipAutoanswer());
    vxmlSet->addChild(new TimeToAnswer());
    vxmlSet->addChild(new DefaultVxmlUrl());
    vxmlSet->addChild(new DefaultVoicemailPrompt());
    vxmlSet->addChild(new TTSVoice());
    addChild(vxmlSet);

    VerticalConfigurationGroup* webcamSet = new VerticalConfigurationGroup(false);
    webcamSet->setLabel(QObject::tr("WEBCAM Settings"));
    webcamSet->addChild(new WebcamDevice());
    webcamSet->addChild(new MicrophoneDevice());
    webcamSet->addChild(new TxResolution());
    webcamSet->addChild(new TransmitFPS());
    webcamSet->addChild(new CaptureResolution());
    addChild(webcamSet);
}




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

class WebcamDevice: public ComboBoxSetting, public GlobalSetting {
public:
    WebcamDevice():ComboBoxSetting(true),
        GlobalSetting("WebcamDevice") {
        setLabel(QObject::tr("Webcam device"));
        QDir dev("/dev", "video*", QDir::Name, QDir::System);
        fillSelectionsFromDir(dev);
        setHelpText(QObject::tr("Select the device path your webcam is "
		    "using. If you have a capture card it may be using "
		    "/dev/video so try /dev/video1"));
    }
};

class TxResolution: public ComboBoxSetting, public GlobalSetting {
public:
    TxResolution():
        GlobalSetting("TxResolution") {
        setLabel(QObject::tr("Transmit Resolution"));
        addSelection(QObject::tr("176x144"), "176x144");
        addSelection(QObject::tr("128x96"), "128x96");
        addSelection(QObject::tr("704x576"), "704x576");
        addSelection(QObject::tr("352x288"), "352x288");
        setHelpText(QObject::tr("Size of video window to transmit; higher "
                    "resolutions require more bandwidth."));
    };
};

class CaptureResolution: public ComboBoxSetting, public GlobalSetting {
public:
    CaptureResolution():
        GlobalSetting("CaptureResolution") {
        setLabel(QObject::tr("Capture Resolution"));
        addSelection("352x288", "352x288");
        addSelection("320x240", "320x240");
        addSelection("176x144", "176x144");
        addSelection("160x120", "160x120");
        addSelection("128x96", "128x96");
        addSelection("704x576", "704x576");
        addSelection("640x480", "640x480");
        setHelpText(QObject::tr("Size of video source from your webcam. Choose a value compatible with your "
                    "webcam hardware. Choose higher values to digitally pan/zoom before transmission."));
    };
};

class TransmitFPS: public SpinBoxSetting, public GlobalSetting {
public:
    TransmitFPS():
        SpinBoxSetting(1,30,1),
        GlobalSetting("TransmitFPS") {
        setLabel(QObject::tr("Transmit Frames/Second"));
        setValue(5);
        setHelpText(QObject::tr("Number of webcam frames/sec to transmit, from 1 to "
                                "30. Higher numbers create better results but use more bandwidth."));
    }
};

/*
--------------- General VXML Settings ---------------
*/

class TimeToAnswer: public LineEditSetting, public GlobalSetting {
public:
    TimeToAnswer():
        GlobalSetting("TimeToAnswer") {
        setLabel(QObject::tr("Time to Answer"));
        setValue(QObject::tr("10"));
        setHelpText(QObject::tr("The time in seconds a call rings before being "
                                "automatically answered and diverted to a VXML script."));
    }
};

class DefaultVxmlUrl: public LineEditSetting, public GlobalSetting {
public:
    DefaultVxmlUrl():
        GlobalSetting("DefaultVxmlUrl") {
        setLabel(QObject::tr("Default VXML URL"));
        setValue(QObject::tr("http://127.0.0.1/vxml/index.vxml"));
        setHelpText(QObject::tr("The URL to retrieve a VXML script which can "
                                "be used to prompt for leaving a voicemail etc. Leave blank if you "
                                "have no HTTP server and a default Voicemail script will be used."));
    }
};

class DefaultVoicemailPrompt: public LineEditSetting, public GlobalSetting {
public:
    DefaultVoicemailPrompt():
        GlobalSetting("DefaultVoicemailPrompt") {
        setLabel(QObject::tr("Default Voicemail Prompt"));
        setValue(QObject::tr("I am not at home, please leave a message after the tone"));
        setHelpText(QObject::tr("Either a text message to be read by the TTS engine or the filename of "
                                "a .wav file to be played to callers. Only used where the above setting is blank."));
    }
};

class TTSVoice: public ComboBoxSetting, public GlobalSetting {
public:
    TTSVoice():ComboBoxSetting(true),
        GlobalSetting("TTSVoice") {
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

class SipRegisterWithProxy: public CheckBoxSetting, public GlobalSetting {
public:
    SipRegisterWithProxy():
        GlobalSetting("SipRegisterWithProxy") {
        setLabel(QObject::tr("Login to a SIP Server"));
        setValue(true);
        setHelpText(QObject::tr("Allows you to register with services such as Free World Dialup; "
                    "or with applications like Asterisk. Restart mythfrontend if you change this."));
    };
};

class SipProxyName: public LineEditSetting, public GlobalSetting {
public:
    SipProxyName():
        GlobalSetting("SipProxyName") {
        setLabel(QObject::tr("SIP Server DNS Name"));
        setValue("fwd.pulver.com");
        setHelpText(QObject::tr("Name of the Proxy, such as fwd.pulver.com for Free World Dialup."));
    }
};

class SipProxyAuthName: public LineEditSetting, public GlobalSetting {
public:
    SipProxyAuthName():
        GlobalSetting("SipProxyAuthName") {
        setLabel(QObject::tr("Sign-in Name"));
        setValue("");
        setHelpText(QObject::tr("Your username for authentication with the SIP Server. For FWD this is your FWD number."));
    }
};

class SipProxyAuthPassword: public LineEditSetting, public GlobalSetting {
public:
    SipProxyAuthPassword():
        GlobalSetting("SipProxyAuthPassword") {
        setLabel(QObject::tr("Password"));
        setValue("");
        setHelpText(QObject::tr("Your password for authentication with the SIP Server."));
    }
};

class SipLocalPort: public LineEditSetting, public GlobalSetting {
public:
    SipLocalPort():
        GlobalSetting("SipLocalPort") {
        setLabel(QObject::tr("SIP Local Port"));
        setValue(QObject::tr("5060"));
        setHelpText(QObject::tr("The port on this machine to use. You may need to make these different "
                                "for each Mythfrontend and setup your firewall to let this port through."));
    }
};

class MicrophoneDevice: public ComboBoxSetting, public GlobalSetting {
public:
    MicrophoneDevice():ComboBoxSetting(true),
        GlobalSetting("MicrophoneDevice") {
        setLabel(QObject::tr("Microphone device"));
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        addSelection("None");
        fillSelectionsFromDir(dev);
        setHelpText(QObject::tr("Select the device path for your microphone. "
		    "Currently this CANNOT be the same device as used for audio output."));
    }
};

class MySipName: public LineEditSetting, public GlobalSetting {
public:
    MySipName():
        GlobalSetting("MySipName") {
        setLabel(QObject::tr("My Display Name"));
        setValue("Me");
        setHelpText(QObject::tr("My common name to display when I call other people. "));
    }
};

/*class MySipUser: public LineEditSetting, public GlobalSetting {
public:
    MySipUser():
        GlobalSetting("MySipUser") {
        setLabel(QObject::tr("My SIP User"));
        setValue("1000");
        setHelpText(QObject::tr("The phone number or username that identifies this SIP client. "
                                "This will be combined with the host address to form the SIP URI user@ip-address."));
    }
};*/

class CodecPriorityList: public LineEditSetting, public GlobalSetting {
public:
    CodecPriorityList():
        GlobalSetting("CodecPriorityList") {
        setLabel(QObject::tr("Codecs Supported"));
        setValue("GSM;G.711u;G.711a");
        setHelpText(QObject::tr("The list of codecs to use, in the preferred order separated by semicolon. "
                                "Supported codecs are G.711u, G.711a and GSM."));
    }
};

class SipBindInterface: public LineEditSetting, public GlobalSetting {
public:
    SipBindInterface():
        GlobalSetting("SipBindInterface") {
        setLabel(QObject::tr("SIP Network Interface"));
        setValue(QObject::tr("eth0"));
        setHelpText(QObject::tr("Enter the name of the network to bind to e.g. eth0"));
    }
};

class NatTraversalMethod: public ComboBoxSetting, public GlobalSetting {
public:
    NatTraversalMethod():
        GlobalSetting("NatTraversalMethod") {
        setLabel(QObject::tr("NAT Traversal Method"));
        addSelection("None");
        addSelection("Manual");
        addSelection("Web Server");
        setHelpText(QObject::tr("Method to use for NAT traversal; needs a Frontend restart after changing. Choose "
                                "NONE if you have a public IP address, "
                                "choose MANUAL if your ISP always gives you the same public address and manually "
                                "enter this address below. Choose Web Server if you have a dynamic NAT address "
                                "and enter a web address like http://checkip.dyndns.org below. "));
    }
};

class NatIpAddress: public LineEditSetting, public GlobalSetting {
public:
    NatIpAddress():
        GlobalSetting("NatIpAddress") {
        setLabel(QObject::tr("NAT IP Address"));
        setValue(QObject::tr("http://checkip.dyndns.org"));
        setHelpText(QObject::tr("If you selected MANUAL above, then enter your Public IP Address here. If you "
                                "selected WEB Server above then enter your web server URL here."));
    }
};

class AudioLocalPort: public LineEditSetting, public GlobalSetting {
public:
    AudioLocalPort():
        GlobalSetting("AudioLocalPort") {
        setLabel(QObject::tr("Audio RTP Port"));
        setValue("21232");
        setHelpText(QObject::tr("Enter the port to use for Audio RTP; an even number between 1024 and 32767. If you have a firewall "
                                "you should enable UDP through the firewall on this port."));
    }
};

class VideoLocalPort: public LineEditSetting, public GlobalSetting {
public:
    VideoLocalPort():
        GlobalSetting("VideoLocalPort") {
        setLabel(QObject::tr("Video RTP Port"));
        setValue("21234");
        setHelpText(QObject::tr("Enter the port to use for Video RTP; an even number between 1024 and 32767. If you have a firewall "
                                "you should enable UDP through the firewall on this port."));
    }
};

class SipAutoanswer: public CheckBoxSetting, public GlobalSetting {
public:
    SipAutoanswer():
        GlobalSetting("SipAutoanswer") {
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




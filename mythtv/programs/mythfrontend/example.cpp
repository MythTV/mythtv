#include "mythmainwindow.h"
#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuitext.h"

// .h
class AirPlayPictureScreen : public MythScreenType
{
  public:
    AirPlayPictureScreen(MythScreenStack *parent);
   ~AirPlayPictureScreen();

   // These two methods are declared by MythScreenType and their signatures
   // should not be changed
    virtual bool Create(void);
    virtual void Init(void);

    void UpdatePicture(const QString &imageFilename,
                       const QString &imageDescription);

  private:
    QString     m_imageFilename;
    QString     m_imageDescription;
    MythUIImage *m_airplayImage;
    MythUIText  *m_airplayText;
};

///////////////////////////////////////////////////
// .cpp

AirPlayPictureScreen::AirPlayPictureScreen(MythScreenStack *parent)
              :MythScreenType(parent, "airplaypicture"),
              m_imageFilename(""), m_imageDescription(""),
              m_airplayImage(NULL), m_airplayText(NULL)
{
}

bool AirPlayPictureScreen::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    // The xml file containing the screen definition is airplay-ui.xml in this
    // example, the name of the screen in the xml is airplaypicture. This
    // should make sense when you look at the xml below
    foundtheme = LoadWindowFromXML("airplay-ui.xml", "airplaypicture", this);

    if (!foundtheme) // If we cannot load the theme for any reason ...
        return false;

    // The xml should contain an <imagetype> named 'picture', if it doesn't
    // then we cannot display the image and may as well abort
    m_airplayImage = dynamic_cast<MythUIImage*>
                                            (GetChild("picture"));
    if (!m_airplayImage)
        return false;

    // As an illustration let's say the picture includes a description/title or some other metadata
    // Let's also say that display of this metadata is entirely optional, so we won't fail if the theme
    // doesn't include 'description'
    m_airplayText = dynamic_cast<MythUIText*>
                                            (GetChild("description"));

    return true;
}

void AirPlayPictureScreen::Init(void)
{
    if (m_airplayImage)
    {
        if (!m_imageFilename.isEmpty())
        {
            m_airplayImage->SetFilename(m_imageFilename); // Absolute path, http or SG url
            m_airplayImage->Load(); // By default the image is loaded in a background thread, use LoadNow() to load in foreground
        }
        else
        {
            // Will default to displaying whatever placeholder image is defined
            // in the xml by the themer, means we can show _something_ rather than
            // a big empty hole. Generally you always want to call Reset() in
            // these circumstances
            m_airplayImage->Reset();
        }
    }

    if (m_airplayText)
    {
        if (!m_imageDescription.isEmpty())
        {
            m_airplayText->SetText(m_imageDescription);
        }
        else
        {
            // Same as above, calling Reset() allows for a sane, themer defined
            //default to be displayed
            m_airplayText->Reset();
        }
    }
}

// If want to update the displayed image or text without closing this screen
// and creating a new one then it might look something like this
void AirPlayPictureScreen::UpdatePicture(const QString &imageFilename,
                                         const QString &imageDescription)
{
    m_imageFilename = imageFilename;
    m_imageDescription = imageDescription;

    Init();
}

////////////////////////////////////////

// Your AirPlay picture event handler, after writing image to disc
void SomeClass::AirPlayPictureEventHandler(blah ...)
{
    QString filename = "/path/to/image";
    QString description = "Description of image, from metadata?";

    // .... //

    MythScreenStack *screenStack = GetMythMainWindow()->GetStack("popup stack");
    AirPlayPictureScreen *picScreen = new AirPlayPictureScreen();

    if (picScreen->Create()) // Reads screen definition from xml, and constructs screen
    {
        picScreen->UpdatePicture(filename, description);
        screenStack->AddScreen(picScreen);
    }
    else
    {
        // If we can't create the screen then we can't display it, so delete
        // and abort
        delete picScreen;
        return;
    }
}

////////////////////////////////////////
// airplay-ui.xml
// See http://www.mythtv.org/wiki/MythUI_Theme_Development for more, this is a
// bare minimum example, the wiki explains everything in detail

/* Comment out the XML to make cppcheck happy
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE mythuitheme SYSTEM "http://www.mythtv.org/schema/mythuitheme.dtd">
<mythuitheme>

    <window name="airplaypicture">
        <!-- No <area> tags for window implies fullscreen -->

        <!-- Required -->
        <imagetype name="picture">
            <area>0,0,100%,100%</area> <!-- Same size as screen, i.e. fullscreen -->
        </imagetype>

        <!-- Optional -->
        <textarea name="description">
            <area>50,50,300,200</area> <!-- x,y,w,h - Origin at top left -->
            <font>basesmall</font> <!-- See base.xml for the theme -->
            <align>hcenter,vcenter</align> <!-- Centre text in available space -->
            <multiline>yes</multiline> <!-- Allow wrapping -->
        </textarea>

    </window>

</mythuitheme>

*/

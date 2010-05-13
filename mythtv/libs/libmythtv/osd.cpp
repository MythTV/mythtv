// C headers
#include <cmath>
#include <unistd.h>

// C++ headers
#include <iostream>
#include <algorithm>
using namespace std;

// Qt headers
#include <QDomDocument>
#include <QStringList>
#include <QImage>
#include <QPixmap>
#include <QBitmap>
#include <QDir>
#include <QFile>
#include <QColor>
#include <QRegExp>
#include <QKeyEvent>

// MythTV headers
#include "ttfont.h"
#include "osd.h"
#include "osdtypes.h"
#include "osdsurface.h"
#include "mythcontext.h"
#include "textsubtitleparser.h"
#include "oldsettings.h"
#include "udpnotify.h"
#include "compat.h"
#include "mythverbose.h"
#include "util.h"
#include "channelutil.h"

#include "x11colors.h"
#include "mythdirs.h"

#include "osdtypeteletext.h"
#include "osdlistbtntype.h"

static char  *cc708_default_font_names[16];
static bool   cc708_defaults_initialized = false;
static QMutex cc708_init_lock;
static void initialize_osd_fonts(void);

#define LOC QString("OSD: ")
#define LOC_ERR QString("OSD, Error: ")

const char *kOSDDialogActive         = "DialogActive";
const char *kOSDDialogAllowRecording = "allowrecordingbox";
const char *kOSDDialogAlreadyEditing = "alreadybeingedited";
const char *kOSDDialogExitOptions    = "exitplayoptions";
const char *kOSDDialogAskDelete      = "askdeleterecording";
const char *kOSDDialogIdleTimeout    = "idletimeout";
const char *kOSDDialogSleepTimeout   = "sleeptimeout";
const char *kOSDDialogChannelTimeout = "channel_timed_out";
const char *kOSDDialogInfo           = "infobox";
const char *kOSDDialogEditChannel    = "channel_editor";

OSD::OSD() :
    QObject(),                          m_listener(NULL),
    osdBounds(),                        frameint(0),
    needPillarBox(false),
    themepath(FindTheme(gCoreContext->GetSetting("OSDTheme"))),
    wscale(1.0f),                       fscale(1.0f),
    m_themeinfo(new ThemeInfo(themepath)),
    m_themeaspect(4.0f/3.0f),
    hmult(1.0f),                        wmult(1.0f),
    xoffset(0),                         yoffset(0),
    displaywidth(0),                    displayheight(0),
    m_setsvisible(false),
    totalfadetime(0),                   timeType(0),
    timeFormat(""),                     setList(new vector<OSDSet*>),
    editarrowleft(NULL),                editarrowright(NULL),
    editarrowRect(),                    drawSurface(NULL),
    changed(false),                     runningTreeMenu(NULL),
    treeMenuContainer(""),
    // EIA-608 captions
    fontname(""),                       ccfontname(""),
    fontSizeType(""),
    // Test file subtitles
    removeHTML(QRegExp("</?.+>"))
{
    VERBOSE(VB_GENERAL, QString("OSD Theme Dimensions W: %1 H: %2")
            .arg(m_themeinfo->BaseRes()->width()).arg(m_themeinfo->BaseRes()->height()));

    for (uint i = 0; i < (sizeof(cc708fontnames) / sizeof(QString)); i++)
        cc708fontnames[i] = QString("");

    if (m_themeinfo->BaseRes()->height())
    {
        m_themeaspect  = (float)m_themeinfo->BaseRes()->width();
        m_themeaspect /= (float)m_themeinfo->BaseRes()->height();
    }
}

OSD::~OSD(void)
{
    QMutexLocker locker(&osdlock);

    {
        QMutexLocker locker(&loadFontLock);
        QMap<QString, TTFFont *>::iterator it = fontMap.begin();
        for (; it != fontMap.end(); ++it)
        {
            if (*it)
                delete *it;
        }

        loadFontHash.clear();
        reinitFontHash.clear();
        fontMap.clear();
    }

    QMap<QString, OSDSet *>::iterator sets = setMap.begin();
    for (; sets != setMap.end(); ++sets)
    {
        if (*sets)
        {
            delete *sets;
            *sets = NULL;
        }
    }
    setMap.clear();

    if (m_themeinfo)
    {
        delete m_themeinfo;
        m_themeinfo = NULL;
    }

    if (editarrowleft)
    {
        delete editarrowleft;
        editarrowleft = NULL;
    }

    if (editarrowright)
    {
        delete editarrowright;
        editarrowright = NULL;
    }

    if (setList)
    {
        delete setList;
        setList = NULL;
    }

    if (drawSurface)
    {
        delete drawSurface;
        drawSurface = NULL;
    }
}

void OSD::Init(const QRect &osd_bounds, int   frameRate,
         const QRect &vis_bounds, float visibleAspect, float fontScaling)
{
    removeHTML.setMinimal(true);

    fscale = fontScaling;
    osdBounds = osd_bounds;
    frameint = frameRate;
    hmult = vis_bounds.height() / (float)m_themeinfo->BaseRes()->height();
    wmult = vis_bounds.width() / (float)m_themeinfo->BaseRes()->width();
    xoffset = vis_bounds.left();
    yoffset = vis_bounds.top();
    displaywidth = vis_bounds.width();
    displayheight = vis_bounds.height();
    drawSurface = new OSDSurface(osd_bounds.width(), osd_bounds.height());

    if (!cc708_defaults_initialized)
        initialize_osd_fonts();

    for (uint i = 0; i < 16; i++)
        cc708fontnames[i] = cc708_default_font_names[i];

    needPillarBox = visibleAspect > 1.51f;
    wscale = visibleAspect / m_themeaspect;
    // adjust for wscale font size scaling
    fscale *= (float) sqrt(2.0/(sq(wscale) + 1.0));

    if (themepath.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Couldn't find OSD theme: "
                <<gCoreContext->GetSetting("OSDTheme"));
        InitDefaults();
        return;
    }

    themepath += "/";
    if (!LoadTheme())
    {
        VERBOSE(VB_IMPORTANT, "Couldn't load OSD theme: "
                <<gCoreContext->GetSetting("OSDTheme")<<" at "<<themepath);
    }

    InitDefaults();

    // Reinit since LoadThemes and InitDefaults() appear to mess things up.
    Reinit(osd_bounds, frameRate, vis_bounds, visibleAspect, fontScaling);

}

void OSD::SetFrameInterval(int frint)
{
    frameint = frint;

    QMap<QString, OSDSet *>::iterator sets = setMap.begin();
    for (; sets != setMap.end(); ++sets)
    {
        OSDSet *set = (*sets);
        if (set)
           set->SetFrameInterval(frameint);
    }
}

bool OSD::InitDefaults(void)
{
    bool ok = true;
    ok &= InitCC608();
    ok &= InitCC708();
    ok &= InitTeletext();
    ok &= InitMenu();
    ok &= InitSubtitles();
    ok &= InitInteractiveTV();
    return ok;
}

// EIA-608 "analog" captions
bool OSD::InitCC608(void)
{
    TTFFont *ccfont = GetFont("cc_font");
    if (!ccfont)
    {
        QString name = "cc_font";
        int fontsize = m_themeinfo->BaseRes()->height() / 27;

        ccfont = LoadFont(gCoreContext->GetSetting("OSDCCFont"), fontsize);

        if (ccfont)
            fontMap[name] = ccfont;
    }

    if (!ccfont)
        return false;

    if (GetSet("cc_page"))
        return true;

    QString name = "cc_page";
    OSDSet *container =
        new OSDSet(name, true,
                   osdBounds.width(), osdBounds.height(),
                   wmult, hmult, frameint);
    container->SetListener(m_listener);
    container->SetPriority(30);
    AddSet(container, name);

    int sub_dispw = displaywidth;
    int sub_disph = displayheight;
    int sub_xoff = xoffset;
    int sub_yoff = yoffset;
    if (needPillarBox)
    {
        // widescreen -- need to "pillarbox" captions
        sub_dispw = (int)(wscale * 4.0f*displayheight/3.0f);
        sub_disph = displayheight;
        sub_xoff = xoffset + (displaywidth-sub_dispw)/2;
        sub_yoff = yoffset;
    }

    OSDTypeCC *ccpage =
        new OSDTypeCC(name, ccfont, sub_xoff, sub_yoff,
                      sub_dispw, sub_disph, wmult, hmult);
    container->AddType(ccpage);
    return true;
}

// EIA-708 "digital" captions
bool OSD::InitCC708(void)
{
    VERBOSE(VB_VBI, LOC + "InitCC708() -- begin");
    // Check if cc708 osd already exists, exit early if it does
    QString name = "cc708_page";
    if (GetSet(name))
    {
        VERBOSE(VB_IMPORTANT, LOC + "InitCC708() -- end (already exists)");
        return true;
    }

    // Create fonts...
    TTFFont* ccfonts[48];
    uint z = gCoreContext->GetNumSetting("OSDCC708TextZoom", 100) *
                    m_themeinfo->BaseRes()->height();
    uint fontsizes[3] = { z / 3600, z / 2900, z / 2200 };
    for (uint i = 0; i < 48; i++)
    {
        TTFFont *ccfont = GetFont(QString("cc708_font%1").arg(i));
        if (!ccfont)
        {
            QString name = QString("cc708_font%1").arg(i);
            int fontsize = fontsizes[i%3];

            ccfont = LoadFont(cc708fontnames[i/3], fontsize);
            if (ccfont)
                fontMap[name] = ccfont;

            if (!ccfont)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "A CC708 font is missing");
                return false;
            }
        }
        ccfonts[i] = ccfont;
    }

    // Create a container for one service
    OSDSet *container =
        new OSDSet(
            name, true, osdBounds.width(), osdBounds.height(),
            wmult, hmult, frameint);
    container->SetListener(m_listener);
    container->SetPriority(30);

    AddSet(container, name);
    OSDType708CC *ccpage =
        new OSDType708CC(name, ccfonts, xoffset, yoffset,
                         displaywidth, displayheight);
    container->AddType(ccpage);

    VERBOSE(VB_VBI, LOC + "InitCC708() -- end");
    return true;
}

bool OSD::InitTeletext(void)
{
    if (GetSet("teletext"))
        return true;

    QString name = "teletext";
    OSDSet *container =
        new OSDSet(name, true, osdBounds.width(), osdBounds.height(),
                   wmult, hmult, frameint);
    container->SetListener(m_listener);
    container->SetAllowFade(false);
    container->SetWantsUpdates(true);
    AddSet(container, name);
    QSize *size = m_themeinfo->BaseRes();
    int safe_x = (int)(((float)size->width() * 0.05f) + 0.5f);
    int safe_y = (int)(((float)size->height() * 0.05f) + 0.5f);
    QRect area = QRect(safe_x, safe_y, size->width() - (2 * safe_x),
                                       size->height() - (2 * safe_y));
    normalizeRect(area);
    // XXX TODO use special teletextfont
    QString fontname = "teletextfont";
    TTFFont *font = GetFont(fontname);
    if (!font)
    {
        int fontsize = (size->height() - (2 * safe_y)) / 26;
        font = LoadFont(gCoreContext->GetSetting("OSDCCFont"), fontsize);

        if (font)
            fontMap[fontname] = font;
    }

    OSDTypeTeletext *ttpage = new OSDTypeTeletext(
        name, font, area, wmult, hmult, this);

    container->SetPriority(30);
    container->AddType(ttpage);
    return true;
}

void OSD::UpdateTeletext(void)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("teletext");
    if (!container)
        return;

    OSDType *type = container->GetType("teletext");
    OSDTypeTeletext *ttpage = dynamic_cast<OSDTypeTeletext*>(type);
    if (ttpage)
    {
        container->Display(1);
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::SetTextSubtitles(const QStringList &lines)
{
    const uint SUBTITLE_FONT_SIZE     = 20;
    const float SUBTITLE_LINE_SPACING = 1.1;
    const int MAX_CHARACTERS_PER_ROW  = 50;
    // how many pixels of empty space at the bottom
    const uint BOTTOM_PAD = SUBTITLE_FONT_SIZE;

    OSDSet *subtitleSet = GetSet("subtitles");
    if (!subtitleSet)
        return;

    // wrap long lines to multiple lines
    QString subText = "";
    int subLines = 0;
    QStringList::const_iterator it = lines.begin();
    for (; it != lines.end(); ++it)
    {
        QString tmp = *it;
        const QString line = tmp.remove((const QRegExp&) removeHTML);

        if (line.length() <= MAX_CHARACTERS_PER_ROW)
        {
            subText.append(line);
            subText.append("\n");
            ++subLines;
            continue;
        }

        // wrap long lines at word spaces
        QStringList words = line.split(" ", QString::SkipEmptyParts);
        QString newString = "";

        do
        {
            QString word = words.first();
            words.pop_front();

            int totLen = newString.length() + word.length() + 1;
            if (totLen > MAX_CHARACTERS_PER_ROW)
            {
                // next word won't fit anymore, create a new line
                subText.append(newString + "\n");
                ++subLines;
                newString = "";
            }
            newString.append(word + " ");
        }
        while (!words.empty());

        subText.append(newString);
        subText.append("\n");
        ++subLines;
    }

    ClearAll("subtitles");

    QString name = "text_subtitles";


    // I couldn't get the space using the LineSpacing accurately
    // (getting it through (SUBTITLE_LINE_SPACING - 1)*H resulted
    // in too small space. Just hard code it, it should work as
    // the variance in the count of subtitle lines is so low.
    const int totalSpaceBetweenLines = (subLines - 1) * 5;
    const int subtitleTotalHeight =
        subLines * SUBTITLE_FONT_SIZE + totalSpaceBetweenLines;

    // put the subtitles to the bottom of the screen
    QRect area(0, displayheight - (int)((subtitleTotalHeight + BOTTOM_PAD)*hmult),
               displaywidth, displayheight);

    QString fontname = "text_subtitle_font";
    TTFFont *font = GetFont(fontname);
    if (!font)
    {
        font = LoadFont(gCoreContext->GetSetting("OSDCCFont"), SUBTITLE_FONT_SIZE);

        if (font)
        {
            // set outline so we can see the font in white background video
            font->setOutline(true);
            fontMap[fontname] = font;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Cannot load font for text subtitles.");
            return;
        }
    }

    OSDTypeText *text = new OSDTypeText(name, font, "", area, wmult, hmult);

    text->SetCentered(true);
    text->SetMultiLine(true);
    text->SetText(subText);
    text->SetSelected(false);
    text->SetLineSpacing(SUBTITLE_LINE_SPACING);
    subtitleSet->AddType(text);
    SetVisible(subtitleSet, 0);
}

void OSD::ClearTextSubtitles()
{
    HideSet("subtitles");
    ClearAll("subtitles");
}

bool OSD::InitMenu(void)
{
    if (GetSet("menu"))
        return true;

    QString name = "menu";
    OSDSet *container =
        new OSDSet(name, true,
                   osdBounds.width(), osdBounds.height(),
                   wmult, hmult, frameint);
    container->SetListener(m_listener);
    AddSet(container, name);

    QRect area = QRect(20, 40, 620, 300);
    QRect listarea = QRect(0, 0, 274, 260);

    normalizeRect(area);
    normalizeRect(listarea);
    listarea.translate((int)(-xoffset*hmult+0.5), (int)(-yoffset*hmult+0.5));

    OSDListTreeType *lb = new OSDListTreeType("menu", area, listarea, 10,
                                              wmult, hmult);

    lb->SetListener(m_listener);

    lb->SetItemRegColor(QColor("#505050"), QColor("#000000"), 100);
    lb->SetItemSelColor(QColor("#52CA38"), QColor("#349838"), 255);

    lb->SetSpacing(2);
    lb->SetMargin(3);

    TTFFont *actfont = GetFont("infofont");
    TTFFont *inactfont = GetFont("infofont");

    if (!actfont)
    {
        actfont = LoadFont(gCoreContext->GetSetting("OSDFont"), 16);

        if (actfont)
            fontMap["treemenulistfont"] = actfont;
    }

    if (!actfont)
    {
        QMap<QString, TTFFont *>::Iterator it = fontMap.begin();
        actfont = *it;
    }

    if (!inactfont)
        inactfont = actfont;

    lb->SetFontActive(actfont);
    lb->SetFontInactive(inactfont);

    container->AddType(lb);

    return true;
}

bool OSD::InitSubtitles(void)
{
    // Create container for subtitles (DVB, DVD and external subs)
    if (GetSet("subtitles"))
        return true;

    QString name = "subtitles";
    OSDSet *container =
        new OSDSet(name, true,
                   osdBounds.width(), osdBounds.height(),
                   wmult, hmult, frameint);

    container->SetListener(m_listener);
    container->SetPriority(30);
    AddSet(container, name);
    return true;
}

bool OSD::InitInteractiveTV(void)
{
    // Create container for interactive TV
    if  (GetSet("interactive"))
        return true;
    QString name = "interactive";
    OSDSet *container =
        new OSDSet(name, true,
                   osdBounds.width(), osdBounds.height(),
                   wmult, hmult, frameint);
    container->SetListener(m_listener);
    container->SetPriority(25);
    container->Display(true);
    AddSet(container, name);
    return true;
}

void OSD::Reinit(const QRect &totalBounds,   int   frameRate,
                 const QRect &visibleBounds,
                 float visibleAspect, float fontScaling)
{
    QMutexLocker locker(&osdlock);

    QRect oldB    = osdBounds;

    osdBounds     = totalBounds;
    xoffset       = visibleBounds.left();
    yoffset       = visibleBounds.top();
    displaywidth  = visibleBounds.width();
    displayheight = visibleBounds.height();
    wmult         = displaywidth  / (float)m_themeinfo->BaseRes()->width();
    hmult         = displayheight / (float)m_themeinfo->BaseRes()->height();
    needPillarBox = visibleAspect > 1.51f;
    frameint      = (frameRate <= 0) ? frameRate : frameint;

    float themeaspect = (float)m_themeinfo->BaseRes()->width() / (float)m_themeinfo->BaseRes()->height();

    wscale = visibleAspect / themeaspect;
    fscale = fontScaling;
    // adjust for wscale font size scaling
    fscale *= (float) sqrt(2.0/(sq(wscale) + 1.0));

    ReinitFonts();

    QMap<QString, OSDSet *>::iterator sets = setMap.begin();
    for (; sets != setMap.end(); ++sets)
    {
        if (!(*sets))
            continue;

        int sub_xoff  = xoffset;
        int sub_yoff  = yoffset;
        int sub_dispw = displaywidth;
        int sub_disph = displayheight;
        if ((*sets)->GetName() == "cc_page" && needPillarBox)
        {
            // widescreen -- need to "pillarbox" captions
            sub_dispw = (int)(wscale * 4.0f*displayheight/3.0f);
            sub_disph = displayheight;
            sub_xoff = xoffset + (displaywidth-sub_dispw)/2;
            sub_yoff = yoffset;
        }
        (*sets)->Reinit(osdBounds.width(), osdBounds.height(),
                        sub_xoff, sub_yoff, sub_dispw, sub_disph,
                        wmult, hmult, frameint);
    }

    if (true || oldB != osdBounds)
    {
        delete drawSurface;
        drawSurface = new OSDSurface(osdBounds.width(), osdBounds.height());
    }
    else
    {
        drawSurface->ClearUsed();
    }
}

QString OSD::FindTheme(QString name)
{
    QString testdir;
    QDir dir;

    if (!name.isEmpty())
    {

        testdir = GetConfDir() + "/osd/" + name;
        dir.setPath(testdir);
        if (dir.exists())
            return testdir;

        testdir = GetShareDir() + "themes/" + name;
        dir.setPath(testdir);
        if (dir.exists())
            return testdir;

        testdir = "../libNuppelVideo/" + name;
        dir.setPath(testdir);
        if (dir.exists())
            return testdir;
    }

    testdir = GetShareDir() + "themes/BlackCurves-OSD";
    dir.setPath(testdir);
    if (dir.exists())
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find OSD theme: %1. "
                "Switching to default.").arg(gCoreContext->GetSetting("OSDTheme")));
        gCoreContext->OverrideSettingForSession("OSDTheme", "BlackCurves-OSD");
        return testdir;
    }

    return "";
}

TTFFont *OSD::LoadFont(const QString &name, int size)
{
    QString fullname = GetConfDir() + "/" + name;
    QString stdFontKey = QString("%1_%2###%3_%4")
        .arg(fullname).arg(size)
        .arg(wscale,8,'g').arg(hmult*fscale,8,'g');

    QMutexLocker locker(&loadFontLock);
    QHash<QString, TTFFont*>::iterator it =
        loadFontHash.find(stdFontKey);
    if (it != loadFontHash.end())
        return (*it) ? new TTFFont(**it) : NULL;

    QString sharedFontKey = QString("%1_%2###%3_%4")
        .arg(GetShareDir() + name).arg(size)
        .arg(wscale,8,'g').arg(hmult*fscale,8,'g');

    it = loadFontHash.find(sharedFontKey);
    if (it != loadFontHash.end())
        return (*it) ? new TTFFont(**it) : NULL;

    QString themeFontKey = QString("%1_%2###%3_%4")
        .arg(themepath + "/" + name).arg(size)
        .arg(wscale,8,'g').arg(hmult*fscale,8,'g');

    if (!themepath.isEmpty())
    {
        it = loadFontHash.find(themeFontKey);
        if (it != loadFontHash.end())
            return (*it) ? new TTFFont(**it) : NULL;
    }

    QString simpleFontKey = QString("%1_%2###%3_%4")
        .arg(name).arg(size)
        .arg(wscale,8,'g').arg(hmult*fscale,8,'g');
    it = loadFontHash.find(simpleFontKey);
    if (it != loadFontHash.end())
    {
        if (!*it)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Unable to find font: %1\n\t\t\t"
                            "No OSD will be displayed.").arg(name));
        }
        return (*it) ? new TTFFont(**it) : NULL;
    }

    //
    // Loading from cache failed, attempt to load the font.
    //

    TTFFont *font = new TTFFont(fullname, size, wscale, hmult * fscale);
    if (font->isValid())
    {
        loadFontHash[stdFontKey] = font;
        reinitFontHash[font] = stdFontKey;
        return font;
    }
    delete font;

    fullname = GetShareDir() + name;
    font = new TTFFont(fullname, size, wscale, hmult*fscale);
    if (font->isValid())
    {
        loadFontHash[sharedFontKey] = font;
        reinitFontHash[font] = sharedFontKey;
        return font;
    }
    delete font;

    if (!themepath.isEmpty())
    {
        fullname = themepath + "/" + name;
        font = new TTFFont(fullname, size, wscale, hmult * fscale);
        if (font->isValid())
        {
            loadFontHash[themeFontKey] = font;
            reinitFontHash[font] = themeFontKey;
            return font;
        }
        delete font;
    }

    font = new TTFFont(name, size, wscale, hmult * fscale);
    if (font->isValid())
    {
        loadFontHash[simpleFontKey] = font;
        reinitFontHash[font] = simpleFontKey;
        return font;
    }
    delete font;

    loadFontHash[simpleFontKey] = NULL;

    VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("Unable to find font: %1\n\t\t\t"
                    "No OSD will be displayed.").arg(name));

    return NULL;
}

void OSD::ReinitFonts(void)
{
    QMutexLocker locker(&loadFontLock);

    QSet<TTFFont*> done;

    QString key_tail =
        QString("###%1_%2").arg(wscale,8,'g').arg(hmult*fscale,8,'g');
    float hscale = hmult*fscale;
    uint key_tail_len = key_tail.length();

    QMap<QString, TTFFont*>::iterator it = fontMap.begin();
    for (; it != fontMap.end(); ++it)
    {
        if (!*it)
            continue;
        if (done.contains(*it))
            continue;

        QHash<TTFFont*, QString>::iterator kit = reinitFontHash.find(*it);
        if (kit == reinitFontHash.end())
            continue;

        QString key = *kit;
        if (key.right(key_tail_len) == key_tail)
        {
            done.insert(*it);
            continue;
        }

        QHash<QString, TTFFont*>::iterator lit = loadFontHash.find(key);
        if (lit != loadFontHash.end() && (*lit == *it))
        {
            loadFontHash.erase(lit);
            (*it)->Reinit(wscale, hscale);
            QString new_key = key.left(key.length() - key_tail_len) + key_tail;
            reinitFontHash[*it] = new_key;
            loadFontHash[new_key] = *it;
            done.insert(*it);
        }
    }
}


QString OSD::getFirstText(QDomElement &element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return "";
}

void OSD::parseFont(QDomElement &element)
{
    QString name;
    QString fontfile = gCoreContext->GetSetting("OSDFont");
    int size = -1;
    int sizeSmall = -1;
    int sizeBig = -1;
    bool outline = false;
    QPoint shadowOffset = QPoint(0, 0);
    int color = 255;
    QColor color_normal, color_outline, color_shadow;

    name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Font needs a name");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "size")
            {
                size = getFirstText(info).toInt();
            }
            else if (info.tagName() == "size:small")
            {
                sizeSmall = getFirstText(info).toInt();
            }
            else if (info.tagName() == "size:big")
            {
                sizeBig = getFirstText(info).toInt();
            }
            else if (info.tagName() == "color")
            {
                color = getFirstText(info).toInt();

                if (info.hasAttribute("normal"))
                    color_normal = parseColor(info.attribute("normal"));

                if (info.hasAttribute("outline"))
                    color_outline = parseColor(info.attribute("outline"));

                if (info.hasAttribute("shadow"))
                    color_shadow = parseColor(info.attribute("shadow"));
            }
            else if (info.tagName() == "outline")
            {
                if (getFirstText(info).toLower() == "yes")
                    outline = true;
            }
            else if (info.tagName() == "shadow")
            {
                shadowOffset = parsePoint(getFirstText(info));
                shadowOffset.setX((int)(shadowOffset.x() * wmult));
                shadowOffset.setY((int)(shadowOffset.y() * hmult));
            }
            else if (info.tagName() == "filename")
            {
                fontfile = getFirstText(info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag "
                        << info.tagName() << " in font");
                continue;
            }
        }
    }

    TTFFont *font = GetFont(name);
    if (font)
    {
        VERBOSE(VB_IMPORTANT, "Error: already have a font called: " << name);
        return;
    }

    QString fontSizeType = gCoreContext->GetSetting("OSDThemeFontSizeType",
                                                "default");
    if (fontSizeType == "small")
    {
        if (sizeSmall > 0)
            size = sizeSmall;
    }
    else if (fontSizeType == "big")
    {
        if (sizeBig > 0)
            size = sizeBig;
    }

    if (size <= 0)
    {
        VERBOSE(VB_IMPORTANT, "Error: font size specified as: " << size);
        size = 10;
    }

    font = LoadFont(fontfile, size);
    if (!font)
    {
        VERBOSE(VB_IMPORTANT, "Couldn't load font: " << fontfile);
        return;
    }

    font->setColor(color);
    font->setOutline(outline);
    font->setShadow(shadowOffset.x(), shadowOffset.y());

    if (color_normal.isValid())
        font->setColor(color_normal, kTTF_Normal);

    if (color_outline.isValid())
        font->setColor(color_outline, kTTF_Outline);

    if (color_shadow.isValid())
        font->setColor(color_shadow, kTTF_Shadow);

    fontMap[name] = font;
}

void OSD::parseBox(OSDSet *container, QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Box needs a name");
        return;
    }

    QRect area(0, 0, 0, 0);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag in box: " << info.tagName());
                return;
            }
        }
    }

    OSDTypeBox *box = new OSDTypeBox(name, area, wmult, hmult);
    container->AddType(box);
}

void OSD::parseImage(OSDSet *container, QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Image needs a name");
        return;
    }

    QString filename = "";
    QPoint pos = QPoint(0, 0);

    QPoint scale = QPoint(-1, -1);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult + xoffset));
                pos.setY((int)(pos.y() * hmult + yoffset));
            }
            else if (info.tagName() == "staticsize")
            {
                scale = parsePoint(getFirstText(info));
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown: %1 in image")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    if (filename != "")
        filename = themepath + filename;

    OSDTypeImage *image = new OSDTypeImage(name, filename, pos, wmult, hmult,
                                           scale.x(), scale.y());
    container->AddType(image);
}

void OSD::parseTextArea(OSDSet *container, QDomElement &element)
{
    QRect area = QRect(0, 0, 0, 0);
    QString font = "", altfont = "";
    QString statictext = "";
    QString defaulttext = "";
    bool multiline = false;
    bool scroller = false;
    int scrollx = 0;
    int scrolly = 0;
    float linespacing = 1.5;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Text area needs a name");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "altfont")
            {
                altfont = getFirstText(info);
            }
            else if (info.tagName() == "multiline")
            {
                if (getFirstText(info).toLower() == "yes")
                    multiline = true;
            }
            else if (info.tagName() == "statictext")
            {
                statictext = getFirstText(info);
            }
            else if (info.tagName() == "value")
            {
                defaulttext = getFirstText(info);
            }
            else if (info.tagName() == "scroller")
            {
                if (getFirstText(info).toLower() == "yes")
                    scroller = true;
            }
            else if (info.tagName() == "linespacing")
            {
                linespacing = getFirstText(info).toFloat();
            }
            else if (info.tagName() == "scrollmovement")
            {
                QPoint pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));

                scrollx = pos.x();
                scrolly = pos.y();
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag in textarea: "
                        << info.tagName());
                return;
            }
        }
    }

    TTFFont *ttffont = GetFont(font);
    if (!ttffont)
    {
        VERBOSE(VB_IMPORTANT, "Unknown font: " << font
                << " in textarea: " << name);
        return;
    }

    OSDTypeText *text = new OSDTypeText(name, ttffont, "", area, wmult, hmult);
    container->AddType(text);

    text->SetMultiLine(multiline);
    text->SetLineSpacing(linespacing);

    if (altfont != "")
    {
        ttffont = GetFont(altfont);
        if (!ttffont)
        {
            VERBOSE(VB_IMPORTANT, "Unknown altfont: " << altfont
                    << " in textarea: " << name);
        }
        else
            text->SetAltFont(ttffont);
    }

    if (statictext != "")
        text->SetText(statictext);
    if (defaulttext != "")
        text->SetDefaultText(defaulttext);

    QString align = element.attribute("align", "");
    if (!align.isNull() && !align.isEmpty())
    {
        if (align.toLower() == "center")
            text->SetCentered(true);
        else if (align.toLower() == "right")
            text->SetRightJustified(true);
    }

    QString entry = element.attribute("entry", "");
    if (!entry.isEmpty())
    {
        int entrynum = entry.toInt();
        text->SetEntryNum(entrynum);
        text->SetSelected(entrynum == 0);
    }

    QString button = element.attribute("button", "");
    if (!button.isEmpty() && (button.toLower() == "yes"))
        text->SetButton(true);

    if (scroller)
    {
        if (scrollx == 0 && scrolly == 0)
        {
            VERBOSE(VB_IMPORTANT,
                    "Text area set as scrolling, but no movement");
            scrollx = -5;
        }

        text->SetScrolling(scrollx, scrolly);
    }
}

void OSD::parseSlider(OSDSet *container, QDomElement &element)
{
    QRect area = QRect(0, 0, 0, 0);
    QString filename = "";
    QString altfilename = "";

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Slider needs a name");
        return;
    }

    QString type = element.attribute("type", "");
    if (type.isNull() || type.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Slider needs a type");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "altfilename")
            {
                altfilename = getFirstText(info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown: %1 in image")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    if (filename == "")
    {
        VERBOSE(VB_IMPORTANT, "Slider needs a filename");
        return;
    }

    filename = themepath + filename;

    if (type.toLower() == "fill")
    {
        OSDTypeFillSlider *slider = new OSDTypeFillSlider(
            name, filename, area, wmult, hmult);
        container->AddType(slider);
    }
    else if (type.toLower() == "edit")
    {
        if (altfilename == "")
        {
            VERBOSE(VB_IMPORTANT, "Edit slider needs an altfilename");
            return;
        }

        altfilename = themepath + altfilename;

        OSDTypeEditSlider *tes = new OSDTypeEditSlider(
            name, filename, altfilename, area, wmult, hmult);
        container->AddType(tes);
    }
    else if (type.toLower() == "position")
    {
        OSDTypePosSlider *pos = new OSDTypePosSlider(
            name, filename, area, wmult, hmult);
        container->AddType(pos);
    }
}

void OSD::parseEditArrow(OSDSet *container, QDomElement &element)
{
    container = container;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "editarrow needs a name");
        return;
    }

    if (name != "left" && name != "right")
    {
        VERBOSE(VB_IMPORTANT, "editarrow name needs "
                "to be either 'left' or 'right'");
        return;
    }

    QRect area(0, 0, 0, 0);
    QString filename = "";

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag in editarrow: "
                        << info.tagName());
                return;
            }
        }
    }

    if (filename == "")
    {
        VERBOSE(VB_IMPORTANT, "editarrow needs a filename");
        return;
    }

    editarrowRect = area;

    QString setname = "arrowimage";

    filename = themepath + filename;

    OSDTypeImage *image = new OSDTypeImage(
        setname, filename, QPoint(0, 0), wmult, hmult);

    if (name == "left")
        editarrowleft = image;
    else
        editarrowright = image;
}

void OSD::parsePositionRects(OSDSet *container, QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "positionsrects needs a name");
        return;
    }

    OSDTypePositionRectangle *rects = new OSDTypePositionRectangle(name);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                QRect area = parseRect(getFirstText(info));
                normalizeRect(area);

                rects->AddPosition(area, wmult, hmult);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag in editarrow: "
                        << info.tagName());
                return;
            }
        }
    }

    container->AddType(rects);
}

void OSD::parsePositionImage(OSDSet *container, QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "positionimage needs a name");
        return;
    }

    QString filename = "";
    QPoint scale = QPoint(-1, -1);

    OSDTypePositionImage *image = new OSDTypePositionImage(name);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "position")
            {
                QPoint pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult + xoffset));
                pos.setY((int)(pos.y() * hmult + yoffset));

                image->AddPosition(pos, wmult, hmult);
            }
            else if (info.tagName() == "staticsize")
            {
                scale = parsePoint(getFirstText(info));
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown: %1 in positionimage")
                        .arg(info.tagName()));
                return;
            }
        }
    }

    if (filename != "")
        filename = themepath + filename;

    image->SetStaticSize(scale.x(), scale.y());
    image->Load(filename, wmult, hmult, scale.x(), scale.y());

    container->AddType(image);
}

void OSD::parseListTree(OSDSet *container, QDomElement &element)
{
    QRect   area = QRect(0,0,0,0);
    QRect   listsize = QRect(0,0,0,0);
    int     leveloffset = 0;
    QString fontActive;
    QString fontInactive;
    bool    showArrow = true;
    bool    showScrollArrows = false;
    QColor  grUnselectedBeg(Qt::black);
    QColor  grUnselectedEnd(80,80,80);
    uint    grUnselectedAlpha(100);
    QColor  grSelectedBeg(82,202,56);
    QColor  grSelectedEnd(52,152,56);
    uint    grSelectedAlpha(255);
    int     spacing = 2;
    int     margin = 3;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "listtree needs a name");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "listsize")
            {
                listsize = parseRect(getFirstText(info));
                normalizeRect(listsize);
                listsize.translate(-xoffset, -yoffset);
            }
            else if (info.tagName() == "leveloffset")
            {
                leveloffset = getFirstText(info).toInt();
            }
            else if (info.tagName() == "fcnfont")
            {
                QString fontName = info.attribute("name", "");
                QString fontFcn  = info.attribute("function", "");

                if (fontFcn.toLower() == "active")
                    fontActive = fontName;
                else if (fontFcn.toLower() == "inactive")
                    fontInactive = fontName;
                else
                {
                    VERBOSE(VB_IMPORTANT, "Unknown font function "
                            "for listtree area: " << fontFcn);
                    return;
                }
            }
            else if (info.tagName() == "showarrow")
            {
                if (getFirstText(info).toLower() == "no")
                    showArrow = false;
            }
            else if (info.tagName() == "showscrollarrows")
            {
                if (getFirstText(info).toLower() == "yes")
                    showScrollArrows = true;
            }
            else if (info.tagName() == "gradient")
            {
                if (info.attribute("type","").toLower() == "selected")
                {
                    grSelectedBeg = createColor(info.attribute("start"));
                    grSelectedEnd = createColor(info.attribute("end"));
                    grSelectedAlpha = info.attribute("alpha","255").toUInt();
                }
                else if (info.attribute("type","").toLower() == "unselected")
                {
                    grUnselectedBeg = createColor(info.attribute("start"));
                    grUnselectedEnd = createColor(info.attribute("end"));
                    grUnselectedAlpha = info.attribute("alpha","100").toUInt();
                }
                else
                {
                    VERBOSE(VB_IMPORTANT,
                            "Unknown type for gradient in listtree area");
                    return;
                }

                if (!grSelectedBeg.isValid() || !grSelectedEnd.isValid() ||
                    !grUnselectedBeg.isValid() || !grUnselectedEnd.isValid())
                {
                    VERBOSE(VB_IMPORTANT,
                            "Unknown color for gradient in listtree area");
                    return;
                }

                if (grSelectedAlpha > 255 || grUnselectedAlpha > 255)
                {
                    VERBOSE(VB_IMPORTANT,
                            "Incorrect alpha for gradient in listtree area");
                    return;
                }
            }
            else if (info.tagName() == "spacing") {
                spacing = getFirstText(info).toInt();
            }
            else if (info.tagName() == "margin") {
                margin = getFirstText(info).toInt();
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown tag in listtree area: "
                     << info.tagName());
                return;
            }
        }
    }

    TTFFont *fpActive = GetFont(fontActive);
    if (!fpActive)
    {
        VERBOSE(VB_IMPORTANT, "Unknown font: " << fontActive << " in listtree");
        return;
    }

    TTFFont *fpInactive = GetFont(fontInactive);
    if (!fpInactive)
    {
        VERBOSE(VB_IMPORTANT, "Unknown font: "
                << fontInactive << " in listtree");
        return;
    }

    OSDListTreeType *lb = new OSDListTreeType(
        name, area, listsize, leveloffset, wmult, hmult);
    lb->SetListener(m_listener);
    lb->SetFontActive(fpActive);
    lb->SetFontInactive(fpInactive);
    lb->SetItemRegColor(grUnselectedBeg, grUnselectedEnd, grUnselectedAlpha);
    lb->SetItemSelColor(grSelectedBeg, grSelectedEnd, grSelectedAlpha);
    lb->SetSpacing(spacing);
    lb->SetMargin(margin);

    container->AddType(lb);
}

void OSD::parseContainer(QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Container needs a name");
        return;
    }

    OSDSet *container = GetSet(name);
    if (container)
    {
        VERBOSE(VB_IMPORTANT, "Container: " << name << " already exists");
        return;
    }

    container = new OSDSet(name, true,
                           osdBounds.width(), osdBounds.height(),
                           wmult, hmult, frameint);
    container->SetListener(m_listener);

    QString prio = element.attribute("priority", "");
    if (!prio.isNull() && !prio.isEmpty())
    {
        container->SetPriority(prio.toInt());
    }

    AddSet(container, name);

    QString movement = element.attribute("fademovement", "");
    if (!movement.isNull() && !movement.isEmpty())
    {
        QPoint movefade = parsePoint(movement);
        container->SetFadeMovement((int)(movefade.x() * wmult),
                                   (int)(movefade.y() * hmult));
    }

    QString showwith = element.attribute("showwith", "");
    if (!showwith.isEmpty())
        container->SetShowWith(showwith);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "image")
            {
                parseImage(container, info);
            }
            else if (info.tagName() == "textarea")
            {
                parseTextArea(container, info);
            }
            else if (info.tagName() == "slider")
            {
                parseSlider(container, info);
            }
            else if (info.tagName() == "box")
            {
                parseBox(container, info);
            }
            else if (info.tagName() == "editarrow")
            {
                parseEditArrow(container, info);
            }
            else if (info.tagName() == "positionrects")
            {
                parsePositionRects(container, info);
            }
            else if (info.tagName() == "positionimage")
            {
                parsePositionImage(container, info);
            }
            else if (info.tagName() == "listtreemenu")
            {
                parseListTree(container, info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown container child: %1")
                        .arg(info.tagName()));
                return;
            }
        }
    }
}

bool OSD::LoadTheme(void)
{
    // HACK begin -- needed to address ticket #989
    xoffset = 0;
    yoffset = 0;
    displaywidth  = m_themeinfo->BaseRes()->width();
    displayheight = m_themeinfo->BaseRes()->height();
    hmult = 1.0f;
    wmult = 1.0f;
    // HACK end

    QString themefile = themepath + "/osd.xml";

    QDomDocument doc;
    QFile f(themefile);

    if (!f.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, "OSD::LoadTheme(): Can't open: " << themefile);
        return false;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT, QString("Error parsing: %1\n\t\t\t"
                                      "at line: %2  column: %3\n\t\t\t%4")
                .arg(themefile).arg(errorLine).arg(errorColumn).arg(errorMsg));
        f.close();
        return false;
    }

    f.close();

    QDomElement docElem = doc.documentElement();
    for (QDomNode n = docElem.firstChild(); !n.isNull();
         n = n.nextSibling())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "timeformat")
            {
                timeFormat = getFirstText(e);
                if (timeFormat.toUpper() == "FROMSETTINGS")
                    timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");
            }
            else if (e.tagName() == "fadeaway")
            {
                totalfadetime = (getFirstText(e).toInt() * 1000000) / 30;
            }
            else if (e.tagName() == "font")
            {
                parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "Unknown element: " << e.tagName());
                continue;
            }
        }
    }

    return true;
}

void OSD::normalizeRect(QRect &rect)
{
    rect.setWidth((int)(rect.width() * wmult));
    rect.setHeight((int)(rect.height() * hmult));
    rect.moveTopLeft(QPoint((int)(xoffset + rect.x() * wmult),
                            (int)(yoffset + rect.y() * hmult)));
    rect = rect.normalized();
}

QPoint OSD::parsePoint(QString text)
{
    int x, y;
    QPoint retval(0, 0);
    if (sscanf(text.toAscii().constData(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);
    return retval;
}

QColor OSD::parseColor(QString text)
{
    QColor retval;
    QRegExp regexp("#([0-9a-fA-F]){6}");
    if (regexp.exactMatch(text))
    {
        int val;
        if (sscanf(text.toAscii().constData(), "#%x", &val) == 1)
            retval = QColor((val >> 16) & 0xff, (val >> 8) & 0xff, val & 0xff);
    }
    else
    {
        int r, g, b;
        if (sscanf(text.toAscii().constData(), "%d,%d,%d", &r, &g, &b) == 3)
            retval = QColor(r, g, b);
    }
    return retval;
}

QRect OSD::parseRect(QString text)
{
    int x, y, w, h;
    QRect retval(0, 0, 0, 0);
    if (sscanf(text.toAscii().constData(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
        retval = QRect(x, y, w, h);

    return retval;
}

void OSD::ClearAll(const QString &name)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet(name);
    if (container)
        container->Clear();
}

void OSD::ClearAllText(const QString &name)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet(name);
    if (container)
        container->ClearAllText();
}

void OSD::SetText(const QString &name, InfoMap &infoMap, int length)
{
    HideAllExcept(name);

    if (infoMap.find("chanid") != infoMap.end())
    {
        uint chanid = infoMap["chanid"].toUInt();
        QString iconpath = ChannelUtil::GetIcon(chanid);
        if (!iconpath.isEmpty())
            infoMap["iconpath"] = iconpath;
    }

    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet(name);
    if (container)
    {
        container->SetText(infoMap);
        if (length >= 0)
            container->DisplayFor(length * 1000000);
        else
            container->Display();

        OSDTypeImage *cs = (OSDTypeImage *)container->GetType("channelicon");
        if (cs)
        {
            if ((infoMap.contains("iconpath")) && (infoMap["iconpath"] != ""))
                cs->Load(infoMap["iconpath"], wmult, hmult, 30, 30);
            else
                cs->Load(" ", wmult, hmult, 30, 30);
        }

        OSDTypeImage *cs1 = (OSDTypeImage *)container->GetType("coverart");
        if (cs1)
        {
            if ((infoMap.contains("coverartpath")) && !infoMap["coverartpath"].isEmpty())
                cs1->Load(infoMap["coverartpath"], wmult, hmult, 30, 30);
            else
                cs1->Load(" ", wmult, hmult, 30, 30);
        }

        m_setsvisible = true;
        changed = true;
    }
}

void OSD::SetInfoText(InfoMap infoMap, int length)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("program_info");
    if (container)
    {
        container->SetText(infoMap);

        OSDTypeImage *cs = (OSDTypeImage *)container->GetType("channelicon");
        if (cs)
        {
            if ((infoMap.contains("iconpath")) && (infoMap["iconpath"] != ""))
                cs->Load(infoMap["iconpath"], wmult, hmult, 30, 30);
            else
                cs->Load(" ", wmult, hmult, 30, 30);
        }

        OSDTypeImage *cs1 = (OSDTypeImage *)container->GetType("coverart");
        if (cs1)
        {
            if ((infoMap.contains("coverartpath")) && !infoMap["coverartpath"].isEmpty())
                cs1->Load(infoMap["coverartpath"], wmult, hmult, 30, 30);
            else
                cs1->Load(" ", wmult, hmult, 30, 30);
        }

        container->DisplayFor(length * 1000000);
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::SetInfoText(const QString &text, const QString &subtitle,
                      const QString &desc, const QString &category,
                      const QString &start, const QString &end,
                      const QString &callsign, const QString &iconpath,
                      int length)
{
    QString tmp = category;
    tmp = start;
    tmp = end;

    bool hassubtitle = true;

    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("program_info");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("title");
        if (type)
            type->SetText(text);

        type = (OSDTypeText *)container->GetType("subtitle");
        if (type)
            type->SetText(subtitle);
        else
            hassubtitle = false;

        type = (OSDTypeText *)container->GetType("description");
        if (type)
        {
            if (!hassubtitle && subtitle.length() > 1)
            {
                QString tmpdesc = "\"" + subtitle + "\"";
                if (desc.length() > 1)
                    tmpdesc += ", " + desc;
                type->SetText(tmpdesc);
            }
            else
                type->SetText(desc);
        }
        type = (OSDTypeText *)container->GetType("callsign");
        if (type)
            type->SetText(callsign.left(5));
        OSDTypeImage *cs = (OSDTypeImage *)container->GetType("channelicon");
        if (cs)
            cs->Load(iconpath, wmult, hmult, 30, 30);

        container->DisplayFor(length * 1000000);
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::SetUpOSDClosedHandler(TV *tv)
{
    OSDSet *container = GetSet("status");

    if (container)
        container->SetListener((QObject*)tv);
}

void OSD::ShowStatus(int pos, bool fill, QString msgtext, QString desc,
                     int displaytime, int osdFunctionalType)
{
    struct StatusPosInfo posInfo;

    posInfo.desc = desc;
    posInfo.position = pos;
    posInfo.progBefore = false;
    posInfo.progAfter = false;

    ShowStatus(posInfo, fill, msgtext, displaytime, osdFunctionalType);
}

void OSD::ShowStatus(struct StatusPosInfo posInfo,
                       bool fill, QString msgtext, int displaytime,
                       int osdFunctionalType)
{
    fill = fill;

    HideAllExcept("status");

    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("status");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("status");
        if (type)
            type->SetText(msgtext);
        type = (OSDTypeText *)container->GetType("slidertext");
        if (type)
            type->SetText(posInfo.desc);

        OSDTypeText *exttext = (OSDTypeText *)container->GetType("extendedslidertext");
        if (exttext)
            exttext->SetText(posInfo.extdesc);

        OSDTypeFillSlider *slider =
                      (OSDTypeFillSlider *)container->GetType("statusslider");
        if (slider)
            slider->SetPosition(posInfo.position);

        OSDTypePosSlider *ppos =
                     (OSDTypePosSlider *)container->GetType("statusposition");
        if (ppos)
            ppos->SetPosition(posInfo.position);

        OSDTypeImage *beforeImage =
                      (OSDTypeImage *)container->GetType("progbefore");
        if (beforeImage)
            beforeImage->Hide(!posInfo.progBefore);

        OSDTypeImage *afterImage =
                      (OSDTypeImage *)container->GetType("progafter");
        if (afterImage)
            afterImage->Hide(!posInfo.progAfter);

        if (displaytime > 0)
            container->DisplayFor(displaytime * 1000000, osdFunctionalType);
        else
            container->Display();

        m_setsvisible = true;
        changed = true;
    }
}

void OSD::UpdateStatus(struct StatusPosInfo posInfo)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("status");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("slidertext");
        if (type)
        {
            if (type->GetText() != posInfo.desc)
            {
                type->SetText(posInfo.desc);
                m_setsvisible = true;
                changed = true;
            }
        }

        OSDTypeText *exttext = (OSDTypeText *)container->GetType("extendedslidertext");
        if (exttext)
        {
            if (exttext->GetText() != posInfo.extdesc)
            {
                exttext->SetText(posInfo.extdesc);
                m_setsvisible = true;
                changed = true;
            }
        }

        OSDTypeFillSlider *slider =
                      (OSDTypeFillSlider *)container->GetType("statusslider");
        if (slider)
        {
            if (slider->GetPosition() != posInfo.position)
            {
                slider->SetPosition(posInfo.position);

                m_setsvisible = true;
                changed = true;
            }
        }

        OSDTypePosSlider *ppos =
             (OSDTypePosSlider *)container->GetType("statusposition");
        if (ppos)
        {
            if (ppos->GetPosition() != posInfo.position)
            {
                ppos->SetPosition(posInfo.position);

                m_setsvisible = true;
                changed = true;
            }
        }

        OSDTypeImage *beforeImage =
                      (OSDTypeImage *)container->GetType("progbefore");
        if (beforeImage)
        {
            if (beforeImage->isHidden() != !posInfo.progBefore)
            {
                beforeImage->Hide(!posInfo.progBefore);

                m_setsvisible = true;
                changed = true;
            }
        }

        OSDTypeImage *afterImage =
                      (OSDTypeImage *)container->GetType("progafter");
        if (afterImage)
        {
            if (afterImage->isHidden() != !posInfo.progAfter)
            {
                afterImage->Hide(!posInfo.progAfter);

                m_setsvisible = true;
                changed = true;
            }
        }
    }
}

void OSD::EndStatus(void)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("status");
    if (container)
    {
        container->Hide();
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::SetChannumText(const QString &text, int length)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("channel_number");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("channel_number");
        if (type)
            type->SetText(text);

        container->DisplayFor(length * 1000000);
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::AddCCText(const QString &text, int x, int y, int color,
                    bool teletextmode)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("cc_page");
    if (container)
    {
        OSDTypeCC *ccpage = (OSDTypeCC *)container->GetType("cc_page");
        if (ccpage)
            ccpage->AddCCText(text, x, y, color, teletextmode);

        container->Display();
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::ClearAllCCText()
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("cc_page");
    if (container)
    {
        OSDTypeCC *ccpage = (OSDTypeCC *)container->GetType("cc_page");
        if (ccpage)
        {
            ccpage->ClearAllCCText();
        }

        container->Display(false);
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::UpdateCCText(vector<ccText*> *ccbuf,
                       int replace, int scroll, bool scroll_prsv,
                       int scroll_yoff, int scroll_ymax)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("cc_page");
    if (container)
    {
        OSDTypeCC *ccpage = (OSDTypeCC *)container->GetType("cc_page");
        int visible = 0;

        if (ccpage)
            visible = ccpage->UpdateCCText(ccbuf,
                                           replace, scroll, scroll_prsv,
                                           scroll_yoff, scroll_ymax);

        container->Display(visible);
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::SetCC708Service(const CC708Service *service)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("cc708_page");
    if (!container)
        return;

    OSDType *type = container->GetType("cc708_page");
    OSDType708CC *ccpage = (OSDType708CC*) type;
    if (!ccpage)
        return;

    ccpage->SetCCService(service);
    container->Display(1/*visible*/);
    m_setsvisible = true;
    changed = true;
}

void OSD::CC708Updated(void)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("cc708_page");
    if (!container)
        return;

    OSDType *type = container->GetType("cc708_page");
    OSDType708CC *ccpage = dynamic_cast<OSDType708CC*>(type);
    if (ccpage)
    {
        container->Display(1/*visible*/);
        m_setsvisible = true;
        changed = true;
    }
}

TeletextViewer *OSD::GetTeletextViewer(void)
{
    OSDSet *oset = GetSet("teletext");
    if (!oset)
        return NULL;

    OSDType *traw = oset->GetType("teletext");
    return dynamic_cast<TeletextViewer*>(traw);
}

void OSD::SetSettingsText(const QString &text, int length)
{
    HideAllExcept("settings");

    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet("settings");
    if (container)
    {
        OSDTypeText *type = (OSDTypeText *)container->GetType("settings");
        if (type)
            type->SetText(text);

        container->DisplayFor(length * 1000000);
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::NewDialogBox(const QString &name, const QString &message,
                       QStringList &options, int length,
                       int initial_selection)
{
    if (name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "NewDialogBox, dialog name was empty. "
                "This is a programmer error.");
        return;
    }

    if (IsDialogExisting(name))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("NewDialogBox, dialog '%1' already exists").arg(name));
        return;
    }

    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet(name);
    if (container)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("NewDialogBox, dialog container for "
                        "'%1' already exists.").arg(name));
        return;
    }

    OSDSet *base = GetSet("basedialog");
    if (!base)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Couldn't find base dialog");
        return;
    }

    container = new OSDSet(*base);
    container->SetListener(m_listener);
    container->SetName(name);
    container->SetCache(false);
    container->SetPriority(0);
    container->SetAllowFade(false);
    AddSet(container, name, false);

    OSDTypeText *question = (OSDTypeText *)container->GetType("message");
    if (question)
        question->SetText(message);

    int availoptions = 0;
    OSDTypeText *text = NULL;
    do
    {
        QString optionName = QString("option%1").arg(availoptions + 1);
        text = (OSDTypeText *)container->GetType(optionName);
        if (text)
            availoptions++;
    }
    while (text);

    int numoptions = options.size();

    if (availoptions < numoptions)
    {
        VERBOSE(VB_IMPORTANT, QString("Theme allows %1 options, "
        "menu contains %2 options").arg(availoptions).arg(numoptions));
        return;
    }

    int offset = availoptions - numoptions;
    initial_selection = max(min(numoptions - 1, initial_selection), 0);

    for (int i = 1; i <= numoptions && i <= availoptions; i++)
    {
        QString optionName = QString("option%1").arg(offset + i);
        text = (OSDTypeText *)container->GetType(optionName);
        if (!text)
        {
            VERBOSE(VB_IMPORTANT, "Couldn't find: " << optionName);
            return;
        }

        text->SetText(options[i - 1]);
        text->SetUseAlt(true);
    }

    OSDTypePositionIndicator *opr =
        dynamic_cast<OSDTypePositionIndicator*>(container->GetType("selector"));
    if (!opr)
    {
        VERBOSE(VB_IMPORTANT,
                "Need a positionindicator named 'selector' in the basedialog");
        return;
    }

    opr->SetOffset(offset);
    opr->SetPosition(initial_selection);

    dialogResponseList[name] = initial_selection;

    HighlightDialogSelection(container, offset + initial_selection);

    if (length > 0)
        container->DisplayFor(length * 1000000);
    else
        container->Display();

    m_setsvisible = true;
    changed = true;

    QString tmp = name; tmp.detach();
    dialogs.push_back(tmp);

    locker.mutex()->unlock();

    int count = 0;
    while (!container->HasDisplayed() && count++ < 10)
        usleep(1000);

    locker.mutex()->lock();
}

void OSD::DialogAbortAndHideAll(void)
{
    while (dialogs.size())
    {
        QString dialog_name = dialogs.back();
        DialogAbort(dialog_name);
        TurnDialogOff(dialog_name);
        usleep(1000);
    }
}

void OSD::PushDialog(const QString &name)
{
    QMutexLocker locker(&osdlock);
    QString tmp = name; tmp.detach();
    dialogs.push_back(tmp);
}

QString OSD::GetDialogActive(void) const
{
    QMutexLocker locker(&osdlock);
    if (dialogs.empty())
        return QString::null;

    QString tmp = dialogs.back();
    tmp.detach();
    return tmp;
}

bool OSD::IsDialogActive(const QString &dialogname) const
{
    QMutexLocker locker(&osdlock);
    return !dialogs.empty() && (dialogs.back() == dialogname);
}

bool OSD::IsDialogExisting(const QString &dialogname) const
{
    QMutexLocker locker(&osdlock);
    deque<QString>::const_iterator it =
        find(dialogs.begin(), dialogs.end(), dialogname);
    return it != dialogs.end();
}

void OSD::HighlightDialogSelection(OSDSet *container, int num)
{
    int availoptions = 0;
    OSDTypeText *text = NULL;
    do
    {
       QString name = QString("option%1").arg(availoptions + 1);
       text = (OSDTypeText *)container->GetType(name);
       if (text)
           availoptions++;
    }
    while (text);

    for (int i = 1; i <= availoptions; i++)
    {
        QString name = QString("option%1").arg(i);
        text = (OSDTypeText *)container->GetType(name);
        if (text)
        {
            if (i == num + 1)
                text->SetUseAlt(false);
            else
                text->SetUseAlt(true);
        }
    }
}

void OSD::TurnDialogOff(const QString &raw_name)
{
    QString name = raw_name;

    if (name == kOSDDialogActive)
        name = GetDialogActive();

    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet(name);
    if (container)
    {
        container->Hide();
        changed = true;
    }

    if (dialogs.back() == name)
    {
        dialogs.pop_back();
        return;
    }

    deque<QString>::iterator it =
        find(dialogs.begin(), dialogs.end(), name);
    if (it != dialogs.end())
        dialogs.erase(it);
}

void OSD::DialogUp(const QString &raw_name)
{
    QString name = raw_name;

    if (name == kOSDDialogActive)
        name = GetDialogActive();

    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet(name);
    if (container)
    {
        OSDType *basetype = container->GetType("selector");
        OSDTypePositionIndicator *type =
                            dynamic_cast<OSDTypePositionIndicator*>(basetype);
        if (type)
        {
            type->PositionUp();
            dialogResponseList[name] = type->GetPosition();

            int selected = type->GetPosition() + type->GetOffset();
            HighlightDialogSelection(container, selected);
            changed = true;
        }
    }
}

void OSD::DialogDown(const QString &raw_name)
{
    QString name = raw_name;

    if (name == kOSDDialogActive)
        name = GetDialogActive();

    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet(name);
    if (container)
    {
        OSDType *basetype = container->GetType("selector");
        OSDTypePositionIndicator *type =
                            dynamic_cast<OSDTypePositionIndicator*>(basetype);
        if (type)
        {
            type->PositionDown();
            dialogResponseList[name] = type->GetPosition();

            int selected = type->GetPosition() + type->GetOffset();
            HighlightDialogSelection(container, selected);
            changed = true;
        }
    }
}

bool OSD::DialogShowing(const QString &raw_name)
{
    QString name = raw_name;

    if (name == kOSDDialogActive)
        name = GetDialogActive();

    if (name.isEmpty())
        return false;

    QMutexLocker locker(&osdlock);

    return (GetSet(name) != NULL);
}

void OSD::DialogAbort(const QString &raw_name)
{
    QString name = raw_name;

    if (name == kOSDDialogActive)
        name = GetDialogActive();

    dialogResponseList[name] = -1;
}

int OSD::GetDialogResponse(const QString &raw_name)
{
    QString name = raw_name;

    if (name == kOSDDialogActive)
        name = GetDialogActive();

    if (dialogResponseList.contains(name))
    {
        int ret = dialogResponseList[name] + 1;
        dialogResponseList.remove(name);

        return ret;
    }
    return -1;
}

void OSD::ShowEditArrow(long long number, long long totalframes, int type)
{
    if (!editarrowleft || !editarrowright)
        return;

    char name[128];
    snprintf(name, 128 - 1, "%lld-%d", number, type);

    int pos  = number * 1000 / totalframes;
    int xtmp = (int)(round(editarrowRect.width() * wmult) / 1000.0 * pos);
    int xpos = xtmp + (int)(editarrowRect.left() * wmult);
    int ypos = (int) round(editarrowRect.top() * hmult);

    QMutexLocker locker(&osdlock);

    OSDSet *set = new OSDSet(name, false,
                             osdBounds.width(), osdBounds.height(),
                             wmult, hmult, frameint, xoffset, yoffset);
    set->SetAllowFade(false);
    OSDSet *container = GetSet("editmode");
    if (container)
        set->SetPriority(container->GetPriority() - 1);
    else
        set->SetPriority(4);

    AddSet(set, name, false);

    OSDTypeImage *image;
    if (type == 0)
        image = new OSDTypeImage(*editarrowleft);
    else
        image = new OSDTypeImage(*editarrowright);

    // Reinit needed since in the cache is onle an unscaled version
    image->Reinit(wmult, hmult);

    xpos -= image->ImageSize().width()/2;

    image->SetPosition(QPoint(xpos, ypos), wmult, hmult);

    set->AddType(image);
    set->Display();

    changed = true;
}

void OSD::HideEditArrow(long long number, int type)
{
    char name[128];
    snprintf(name, 128 - 1, "%lld-%d", number, type);

    QMutexLocker locker(&osdlock);

    OSDSet *set = GetSet(name);
    if (set)
        set->Hide();

    changed = true;
}

bool OSD::HideAllExcept(const QString &other)
{
    bool result = false;

    QMutexLocker locker(&osdlock);

    OSDSet *oset = GetSet(other);

    vector<OSDSet *>::iterator i;
    for (i = setList->begin(); i != setList->end(); i++)
        if (*i && (*i)->Displaying())
        {
            QString name = (*i)->GetName();
            if (name != "cc_page" && name != "cc708_page" &&
                name != "menu"    && name != "subtitles"  &&
                name != "interactive" &&
                name != other && (!oset || !oset->CanShowWith(name)))
            {
                (*i)->Hide();
                result = true;
            }
        }

    changed = true;

    return result;
}

bool OSD::HideSet(const QString &name, bool wait)
{
    QMutexLocker locker(&osdlock);

    bool ret = false;

    OSDSet *set = GetSet(name);
    if (set)
    {
        if (set->Displaying())
            ret = true;
        set->Hide();
    }

    changed = true;

    if (set && wait)
    {
        locker.mutex()->unlock();
        while (GetSet(name))
            usleep(10000);
        locker.mutex()->lock();
    }

    return ret;
}

bool OSD::HideSets(QStringList &name)
{
    QMutexLocker locker(&osdlock);

    bool ret = false;

    OSDSet *set;
    QStringList::Iterator i = name.begin();
    for (; i != name.end(); i++)
    {
        set = GetSet(*i);
        if (set)
        {
            if (set->Displaying())
                ret = true;
            set->Hide();
        }
    }

    changed = true;

    return ret;
}

void OSD::UpdateEditText(const QString &seek_amount, const QString &deletemarker,
                         const QString &edittime, const QString &framecnt)
{
    QMutexLocker locker(&osdlock);

    QString name = "editmode";
    OSDSet *set = GetSet(name);
    if (set)
    {
        OSDTypeText *text = (OSDTypeText *)set->GetType("seekamount");
        if (text && seek_amount != QString::null)
            text->SetText(seek_amount);
        text = (OSDTypeText *)set->GetType("cutindicator");
        if (text && deletemarker != QString::null)
            text->SetText(deletemarker);
        text = (OSDTypeText *)set->GetType("timedisplay");
        if (text && edittime != QString::null)
            text->SetText(edittime);
        text = (OSDTypeText *)set->GetType("framedisplay");
        if (text && framecnt != QString::null)
            text->SetText(framecnt);

        set->Display();
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::DoEditSlider(QMap<long long, int> deleteMap, long long curFrame,
                       long long totalFrames)
{
    QMutexLocker locker(&osdlock);

    QString name = "editmode";
    OSDSet *set = GetSet(name);
    if (set)
    {
        QString name = "editslider";
        OSDTypeEditSlider *tes = (OSDTypeEditSlider *)set->GetType(name);
        if (tes)
        {
            tes->ClearAll();

            bool indelete = false;
            int startpos = 0;
            int endpos = 0;
            bool first = true;

            QMap<long long, int>::Iterator i = deleteMap.begin();
            for (; i != deleteMap.end(); ++i)
            {
                long long frame = i.key();
                int direction = *i;

                if (direction == 0 && !indelete && first)
                {
                    startpos = 0;
                    endpos = frame * 1000 / totalFrames;
                    tes->SetRange(startpos, endpos);
                    first = false;
                }
                else if (direction == 0)
                {
                    endpos = frame * 1000 / totalFrames;
                    tes->SetRange(startpos, endpos);
                    indelete = false;
                    first = false;
                }
                else if (direction == 1 && !indelete)
                {
                    startpos = frame * 1000 / totalFrames;
                    indelete = true;
                    first = false;
                }
            }

            if (indelete)
            {
                endpos = 1000;
                tes->SetRange(startpos, endpos);
            }
        }

        name = "editposition";
        OSDTypePosSlider *pos = (OSDTypePosSlider *)set->GetType(name);
        if (pos)
        {
            int num = curFrame * 1000 / totalFrames;
            pos->SetPosition(num);
        }

        set->Display();
        m_setsvisible = true;
        changed = true;
    }
}

void OSD::SetVisible(OSDSet *set, int length)
{
    QMutexLocker locker(&osdlock);

    if (length > 0)
        set->DisplayFor(length * 1000000);
    else
        set->Display();

    m_setsvisible = true;
    changed = true;
}

void OSD::DisableFade(void)
{
    totalfadetime = 0;
}

OSDSurface *OSD::GetDisplaySurface(void)
{
    return drawSurface;
}

OSDSurface *OSD::Display(void)
{
    bool anytodisplay = false;
    if (!setList)
        return NULL;

    bool actuallydraw = false;

    if (changed)
    {
        actuallydraw = true;
        changed = false;
    }

    drawSurface->SetChanged(false);

    //actuallydraw = true;

    if (actuallydraw)
    {
        drawSurface->SetChanged(true);
        drawSurface->ClearUsed();
    }

    vector<OSDSet *> removeList;

    QMutexLocker locker(&osdlock);

    vector<OSDSet *>::iterator i = setList->begin();
    for (; i != setList->end(); i++)
    {
        OSDSet *container = (*i);
        if (container->Displaying())
        {
            OSDTypeText *timedisp = (OSDTypeText *)container->GetType("time");
            if (timedisp)
            {
                QString thetime = QTime::currentTime().toString(timeFormat);
                if (timedisp->GetText() != thetime)
                {
                    timedisp->SetText(thetime);
                    changed = true;
                }
            }

            int fadetime = container->GetFadeTime();
            if (!container->IsFading() && (totalfadetime != fadetime))
                container->SetFadeTime(totalfadetime);

            container->Draw(drawSurface, actuallydraw);
            anytodisplay = true;

            changed |= container->IsFading() || container->NeedsUpdate();
        }
        else if (container->HasDisplayed())
        {
            if (!container->GetCache())
                removeList.push_back(container);
        }
    }

    while (removeList.size() > 0)
    {
        OSDSet *container = removeList.back();
        RemoveSet(container);
        removeList.pop_back();
    }

    locker.unlock();

    m_setsvisible = anytodisplay;

    if (m_setsvisible && !drawSurface->IsClear())
        return drawSurface;

    return NULL;
}

bool OSD::Visible(void)
{
    return m_setsvisible;
}

OSDSet *OSD::GetSet(const QString &text)
{
    OSDSet *ret = NULL;

    if (!text.isEmpty() && setMap.contains(text))
        ret = setMap[text];

    return ret;
}

TTFFont *OSD::GetFont(const QString &text)
{
    QMap<QString,TTFFont*>::iterator it = fontMap.find(text);
    return (it != fontMap.end()) ? *it : NULL;
}

class comp
{
  public:
    bool operator()(const OSDSet *a, const OSDSet *b)
    {
        return (a->GetPriority() > b->GetPriority());
    }
};

void OSD::AddSet(OSDSet *set, QString name, bool withlock)
{
    if (withlock)
        osdlock.lock();

    setMap[name] = set;
    setList->push_back(set);

    sort(setList->begin(), setList->end(), comp());

    if (withlock)
        osdlock.unlock();
}

void OSD::RemoveSet(OSDSet *set)
{
    setMap.remove(set->GetName());
    vector<OSDSet *>::iterator i = setList->begin();
    for (; i != setList->end(); i++)
        if (*i == set)
            break;

    if (i != setList->end())
        setList->erase(i);

    delete set;
}

/* Ken Bass additions for notify_info container */
void OSD::StartNotify(const UDPNotifyOSDSet *notifySet)
{
    if (!notifySet)
        return;

    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet(notifySet->GetName());
    if (!container)
        return;

    notifySet->Lock();

    UDPNotifyOSDSet::const_iterator it = notifySet->begin();

    bool set_visible = false;
    for (; it != notifySet->end(); ++it)
    {
        OSDTypeText *osdtype = (OSDTypeText *)container->GetType(it.key());
        if (osdtype)
        {
            osdtype->SetText(*it);
            set_visible = true;
        }
    }

    notifySet->Unlock();

    if (set_visible)
    {
        uint displaytime = notifySet->GetTimeout();
        if (displaytime > 0)
            container->DisplayFor(displaytime * 1000000);
        else
            container->Display();

        m_setsvisible = true;
        changed = true;
    }
}

void OSD::ClearNotify(const QString &name)
{
    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet(name);
    if (container)
    {
        container->ClearAllText();
        container->Hide();
        m_setsvisible = true;
        changed = true;
    }
}

OSDListTreeType *OSD::ShowTreeMenu(const QString &name,
                                   OSDGenericTree *treeToShow)
{
    if (runningTreeMenu || !treeToShow)
        return NULL;

    OSDListTreeType *rettree = NULL;

    QMutexLocker locker(&osdlock);

    OSDSet *container = GetSet(name);
    if (container)
    {
        rettree = (OSDListTreeType *)container->GetType("menu");
        if (rettree)
        {
            rettree->SetAsTree(treeToShow);
            rettree->SetVisible(true);
            runningTreeMenu = rettree;
            treeMenuContainer = name;
            container->Display();
            m_setsvisible = true;
            changed = true;
        }
    }

    return rettree;
}

bool OSD::IsRunningTreeMenu(void)
{
    if (runningTreeMenu)
        return true;
    return false;
}

bool OSD::TreeMenuHandleKeypress(QKeyEvent *e)
{
    if (!runningTreeMenu)
        return false;

    bool ret = runningTreeMenu->HandleKeypress(e);
    HideTreeMenu();
    changed = true;
    return ret;
}

void OSD::HideTreeMenu(bool hideMenu)
{
    if (runningTreeMenu && hideMenu)
        runningTreeMenu->SetVisible(false);

    QMutexLocker locker(&osdlock);

    if (runningTreeMenu && !runningTreeMenu->IsVisible())
    {
        OSDSet *container = GetSet(treeMenuContainer);
        if (container)
            container->Hide();

        runningTreeMenu = NULL;
    }
}

bool OSD::IsSetDisplaying(const QString &name)
{
    OSDSet *oset = GetSet(name);
    return Visible() && (oset != NULL) && oset->Displaying();
}

bool OSD::HasSet(const QString &name)
{
    return setMap.contains(name);
}

QRect OSD::GetSubtitleBounds()
{
    return QRect(xoffset, yoffset, displaywidth, displayheight);
}

#define OSD_STRDUP(X) strdup(gCoreContext->GetSetting(X).toLocal8Bit().constData())

static void initialize_osd_fonts(void)
{
    QMutexLocker locker(&cc708_init_lock);
    if (cc708_defaults_initialized)
        return;
    cc708_defaults_initialized = true;

    QString default_font_type = gCoreContext->GetSetting(
        "OSDCC708DefaultFontType", "MonoSerif");

    // 0
    cc708_default_font_names[0]  =
        OSD_STRDUP(QString("OSDCC708%1Font").arg(default_font_type));
    cc708_default_font_names[1]  =
        OSD_STRDUP(QString("OSDCC708%1ItalicFont").arg(default_font_type));

    // 1
    cc708_default_font_names[2]  =
        OSD_STRDUP("OSDCC708MonoSerifFont");
    cc708_default_font_names[3]  =
        OSD_STRDUP("OSDCC708MonoSerifItalicFont");

    // 2
    cc708_default_font_names[4]  =
        OSD_STRDUP("OSDCC708PropSerifFont");
    cc708_default_font_names[5]  =
        OSD_STRDUP("OSDCC708PropSerifItalicFont");

    // 3
    cc708_default_font_names[6]  =
        OSD_STRDUP("OSDCC708MonoSansSerifFont");
    cc708_default_font_names[7]  =
        OSD_STRDUP("OSDCC708MonoSansSerifItalicFont");

    // 4
    cc708_default_font_names[8]  =
        OSD_STRDUP("OSDCC708PropSansSerifFont");
    cc708_default_font_names[9]  =
        OSD_STRDUP("OSDCC708PropSansSerifItalicFont");

    // 5
    cc708_default_font_names[10]  =
        OSD_STRDUP("OSDCC708CasualFont");
    cc708_default_font_names[11]  =
        OSD_STRDUP("OSDCC708CasualItalicFont");

    // 6
    cc708_default_font_names[12] =
        OSD_STRDUP("OSDCC708CursiveFont");
    cc708_default_font_names[13] =
        OSD_STRDUP("OSDCC708CursiveItalicFont");

    // 7
    cc708_default_font_names[14] =
        OSD_STRDUP("OSDCC708CapitalsFont");
    cc708_default_font_names[15] =
        OSD_STRDUP("OSDCC708CapitalsItalicFont");
}

#undef OSD_STRDUP

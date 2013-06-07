
#include "util-nvctrl.h"

#include "mythlogging.h"
#include "mythdb.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <cmath>
#include <sstream>
#include <locale>

#include "mythxdisplay.h"

#include "libmythnvctrl/NVCtrl.h"
#include "libmythnvctrl/NVCtrlLib.h"

#ifdef USING_XRANDR
#include "DisplayResX.h"
#endif

#ifdef USING_XRANDR
static unsigned int display_device_mask(char *str);
static void parse_mode_string(char *modeString, char **modeName, int *mask);
static char *find_modeline(char *modeString, char *pModeLines,
                           int ModeLineLen);
static int extract_id_string(char *str);
static int modeline_is_interlaced(char *modeLine);
#endif

#define LOC QString("NVCtrl: ")

int CheckNVOpenGLSyncToVBlank(void)
{
    MythXDisplay *d = OpenMythXDisplay();
    if (!d)
        return -1;

    Display *dpy = d->GetDisplay();
    int screen   = d->GetScreen();

    if (!XNVCTRLIsNvScreen(dpy, screen))
    {
        delete d;
        return -1;
    }

    int major, minor;
    if (!XNVCTRLQueryVersion(dpy, &major, &minor))
        return -1;

    int sync = NV_CTRL_SYNC_TO_VBLANK_OFF;
    if (!XNVCTRLQueryAttribute(dpy, screen, 0, NV_CTRL_SYNC_TO_VBLANK, &sync))
    {
        delete d;
        return -1;
    }

    if (!sync)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenGL Sync to VBlank is disabled.");
        LOG(VB_GENERAL, LOG_WARNING, LOC + "For best results enable this in NVidia settings or try running:");
        LOG(VB_GENERAL, LOG_WARNING, LOC + "nvidia-settings -a \"SyncToVBlank=1\"");
    }

    if (!sync && getenv("__GL_SYNC_TO_VBLANK"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "OpenGL Sync to VBlank enabled via __GL_SYNC_TO_VBLANK.");
        sync = 1;
    }
    else if (!sync)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Alternatively try setting the '__GL_SYNC_TO_VBLANK' environment variable.");
    }

    return sync;
}

int GetNvidiaRates(t_screenrate& screenmap)
{
#ifdef USING_XRANDR

    MythXDisplay *d = OpenMythXDisplay();
    if (!d)
    {
        return -1;
    }
    Display *dpy;
    bool ret;
    int screen, display_devices, mask, major, minor, len, j;
    char *str, *start;
    int nDisplayDevice;
    
    char *pMetaModes, *pModeLines[8], *tmp, *modeString;
    char *modeLine, *modeName;
    int MetaModeLen, ModeLineLen[8];
    int thisMask;
    int id;
    int twinview =  0;
    map<int, map<int,bool> > maprate;

    memset(pModeLines, 0, sizeof(pModeLines));
    memset(ModeLineLen, 0, sizeof(ModeLineLen));

    /*
     * Open a display connection, and make sure the NV-CONTROL X
     * extension is present on the screen we want to use.
     */

    dpy = d->GetDisplay();    
    screen = d->GetScreen();

    if (!XNVCTRLIsNvScreen(dpy, screen))
    {
        LOG(VB_GUI, LOG_INFO,
            QString("The NV-CONTROL X extension is not available on screen %1 "
                    "of '%2'.")
                .arg(screen) .arg(XDisplayName(NULL)));
        delete d;
        return -1;
    }

    ret = XNVCTRLQueryVersion(dpy, &major, &minor);
    if (ret != True)
    {
        LOG(VB_GUI, LOG_INFO,
            QString("The NV-CONTROL X extension does not exist on '%1'.")
                .arg(XDisplayName(NULL)));
        delete d;
        return -1;
    }

    ret = XNVCTRLQueryAttribute(dpy, screen, 0, NV_CTRL_DYNAMIC_TWINVIEW, &twinview);

    if (!ret)
    {
        LOG(VB_GUI, LOG_ERR,
             "Failed to query if Dynamic Twinview is enabled");
        XCloseDisplay(dpy);
        return -1;
    }
    if (!twinview)
    {
        LOG(VB_GUI, LOG_ERR, "Dynamic Twinview not enabled, ignoring");
        delete d;
        return 0;
    }

    /*
     * query the connected display devices on this X screen and print
     * basic information about each X screen
     */

    ret = XNVCTRLQueryAttribute(dpy, screen, 0,
                                NV_CTRL_CONNECTED_DISPLAYS, &display_devices);

    if (!ret)
    {
        LOG(VB_GUI, LOG_ERR,
            "Failed to query the enabled Display Devices.");
        delete d;
        return -1;
    }

    /* first, we query the MetaModes on this X screen */

    ret = XNVCTRLQueryBinaryData(dpy, screen, 0, // n/a
                           NV_CTRL_BINARY_DATA_METAMODES,
                           (unsigned char **)&pMetaModes, &MetaModeLen);
    if (!ret)
    {
        LOG(VB_GUI, LOG_ERR,
            "Failed to query the metamode on selected display device.");
        delete d;
        return -1;
    }

    /*
     * then, we query the ModeLines for each display device on
     * this X screen; we'll need these later
     */

    nDisplayDevice = 0;

    for (mask = 1; mask < (1 << 24); mask <<= 1)
    {
        if (!(display_devices & mask)) continue;

        ret = XNVCTRLQueryBinaryData(dpy, screen, mask,
                               NV_CTRL_BINARY_DATA_MODELINES,
                               (unsigned char **)&str, &len);
        if (!ret)
        {
            LOG(VB_GUI, LOG_ERR,
               "Unknown error. Failed to query the enabled Display Devices.");
            // Free Memory currently allocated
            for (j=0; j < nDisplayDevice; ++j)
            {
                free(pModeLines[j]);
            }
            delete d;
            return -1;
        }

        pModeLines[nDisplayDevice] = str;
        ModeLineLen[nDisplayDevice] = len;

        nDisplayDevice++;
    }

    /* now, parse each MetaMode */
    str = start = pMetaModes;

    for (j = 0; j < MetaModeLen - 1; ++j)
    {
        /*
         * if we found the end of a line, treat the string from
         * start to str[j] as a MetaMode
         */

        if ((str[j] == '\0') && (str[j+1] != '\0'))
        {
            id = extract_id_string(start);
            /*
             * the MetaMode may be preceded with "token=value"
             * pairs, separated by the main MetaMode with "::"; if
             * "::" exists in the string, skip past it
             */

            tmp = strstr(start, "::");
            if (tmp)
            {
                tmp += 2;
            }
            else
            {
                tmp = start;
            }

            /* split the MetaMode string by comma */

            char *strtok_state = NULL;
            for (modeString = strtok_r(tmp, ",", &strtok_state);
                 modeString;
                 modeString = strtok_r(NULL, ",", &strtok_state))
            {
                /*
                 * retrieve the modeName and display device mask
                 * for this segment of the Metamode
                 */

                parse_mode_string(modeString, &modeName, &thisMask);

                /* lookup the modeline that matches */
                nDisplayDevice = 0;
                if (thisMask)
                {
                    for (mask = 1; mask < (1 << 24); mask <<= 1)
                    {
                        if (!(display_devices & mask)) continue;
                        if (thisMask & mask) break;
                        nDisplayDevice++;
                    }
                }

                modeLine = find_modeline(modeName,
                                         pModeLines[nDisplayDevice],
                                         ModeLineLen[nDisplayDevice]);

                if (modeLine && !modeline_is_interlaced(modeLine))
                {
                    int w, h, vfl, hfl, i, irate;
                    double dcl, r;
                    char *buf[9] = {NULL};
                    uint64_t key, key2;

                    // skip name
                    tmp = strchr(modeLine, '"');
                    tmp = strchr(tmp+1, '"') +1 ;
                    while (*tmp == ' ')
                        tmp++;
                    i = 0;
                    for (modeString = strtok_r(tmp, " ", &strtok_state);
                         modeString;
                         modeString = strtok_r(NULL, " ", &strtok_state))
                    {
                        buf[i++] = modeString;

                        if (i == 9) // We're only interested in the first 9 tokens [0 ... 8]
                            break;
                    }

                    if (i < 9)
                    {
                        LOG(VB_GUI, LOG_WARNING, "Invalid modeline string "
                                                      "received, skipping");
                        continue;
                    }

                    w = strtol(buf[1], NULL, 10);
                    h = strtol(buf[5], NULL, 10);
                    vfl = strtol(buf[8], NULL, 10);
                    hfl = strtol(buf[4], NULL, 10);
                    istringstream istr(buf[0]);
                    istr.imbue(locale("C"));
                    istr >> dcl;
                    r = (dcl * 1000000.0) / (vfl * hfl);
                    irate = (int) round(r * 1000.0);
                    key = DisplayResScreen::CalcKey(w, h, (double) id);
                    key2 = DisplayResScreen::CalcKey(w, h, 0.0);
                    // We need to eliminate duplicates, giving priority to the first entries found
                    if (maprate.find(key2) == maprate.end())
                    {
                        // First time we see this resolution, create a map for it
                        maprate[key2] = map<int, bool>();
                    }
                    if ((maprate[key2].find(irate) == maprate[key2].end()) &&
                        (screenmap.find(key) == screenmap.end()))
                    {
                        screenmap[key] = r;
                        maprate[key2][irate] = true;
                    }
                }
                free(modeName);
            }

            /* move to the next MetaMode */
            start = &str[j+1];
        }
    }
    // Free Memory
    for (j=0; j < nDisplayDevice; ++j)
    {
        free(pModeLines[j]);
    }

    delete d;
    return 1;
#else // USING_XRANDR
    return -1;
#endif
}

#ifdef USING_XRANDR
/*
 * display_device_mask() - given a display device name, translate to
 * the display device mask
 */

static unsigned int display_device_mask(char *str)
{
    if (strcmp(str, "CRT-0") == 0) return (1 <<  0);
    if (strcmp(str, "CRT-1") == 0) return (1 <<  1);
    if (strcmp(str, "CRT-2") == 0) return (1 <<  2);
    if (strcmp(str, "CRT-3") == 0) return (1 <<  3);
    if (strcmp(str, "CRT-4") == 0) return (1 <<  4);
    if (strcmp(str, "CRT-5") == 0) return (1 <<  5);
    if (strcmp(str, "CRT-6") == 0) return (1 <<  6);
    if (strcmp(str, "CRT-7") == 0) return (1 <<  7);

    if (strcmp(str, "TV-0") == 0)  return (1 <<  8);
    if (strcmp(str, "TV-1") == 0)  return (1 <<  9);
    if (strcmp(str, "TV-2") == 0)  return (1 << 10);
    if (strcmp(str, "TV-3") == 0)  return (1 << 11);
    if (strcmp(str, "TV-4") == 0)  return (1 << 12);
    if (strcmp(str, "TV-5") == 0)  return (1 << 13);
    if (strcmp(str, "TV-6") == 0)  return (1 << 14);
    if (strcmp(str, "TV-7") == 0)  return (1 << 15);

    if (strcmp(str, "DFP-0") == 0) return (1 << 16);
    if (strcmp(str, "DFP-1") == 0) return (1 << 17);
    if (strcmp(str, "DFP-2") == 0) return (1 << 18);
    if (strcmp(str, "DFP-3") == 0) return (1 << 19);
    if (strcmp(str, "DFP-4") == 0) return (1 << 20);
    if (strcmp(str, "DFP-5") == 0) return (1 << 21);
    if (strcmp(str, "DFP-6") == 0) return (1 << 22);
    if (strcmp(str, "DFP-7") == 0) return (1 << 23);

    return 0;

}



/*
 * parse_mode_string() - extract the modeName and the display device
 * mask for the per-display device MetaMode string in 'modeString'
 */

static void parse_mode_string(char *modeString, char **modeName, int *mask)
{
    char *colon, *s_end, tmp;

    // Skip space
    while (*modeString == ' ')
    {
        modeString++;
    }
    colon = strchr(modeString, ':');

    if (colon)
    {
        *colon = '\0';
        *mask = display_device_mask(modeString);
        *colon = ':';
        modeString = colon + 1;
    }
    else
    {
        *mask = 0;
    }

    // Skip space
    while (*modeString == ' ')
    {
        modeString++;
    }

    /*
     * find the modename; stop at the last ' @' (would be easier with regex)
     */

    s_end = strchr(modeString, '\0');

    for (char *s = modeString; *s; s++)
    {
        if (*s == ' ' && *(s+1) == '@')
            s_end = s;
    } 

    tmp = *s_end;
    *s_end = '\0';
    *modeName = strdup(modeString);
    *s_end = tmp;
}


/*
 * find_modeline() - search the pModeLines list of ModeLines for the
 * mode named 'modeName'; return a pointer to the matching ModeLine,
 * or NULL if no match is found
 */

static char *find_modeline(char *modeName, char *pModeLines, int ModeLineLen)
{
    char *start, *beginQuote, *endQuote;
    int j, match = 0;

    start = pModeLines;

    for (j = 0; j < ModeLineLen; j++)
    {
        if (pModeLines[j] == '\0')
        {
            /*
             * the modeline will contain the modeName in quotes; find
             * the begin and end of the quoted modeName, so that we
             * can compare it to modeName
             */

            beginQuote = strchr(start, '"');
            endQuote = beginQuote ? strchr(beginQuote+1, '"') : NULL;

            if (beginQuote && endQuote)
            {
                *endQuote = '\0';
                match = (strcmp(modeName, beginQuote+1) == 0);
                *endQuote = '"';

                /*
                 * if we found a match, return a pointer to the start
                 * of this modeLine
                 */

                if (match)
                {
                    char *tmp = strstr(start, "::");
                    if (tmp)
                    {
                        tmp += 2;
                    }
                    else
                    {
                        tmp = start;
                    }
                    return tmp;
                }
            }
            start = &pModeLines[j+1];
        }
    }

    return NULL;

}

/*
 * extract_id_string() - Extract the xrandr twinview id from modestr and return it as an int.
 */
static int extract_id_string(char *str)
{
    char *begin, *end;
    int id;

    begin = strstr(str, "id=");
    if (!begin)
    {
        return -1;
    }
    if (!(begin = end = strdup(begin + 3)))
    {
        return -1;
    }
    while (isdigit(*end))
    {
        end++;
    }
    *end = '\0';
    id = atoi(begin);
    free(begin);
    return id;
}

/*
 * modeline_is_interlaced() - return true if the Modeline is Interlaced, false otherwise.
 */
static int modeline_is_interlaced(char *modeLine)
{
    return (strstr(modeLine, "Interlace") != NULL);
}

#endif

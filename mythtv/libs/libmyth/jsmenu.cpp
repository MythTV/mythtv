/*----------------------------------------------------------------------------
**  jsmenu.cpp
**
**  Description:
**      Set of functions to generate key events based on
**  input from a Joystick.
**
**  Original Copyright 2004 by Jeremy White <jwhite@whitesen.org>
**
**  License:
**      This program is free software; you can redistribute it
**  and/or modify it under the terms of the GNU General
**  Public License as published bythe Free Software Foundation;
**  either version 2, or (at your option)
**  any later version.
** 
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
** 
**--------------------------------------------------------------------------*/

#include <qapplication.h>
#include <qevent.h>
#include <qkeysequence.h>
#include <cstdio>
#include <cerrno>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "mythcontext.h"

#include <iostream>
using namespace std;

#include <linux/joystick.h>

#include "jsmenu.h"
#include "jsmenuevent.h"

#if (QT_VERSION < 0x030100)
#error Native LIRC support requires Qt 3.1 or greater.
#endif



/*----------------------------------------------------------------------------
** JoystickMenuClient Constructor
**--------------------------------------------------------------------------*/
JoystickMenuClient::JoystickMenuClient(QObject *main_window)
{
    mainWindow = main_window;

    fd = -1;
    axes = NULL;
    buttons = NULL;

}

/*----------------------------------------------------------------------------
** JoystickMenuClient Destructor
**--------------------------------------------------------------------------*/
JoystickMenuClient::~JoystickMenuClient()
{
    if (fd != -1)
    {
        close(fd);
        fd = -1;
    }

    if (axes)
    {
        free(axes);
        buttons = NULL;
    }

    if (buttons)
    {
        free(buttons);
        buttons = NULL;
    }
}

/*----------------------------------------------------------------------------
** Init
**--------------------------------------------------------------------------*/
int JoystickMenuClient::Init(QString &config_file)
{
    int rc;

    /*------------------------------------------------------------------------
    ** Read the config file
    **----------------------------------------------------------------------*/
    rc = ReadConfig(config_file);
    if (rc)
    {
        VERBOSE(VB_FILE, QString("%1 not found.").arg(config_file));
        VERBOSE(VB_GENERAL, "Joystick disabled.");
        return(rc);
    }

    /*------------------------------------------------------------------------
    ** Open the joystick device, retrieve basic info
    **----------------------------------------------------------------------*/
    fd = open((const char *) devicename, O_RDONLY);
    if (fd == -1)
    {
        cerr << "Could not initialize " << devicename << "\n";
        perror("open");
    }
    
    rc = ioctl(fd, JSIOCGAXES, &axes_count);
    if (rc == -1)
    {
        perror("ioctl JSIOCGAXES");
        return(rc);
    }

    ioctl(fd, JSIOCGBUTTONS, &button_count);
    if (rc == -1)
    {
        perror("ioctl JSIOCGBUTTONS");
        return(rc);
    }

    /*------------------------------------------------------------------------
    ** Allocate the arrays in which we track button and axis status
    **----------------------------------------------------------------------*/
    buttons = new int[button_count];
    memset(buttons, '\0', sizeof(*buttons * button_count));

    axes = new int[axes_count];
    memset(axes, '\0', sizeof(*axes * axes_count));


    return 0;
}

/*----------------------------------------------------------------------------
** ReadConfig
**  Read from a flat file config file, with a format of:
**    # Starts a comment
**    devicename devname            - Name of physical joystick device
**    button num keystring          - Represents a button
**    chord cnum bnum keystring     - A chorded button sequence; hold down cnum
**                                    and press bnum to generate a key
**    axis num from to keystring    - Represents an axis range to trigger a key
**                                    move that axis into the range and the
**                                    keystring is sent
**--------------------------------------------------------------------------*/
int JoystickMenuClient::ReadConfig(QString config_file)
{
    FILE *fp;

    fp = fopen((const char *) config_file, "r");
    if (!fp)
        return(-1);

    QTextIStream istream(fp);
    for (int line = 1; ! istream.atEnd(); line++)
    {
        QString rawline = istream.readLine();
        QString simple_line = rawline.simplifyWhiteSpace(); 
        if (simple_line.isEmpty() || simple_line.startsWith("#"))
            continue;

        QStringList tokens = QStringList::split(" ", simple_line);
        if (tokens.count() < 1)
            continue;

        QString firstTok = tokens[0].lower();

        if (firstTok.startsWith("devicename") && tokens.count() == 2)
            devicename = tokens[1];
        else if (firstTok.startsWith("button") && tokens.count() == 3)
            map.AddButton(tokens[1].toInt(), tokens[2]);
        else if (firstTok.startsWith("axis") && tokens.count() == 5)
            map.AddAxis(tokens[1].toInt(), tokens[2].toInt(), tokens[3].toInt(), tokens[4]);
        else if (firstTok.startsWith("chord") && tokens.count() == 4)
            map.AddButton(tokens[2].toInt(), tokens[3], tokens[1].toInt());
        else
            cerr << config_file << "(" << line << "): unrecognized or malformed line: '" << rawline << "'\n";

    }

    fclose(fp);
    return(0);
}


/*----------------------------------------------------------------------------
** Process
**  This function is intended to run as the mainline of a thread which
** looks for Joystick input and translates it into key stroke events
** for MythTv.
**--------------------------------------------------------------------------*/
void JoystickMenuClient::Process(void)
{
    int rc;

    fd_set readfds;
    struct js_event js;

    while (1)
    {

        /*--------------------------------------------------------------------
        ** Wait for activity from the joy stick (we wait a configurable
        **      poll time)
        **------------------------------------------------------------------*/
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        rc = select(fd + 1, &readfds, NULL, NULL, NULL);
        if (rc == -1)
        {
            /*----------------------------------------------------------------
            ** TODO:  In theory, we could recover from file errors
            **        (what happens when we unplug a joystick?)
            **--------------------------------------------------------------*/
            perror("select");
            return;
        }

        if (rc == 1)
        {
            /*----------------------------------------------------------------
            ** Read a joystick event
            **--------------------------------------------------------------*/
            rc = read(fd, &js, sizeof(js));
            if (rc != sizeof(js))
            {
                    perror("error reading js");
                    return;
            }

            /*----------------------------------------------------------------
            ** Events sent with the JS_EVENT_INIT flag are always sent
            **  right after you open the joy stick; they are useful
            **  for learning the initial state of buttons and axes
            **--------------------------------------------------------------*/
            if (js.type & JS_EVENT_INIT)
            {
                if (js.type & JS_EVENT_BUTTON && js.number < button_count)
                    buttons[js.number] = js.value;

                if (js.type & JS_EVENT_AXIS && js.number < axes_count)
                    axes[js.number] = js.value;
            }
            else
            {
                /*------------------------------------------------------------
                ** Record new button states and look for triggers
                **  that would make us send a key.
                ** Things are a little tricky here; for buttons, we
                **  only act on button up events, not button down
                **  (this lets us implement the chord function).
                ** For axes, we only register a change if the
                **  Joystick moves into the specified range
                **  (that way, we only get one event per joystick
                **  motion).
                **----------------------------------------------------------*/
                if (js.type & JS_EVENT_BUTTON && js.number < button_count)
                {
                    if (js.value == 0 && buttons[js.number] == 1)
                        ButtonUp(js.number);

                    buttons[js.number] = js.value;
                }

                if (js.type & JS_EVENT_AXIS && js.number < button_count)
                {
                    AxisChange(js.number, js.value);
                    axes[js.number] = js.value;
                }

            }

        }

    }

}

/*----------------------------------------------------------------------------
** EmitKey
**  Send an event to the main UI loop with the appropriate keycode
**  (looking up the string using QT)
**--------------------------------------------------------------------------*/
void JoystickMenuClient::EmitKey(QString code)
{
    QKeySequence a(code);

    int keycode = 0;

    // Send a dummy keycode if we couldn't convert the key sequence.
    // This is done so the main code can output a warning for bad
    // mappings.
    if (!a.count())
        QApplication::postEvent(mainWindow, new JoystickKeycodeEvent(code, 
                                keycode, true));

    for (unsigned int i = 0; i < a.count(); i++)
    {
        keycode = a[i];

        QApplication::postEvent(mainWindow, new JoystickKeycodeEvent(code, 
                                keycode, true));
        QApplication::postEvent(mainWindow, new JoystickKeycodeEvent(code, 
                                keycode, false));

    }
}


/*----------------------------------------------------------------------------
** ButtonUp
**  Handle a button up event; this is mildly complicated by
** the support for 'chords'; holding down a button and pushing down
** another can create one type of event.
**--------------------------------------------------------------------------*/
void JoystickMenuClient::ButtonUp(int button)
{
    vector<button_map_type>::iterator bmap;

    /*------------------------------------------------------------------------
    ** Process chords first
    **----------------------------------------------------------------------*/
    for (bmap = map.button_map.begin(); bmap < map.button_map.end(); bmap++)
        if (button == bmap->button && bmap->chord != -1 && buttons[bmap->chord] == 1)
        {
            EmitKey(bmap->keystring);
            buttons[bmap->chord] = 0;
            return;
        }

    /*------------------------------------------------------------------------
    ** Process everything else
    **----------------------------------------------------------------------*/
    for (bmap = map.button_map.begin(); bmap < map.button_map.end(); bmap++)
        if (button == bmap->button && bmap->chord == -1)
            EmitKey(bmap->keystring);
}

/*----------------------------------------------------------------------------
** AxisChange
**  Handle a registerd change in a joystick axis
**--------------------------------------------------------------------------*/
void JoystickMenuClient::AxisChange(int axis, int value)
{
    vector<axis_map_type>::iterator amap;
    for (amap = map.axis_map.begin(); amap < map.axis_map.end(); amap++)
        if (axis == amap->axis)
        {
            /* If we're currently outside the range, and the move is
            **   into the range, then we trigger                        */
            if (axes[axis] < amap->from || axes[axis] > amap->to)
                if (value >= amap->from && value <= amap->to)
                    EmitKey(amap->keystring);
        }
}


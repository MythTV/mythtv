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

// Own header
#include "jsmenu.h"

// QT headers
#include <QCoreApplication>
#include <QEvent>
#include <QKeySequence>
#include <QTextStream>
#include <QStringList>

// C/C++ headers
#include <cstdio>
#include <cerrno>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

// Kernel joystick header
#include <linux/joystick.h>

// Myth headers
#include "mythverbose.h"

// Mythui headers
#include "jsmenuevent.h"

#define LOC QString("JoystickMenuThread: ")
#define LOC_ERROR QString("JoystickMenuThread Error: ")

JoystickMenuThread::JoystickMenuThread(QObject *main_window)
    : QThread(),
      m_mainWindow(main_window), m_devicename(""),
      m_fd(-1),                  m_buttonCount(0),
      m_axesCount(0),            m_buttons(NULL),
      m_axes(NULL),              m_bStop(false)
{
}

JoystickMenuThread::~JoystickMenuThread()
{
    if (m_fd != -1)
    {
        close(m_fd);
        m_fd = -1;
    }

    delete [] m_axes;
    m_axes = NULL;

    delete [] m_buttons;
    m_buttons = NULL;
}

/**
 *  \brief Initialise the class variables with values from the config file
 */
int JoystickMenuThread::Init(QString &config_file)
{
    int rc;

    /*------------------------------------------------------------------------
    ** Read the config file
    **----------------------------------------------------------------------*/
    rc = ReadConfig(config_file);
    if (rc)
    {
        VERBOSE(VB_GENERAL, LOC + QString("Joystick disabled - Failed "
                                                "to read %1")
                                                .arg(config_file));
        return(rc);
    }

    /*------------------------------------------------------------------------
    ** Open the joystick device, retrieve basic info
    **----------------------------------------------------------------------*/
    m_fd = open(qPrintable(m_devicename), O_RDONLY);
    if (m_fd == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERROR + QString("Joystick disabled - Failed "
                                                  "to open device %1")
                                                    .arg(m_devicename));
        return -1;
    }

    rc = ioctl(m_fd, JSIOCGAXES, &m_axesCount);
    if (rc == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERROR + "Joystick disabled - ioctl "
                                          "JSIOCGAXES failed");
        return(rc);
    }

    ioctl(m_fd, JSIOCGBUTTONS, &m_buttonCount);
    if (rc == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERROR + "Joystick disabled - ioctl "
                                          "JSIOCGBUTTONS failed");
        return(rc);
    }

    /*------------------------------------------------------------------------
    ** Allocate the arrays in which we track button and axis status
    **----------------------------------------------------------------------*/
    m_buttons = new int[m_buttonCount];
    memset(m_buttons, '\0', sizeof(*m_buttons * m_buttonCount));

    m_axes = new int[m_axesCount];
    memset(m_axes, '\0', sizeof(*m_axes * m_axesCount));

    VERBOSE(VB_GENERAL, LOC + QString("Initialization of %1 succeeded using "
                                      "config file %2")
                                        .arg(m_devicename)
                                        .arg(config_file));
    return 0;
}

/**
 *  \brief Read from action to key mappings from flat file config file
 *
 *  Config file has the following format:
 *  # Starts a comment
 *  devicename devname            - Name of physical joystick device
 *  button num keystring          - Represents a button
 *  chord cnum bnum keystring     - A chorded button sequence; hold down cnum
 *                                  and press bnum to generate a key
 *  axis num from to keystring    - Represents an axis range to trigger a key
 *                                  move that axis into the range and the
 *                                  keystring is sent
 */
int JoystickMenuThread::ReadConfig(QString config_file)
{
    FILE *fp;

    fp = fopen(qPrintable(config_file), "r");
    if (!fp)
        return(-1);

    QTextStream istream(fp);
    for (int line = 1; ! istream.atEnd(); line++)
    {
        QString rawline = istream.readLine();
        QString simple_line = rawline.simplified();
        if (simple_line.isEmpty() || simple_line.startsWith('#'))
            continue;

        QStringList tokens = simple_line.split(" ");
        if (tokens.count() < 1)
            continue;

        QString firstTok = tokens[0].toLower();

        if (firstTok.startsWith("devicename") && tokens.count() == 2)
            m_devicename = tokens[1];
        else if (firstTok.startsWith("button") && tokens.count() == 3)
            m_map.AddButton(tokens[1].toInt(), tokens[2]);
        else if (firstTok.startsWith("axis") && tokens.count() == 5)
            m_map.AddAxis(tokens[1].toInt(), tokens[2].toInt(),
                          tokens[3].toInt(), tokens[4]);
        else if (firstTok.startsWith("chord") && tokens.count() == 4)
            m_map.AddButton(tokens[2].toInt(), tokens[3], tokens[1].toInt());
        else
            VERBOSE(VB_IMPORTANT, LOC_ERROR + QString("ReadConfig(%1) "
                                        "unrecognized or malformed line "
                                        "\"%2\" ")
                                        .arg(line)
                                        .arg(rawline));

    }

    fclose(fp);
    return(0);
}


/**
 *  \brief This function is the heart of a thread which looks for Joystick
 *         input and translates it into key press events
 */
void JoystickMenuThread::run(void)
{
    int rc;

    fd_set readfds;
    struct js_event js;
    struct timeval timeout;

    while (!m_bStop)
    {

        /*--------------------------------------------------------------------
        ** Wait for activity from the joy stick (we wait a configurable
        **      poll time)
        **------------------------------------------------------------------*/
        FD_ZERO(&readfds);
        FD_SET(m_fd, &readfds);

        // the maximum time select() should wait
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        rc = select(m_fd + 1, &readfds, NULL, NULL, &timeout);
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
            rc = read(m_fd, &js, sizeof(js));
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
                if (js.type & JS_EVENT_BUTTON && js.number < m_buttonCount)
                    m_buttons[js.number] = js.value;

                if (js.type & JS_EVENT_AXIS && js.number < m_axesCount)
                    m_axes[js.number] = js.value;
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
                if (js.type & JS_EVENT_BUTTON && js.number < m_buttonCount)
                {
                    if (js.value == 0 && m_buttons[js.number] == 1)
                        ButtonUp(js.number);

                    m_buttons[js.number] = js.value;
                }

                if (js.type & JS_EVENT_AXIS && js.number < m_axesCount)
                {
                    AxisChange(js.number, js.value);
                    m_axes[js.number] = js.value;
                }

            }

        }

    }

}

/**
 *  \brief Send a keyevent to the main UI loop with the appropriate keycode
 */
void JoystickMenuThread::EmitKey(QString code)
{
    QKeySequence a(code);

    int keycode = 0;

    // Send a dummy keycode if we couldn't convert the key sequence.
    // This is done so the main code can output a warning for bad
    // mappings.
    if (!a.count())
        QCoreApplication::postEvent(m_mainWindow, new JoystickKeycodeEvent(code,
                                keycode, true));

    for (unsigned int i = 0; i < a.count(); i++)
    {
        keycode = a[i];

        QCoreApplication::postEvent(m_mainWindow, new JoystickKeycodeEvent(code,
                                keycode, true));
        QCoreApplication::postEvent(m_mainWindow, new JoystickKeycodeEvent(code,
                                keycode, false));
    }
}


/**
 *  \brief Handle a button up event
 *
 * This is mildly complicated by the support for 'chords'; holding down a
 * button and pushing down another can create one type of event.
 */
void JoystickMenuThread::ButtonUp(int button)
{
    /*------------------------------------------------------------------------
    ** Process chords first
    **----------------------------------------------------------------------*/
    JoystickMap::button_map_t::const_iterator bmap;
    for (bmap = m_map.buttonMap().begin(); bmap != m_map.buttonMap().end();
            ++bmap)
    {
        if (button == bmap->button && bmap->chord != -1
            && m_buttons[bmap->chord] == 1)
        {
            EmitKey(bmap->keystring);
            m_buttons[bmap->chord] = 0;
            return;
        }
    }

    /*------------------------------------------------------------------------
    ** Process everything else
    **----------------------------------------------------------------------*/
    for (bmap = m_map.buttonMap().begin(); bmap != m_map.buttonMap().end();
            ++bmap)
    {
        if (button == bmap->button && bmap->chord == -1)
            EmitKey(bmap->keystring);
    }
}

/**
 *  \brief Handle a registered change in a joystick axis
 */
void JoystickMenuThread::AxisChange(int axis, int value)
{
    JoystickMap::axis_map_t::const_iterator amap;
    for (amap = m_map.axisMap().begin(); amap < m_map.axisMap().end(); ++amap)
    {
        if (axis == amap->axis)
        {
            /* If we're currently outside the range, and the move is
            **   into the range, then we trigger                        */
            if (m_axes[axis] < amap->from || m_axes[axis] > amap->to)
                if (value >= amap->from && value <= amap->to)
                    EmitKey(amap->keystring);
        }
    }
}

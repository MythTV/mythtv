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
#include "devices/jsmenu.h"

// QT headers
#include <QCoreApplication>
#include <QEvent>
#include <QKeySequence>
#include <QTextStream>
#include <QStringList>
#include <QFile>

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
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"

// Mythui headers
#include "jsmenuevent.h"

#if HAVE_LIBUDEV
#include <libudev.h>
#endif

#define LOC QString("JoystickMenuThread: ")

JoystickMenuThread::~JoystickMenuThread()
{
    if (m_fd != -1)
    {
        close(m_fd);
        m_fd = -1;
    }

    delete [] m_axes;
    m_axes = nullptr;

    delete [] m_buttons;
    m_buttons = nullptr;
}

/**
 *  \brief Initialise the class variables with values from the config file
 */
bool JoystickMenuThread::Init(QString &config_file)
{
    /*------------------------------------------------------------------------
    ** Read the config file
    **----------------------------------------------------------------------*/
    if (!ReadConfig(config_file)){
        m_configRead = false;
        return false;
    }
    m_configFile = config_file;
    m_configRead = true;

    /*------------------------------------------------------------------------
    ** Open the joystick device, retrieve basic info
    **----------------------------------------------------------------------*/
    m_fd = open(qPrintable(m_devicename), O_RDONLY);
    if (m_fd == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Joystick disabled - Failed to open device %1")
                                                    .arg(m_devicename));
        m_readError = true;
        // If udev is avaliable we want to return true on read error to start the required loop
#if HAVE_LIBUDEV
        return true;
#else
        return false;
#endif
    }

    int rc = ioctl(m_fd, JSIOCGAXES, &m_axesCount);
    if (rc == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Joystick disabled - ioctl JSIOCGAXES failed");
        return false;
    }

    rc = ioctl(m_fd, JSIOCGBUTTONS, &m_buttonCount);
    if (rc == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Joystick disabled - ioctl JSIOCGBUTTONS failed");
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Controller has %1 axes and %2 buttons")
        .arg(m_axesCount).arg(m_buttonCount));

    /*------------------------------------------------------------------------
    ** Allocate the arrays in which we track button and axis status
    **----------------------------------------------------------------------*/
    m_buttons = new int[m_buttonCount];
    memset(m_buttons, '\0', m_buttonCount * sizeof(*m_buttons));

    m_axes = new int[m_axesCount];
    memset(m_axes, '\0', m_axesCount * sizeof(*m_axes));

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Initialization of %1 succeeded using config file %2")
                                        .arg(m_devicename, config_file));
    m_readError = false;
    return true;
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
bool JoystickMenuThread::ReadConfig(const QString& config_file)
{
    if (!QFile::exists(config_file))
    {
        LOG(VB_GENERAL, LOG_INFO, "No joystick configuration found, not enabling joystick control");
        return false;
    }

    FILE *fp = fopen(qPrintable(config_file), "r");
    if (!fp)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Joystick disabled - Failed to open %1") .arg(config_file));
        return false;
    }

    m_map.Clear();

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
        {
            m_devicename = tokens[1];
        }
        else if (firstTok.startsWith("button") && tokens.count() == 3)
        {
            m_map.AddButton(tokens[1].toInt(), tokens[2]);
        }
        else if (firstTok.startsWith("axis") && tokens.count() == 5)
        {
            m_map.AddAxis(tokens[1].toInt(), tokens[2].toInt(),
                          tokens[3].toInt(), tokens[4]);
        }
        else if (firstTok.startsWith("chord") && tokens.count() == 4)
        {
            m_map.AddButton(tokens[2].toInt(), tokens[3], tokens[1].toInt());
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("ReadConfig(%1) unrecognized or malformed line \"%2\" ")
                                        .arg(line) .arg(rawline));
        }
    }

    fclose(fp);
    return true;
}


/**
 *  \brief This function is the heart of a thread which looks for Joystick
 *         input and translates it into key press events
 */
void JoystickMenuThread::run(void)
{
    RunProlog();

    fd_set readfds;
    struct js_event js {};
    struct timeval timeout {};

    while (!m_bStop && m_configRead)
    {
#if HAVE_LIBUDEV
        if (m_configRead && m_readError)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Joystick error, Awaiting Reconnection"));
            struct udev *udev = udev_new();
            /* Set up a monitor to monitor input devices */
            struct udev_monitor *mon =
                udev_monitor_new_from_netlink(udev, "udev");
            udev_monitor_filter_add_match_subsystem_devtype(mon, "input", nullptr);
            udev_monitor_enable_receiving(mon);
            /* Get the file descriptor (fd) for the monitor.
            This fd will get passed to select() */
            int fd = udev_monitor_get_fd(mon);
            /* This section will run till no error, calling usleep() at
            the end of each pass. This is to use a udev_monitor in a
            non-blocking way. */
            /*===========================================================
             * instead of a loop, could QSocketNotifier be used here
             *=========================================================*/
            while (!m_bStop && m_configRead && m_readError)
            {
                /* Set up the call to select(). In this case, select() will
                   only operate on a single file descriptor, the one
                   associated with our udev_monitor. Note that the timeval
                   object is set to 0, which will cause select() to not
                   block.
                */
                fd_set fds;
                struct timeval tv {};
                FD_ZERO(&fds);
                FD_SET(fd, &fds);
                tv.tv_sec = 0;
                tv.tv_usec = 0;
                int ret = select(fd+1, &fds, nullptr, nullptr, &tv);
                /* Check if our file descriptor has received data. */
                if (ret > 0 && FD_ISSET(fd, &fds))
                {
                    struct udev_device *dev = udev_monitor_receive_device(mon);
                    if (dev)
                    {
                        this->Init(m_configFile);
                        udev_device_unref(dev);
                    }
                }
                usleep(250ms);
            }
            // unref the monitor
            udev_monitor_unref(mon); // Also closes fd.
            udev_unref(udev);
        }
#endif

        /*--------------------------------------------------------------------
        ** Wait for activity from the joy stick (we wait a configurable
        **      poll time)
        **------------------------------------------------------------------*/
        FD_ZERO(&readfds); // NOLINT(readability-isolate-declaration)
        FD_SET(m_fd, &readfds);

        // the maximum time select() should wait
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        int rc = select(m_fd + 1, &readfds, nullptr, nullptr, &timeout);
        if (rc == -1)
        {
            /*----------------------------------------------------------------
            ** TODO:  In theory, we could recover from file errors
            **        (what happens when we unplug a joystick?)
            **--------------------------------------------------------------*/
            LOG(VB_GENERAL, LOG_ERR, "select: " + ENO);
#if HAVE_LIBUDEV
            m_readError = true;
            continue;
#else
            return;
#endif
        }

        if (rc == 1)
        {
            /*----------------------------------------------------------------
            ** Read a joystick event
            **--------------------------------------------------------------*/
            rc = read(m_fd, &js, sizeof(js));
            if (rc != sizeof(js))
            {
                    LOG(VB_GENERAL, LOG_ERR, "error reading js:" + ENO);
#if HAVE_LIBUDEV
            m_readError = true;
            continue;
#else
            return;
#endif
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

    RunEpilog();
}

/**
 *  \brief Send a keyevent to the main UI loop with the appropriate keycode
 */
void JoystickMenuThread::EmitKey(const QString& code)
{
    QKeySequence a(code);

    int key { QKeySequence::UnknownKey };
    Qt::KeyboardModifiers modifiers { Qt::NoModifier };

    // Send a dummy keycode if we couldn't convert the key sequence.
    // This is done so the main code can output a warning for bad
    // mappings.
    if (a.isEmpty())
        QCoreApplication::postEvent(m_mainWindow, new JoystickKeycodeEvent(code,
                                key, Qt::NoModifier, QEvent::KeyPress));

    for (int i = 0; i < a.count(); i++)
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        key       = a[i] & ~(Qt::MODIFIER_MASK);
        modifiers = static_cast<Qt::KeyboardModifiers>(a[i] & Qt::MODIFIER_MASK);
#else
        key       = a[i].key();
        modifiers = a[i].keyboardModifiers();
#endif

        QCoreApplication::postEvent(m_mainWindow, new JoystickKeycodeEvent(code,
                                key, modifiers, QEvent::KeyPress));
        QCoreApplication::postEvent(m_mainWindow, new JoystickKeycodeEvent(code,
                                key, modifiers, QEvent::KeyRelease));
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

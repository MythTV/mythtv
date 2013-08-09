/* -*- Mode: c++ -*-
 * \file dvbdevtree.cpp
 * \brief DVB-S Device Tree Control Classes.
 * \author Copyright (C) 2006, Yeasah Pell
 */

// Std C headers
#include <cstring>
#include <cmath>
#include <unistd.h>

// POSIX headers
#include <sys/time.h>

// Qt headers
#include <QString>

// MythTV headers
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "diseqc.h"
#include "dtvmultiplex.h"
#include "compat.h"

#ifdef USING_DVB
#   include "dvbtypes.h"
#else
#   define SEC_VOLTAGE_13  0
#   define SEC_VOLTAGE_18  1
#   define SEC_VOLTAGE_OFF 2
#   define SEC_MINI_A      0
#   define SEC_MINI_B      1
#endif

// DiSEqC sleep intervals per eutelsat spec
#define DISEQC_SHORT_WAIT     (15 * 1000)
#define DISEQC_LONG_WAIT      (100 * 1000)
#define DISEQC_POWER_OFF_WAIT ((1000 * 1000) - 1)
#define DISEQC_POWER_ON_WAIT  (500 * 1000)

// Number of times to retry ioctls after receiving ETIMEDOUT before giving up
#define TIMEOUT_RETRIES       10
#define TIMEOUT_WAIT          (250 * 1000)

// Framing byte
#define DISEQC_FRM            0xe0
#define DISEQC_FRM_REPEAT     (1 << 0)
#define DISEQC_FRM_REPLY_REQ  (1 << 1)

// Address byte
#define DISEQC_ADR_ALL        0x00
#define DISEQC_ADR_SW_ALL     0x10
#define DISEQC_ADR_LNB        0x11
#define DISEQC_ADR_LNB_SW     0x12
#define DISEQC_ADR_SW_BLK     0x14
#define DISEQC_ADR_SW         0x15
#define DISEQC_ADR_SMATV      0x18
#define DISEQC_ADR_POL_ALL    0x20
#define DISEQC_ADR_POL_LIN    0x21
#define DISEQC_ADR_POS_ALL    0x30
#define DISEQC_ADR_POS_AZ     0x31
#define DISEQC_ADR_POS_EL     0x32

// Command byte
#define DISEQC_CMD_RESET      0x00
#define DISEQC_CMD_CLR_RESET  0x01
#define DISEQC_CMD_WRITE_N0   0x38
#define DISEQC_CMD_WRITE_N1   0x39
#define DISEQC_CMD_WRITE_FREQ 0x58
#define DISEQC_CMD_HALT       0x60
#define DISEQC_CMD_LMT_OFF    0x63
#define DISEQC_CMD_LMT_E      0x66
#define DISEQC_CMD_LMT_W      0x67
#define DISEQC_CMD_DRIVE_E    0x68
#define DISEQC_CMD_DRIVE_W    0x69
#define DISEQC_CMD_STORE_POS  0x6a
#define DISEQC_CMD_GOTO_POS   0x6b
#define DISEQC_CMD_GOTO_X     0x6e

#define TO_RADS (M_PI / 180.0)
#define TO_DEC  (180.0 / M_PI)

#define EPS     1E-4

#define LOC      QString("DiSEqCDevTree: ")

QString DiSEqCDevDevice::TableToString(uint type, const TypeTable *table)
{
    for (; !table->name.isEmpty(); table++)
    {
        if (type == table->value)
        {
            QString tmp = table->name;
            tmp.detach();
            return tmp;
        }
    }
    return QString::null;
}

uint DiSEqCDevDevice::TableFromString(const QString   &type,
                                      const TypeTable *table)
{
    uint first_val = table->value;
    for (; !table->name.isEmpty(); table++)
    {
        if (type == table->name)
            return table->value;
    }
    return first_val;
}

//////////////////////////////////////// DiSEqCDevSettings

/** \class DiSEqCDevSettings
 *  \brief DVB-S device settings class.
 *
 *  Represents a single possible configuration
 *  of a given network of DVB-S devices.
 */

DiSEqCDevSettings::DiSEqCDevSettings()
    : m_input_id((uint) -1)
{
}

/** \fn DiSEqCDevSettings::Load(uint)
 *  \brief Loads configuration chain from DB for specified card input id.
 *  \param card_input_id Desired capture card input ID.
 *  \return True if successful.
 */
bool DiSEqCDevSettings::Load(uint card_input_id)
{
    if (card_input_id == m_input_id)
        return true;

    m_config.clear();

    // load settings from DB
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT diseqcid, value "
        "FROM diseqc_config "
        "WHERE cardinputid = :INPUTID");

    query.bindValue(":INPUTID", card_input_id);
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("DiSEqCDevSettings::Load", query);
        return false;
    }

    while (query.next())
        m_config[query.value(0).toUInt()] = query.value(1).toDouble();

    m_input_id = card_input_id;

    return true;
}

/** \fn DiSEqCDevSettings::Store(uint card_input_id) const
 *  \brief Stores configuration chain to DB for specified card input id.
 *  \param card_input_id Desired capture card input ID.
 *  \return True if successful.
 */
bool DiSEqCDevSettings::Store(uint card_input_id) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    // clear out previous settings
    query.prepare(
        "DELETE from diseqc_config "
        "WHERE cardinputid = :INPUTID");
    query.bindValue(":INPUTID", card_input_id);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("DiSEqCDevSettings::Store 1", query);
        return false;
    }

    // insert new settings
    query.prepare(
        "INSERT INTO diseqc_config "
        "       ( cardinputid, diseqcid, value) "
        "VALUES (:INPUTID,    :DEVID,     :VALUE) ");

    uint_to_dbl_t::const_iterator it = m_config.begin();
    for (; it != m_config.end(); ++it)
    {
        query.bindValue(":INPUTID", card_input_id);
        query.bindValue(":DEVID",   it.key());
        query.bindValue(":VALUE",   *it);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("DiSEqCDevSettings::Store 2", query);
            return false;
        }
    }

    return true;
}

/**
 *  \brief Retrieves a value from this configuration chain by device id.
 *  \param devid Device id.
 *  \return Device scalar value.
 */
double DiSEqCDevSettings::GetValue(uint devid) const
{
    uint_to_dbl_t::const_iterator it = m_config.find(devid);

    if (it != m_config.end())
        return *it;

    return 0.0;
}

/**
 *  \brief Sets a value for this configuration chain by device id.
 *  \param devid Device id.
 *  \param value Device scalar value.
 */
void DiSEqCDevSettings::SetValue(uint devid, double value)
{
    m_config[devid] = value;
    m_input_id = (uint) -1;
}

//////////////////////////////////////// DiSEqCDev

/** \class DiSEqCDev
 *  \brief Main DVB-S device interface.
 */

DiSEqCDevTrees DiSEqCDev::m_trees;

/** \fn DiSEqCDev::FindTree(uint)
 *  \brief Retrieve device tree.
 *  \param cardid Capture card id.
 *  \param fd_frontend DVB frontend device file descriptor.
 */
DiSEqCDevTree *DiSEqCDev::FindTree(uint cardid)
{
    return m_trees.FindTree(cardid);
}

/** \fn DiSEqCDev::InvalidateTrees(void)
 *  \brief Invalidate cached trees.
 */
void DiSEqCDev::InvalidateTrees(void)
{
    m_trees.InvalidateTrees();
}

//////////////////////////////////////// DiSEqCDevTrees

/** \class DiSEqCDevTrees
 *  \brief Static-scoped locked tree list class.
 */

DiSEqCDevTrees::~DiSEqCDevTrees()
{
    InvalidateTrees();
}

/** \fn DiSEqCDevTrees::FindTree(uint)
 *  \brief Retrieve device tree.
 *  \param cardid Capture card id.
 *  \param fd_frontend DVB frontend device file descriptor.
 */
DiSEqCDevTree *DiSEqCDevTrees::FindTree(uint cardid)
{
    QMutexLocker lock(&m_trees_lock);

    cardid_to_diseqc_tree_t::iterator it = m_trees.find(cardid);
    if (it != m_trees.end())
        return *it;

    DiSEqCDevTree *tree = new DiSEqCDevTree;
    tree->Load(cardid);
    m_trees[cardid] = tree;

    return tree;
}

/** \fn DiSEqCDevTrees::InvalidateTrees(void)
 *  \brief Invalidate cached trees.
 */
void DiSEqCDevTrees::InvalidateTrees(void)
{
    QMutexLocker lock(&m_trees_lock);

    cardid_to_diseqc_tree_t::iterator it = m_trees.begin();
    for (; it != m_trees.end(); ++it)
        delete *it;

    m_trees.clear();
}

//////////////////////////////////////// DiSEqCDevTree

/** \class DiSEqCDevTree
 *  \brief DVB-S device tree class. Represents a tree of DVB-S devices.
 */

const uint DiSEqCDevTree::kFirstFakeDiSEqCID = 0xf0000000;

DiSEqCDevTree::DiSEqCDevTree() :
    m_fd_frontend(-1), m_root(NULL),
    m_previous_fake_diseqcid(kFirstFakeDiSEqCID)
{
    Reset();
}

DiSEqCDevTree::~DiSEqCDevTree()
{
    delete m_root;
}

/** \fn DiSEqCDevTree::Load(uint)
 *  \brief Loads the device tree from the database.
 *  \param cardid Capture card id.
 *  \return True if successful.
 */
bool DiSEqCDevTree::Load(uint cardid)
{
    // clear children
    delete m_root;
    m_delete.clear();
    m_root = NULL;

    // lookup configuration for this card
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT diseqcid, cardtype "
        "FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec())
    {
        MythDB::DBError("DiSEqCDevTree::Load", query);
    }
    else if (!query.next())
    {
        return m_root;
    }

    if (query.value(0).toUInt())
    {
        m_root = DiSEqCDevDevice::CreateById(
            *this, query.value(0).toUInt());
    }
    else if (query.value(1).toString().toUpper() == "DVB")
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("No device tree for cardid %1").arg(cardid));
    }

    return m_root;
}

/** \fn DiSEqCDevTree::Exists(uint)
 *  \brief Check if a Diseqc device tree exists.
 *  \param cardid Capture card id.
 *  \return True if exists.
 */
bool DiSEqCDevTree::Exists(int cardid)
{
    // lookup configuration for this card
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT diseqcid "
        "FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec())
    {
        MythDB::DBError("DiSEqCDevTree::Load", query);
    }
    else if (query.next())
    {
        if (query.value(0).toUInt() > 0)
            return true;
    }

    return false;
}

/** \fn DiSEqCDevTree::Store(uint)
 *  \brief Stores the device tree to the database.
 *  \param cardid Capture card id.
 *  \return True if successful.
 */
bool DiSEqCDevTree::Store(uint cardid)
{
    MSqlQuery query0(MSqlQuery::InitCon());

    // apply pending node deletions
    if (!m_delete.empty())
    {
        MSqlQuery query1(MSqlQuery::InitCon());

        query0.prepare(
            "DELETE FROM diseqc_tree "
            "WHERE diseqcid = :DEVID");
        query1.prepare(
            "DELETE FROM diseqc_config "
            "WHERE diseqcid = :DEVID");

        vector<uint>::const_iterator it = m_delete.begin();
        for (; it != m_delete.end(); ++it)
        {
            query0.bindValue(":DEVID", *it);
            if (!query0.exec())
                MythDB::DBError("DiSEqCDevTree::Store 1", query0);

            query1.bindValue(":DEVID", *it);
            if (!query1.exec())
                MythDB::DBError("DiSEqCDevTree::Store 2", query1);

        }
        m_delete.clear();
    }

    // store changed and new nodes
    uint devid = 0;
    if (m_root && m_root->Store())
        devid = m_root->GetDeviceID();
    else if (m_root)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to save DiSEqC tree.");
        return false;
    }

    // update capture card to point to tree, or 0 if there is no tree
    query0.prepare(
        "UPDATE capturecard "
        "SET diseqcid = :DEVID "
        "WHERE cardid = :CARDID");
    query0.bindValue(":DEVID",  devid);
    query0.bindValue(":CARDID", cardid);
    if (!query0.exec())
    {
        MythDB::DBError("DiSEqCDevTree::Store 3", query0);
        return false;
    }

    return true;
}

bool DiSEqCDevTree::SetTone(bool on)
{
    (void) on;

    bool success = false;

#ifdef USING_DVB
    for (uint retry = 0; !success && (retry < TIMEOUT_RETRIES); retry++)
    {
        if (ioctl(m_fd_frontend, FE_SET_TONE,
                  on ? SEC_TONE_ON : SEC_TONE_OFF) == 0)
            success = true;
        else
            usleep(TIMEOUT_WAIT);
    }
#endif // USING_DVB

    if (!success)
        LOG(VB_GENERAL, LOG_ERR, LOC + "FE_SET_TONE failed" + ENO);

    return success;
}

/** \fn DiSEqCDevTree::Execute(const DiSEqCDevSettings&, const DTVMultiplex&)
 *  \brief Applies settings to the entire tree.
 *  \param settings Configuration chain to apply.
 *  \param tuning Tuning parameters.
 *  \return True if execution completed successfully.
 */
bool DiSEqCDevTree::Execute(const DiSEqCDevSettings &settings,
                            const DTVMultiplex      &tuning)
{
    if (!m_root)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No root device tree node!");
        return false;
    }

    // apply any voltage change
    ApplyVoltage(settings, tuning);

    // turn off tone burst first if commands need to be sent
    if (m_root->IsCommandNeeded(settings, tuning))
    {
        SetTone(false);
        usleep(DISEQC_SHORT_WAIT);
    }

    return m_root->Execute(settings, tuning);
}

/** \fn DiSEqCDevTree::Reset(void)
 *  \brief Reset state of nodes in tree, forcing updates on the
 *         next Execute command.
 *  \return True if reset completed successfully.
 */
void DiSEqCDevTree::Reset(void)
{
    if (m_root)
        m_root->Reset();

    m_last_voltage = (uint) -1;
}

/** \fn DiSEqCDevTree::FindRotor(const DiSEqCDevSettings&,uint)
 *  \brief Returns the nth rotor device object in the tree.
 *  \param settings Configuration chain in effect.
 *  \param index 0 for first rotor, 1 for second, etc.
 *  \return Pointer to rotor object if found, NULL otherwise.
 */
DiSEqCDevRotor *DiSEqCDevTree::FindRotor(const DiSEqCDevSettings &settings, uint index)
{
    DiSEqCDevDevice *node  = m_root;
    DiSEqCDevRotor  *rotor = NULL;

    for (uint count = 0; node;)
    {
        rotor = dynamic_cast<DiSEqCDevRotor*>(node);

        if (rotor && (++count > index))
            break;

        node = node->GetSelectedChild(settings);
    }

    return rotor;
}

/** \fn DiSEqCDevTree::FindLNB(const DiSEqCDevSettings&)
 *  \brief Returns the LNB device object selected by the configuration chain.
 *  \param settings Configuration chain in effect.
 *  \return Pointer to LNB object if found, NULL otherwise.
 */
DiSEqCDevLNB *DiSEqCDevTree::FindLNB(const DiSEqCDevSettings &settings)
{
    DiSEqCDevDevice *node = m_root;
    DiSEqCDevLNB    *lnb  = NULL;

    while (node)
    {
        lnb = dynamic_cast<DiSEqCDevLNB*>(node);

        if (lnb)
            break;

        node = node->GetSelectedChild(settings);
    }

    return lnb;
}


/** \fn DiSEqCDevTree::FindDevice(uint)
 *  \brief Returns a device by ID.
 *  \param dev_id Device ID to find.
 *  \return Pointer to device, or NULL if not found in this tree.
 */
DiSEqCDevDevice *DiSEqCDevTree::FindDevice(uint dev_id)
{
    if (m_root)
        return m_root->FindDevice(dev_id);

    return NULL;
}

/** \fn DiSEqCDevTree::SetRoot(DiSEqCDevDevice*)
 *  \brief Changes the root node of the tree.
 *  \param root New root node (may be NULL).
 */
void DiSEqCDevTree::SetRoot(DiSEqCDevDevice *root)
{
    DiSEqCDevDevice *old_root = m_root;

    m_root = root;

    if (old_root)
        delete old_root;
}

#ifdef USING_DVB
static bool send_diseqc(int fd, const dvb_diseqc_master_cmd &cmd)
{
    (void) fd;
    (void) cmd;

    bool success = false;

    for (uint retry = 0; !success && (retry < TIMEOUT_RETRIES); retry++)
    {
        if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd) == 0)
            success = true;
        else
            usleep(TIMEOUT_WAIT);
    }

    if (!success)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "send_diseqc FE_DISEQC_SEND_MASTER_CMD failed" + ENO);
    }

    return success;
}
#endif //USING_DVB

/** \fn DiSEqCDevTree::SendCommand(uint,uint,uint,uint,unsigned char*)
 *  \brief Sends a DiSEqC command.
 *  \param adr DiSEqC destination address.
 *  \param cmd DiSEqC command.
 *  \param repeats Number of times to repeat command.
 *  \param data_len Length of optional data.
 *  \param data Pointer to optional data.
 */
bool DiSEqCDevTree::SendCommand(uint adr, uint cmd, uint repeats,
                                uint data_len, unsigned char *data)
{
    // check payload validity
    if (data_len > 3 || (data_len > 0 && !data))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Bad DiSEqC command");
        return false;
    }

#ifndef USING_DVB

    (void) adr;
    (void) cmd;
    (void) repeats;
    return false;

#else // if USING_DVB

    // prepare command
    dvb_diseqc_master_cmd mcmd;
    mcmd.msg[0] = DISEQC_FRM;
    mcmd.msg[1] = adr;
    mcmd.msg[2] = cmd;
    mcmd.msg_len = data_len + 3;

    if (data_len > 0)
        memcpy(mcmd.msg + 3, data, data_len);

    // diagnostic
    QString cmdstr;
    for (uint byte = 0; byte < mcmd.msg_len; byte++)
        cmdstr += QString("%1 ").arg(mcmd.msg[byte], 2, 16);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Sending DiSEqC Command: " + cmdstr);

    // send the command
    for (uint i = 0; i <= repeats; i++)
    {
        if (!send_diseqc(GetFD(), mcmd))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "DiSEqC command failed" + ENO);
            return false;
        }

        mcmd.msg[0] |= DISEQC_FRM_REPEAT;
        usleep(DISEQC_SHORT_WAIT);
    }

    return true;

#endif // USING_DVB
}

/** \fn DiSEqCDevTree::ResetDiseqc(bool)
 *  \brief Resets the DiSEqC bus.
 *  \param hard_reset If true, the bus will be power cycled.
 *  \return True if successful.
 */
bool DiSEqCDevTree::ResetDiseqc(bool hard_reset)
{
    Reset();

    // power cycle the bus if requested
    // tests show that the wait times required can be very long (~1sec)
    if (hard_reset)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Power-cycling DiSEqC Bus");

        SetVoltage(SEC_VOLTAGE_OFF);
        usleep(DISEQC_POWER_OFF_WAIT);
    }

    // make sure the bus is powered
    SetVoltage(SEC_VOLTAGE_18);
    usleep(DISEQC_POWER_ON_WAIT);
    // some DiSEqC devices need more time. see #8465
    usleep(DISEQC_POWER_ON_WAIT);

    // issue a global reset command
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Resetting DiSEqC Bus");
    if (!SendCommand(DISEQC_ADR_ALL, DISEQC_CMD_RESET))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "DiSEqC reset failed" + ENO);
        return false;
    }

    usleep(DISEQC_LONG_WAIT);

    return true;
}

void DiSEqCDevTree::Open(int fd_frontend)
{
    m_fd_frontend = fd_frontend;

    // issue reset command
    ResetDiseqc(false /* do a soft reset */);
}

bool DiSEqCDevTree::SetVoltage(uint voltage)
{

    if (voltage == m_last_voltage)
        return true;

    int volts = ((voltage == SEC_VOLTAGE_18) ? 18 :
                 ((voltage == SEC_VOLTAGE_13) ? 13 : 0));

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Changing LNB voltage to " +
            QString("%1V").arg(volts));

    bool success = false;

#ifdef USING_DVB
    for (uint retry = 0; !success && retry < TIMEOUT_RETRIES; retry++)
    {
        if (ioctl(m_fd_frontend, FE_SET_VOLTAGE, voltage) == 0)
            success = true;
        else
            usleep(TIMEOUT_WAIT);
    }
#endif // USING_DVB

    if (!success)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "FE_SET_VOLTAGE failed" + ENO);
        return false;
    }

    m_last_voltage = voltage;
    return true;
}

bool DiSEqCDevTree::IsInNeedOfConf(void) const
{
    if (m_root)
        return m_root->GetDeviceType() != DiSEqCDevDevice::kTypeLNB;

    return false;
}

bool DiSEqCDevTree::ApplyVoltage(const DiSEqCDevSettings &settings,
                                 const DTVMultiplex      &tuning)
{
    uint voltage = SEC_VOLTAGE_18;

    if (m_root)
        voltage = m_root->GetVoltage(settings, tuning);

    return SetVoltage(voltage);
}

//////////////////////////////////////// DiSEqCDevDevice

/** \class DiSEqCDevDevice
 *  \brief Represents a node in a DVB-S device network.
 */

const DiSEqCDevDevice::TypeTable DiSEqCDevDevice::dvbdev_lookup[4] =
{
    { "switch",      kTypeSwitch },
    { "rotor",       kTypeRotor  },
    { "lnb",         kTypeLNB    },
    { QString::null, kTypeLNB    },
};


/**
 *  \param tree Parent reference to tree object.
 *  \param devid Device ID of this node.
 */
DiSEqCDevDevice::DiSEqCDevDevice(DiSEqCDevTree &tree, uint devid)
    : m_devid(devid),           m_dev_type(kTypeLNB),
      m_desc(QString::null),    m_tree(tree),
      m_parent(NULL),           m_ordinal(0),
      m_repeat(1)
{
}

DiSEqCDevDevice::~DiSEqCDevDevice()
{
    if (IsRealDeviceID())
        m_tree.AddDeferredDelete(GetDeviceID());
}

DiSEqCDevDevice *DiSEqCDevDevice::FindDevice(uint dev_id)
{
    DiSEqCDevDevice *dev = NULL;

    if (GetDeviceID() == dev_id)
        dev = this;

    uint num_children = GetChildCount();

    for (uint ch = 0; !dev && ch < num_children; ch++)
    {
        DiSEqCDevDevice *child = GetChild(ch);
        if (child)
        {
            if (child->GetDeviceID() == dev_id)
                dev = child;
            else
                dev = child->FindDevice(dev_id);
        }
    }

    return dev;
}

DiSEqCDevDevice *DiSEqCDevDevice::CreateById(DiSEqCDevTree &tree, uint devid)
{
    // load settings from DB
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT type, description "
        "FROM diseqc_tree "
        "WHERE diseqcid = :DEVID");
    query.bindValue(":DEVID", devid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("DiSEqCDevDevice::CreateById", query);
        return NULL;
    }
    else if (!query.next())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "CreateById failed to find dtv dev " +
                QString("%1").arg(devid));

        return NULL;
    }

    dvbdev_t      type = DevTypeFromString(query.value(0).toString());
    QString       desc = query.value(1).toString();
    DiSEqCDevDevice *node = CreateByType(tree, type, devid);

    if (node)
    {
        node->SetDescription(desc);
        node->Load();
    }

    return node;
}

DiSEqCDevDevice *DiSEqCDevDevice::CreateByType(DiSEqCDevTree &tree,
                                               dvbdev_t type,
                                               uint dev_id)
{
    if (!dev_id)
        dev_id = tree.CreateFakeDiSEqCID();

    DiSEqCDevDevice *node = NULL;
    switch (type)
    {
        case kTypeSwitch:
            node = new DiSEqCDevSwitch(tree, dev_id);
            if (node)
                node->SetDescription("Switch");
            break;
        case kTypeRotor:
            node = new DiSEqCDevRotor(tree, dev_id);
            if (node)
                node->SetDescription("Rotor");
            break;
        case kTypeLNB:
            node = new DiSEqCDevLNB(tree, dev_id);
            if (node)
                node->SetDescription("LNB");
            break;
        default:
            break;
    }

    if (node)
        node->SetDeviceType(type);

    return node;
}

/** \fn DiSEqCDevDevice::Execute(const DiSEqCDevSettings&,const DTVMultiplex&)
 *  \brief Applies DiSEqC settings to this node and any children.
 *  \param settings Configuration chain to apply.
 *  \param tuning Tuning parameters.
 *  \return True if execution completed successfully.
 */

/** \fn DiSEqCDevDevice::Reset(void)
 *  \brief Resets to the last known settings for this device.
 *
 *   Device will not actually have commands issued until next Execute() method.
 */

/** \fn DiSEqCDevDevice::IsCommandNeeded(const DiSEqCDevSettings &settings,
 *                                       const DTVMultiplex      &tuning) const
 *  \brief Determines if this device or any child will be sending a command
 *         for the given configuration chain.
 *  \param settings Configuration chain in effect.
 *  \return true if a command would be sent if Execute() were called.
 */

/** \fn DiSEqCDevDevice::GetSelectedChild(const DiSEqCDevSettings&) const
 *  \brief Retrieves the selected child for this configuration, if any.
 *  \param settings Configuration chain in effect.
 *  \return Child node object, or NULL if none.
 */

/** \fn DiSEqCDevDevice::GetChildCount(void) const
 *  \brief Retrieves the proper number of children for this node.
 *  \return Number of children
 */

/** \fn DiSEqCDevDevice::GetChild(uint)
 *  \brief Retrieves the nth child of this node.
 *  \param ordinal Child number (starting at 0).
 *  \return Pointer to device object, or NULL if no child.
 */

/** \fn DiSEqCDevDevice::SetChild(uint,DiSEqCDevDevice*)
 *  \brief Changes the nth child of this node.
 *  \param ordinal Child number (starting at 0).
 *  \param device New child device. (may be NULL)
 *  \return true if object was added to tree.
 */

/** \fn DiSEqCDevDevice::GetVoltage(const DiSEqCDevSettings&,const DTVMultiplex&) const
 *  \brief Retrives the desired voltage for this config.
 *  \param settings Configuration chain in effect.
 *  \param tuning Tuning parameters.
 *  \return Voltage required.
 */

/** \fn DiSEqCDevDevice::Load(void)
 *  \brief Loads this device from the database.
 *  \return True if successful.
 */

/** \fn DiSEqCDevDevice::Store(void) const
 *   Stores this device to the database.
 *  \return True if successful.
 */

/** \fn DiSEqCDevDevice::FindDevice(uint)
 *   Returns a device by ID.
 *  \param dev_id Device ID to find.
 *  \return Pointer to device, or NULL if not found in this tree.
 */

//////////////////////////////////////// DiSEqCDevSwitch

/** \class DiSEqCDevSwitch
 *  \brief Switch class, including tone, legacy and DiSEqC switches.
 */

const DiSEqCDevDevice::TypeTable DiSEqCDevSwitch::SwitchTypeTable[9] =
{
    { "legacy_sw21",  kTypeLegacySW21        },
    { "legacy_sw42",  kTypeLegacySW42        },
    { "legacy_sw64",  kTypeLegacySW64        },
    { "tone",         kTypeTone          },
    { "diseqc",       kTypeDiSEqCCommitted   },
    { "diseqc_uncom", kTypeDiSEqCUncommitted },
    { "voltage",      kTypeVoltage           },
    { "mini_diseqc",  kTypeMiniDiSEqC        },
    { QString::null,  kTypeTone              },
};

DiSEqCDevSwitch::DiSEqCDevSwitch(DiSEqCDevTree &tree, uint devid)
    : DiSEqCDevDevice(tree, devid),
      m_type(kTypeTone), m_address(DISEQC_ADR_SW_ALL),
      m_num_ports(2)
{
    m_children.resize(m_num_ports);

    for (uint i = 0; i < m_num_ports; i++)
        m_children[i] = NULL;

    Reset();
}

DiSEqCDevSwitch::~DiSEqCDevSwitch()
{
    dvbdev_vec_t::iterator it = m_children.begin();
    for (; it != m_children.end(); ++it)
    {
        if (*it)
            delete *it;
    }
}

bool DiSEqCDevSwitch::Execute(const DiSEqCDevSettings &settings,
                              const DTVMultiplex      &tuning)
{
    bool success = true;

    // sanity check switch position
    int pos = GetPosition(settings);
    if (pos < 0)
        return false;

    // perform switching
    if (ShouldSwitch(settings, tuning))
    {
        switch (m_type)
        {
            case kTypeTone:
                success = ExecuteTone(settings, tuning, pos);
                break;
            case kTypeDiSEqCCommitted:
            case kTypeDiSEqCUncommitted:
                success = ExecuteDiseqc(settings, tuning, pos);
                break;
            case kTypeLegacySW21:
            case kTypeLegacySW42:
            case kTypeLegacySW64:
                success = ExecuteLegacy(settings, tuning, pos);
                break;
            case kTypeVoltage:
                success = ExecuteVoltage(settings, tuning, pos);
                break;
            case kTypeMiniDiSEqC:
                success = ExecuteMiniDiSEqC(settings, tuning, pos);
                break;
            default:
                success = false;
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Unknown switch type (%1)").arg((uint)m_type));
                break;
        }

        // if a child device will be sending a diseqc command, wait 100ms
        if (m_children[pos]->IsCommandNeeded(settings, tuning))
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC + "Waiting for switch");
            usleep(DISEQC_LONG_WAIT);
        }

        m_last_pos = pos;
    }

    // chain to child if the switch was successful
    if (success)
        success = m_children[pos]->Execute(settings, tuning);

    return success;
}

void DiSEqCDevSwitch::Reset(void)
{
    m_last_pos = (uint) -1;
    m_last_high_band = (uint) -1;
    m_last_horizontal = (uint) -1;
    dvbdev_vec_t::iterator it = m_children.begin();
    for (; it != m_children.end(); ++it)
    {
        if (*it)
            (*it)->Reset();
    }
}

bool DiSEqCDevSwitch::IsCommandNeeded(const DiSEqCDevSettings &settings,
                                      const DTVMultiplex      &tuning) const
{
    int pos = GetPosition(settings);
    if (pos < 0)
        return false;

    return (ShouldSwitch(settings, tuning) ||
            m_children[pos]->IsCommandNeeded(settings, tuning));
}

DiSEqCDevDevice *DiSEqCDevSwitch::GetSelectedChild(const DiSEqCDevSettings &settings) const
{
    // sanity check switch position
    int pos = GetPosition(settings);
    if (pos < 0)
        return NULL;

    return m_children[pos];
}

uint DiSEqCDevSwitch::GetChildCount(void) const
{
    return m_num_ports;
}

DiSEqCDevDevice *DiSEqCDevSwitch::GetChild(uint ordinal)
{
    if (ordinal < m_children.size())
        return m_children[ordinal];

    return NULL;
}

bool DiSEqCDevSwitch::SetChild(uint ordinal, DiSEqCDevDevice *device)
{
    if (ordinal >= m_children.size())
        return false;

    if (m_children[ordinal])
        delete m_children[ordinal];

    m_children[ordinal] = device;
    if (device)
    {
        device->SetOrdinal(ordinal);
        device->SetParent(this);
    }

    return true;
}

uint DiSEqCDevSwitch::GetVoltage(const DiSEqCDevSettings &settings,
                                 const DTVMultiplex         &tuning) const
{
    uint voltage = SEC_VOLTAGE_18;
    DiSEqCDevDevice *child = GetSelectedChild(settings);

    if (child)
        voltage = child->GetVoltage(settings, tuning);

    return voltage;
}

bool DiSEqCDevSwitch::Load(void)
{
    // clear old children
    dvbdev_vec_t::iterator it = m_children.begin();
    for (; it != m_children.end(); ++it)
    {
        if (*it)
            delete *it;
    }

    m_children.clear();

    // populate switch parameters from db
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT subtype, address, switch_ports, cmd_repeat "
        "FROM diseqc_tree "
        "WHERE diseqcid = :DEVID");
    query.bindValue(":DEVID", GetDeviceID());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("DiSEqCDevSwitch::Load 1", query);
        return false;
    }
    else if (query.next())
    {
        m_type = SwitchTypeFromString(query.value(0).toString());
        m_address = query.value(1).toUInt();
        m_num_ports = query.value(2).toUInt();
        m_repeat = query.value(3).toUInt();
        m_children.resize(m_num_ports);
        for (uint i = 0; i < m_num_ports; i++)
            m_children[i] = NULL;
    }

    // load children from db
    query.prepare(
        "SELECT diseqcid, ordinal "
        "FROM diseqc_tree "
        "WHERE parentid = :DEVID");
    query.bindValue(":DEVID", GetDeviceID());
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("DiSEqCDevSwitch::Load 2", query);
        return false;
    }

    while (query.next())
    {
        uint          child_dev_id = query.value(0).toUInt();
        uint          ordinal      = query.value(1).toUInt();
        DiSEqCDevDevice *child        = CreateById(m_tree, child_dev_id);
        if (child && !SetChild(ordinal, child))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Switch port out of range (%1 > %2)")
                    .arg(ordinal + 1).arg(m_num_ports));
            delete child;
        }
    }

    return true;
}

bool DiSEqCDevSwitch::Store(void) const
{
    QString type = SwitchTypeToString(m_type);
    MSqlQuery query(MSqlQuery::InitCon());

    // insert new or update old
    if (IsRealDeviceID())
    {
        query.prepare(
            "UPDATE diseqc_tree "
            "SET parentid     = :PARENT, "
            "    ordinal      = :ORDINAL, "
            "    type         = 'switch', "
            "    description  = :DESC, "
            "    subtype      = :TYPE, "
            "    address      = :ADDRESS, "
            "    switch_ports = :PORTS, "
            "    cmd_repeat   = :REPEAT "
            "WHERE diseqcid = :DEVID");
        query.bindValue(":DEVID",   GetDeviceID());
    }
    else
    {
        query.prepare(
            "INSERT INTO diseqc_tree"
            " ( parentid,      ordinal,         type,    "
            "   description,   address,         subtype, "
            "   switch_ports,  cmd_repeat )              "
            "VALUES "
            " (:PARENT,       :ORDINAL,         'switch', "
            "  :DESC,         :ADDRESS,         :TYPE,    "
            "  :PORTS,        :REPEAT )");
    }

    if (m_parent)
        query.bindValue(":PARENT", m_parent->GetDeviceID());

    query.bindValue(":ORDINAL", m_ordinal);
    query.bindValue(":DESC",    GetDescription());
    query.bindValue(":ADDRESS", m_address);
    query.bindValue(":TYPE",    type);
    query.bindValue(":PORTS",   m_num_ports);
    query.bindValue(":REPEAT",  m_repeat);

    if (!query.exec())
    {
        MythDB::DBError("DiSEqCDevSwitch::Store", query);
        return false;
    }

    // figure out devid if we did an insert
    if (!IsRealDeviceID())
        SetDeviceID(query.lastInsertId().toUInt());

    // chain to children
    bool success = true;
    for (uint ch = 0; ch < m_children.size(); ch++)
    {
        if (m_children[ch])
            success &= m_children[ch]->Store();
    }

    return success;
}

void DiSEqCDevSwitch::SetNumPorts(uint num_ports)
{
    uint old_num = m_children.size();

    if (old_num > num_ports)
    {
        for (uint ch = num_ports; ch < old_num; ch++)
        {
            if (m_children[ch])
                delete m_children[ch];
        }
        m_children.resize(num_ports);
    }
    else if (old_num < num_ports)
    {
        m_children.resize(num_ports);
        for (uint ch = old_num; ch < num_ports; ch++)
            m_children[ch] = NULL;
    }

    m_num_ports = num_ports;
}

bool DiSEqCDevSwitch::ExecuteLegacy(const DiSEqCDevSettings &settings,
                                    const DTVMultiplex &tuning,
                                    uint pos)
{
    (void) settings;
    (void) tuning;
    (void) pos;

#if defined(USING_DVB) && defined(FE_DISHNETWORK_SEND_LEGACY_CMD)
    static const unsigned char sw21_cmds[]   = { 0x34, 0x65, };
    static const unsigned char sw42_cmds[]   = { 0x46, 0x17, };
    static const unsigned char sw64_v_cmds[] = { 0x39, 0x4b, 0x0d, };
    static const unsigned char sw64_h_cmds[] = { 0x1a, 0x5c, 0x2e, };

    const unsigned char *cmds = NULL;
    unsigned char horizcmd = 0x00;
    uint num_ports = 0;

    // determine polarity from lnb
    bool horizontal = false;
    DiSEqCDevLNB *lnb = m_tree.FindLNB(settings);
    if (lnb)
        horizontal = lnb->IsHorizontal(tuning);

    // get command table for this switch
    switch (m_type)
    {
        case kTypeLegacySW21:
            cmds = sw21_cmds;
            num_ports = 2;
            if (horizontal)
                horizcmd = 0x80;
            break;
        case kTypeLegacySW42:
            cmds = sw42_cmds;
            num_ports = 2;
            break;
        case kTypeLegacySW64:
            if (horizontal)
                cmds = sw64_h_cmds;
            else
                cmds = sw64_v_cmds;
            num_ports = 3;
            break;
        default:
            return false;
    }
    if (num_ports)
        pos %= num_ports;

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Changing to Legacy switch port %1/%2")
            .arg(pos + 1).arg(num_ports));

    // send command
    if (ioctl(m_tree.GetFD(), FE_DISHNETWORK_SEND_LEGACY_CMD,
              cmds[pos] | horizcmd) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "FE_DISHNETWORK_SEND_LEGACY_CMD failed" + ENO);

        return false;
    }

    return true;

#else // !FE_DISHNETWORK_SEND_LEGACY_CMD

    LOG(VB_GENERAL, LOG_ERR, LOC + "You must compile with a newer "
            "version of the linux headers for DishNet Legacy switch support.");
    return false;

#endif // !FE_DISHNETWORK_SEND_LEGACY_CMD
}

#ifdef USING_DVB
static bool set_tone(int fd, fe_sec_tone_mode tone)
{
    (void) fd;
    (void) tone;

    bool success = false;

    for (uint retry = 0; !success && (retry < TIMEOUT_RETRIES); retry++)
    {
        if (ioctl(fd, FE_SET_TONE, tone) == 0)
            success = true;
        else
            usleep(TIMEOUT_WAIT);
    }

    if (!success)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "set_tone failed" + ENO);
    }

    return success;
}
#endif // USING_DVB

#ifdef USING_DVB
static bool set_voltage(int fd, fe_sec_voltage volt)
{
    (void) fd;
    (void) volt;

    bool success = false;

    for (uint retry = 0; !success && (retry < TIMEOUT_RETRIES); retry++)
    {
        if (0 == ioctl(fd, FE_SET_VOLTAGE, volt))
            success = true;
        else
            usleep(TIMEOUT_WAIT);
    }

    if (!success)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "FE_SET_VOLTAGE failed" + ENO);
    }

    return success;
}
#endif // USING_DVB

#ifdef USING_DVB
static bool mini_diseqc(int fd, fe_sec_mini_cmd cmd)
{
    (void) fd;
    (void) cmd;

    bool success = false;

    for (uint retry = 0; !success && (retry < TIMEOUT_RETRIES); retry++)
    {
        if (ioctl(fd, FE_DISEQC_SEND_BURST, cmd) == 0)
            success = true;
        else
            usleep(TIMEOUT_WAIT);
    }

    if (!success)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "mini_diseqc FE_DISEQC_SEND_BURST failed" + ENO);
    }

    return success;
}
#endif // USING_DVB

bool DiSEqCDevSwitch::ExecuteTone(const DiSEqCDevSettings &/*settings*/,
                                  const DTVMultiplex &/*tuning*/,
                                  uint pos)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Changing to Tone switch port " +
            QString("%1/2").arg(pos + 1));

#ifdef USING_DVB
    if (set_tone(m_tree.GetFD(), (0 == pos) ? SEC_TONE_OFF : SEC_TONE_ON))
        return true;
#endif // USING_DVB

    LOG(VB_GENERAL, LOG_ERR, LOC + "Setting Tone Switch failed." + ENO);
    return false;
}

bool DiSEqCDevSwitch::ExecuteVoltage(const DiSEqCDevSettings &settings,
                                     const DTVMultiplex &tuning, uint pos)
{
    (void) settings;
    (void) tuning;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Changing to Voltage Switch port " +
            QString("%1/2").arg(pos + 1));

#ifdef USING_DVB
    if (set_voltage(m_tree.GetFD(),
                    (0 == pos) ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18))
    {
        return true;
    }
#endif // USING_DVB

    LOG(VB_GENERAL, LOG_ERR, LOC + "Setting Voltage Switch failed." + ENO);

    return false;
}

bool DiSEqCDevSwitch::ExecuteMiniDiSEqC(const DiSEqCDevSettings &settings,
                                        const DTVMultiplex &tuning, uint pos)
{
    (void) settings;
    (void) tuning;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Changing to MiniDiSEqC Switch port " +
            QString("%1/2").arg(pos + 1));

#ifdef USING_DVB
    if (mini_diseqc(m_tree.GetFD(), (0 == pos) ? SEC_MINI_A : SEC_MINI_B))
        return true;
#endif // USING_DVB

    LOG(VB_GENERAL, LOG_ERR, LOC + "Setting Mini DiSEqC Switch failed." + ENO);

    return false;
}

bool DiSEqCDevSwitch::ShouldSwitch(const DiSEqCDevSettings &settings,
                                   const DTVMultiplex &tuning) const
{
    int pos = GetPosition(settings);
    if (pos < 0)
        return false;

    // committed switch should change for band and polarity as well
    if (kTypeDiSEqCCommitted == m_type)
    {
        // retrieve LNB info
        bool high_band  = false;
        bool horizontal = false;
        DiSEqCDevLNB *lnb  = m_tree.FindLNB(settings);
        if (lnb)
        {
            high_band   = lnb->IsHighBand(tuning);
            horizontal  = lnb->IsHorizontal(tuning);
        }

        if(high_band != m_last_high_band ||
           horizontal != m_last_horizontal)
            return true;
    }
    else if (kTypeLegacySW42 == m_type ||
             kTypeLegacySW64 == m_type)
    {
        // retrieve LNB info
        bool horizontal = false;
        DiSEqCDevLNB *lnb  = m_tree.FindLNB(settings);
        if (lnb)
            horizontal  = lnb->IsHorizontal(tuning);

        if (horizontal != m_last_horizontal)
            return true;
    }
    else if (kTypeVoltage == m_type ||
             kTypeTone == m_type)
        return true;

    return m_last_pos != (uint)pos;
}

bool DiSEqCDevSwitch::ExecuteDiseqc(const DiSEqCDevSettings &settings,
                                    const DTVMultiplex &tuning,
                                    uint pos)
{
    // retrieve LNB info
    bool high_band  = false;
    bool horizontal = false;
    DiSEqCDevLNB *lnb  = m_tree.FindLNB(settings);
    if (lnb)
    {
        high_band   = lnb->IsHighBand(tuning);
        horizontal  = lnb->IsHorizontal(tuning);
    }

    // check number of ports
    if (((kTypeDiSEqCCommitted   == m_type) && (m_num_ports > 4)) ||
        ((kTypeDiSEqCUncommitted == m_type) && (m_num_ports > 16)))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid number of ports for DiSEqC 1.x Switch (%1)")
                .arg(m_num_ports));
        return false;
    }

    // build command
    uint          cmd  = DISEQC_CMD_WRITE_N1;
    unsigned char data = pos;
    if (kTypeDiSEqCUncommitted != m_type)
    {
        cmd  = DISEQC_CMD_WRITE_N0;
        data = ((pos << 2) | (horizontal ? 2 : 0) | (high_band  ? 1 : 0));
    }
    data |= 0xf0;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Changing to DiSEqC switch port " +
            QString("%1/%2").arg(pos + 1).arg(m_num_ports));

    bool ret = m_tree.SendCommand(m_address, cmd, m_repeat, 1, &data);
    if(ret)
    {
        m_last_high_band = high_band;
        m_last_horizontal = horizontal;
    }
    return ret;
}

int DiSEqCDevSwitch::GetPosition(const DiSEqCDevSettings &settings) const
{
    int pos = (int) settings.GetValue(GetDeviceID());

    if (pos >= (int)m_num_ports)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Port %1 ").arg(pos + 1) +
                QString("is not in range [0..%1)").arg(m_num_ports));

        return -1;
    }

    if ((pos >= 0) && !m_children[pos])
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Port %1 ").arg(pos + 1) +
                "has no connected devices configured.");

        return -1;
    }

    return pos;
}

//////////////////////////////////////// DiSEqCDevRotor

static double GetCurTimeFloating(void)
{
    struct timeval curtime;
    gettimeofday(&curtime, NULL);
    return (double)curtime.tv_sec + (((double)curtime.tv_usec) / 1000000);
}

/** \class DiSEqCDevRotor
 *  \brief Rotor class.
 */

const DiSEqCDevDevice::TypeTable DiSEqCDevRotor::RotorTypeTable[] =
{
    { "diseqc_1_2", kTypeDiSEqC_1_2 },
    { "diseqc_1_3", kTypeDiSEqC_1_3 },
    { NULL, kTypeDiSEqC_1_3 }
};

DiSEqCDevRotor::DiSEqCDevRotor(DiSEqCDevTree &tree, uint devid)
    : DiSEqCDevDevice(tree, devid),
      m_type(kTypeDiSEqC_1_3),
      m_speed_hi(2.5),          m_speed_lo(1.9),
      m_child(NULL),
      m_last_position(0.0),     m_desired_azimuth(0.0),
      m_reset(true),            m_move_time(0.0),
      m_last_pos_known(false),  m_last_azimuth(0.0)
{
    Reset();
}

DiSEqCDevRotor::~DiSEqCDevRotor()
{
    if (m_child)
        delete m_child;
}

bool DiSEqCDevRotor::Execute(const DiSEqCDevSettings &settings,
                             const DTVMultiplex &tuning)
{
    bool success = true;

    double position = settings.GetValue(GetDeviceID());
    if (m_reset || (position != m_last_position))
    {
        switch (m_type)
        {
            case kTypeDiSEqC_1_2:
                success = ExecuteRotor(settings, tuning, position);
                break;
            case kTypeDiSEqC_1_3:
                success = ExecuteUSALS(settings, tuning, position);
                break;
            default:
                success = false;
                LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown rotor type " +
                        QString("(%1)").arg((uint) m_type));
                break;
        }

        m_last_position = position;
        m_reset = false;
        if (success)
            // prevent tuning paramaters overiding rotor parameters
            usleep(DISEQC_LONG_WAIT);
    }

    // chain to child
    if (success && m_child)
        success = m_child->Execute(settings, tuning);

    return success;
}

void DiSEqCDevRotor::Reset(void)
{
    m_reset = true;
    if (m_child)
        m_child->Reset();
}

bool DiSEqCDevRotor::IsCommandNeeded(const DiSEqCDevSettings &settings,
                                     const DTVMultiplex         &tuning) const
{
    double position = settings.GetValue(GetDeviceID());

    if (m_reset || (position != m_last_position))
        return true;

    if (m_child)
        return m_child->IsCommandNeeded(settings, tuning);

    return false;
}

DiSEqCDevDevice *DiSEqCDevRotor::GetSelectedChild(const DiSEqCDevSettings&) const
{
    return m_child;
}

bool DiSEqCDevRotor::SetChild(uint ordinal, DiSEqCDevDevice *device)
{
    if (ordinal)
        return false;

    DiSEqCDevDevice *old_child = m_child;
    m_child = NULL;
    if (old_child)
        delete old_child;

    m_child = device;
    if (m_child)
    {
        m_child->SetOrdinal(ordinal);
        m_child->SetParent(this);
    }

    return true;
}

bool DiSEqCDevRotor::IsMoving(const DiSEqCDevSettings &settings) const
{
    double position = settings.GetValue(GetDeviceID());
    double completed = GetProgress();
    bool   moving   = (completed < 1.0) || (position != m_last_position);

    return (m_last_pos_known && moving);
}

uint DiSEqCDevRotor::GetVoltage(const DiSEqCDevSettings &settings,
                                const DTVMultiplex         &tuning) const
{
    // override voltage if the last position is known and the rotor is moving
    if (IsMoving(settings))
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            "Overriding voltage to 18V for faster rotor movement");
    }
    else if (m_child)
    {
        return m_child->GetVoltage(settings, tuning);
    }

    return SEC_VOLTAGE_18;
}

bool DiSEqCDevRotor::Load(void)
{
    // populate switch parameters from db
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT subtype,         rotor_positions, "
        "       rotor_hi_speed,  rotor_lo_speed, "
        "       cmd_repeat "
        "FROM diseqc_tree "
        "WHERE diseqcid = :DEVID");
    query.bindValue(":DEVID", GetDeviceID());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("DiSEqCDevRotor::Load 1", query);
        return false;
    }
    else if (query.next())
    {
        m_type     = RotorTypeFromString(query.value(0).toString());
        m_speed_hi = query.value(2).toDouble();
        m_speed_lo = query.value(3).toDouble();
        m_repeat   = query.value(4).toUInt();

        // form of "angle1=index1:angle2=index2:..."
        QString positions = query.value(1).toString();
        QStringList pos = positions.split(":", QString::SkipEmptyParts);
        QStringList::const_iterator it = pos.begin();
        for (; it != pos.end(); ++it)
        {
            const QStringList eq = (*it).split("=", QString::SkipEmptyParts);
            if (eq.size() == 2)
                m_posmap[eq[0].toFloat()] = eq[1].toUInt();
        }
    }

    // load children from db
    if (m_child)
    {
        delete m_child;
        m_child = NULL;
    }

    query.prepare(
        "SELECT diseqcid "
        "FROM diseqc_tree "
        "WHERE parentid = :DEVID");
    query.bindValue(":DEVID", GetDeviceID());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("DiSEqCDevRotor::Load 2", query);
        return false;
    }
    else if (query.next())
    {
        uint child_dev_id = query.value(0).toUInt();
        SetChild(0, CreateById(m_tree, child_dev_id));
    }

    return true;
}

bool DiSEqCDevRotor::Store(void) const
{
    QString posmap = "";
    QString type   = RotorTypeToString(m_type);

    if (!m_posmap.empty())
    {
        QStringList pos;

        dbl_to_uint_t::const_iterator it = m_posmap.begin();
        for (; it != m_posmap.end(); ++it)
            pos.push_back(QString("%1=%2").arg(it.key()).arg(*it));

        posmap = pos.join(":");
    }

    MSqlQuery query(MSqlQuery::InitCon());

    // insert new or update old
    if (IsRealDeviceID())
    {
        query.prepare(
            "UPDATE diseqc_tree "
            "SET parentid        = :PARENT,  "
            "    ordinal         = :ORDINAL, "
            "    type            = 'rotor',  "
            "    description     = :DESC,    "
            "    subtype         = :TYPE,    "
            "    rotor_hi_speed  = :HISPEED, "
            "    rotor_lo_speed  = :LOSPEED, "
            "    rotor_positions = :POSMAP,  "
            "    cmd_repeat      = :REPEAT   "
            "WHERE diseqcid = :DEVID");
        query.bindValue(":DEVID",   GetDeviceID());
    }
    else
    {
        query.prepare(
            "INSERT INTO diseqc_tree "
            " ( parentid,       ordinal,         type,   "
            "   description,    subtype,         rotor_hi_speed, "
            "   rotor_lo_speed, rotor_positions, cmd_repeat ) "
            "VALUES "
            " (:PARENT,         :ORDINAL,        'rotor',  "
            "  :DESC,           :TYPE,           :HISPEED, "
            "  :LOSPEED,        :POSMAP,         :REPEAT )");
    }

    if (m_parent)
        query.bindValue(":PARENT", m_parent->GetDeviceID());

    query.bindValue(":ORDINAL", m_ordinal);
    query.bindValue(":DESC",    GetDescription());
    query.bindValue(":TYPE",    type);
    query.bindValue(":HISPEED", m_speed_hi);
    query.bindValue(":LOSPEED", m_speed_lo);
    query.bindValue(":POSMAP",  posmap);
    query.bindValue(":REPEAT",  m_repeat);

    if (!query.exec())
    {
        MythDB::DBError("DiSEqCDevRotor::Store", query);
        return false;
    }

    // figure out devid if we did an insert
    if (!IsRealDeviceID())
        SetDeviceID(query.lastInsertId().toUInt());

    // chain to child
    if (m_child)
        return m_child->Store();

    return true;
}

/** \fn DiSEqCDevRotor::GetProgress(void) const
 *  \brief Returns an indication of rotor progress.
 *  \return Scale from 0.0..1.0 indicating percentage complete of
 *          current move. A value of 1.0 indicates motion is complete.
 */
double DiSEqCDevRotor::GetProgress(void) const
{
    if (m_move_time == 0.0)
        return 1.0;

    // calculate duration of move
    double speed    = ((m_tree.GetVoltage() == SEC_VOLTAGE_18) ?
                       m_speed_hi : m_speed_lo);
    double change   = abs(m_desired_azimuth - m_last_azimuth);
    double duration = change / speed;

    // determine completion percentage
    double time_since_move = GetCurTimeFloating() - m_move_time;
    double completed = time_since_move / duration;
    if(completed > 1.0)
    {
        RotationComplete();
        completed = 1.0;
    }

    return completed;
}

/** \fn DiSEqCDevRotor::IsPositionKnown(void) const
 *  \brief Returns true if there is reasonable confidence in the
 *         value returned by GetProgress().
 *
 *   If this returns false, GetProgress() returns progress toward
 *   the time when the position will be approximately known.
 */
bool DiSEqCDevRotor::IsPositionKnown(void) const
{
    return m_last_pos_known;
}

uint_to_dbl_t DiSEqCDevRotor::GetPosMap(void) const
{
    uint_to_dbl_t inv_posmap;
    dbl_to_uint_t::const_iterator it;
    for (it = m_posmap.begin(); it != m_posmap.end(); ++it)
        inv_posmap[*it] = it.key();

    return inv_posmap;
}

void DiSEqCDevRotor::SetPosMap(const uint_to_dbl_t &inv_posmap)
{
    m_posmap.clear();

    uint_to_dbl_t::const_iterator it;
    for (it = inv_posmap.begin(); it != inv_posmap.end(); ++it)
        m_posmap[*it] = it.key();
}

bool DiSEqCDevRotor::ExecuteRotor(const DiSEqCDevSettings&, const DTVMultiplex&,
                                  double angle)
{
    // determine stored position from position map
    dbl_to_uint_t::const_iterator it = m_posmap.lowerBound(angle - EPS);
    unsigned char index = (uint) angle;
    if (it != m_posmap.end())
    {
        index = *it;
        StartRotorPositionTracking(CalculateAzimuth(angle));
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Rotor - " +
            QString("Goto Stored Position %1").arg(index));

    return m_tree.SendCommand(DISEQC_ADR_POS_AZ, DISEQC_CMD_GOTO_POS,
                              m_repeat, 1, &index);
}

bool DiSEqCDevRotor::ExecuteUSALS(const DiSEqCDevSettings&, const DTVMultiplex&,
                                  double angle)
{
    double azimuth = CalculateAzimuth(angle);
    StartRotorPositionTracking(azimuth);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "USALS Rotor - " +
            QString("Goto %1 (Azimuth %2)").arg(angle).arg(azimuth));

    uint az16 = (uint) (abs(azimuth) * 16.0);
    unsigned char cmd[2];
    cmd[0] = ((azimuth > 0.0) ? 0xE0 : 0xD0) | ((az16 >> 8)  &0x0f);
    cmd[1] = (az16  &0xff);

    return m_tree.SendCommand(DISEQC_ADR_POS_AZ, DISEQC_CMD_GOTO_X,
                              m_repeat, 2, cmd);
}

double DiSEqCDevRotor::CalculateAzimuth(double angle) const
{
    // Azimuth Calculation references:
    // http://engr.nmsu.edu/~etti/3_2/3_2e.html
    // http://www.angelfire.com/trek/ismail/theory.html

    // Earth Station Latitude and Longitude in radians
    double P  = gCoreContext->GetSetting("Latitude",  "").toFloat() * TO_RADS;
    double Ue = gCoreContext->GetSetting("Longitude", "").toFloat() * TO_RADS;

    // Satellite Longitude in radians
    double Us = angle * TO_RADS;

    return TO_DEC * atan( tan(Us - Ue) / sin(P) );
}

double DiSEqCDevRotor::GetApproxAzimuth(void) const
{
    if (m_move_time == 0.0)
        return m_last_azimuth;

    double change = m_desired_azimuth - m_last_azimuth;
    return m_last_azimuth + (change * GetProgress());
}

void DiSEqCDevRotor::StartRotorPositionTracking(double azimuth)
{
    // save time and angle of this command
    m_desired_azimuth = azimuth;

    // set last to approximate current position (or worst case if unknown)
    if (m_last_pos_known || m_move_time > 0.0)
        m_last_azimuth = GetApproxAzimuth();
    else
        m_last_azimuth = azimuth > 0.0 ? -75.0 : 75.0;

    m_move_time = GetCurTimeFloating();
}

void DiSEqCDevRotor::RotationComplete(void) const
{
    m_move_time = 0.0;
    m_last_pos_known = true;
    m_last_azimuth = m_desired_azimuth;
}

////////////////////////////////////////

/** \class DiSEqCDevLNB
 *  \brief LNB Class.
 */

const DiSEqCDevDevice::TypeTable DiSEqCDevLNB::LNBTypeTable[5] =
{
    { "fixed",        kTypeFixed                 },
    { "voltage",      kTypeVoltageControl        },
    { "voltage_tone", kTypeVoltageAndToneControl },
    { "bandstacked",  kTypeBandstacked           },
    { QString::null,  kTypeVoltageAndToneControl },
};

DiSEqCDevLNB::DiSEqCDevLNB(DiSEqCDevTree &tree, uint devid)
    : DiSEqCDevDevice(tree, devid),
      m_type(kTypeVoltageAndToneControl), m_lof_switch(11700000),
      m_lof_hi(10600000),       m_lof_lo(9750000),
      m_pol_inv(false)
{
    Reset();
}

bool DiSEqCDevLNB::Execute(const DiSEqCDevSettings&, const DTVMultiplex &tuning)
{
    // set tone for bandselect
    if (m_type == kTypeVoltageAndToneControl)
        m_tree.SetTone(IsHighBand(tuning));

    return true;
}

uint DiSEqCDevLNB::GetVoltage(const DiSEqCDevSettings&,
                              const DTVMultiplex &tuning) const
{
    uint voltage = SEC_VOLTAGE_18;

    if ((kTypeVoltageControl        == m_type) ||
        (kTypeVoltageAndToneControl == m_type))
    {
        voltage = (IsHorizontal(tuning) ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13);
    }

    return voltage;
}

bool DiSEqCDevLNB::Load(void)
{
    // populate lnb parameters from db
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT subtype,         lnb_lof_switch, "
        "       lnb_lof_hi,      lnb_lof_lo, "
        "       lnb_pol_inv,     cmd_repeat "
        "FROM diseqc_tree "
        "WHERE diseqcid = :DEVID");
    query.bindValue(":DEVID", GetDeviceID());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("DiSEqCDevLNB::Load", query);
        return false;
    }
    else if (query.next())
    {
        m_type       = LNBTypeFromString(query.value(0).toString());
        m_lof_switch = query.value(1).toInt();
        m_lof_hi     = query.value(2).toInt();
        m_lof_lo     = query.value(3).toInt();
        m_pol_inv    = query.value(4).toUInt();
        m_repeat     = query.value(5).toUInt();
    }

    return true;
}

bool DiSEqCDevLNB::Store(void) const
{
    QString type = LNBTypeToString(m_type);
    MSqlQuery query(MSqlQuery::InitCon());

    // insert new or update old
    if (IsRealDeviceID())
    {
        query.prepare(
            "UPDATE diseqc_tree "
            "SET parentid        = :PARENT,  "
            "    ordinal         = :ORDINAL, "
            "    type            = 'lnb',    "
            "    description     = :DESC,    "
            "    subtype         = :TYPE,    "
            "    lnb_lof_switch  = :LOFSW,   "
            "    lnb_lof_lo      = :LOFLO,   "
            "    lnb_lof_hi      = :LOFHI,   "
            "    lnb_pol_inv     = :POLINV,  "
            "    cmd_repeat      = :REPEAT   "
            "WHERE diseqcid = :DEVID");
        query.bindValue(":DEVID",   GetDeviceID());
    }
    else
    {
        query.prepare(
            "INSERT INTO diseqc_tree"
            " ( parentid,      ordinal,         type, "
            "   description,   subtype,         lnb_lof_switch, "
            "   lnb_lof_lo,    lnb_lof_hi,      lnb_pol_inv, "
            "   cmd_repeat ) "
            "VALUES "
            " (:PARENT,       :ORDINAL,         'lnb', "
            "  :DESC,         :TYPE,            :LOFSW, "
            "  :LOFLO,        :LOFHI,           :POLINV, "
            "  :REPEAT ) ");
    }

    if (m_parent)
        query.bindValue(":PARENT", m_parent->GetDeviceID());

    query.bindValue(":ORDINAL", m_ordinal);
    query.bindValue(":DESC",    GetDescription());
    query.bindValue(":TYPE",    type);
    query.bindValue(":LOFSW",   m_lof_switch);
    query.bindValue(":LOFLO",   m_lof_lo);
    query.bindValue(":LOFHI",   m_lof_hi);
    query.bindValue(":POLINV",  m_pol_inv);
    query.bindValue(":REPEAT",  m_repeat);

    // update dev_id
    if (!query.exec())
    {
        MythDB::DBError("DiSEqCDevLNB::Store", query);
        return false;
    }

    // figure out devid if we did an insert
    if (!IsRealDeviceID())
        SetDeviceID(query.lastInsertId().toUInt());

    return true;
}

/** \fn DiSEqCDevLNB::IsHighBand(const DTVMultiplex&) const
 *  \brief Determine if the high frequency band is active
 *         (for switchable LNBs).
 *  \param tuning Tuning parameters.
 *  \return True if high band is active.
 */
bool DiSEqCDevLNB::IsHighBand(const DTVMultiplex &tuning) const
{
    switch (m_type)
    {
        case kTypeVoltageAndToneControl:
            return (tuning.frequency > m_lof_switch);
        case kTypeBandstacked:
            return IsHorizontal(tuning);
        default:
            return false;
    }

    return false;
}

/** \fn DiSEqCDevLNB::IsHorizontal(const DTVMultiplex&) const
 *  \brief Determine if horizontal polarity is active (for switchable LNBs).
 *  \param tuning Tuning parameters.
 *  \return True if polarity is horizontal.
 */
bool DiSEqCDevLNB::IsHorizontal(const DTVMultiplex &tuning) const
{
    QString pol = tuning.polarity.toString().toLower();
    return (pol == "h" || pol == "l") ^ IsPolarityInverted();
}

/** \fn DiSEqCDevLNB::GetIntermediateFrequency(const DiSEqCDevSettings&,
                                               const DTVMultiplex&) const
 *  \brief Calculate proper intermediate frequency for the given settings
 *         and tuning parameters.
 *  \param settings Configuration chain in effect.
 *  \param tuning Tuning parameters.
 *  \return Frequency for use with FE_SET_FRONTEND.
 */
uint32_t DiSEqCDevLNB::GetIntermediateFrequency(
    const DiSEqCDevSettings&, const DTVMultiplex &tuning) const
{
    (void) tuning;

    uint64_t abs_freq = tuning.frequency;
    uint lof = (IsHighBand(tuning)) ? m_lof_hi : m_lof_lo;

    return (lof > abs_freq) ? (lof - abs_freq) : (abs_freq - lof);
}

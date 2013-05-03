/**
 *  DarwinFirewireChannel
 *  Copyright (c) 2006 by Daniel Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// Std C++ headers
#include <vector>
using namespace std;

// MythTV headers
#include "darwinfirewiredevice.h"
#include "darwinavcinfo.h"
#include "mythlogging.h"

#ifndef kIOFireWireAVCLibUnitInterfaceID2
#define kIOFireWireAVCLibUnitInterfaceID2 \
    CFUUIDGetConstantUUIDWithBytes( \
        NULL, \
        0x85, 0xB5, 0xE9, 0x54, 0x0A, 0xEF, 0x11, 0xD8, \
        0x8D, 0x19, 0x00, 0x03, 0x93, 0x91, 0x4A, 0xBA)
#endif

static void dfd_device_change_msg(
    void*, io_service_t, natural_t messageType, void*);

void DarwinAVCInfo::Update(uint64_t _guid, DarwinFirewireDevice *dev,
                           IONotificationPortRef notify_port,
                           CFRunLoopRef &thread_cf_ref, io_object_t obj)
{
    IOObjectRelease(fw_device_notifier_ref);
    IOObjectRelease(fw_node_ref);
    IOObjectRelease(fw_device_ref);
    IOObjectRelease(fw_service_ref);
    IOObjectRelease(avc_service_ref);

    avc_service_ref = obj;

    IORegistryEntryGetParentEntry(
        avc_service_ref, kIOServicePlane, &fw_service_ref);
    IORegistryEntryGetParentEntry(
        fw_service_ref,  kIOServicePlane, &fw_device_ref);
    IORegistryEntryGetParentEntry(
        fw_device_ref,   kIOServicePlane, &fw_node_ref);

    if (notify_port)
    {
        IOServiceAddInterestNotification(
            notify_port, obj, kIOGeneralInterest,
            dfd_device_change_msg, dev,
            &fw_device_notifier_ref);
    }

    if (guid == _guid)
        return; // we're done

    guid = _guid;

    //////////////////////////
    // get basic info

    CFMutableDictionaryRef props;
    int ret = IORegistryEntryCreateCFProperties(
        obj, &props, kCFAllocatorDefault, kNilOptions);
    if (kIOReturnSuccess != ret)
        return; // this is bad

    CFNumberRef specDesc = (CFNumberRef)
        CFDictionaryGetValue(props, CFSTR("Unit_Spec_ID"));
    CFNumberGetValue(specDesc, kCFNumberSInt32Type, &specid);

    CFNumberRef typeDesc = (CFNumberRef)
        CFDictionaryGetValue(props, CFSTR("Unit_Type"));
    CFNumberGetValue(typeDesc, kCFNumberSInt32Type, &modelid);

    CFNumberRef vendorDesc = (CFNumberRef)
        CFDictionaryGetValue(props, CFSTR("Vendor_ID"));
    CFNumberGetValue(vendorDesc, kCFNumberSInt32Type, &vendorid);

    CFNumberRef versionDesc = (CFNumberRef)
        CFDictionaryGetValue(props, CFSTR("Unit_SW_Version"));
    CFNumberGetValue(versionDesc, kCFNumberSInt32Type, &firmware_revision);

    CFStringRef tmp0 = (CFStringRef)
        CFDictionaryGetValue(props, CFSTR("FireWire Product Name"));
    if (tmp0)
    {
        char tmp1[1024];
        memset(tmp1, 0, sizeof(tmp1));
        CFStringGetCString(tmp0, tmp1, sizeof(tmp1) - sizeof(char),
                           kCFStringEncodingMacRoman);
        product_name = QString("%1").arg(tmp1);
    }

    CFRelease(props);

    //////////////////////////
    // get subunit info

    LOG(VB_RECORD, LOG_INFO, QString("Scanning guid: 0x%1").arg(guid, 0, 16));

    bool wasOpen = IsAVCInterfaceOpen();
    if (OpenAVCInterface(thread_cf_ref))
    {
        if (!GetSubunitInfo())
        {
            LOG(VB_GENERAL, LOG_ERR, "GetSubunitInfo failed");
        }

        if (!wasOpen)
            CloseAVCInterface();
    }
}

bool DarwinAVCInfo::SendAVCCommand(
    const vector<uint8_t> &cmd,
    vector<uint8_t>       &result,
    int                   /*retry_cnt*/)
{
    result.clear();

    uint32_t result_length = 4096;
    uint8_t response[4096];

    if (!avc_handle)
        return false;

    int ret = (*avc_handle)->
        AVCCommand(avc_handle, (const UInt8*) &cmd[0], cmd.size(),
                   response, (UInt32*) &result_length);

    if (ret != kIOReturnSuccess)
        return false;

    if (result_length)
        result.insert(result.end(), response, response + result_length);

    return true;
}

bool DarwinAVCInfo::OpenPort(CFRunLoopRef &thread_cf_ref)
{
    if (IsPortOpen())
        return true;

    if (!OpenAVCInterface(thread_cf_ref))
        return false;

    if (!OpenDeviceInterface(thread_cf_ref))
    {
        CloseAVCInterface();
        return false;
    }

    return true;
}

bool DarwinAVCInfo::ClosePort(void)
{
    CloseDeviceInterface();
    CloseAVCInterface();
    return true;
}

bool DarwinAVCInfo::OpenAVCInterface(CFRunLoopRef &thread_cf_ref)
{
    if (IsAVCInterfaceOpen())
        return true;

    if (!avc_service_ref)
        return false;

    IOCFPlugInInterface **input_plug;
    int32_t dummy;
    int ret = IOCreatePlugInInterfaceForService(
        avc_service_ref, kIOFireWireAVCLibUnitTypeID, kIOCFPlugInInterfaceID,
        &input_plug, (SInt32*) &dummy);

    if (kIOReturnSuccess != ret)
        return false;

    // Try to get post-Jaguar interface
    HRESULT err = (*input_plug)->QueryInterface(
            input_plug, CFUUIDGetUUIDBytes(kIOFireWireAVCLibUnitInterfaceID2),
            (void**) &avc_handle);

    // On failure, try Jaguar interface
    if (S_OK != err)
    {
        err = (*input_plug)->QueryInterface(
            input_plug, CFUUIDGetUUIDBytes(kIOFireWireAVCLibUnitInterfaceID),
            (void**) &avc_handle);
    }

    if (S_OK != err)
    {
        (*input_plug)->Release(input_plug);
        return false;
    }

    // Add avc_handle to the event loop
    ret = (*avc_handle)->addCallbackDispatcherToRunLoop(
        avc_handle, thread_cf_ref);

    (*input_plug)->Release(input_plug);

    if (kIOReturnSuccess != ret)
    {
        (*avc_handle)->Release(avc_handle);
        avc_handle = NULL;
        return false;
    }

    ret = (*avc_handle)->open(avc_handle);
    if (kIOReturnSuccess != ret)
    {
        (*avc_handle)->Release(avc_handle);
        avc_handle = NULL;
        return false;
    }

    return true;
}

void DarwinAVCInfo::CloseAVCInterface(void)
{
    if (!avc_handle)
        return;

    (*avc_handle)->removeCallbackDispatcherFromRunLoop(avc_handle);
    (*avc_handle)->close(avc_handle);
    (*avc_handle)->Release(avc_handle);

    avc_handle = NULL;
}

bool DarwinAVCInfo::OpenDeviceInterface(CFRunLoopRef &thread_cf_ref)
{
    if (fw_handle)
        return true;

    if (!avc_handle)
        return false;

    IOCFPlugInInterface **input_plug;
    int32_t dummy;
    int ret = IOCreatePlugInInterfaceForService(
        fw_device_ref, kIOFireWireLibTypeID, kIOCFPlugInInterfaceID,
        &input_plug, (SInt32*) &dummy);

    if (kIOReturnSuccess != ret)
        return false;

    HRESULT err = (*input_plug)->QueryInterface(
        input_plug, CFUUIDGetUUIDBytes(kIOFireWireNubInterfaceID),
        (void**) &fw_handle);

    if (S_OK != err)
    {
        (*input_plug)->Release(input_plug);
        return false;
    }

    // Add fw_handle to the event loop
    ret = (*fw_handle)->AddCallbackDispatcherToRunLoop(
        fw_handle, thread_cf_ref);

    (*input_plug)->Release(input_plug);

    if (kIOReturnSuccess == ret)
    {
        // open the interface
        ret = (*fw_handle)->OpenWithSessionRef(
            fw_handle, (*avc_handle)->getSessionRef(avc_handle));
    }

    if (kIOReturnSuccess != ret)
    {
        (*fw_handle)->Release(fw_handle);
        fw_handle = NULL;
        return false;
    }

    return true;
}

void DarwinAVCInfo::CloseDeviceInterface(void)
{
    if (!fw_handle)
        return;

    (*fw_handle)->RemoveCallbackDispatcherFromRunLoop(fw_handle);
    (*fw_handle)->Close(fw_handle);
    (*fw_handle)->Release(fw_handle);

    fw_handle = NULL;
}

bool DarwinAVCInfo::GetDeviceNodes(int &local_node, int &remote_node)
{
    uint32_t generation = 0;
    uint16_t node       = 0;
    local_node  = -1;
    remote_node = -1;

    if ((*fw_handle)->version < 4)
    {
        if (kIOReturnSuccess == (*fw_handle)->GetGenerationAndNodeID(
                fw_handle, (UInt32*) &generation, (UInt16*) &node))
        {
            remote_node = node;
        }

        if (kIOReturnSuccess == (*fw_handle)->GetLocalNodeID(
                fw_handle, (UInt16*) &node))
        {
            local_node = node;
        }
    }

    int ret = (*fw_handle)->GetBusGeneration(fw_handle, (UInt32*)&generation);
    if (kIOReturnSuccess == ret)
    {
        if (kIOReturnSuccess == (*fw_handle)->GetLocalNodeIDWithGeneration(
                fw_handle, generation, (UInt16*) &node))
        {
            local_node = node;
        }

        if (kIOReturnSuccess == (*fw_handle)->GetRemoteNodeID(
                fw_handle, generation, (UInt16*) &node))
        {
            remote_node = node;
        }
    }

    return (local_node >= 0) && (remote_node >= 0);
}

static void dfd_device_change_msg(
    void *dfd, io_service_t, natural_t messageType, void*)
{
    DarwinFirewireDevice *dev =
        reinterpret_cast<DarwinFirewireDevice*>(dfd);
    dev->HandleDeviceChange(messageType);
}

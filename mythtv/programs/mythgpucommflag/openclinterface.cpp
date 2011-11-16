#include <QString>
#include <QStringList>
#include <QMap>
#include <QFile>
#include <QByteArray>
#include <iostream>
#include <sstream>
#include <fstream>
#include <strings.h>

#include "mythlogging.h"
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif
#include <oclUtils.h>
#include "openclinterface.h"
#include "openglsupport.h"

void openCLDisplayImageFormats(cl_context context);
void openCLPrintDevInfo(cl_device_id device);

cl_platform_id clSelectedPlatformID = NULL; 

OpenCLDeviceMap::OpenCLDeviceMap(void)
{
    OpenCLDevice *dev;

    m_valid = false;

    // Get OpenCL platform ID for NVIDIA if avaiable, otherwise default
    cl_int ciErrNum = oclGetPlatformID (&clSelectedPlatformID);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("OpenCL Error # %1 (%2)")
            .arg(ciErrNum) .arg(oclErrorString(ciErrNum)));
        return;
    }

    // Get OpenCL platform name and version
    char cBuffer[1024];
    ciErrNum = clGetPlatformInfo(clSelectedPlatformID, CL_PLATFORM_NAME,
                                 sizeof(cBuffer), cBuffer, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error %1 in clGetPlatformInfo Call")
            .arg(ciErrNum));
        return;
    } 

    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Platform: %1").arg(cBuffer));

    ciErrNum = clGetPlatformInfo(clSelectedPlatformID, CL_PLATFORM_VERSION,
                                 sizeof(cBuffer), cBuffer, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error %1 in clGetPlatformInfo Call")
            .arg(ciErrNum));
        return;
    } 

    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Version: %1") .arg(cBuffer));

    // Log OpenCL SDK Revision # 
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL SDK Revision: %1")
        .arg(OCL_SDKREVISION));

    // Get and log OpenCL device info 
    cl_uint ciDeviceCount;
    cl_device_id *devices;
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Device Info:");
    ciErrNum = clGetDeviceIDs(clSelectedPlatformID, CL_DEVICE_TYPE_ALL, 0, 
                              NULL, &ciDeviceCount);

    // check for 0 devices found or errors... 
    if (ciDeviceCount == 0)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("No devices found supporting OpenCL "
                                          "(return code %1)").arg(ciErrNum));
        return;
    } 

    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error %1 in clGetDeviceIDs call")
            .arg(ciErrNum));
        return;
    }

    // Get and log the OpenCL device ID's
    LOG(VB_GENERAL, LOG_INFO, QString("%1 devices found supporting OpenCL:")
        .arg(ciDeviceCount));

    if ((devices = new cl_device_id[ciDeviceCount]) == NULL)
    {
       LOG(VB_GENERAL, LOG_ERR, "Failed to allocate memory for devices");
       return;
    }

    ciErrNum = clGetDeviceIDs(clSelectedPlatformID, CL_DEVICE_TYPE_ALL,
                              ciDeviceCount, devices, &ciDeviceCount);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error %1 in clGetDeviceIDs call")
            .arg(ciErrNum));
        delete [] devices;
        return;
    }

    // show info for each device in the context
    for(unsigned int i = 0; i < ciDeviceCount; ++i ) 
    {  
        dev = new OpenCLDevice(devices[i]);
        if (dev->m_valid)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Device: %1").arg(dev->m_name));
            insertMulti(dev->GetHash(), dev);
        }
        else
            delete dev;
    }

    m_valid = true;
}

void openCLDisplayImageFormats(cl_context context)
{
    // Determine and show image format support 
    cl_uint uiNumSupportedFormats = 0;

    // 2D
    clGetSupportedImageFormats(context, CL_MEM_READ_ONLY, 
                               CL_MEM_OBJECT_IMAGE2D,   
                               NULL, NULL, &uiNumSupportedFormats);
    cl_image_format* ImageFormats = 
        new cl_image_format[uiNumSupportedFormats];
    clGetSupportedImageFormats(context, CL_MEM_READ_ONLY, 
                               CL_MEM_OBJECT_IMAGE2D,   
                               uiNumSupportedFormats, ImageFormats,
                               NULL);
    LOG(VB_GENERAL, LOG_INFO, "---------------------------------");
    LOG(VB_GENERAL, LOG_INFO,
        QString("2D Image Formats Supported (%1)")
        .arg(uiNumSupportedFormats));
    LOG(VB_GENERAL, LOG_INFO, "---------------------------------");
    LOG(VB_GENERAL, LOG_INFO, QString("%1%2%3")
        .arg("#", -6) .arg("Channel Order", -16)
        .arg("Channel Type", -22));
    for(unsigned int i = 0; i < uiNumSupportedFormats; i++) 
    {  
        LOG(VB_GENERAL, LOG_INFO, QString("%1%2%3")
            .arg(i + 1, -6)
            .arg(oclImageFormatString(ImageFormats[i].
                                      image_channel_order), -16) 
            .arg(oclImageFormatString(ImageFormats[i].
                                   image_channel_data_type), -22));
    }
    delete [] ImageFormats;

    // 3D
    clGetSupportedImageFormats(context, CL_MEM_READ_ONLY, 
                               CL_MEM_OBJECT_IMAGE3D,   
                               NULL, NULL, &uiNumSupportedFormats);
    ImageFormats = new cl_image_format[uiNumSupportedFormats];
    clGetSupportedImageFormats(context, CL_MEM_READ_ONLY, 
                               CL_MEM_OBJECT_IMAGE3D,   
                               uiNumSupportedFormats, ImageFormats,
                               NULL);
    LOG(VB_GENERAL, LOG_INFO, "---------------------------------");
    LOG(VB_GENERAL, LOG_INFO,
        QString("3D Image Formats Supported (%1)")
        .arg(uiNumSupportedFormats)); 
    LOG(VB_GENERAL, LOG_INFO, "---------------------------------");
    LOG(VB_GENERAL, LOG_INFO, QString("%1%2%3")
        .arg("#", -6) .arg("Channel Order", -16)
        .arg("Channel Type", -22));
    for(unsigned int i = 0; i < uiNumSupportedFormats; i++) 
    {  
        LOG(VB_GENERAL, LOG_INFO, QString("%1%2%3")
            .arg(i + 1, -6)
            .arg(oclImageFormatString(ImageFormats[i].
                                      image_channel_order), -16)
            .arg(oclImageFormatString(ImageFormats[i].
                                   image_channel_data_type), -22));
    }
    delete [] ImageFormats;
}

void OpenCLDeviceMap::Display(void)
{
    for (OpenCLDeviceMap::iterator it = begin(); it != end(); ++it)
    {
        OpenCLDevice *dev = it.value();
        dev->Display();
    }
}

OpenCLDevice *OpenCLDeviceMap::GetBestDevice(OpenCLDevice *prev, bool forceSw)
{
    if (forceSw)
        return NULL;

    OpenCLDevice *previous = NULL;
    for (OpenCLDeviceMap::iterator it = begin(); it != end(); ++it)
    {
        if (prev == previous)
            return it.value();
        previous = it.value();
    }

    return NULL;
}

bool OpenCLDevice::Initialize(void)
{
    m_commandQ = clCreateCommandQueue(m_context, m_deviceId, 0, NULL);
    if (!m_commandQ)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error in clCreateCommandQueue");
        return false;
    }
    return true;
}

bool OpenCLDevice::RegisterKernel(QString entry, QString filename)
{
    OpenCLKernel *kernel = OpenCLKernel::Create(this, entry, filename);
    if (!kernel)
        return false;

    m_kernelMap.insert(entry, kernel);
    return true;
}

OpenCLDevice::OpenCLDevice(cl_device_id device, bool needOpenGL) :
    m_valid(false), m_deviceId(device), m_commandQ(NULL)
{
    cl_int ciErrNum;

    m_props = NULL;

    if (needOpenGL)
    {
        if (!openGLInitialize())
        {
            LOG(VB_GENERAL, LOG_ERR, "Could not open OpenGL Context!");
            return;
        }

        cl_context_properties openGLProps[] = {
            CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(), 
            CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
            0 };
        m_props = new cl_context_properties[5];
        memcpy(m_props, openGLProps, sizeof(openGLProps));
    }

    // Create a context for the device
    m_context = clCreateContext(m_props, 1, &device, NULL, NULL, &ciErrNum);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error %1 in clCreateContext call")
            .arg(ciErrNum));
        return;
    }

    char device_string[1024];

    // CL_DEVICE_NAME
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_string),
                    &device_string, NULL);
    m_name = QString(device_string);

    // CL_DEVICE_VENDOR
    clGetDeviceInfo(device, CL_DEVICE_VENDOR, sizeof(device_string),
                    &device_string, NULL);
    m_vendor = QString(device_string);

    // CL_DRIVER_VERSION
    clGetDeviceInfo(device, CL_DRIVER_VERSION, sizeof(device_string),
                    &device_string, NULL);
    m_driverVersion = QString(device_string);

    // CL_DEVICE_VERSION
    clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(device_string),
                    &device_string, NULL);
    m_deviceVersion = QString(device_string);

#if !defined(__APPLE__) && !defined(__MACOSX)
    // CL_DEVICE_OPENCL_C_VERSION (if CL_DEVICE_VERSION version > 1.0)
    if(strncmp("OpenCL 1.0", device_string, 10) != 0) 
    {
        // This code is unused for devices reporting OpenCL 1.0, but a def is needed anyway to allow compilation using v 1.0 headers 
        // This constant isn't #defined in 1.0
        #ifndef CL_DEVICE_OPENCL_C_VERSION
            #define CL_DEVICE_OPENCL_C_VERSION 0x103D   
        #endif

        clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION,
                        sizeof(device_string), &device_string, NULL);
        m_openclCVersion = QString(device_string);
    }
#endif

    // CL_DEVICE_TYPE
    m_deviceType = kOpenCLDeviceUnknown;
    cl_device_type type;
    clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(type), &type, NULL);
    if( type & CL_DEVICE_TYPE_CPU )
        m_deviceType = kOpenCLDeviceCPU;
    if( type & CL_DEVICE_TYPE_GPU )
        m_deviceType = kOpenCLDeviceGPU;
    if( type & CL_DEVICE_TYPE_ACCELERATOR )
        m_deviceType = kOpenCLDeviceAccelerator;
    if( type & CL_DEVICE_TYPE_DEFAULT )
        m_deviceType = kOpenCLDeviceDefault;
    
    // CL_DEVICE_MAX_COMPUTE_UNITS
    clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS,
                    sizeof(m_maxComputeUnits), &m_maxComputeUnits, NULL);

#if 0
	// CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS
    size_t workitem_dims;
    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
                    sizeof(workitem_dims), &workitem_dims, NULL);
    LOG(VB_GENERAL, LOG_INFO, QString("CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS: %1")
        .arg(workitem_dims));
#endif

    // CL_DEVICE_MAX_WORK_ITEM_SIZES
    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES,
                    sizeof(m_maxWorkItemSizes), &m_maxWorkItemSizes, NULL);
    
    // CL_DEVICE_MAX_WORK_GROUP_SIZE
    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE,
                    sizeof(m_maxWorkGroupSize), &m_maxWorkGroupSize, NULL);

    // CL_DEVICE_MAX_CLOCK_FREQUENCY
    clGetDeviceInfo(device, CL_DEVICE_MAX_CLOCK_FREQUENCY,
                    sizeof(m_maxClockFrequency), &m_maxClockFrequency, NULL);

    // CL_DEVICE_ADDRESS_BITS
    clGetDeviceInfo(device, CL_DEVICE_ADDRESS_BITS, sizeof(m_addressBits),
                    &m_addressBits, NULL);

    // CL_DEVICE_MAX_MEM_ALLOC_SIZE
    cl_ulong mem_size;
    clGetDeviceInfo(device, CL_DEVICE_MAX_MEM_ALLOC_SIZE,
                    sizeof(mem_size), &mem_size, NULL);
    m_maxMemAllocSize = (int)(mem_size / (1024 * 1024));

    // CL_DEVICE_GLOBAL_MEM_SIZE
    clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(mem_size),
                    &mem_size, NULL);
    m_globalMemSize = (int)(mem_size / (1024 * 1024));

    // CL_DEVICE_ERROR_CORRECTION_SUPPORT
    cl_bool error_correction_support;
    clGetDeviceInfo(device, CL_DEVICE_ERROR_CORRECTION_SUPPORT,
                    sizeof(error_correction_support),
                    &error_correction_support, NULL);
    m_errCorrection = (error_correction_support == CL_TRUE);

    // CL_DEVICE_LOCAL_MEM_TYPE
    cl_device_local_mem_type local_mem_type;
    clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_TYPE, sizeof(local_mem_type),
                    &local_mem_type, NULL);
    m_memType = (local_mem_type == 1 ? kOpenCLMemLocal : kOpenCLMemGlobal);

    // CL_DEVICE_LOCAL_MEM_SIZE
    clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(mem_size),
                    &mem_size, NULL);
    m_localMemSize = (int)(mem_size / 1024);

    // CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE
    clGetDeviceInfo(device, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,
                    sizeof(mem_size), &mem_size, NULL);
    m_maxConstBufSize = (int)(mem_size / 1024);

    // CL_DEVICE_QUEUE_PROPERTIES
    clGetDeviceInfo(device, CL_DEVICE_QUEUE_PROPERTIES,
                    sizeof(m_queueProperties), &m_queueProperties, NULL);

    // CL_DEVICE_IMAGE_SUPPORT
    cl_bool image_support;
    clGetDeviceInfo(device, CL_DEVICE_IMAGE_SUPPORT, sizeof(image_support),
                    &image_support, NULL);
    m_imageSupport = (image_support == CL_TRUE);

    // CL_DEVICE_MAX_READ_IMAGE_ARGS
    clGetDeviceInfo(device, CL_DEVICE_MAX_READ_IMAGE_ARGS,
                    sizeof(m_maxReadImageArgs), &m_maxReadImageArgs, NULL);

    // CL_DEVICE_MAX_WRITE_IMAGE_ARGS
    clGetDeviceInfo(device, CL_DEVICE_MAX_WRITE_IMAGE_ARGS,
                    sizeof(m_maxWriteImageArgs), &m_maxWriteImageArgs, NULL);

    // CL_DEVICE_SINGLE_FP_CONFIG
    clGetDeviceInfo(device, CL_DEVICE_SINGLE_FP_CONFIG,
                    sizeof(m_fpConfig), &m_fpConfig, NULL);
    
    // CL_DEVICE_IMAGE2D_MAX_WIDTH, CL_DEVICE_IMAGE2D_MAX_HEIGHT,
    clGetDeviceInfo(device, CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(size_t),
                    &m_max2DImageSize[0], NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE2D_MAX_HEIGHT, sizeof(size_t),
                    &m_max2DImageSize[1], NULL);

    // CL_DEVICE_IMAGE3D_MAX_WIDTH, CL_DEVICE_IMAGE3D_MAX_HEIGHT,
    // CL_DEVICE_IMAGE3D_MAX_DEPTH
    clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_WIDTH, sizeof(size_t),
                    &m_max3DImageSize[0], NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_HEIGHT, sizeof(size_t),
                    &m_max3DImageSize[1], NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_DEPTH, sizeof(size_t),
                    &m_max3DImageSize[2], NULL);

    m_float64 = false;
    m_opengl  = false;

    // CL_DEVICE_EXTENSIONS: get device extensions, and if any then parse & log
    // the string onto separate lines
    m_vendorType = kOpenCLVendorUnknown;
    clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof(device_string),
                    &device_string, NULL);
    if (device_string != 0) 
    {
        QString devString(device_string);
        m_extensions = devString.split(" ", QString::SkipEmptyParts);

        if (m_extensions.indexOf("cl_nv_device_attribute_query") != -1)
            m_vendorType = kOpenCLVendorNvidia;
        m_float64 = (m_extensions.indexOf("cl_khr_fp64") != -1);
        m_opengl = (m_extensions.indexOf("cl_khr_gl_sharing") != -1);
    }

    switch (m_vendorType)
    {
      case kOpenCLVendorNvidia:
        m_vendorSpecific = new OpenCLDeviceNvidiaSpecific(this);
        break;
      default:
        m_vendorSpecific = NULL;
        break;
    }

    // CL_DEVICE_PREFERRED_VECTOR_WIDTH_<type>
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,
                    sizeof(cl_uint), &m_vectorWidth[0], NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,
                    sizeof(cl_uint), &m_vectorWidth[1], NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,
                    sizeof(cl_uint), &m_vectorWidth[2], NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,
                    sizeof(cl_uint), &m_vectorWidth[3], NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,
                    sizeof(cl_uint), &m_vectorWidth[4], NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,
                    sizeof(cl_uint), &m_vectorWidth[5], NULL);

    m_valid = true;
}

OpenCLDevice::~OpenCLDevice()
{
    m_valid = false;
    if (m_vendorSpecific)
    {
        delete m_vendorSpecific;
        m_vendorSpecific = NULL;
    }

    if (m_props)
    {
        delete [] m_props;
        m_props = NULL;
    }
}

uint64_t OpenCLDevice::GetHash(void)
{
    uint64_t value = ((uint64_t)m_maxComputeUnits)   << 48 |
                     ((uint64_t)m_globalMemSize)     << 32 |
                     ((uint64_t)m_maxClockFrequency);
    return (uint64_t)~value;
}

void OpenCLDevice::Display(void)
{
    if (!m_valid)
        return;

    LOG(VB_GENERAL, LOG_INFO, "------------------------------------------");
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Device Name: " + m_name);
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Vendor: " + m_vendor);
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Driver Version: " + m_driverVersion);
    LOG(VB_GENERAL, LOG_INFO, "OpenCL Device Version: " + m_deviceVersion);
    if (!m_openclCVersion.isEmpty())
        LOG(VB_GENERAL, LOG_INFO, "OpenCL C Version: " + m_openclCVersion);

    switch (m_deviceType)
    {
      case kOpenCLDeviceCPU:
        LOG(VB_GENERAL, LOG_INFO, "OpenCL Device Type: CPU");
        break;
      case kOpenCLDeviceGPU:
        LOG(VB_GENERAL, LOG_INFO, "OpenCL Device Type: GPU");
        break;
      case kOpenCLDeviceAccelerator:
        LOG(VB_GENERAL, LOG_INFO, "OpenCL Device Type: Accelerator");
        break;
      case kOpenCLDeviceDefault:
        LOG(VB_GENERAL, LOG_INFO, "OpenCL Device Type: Default");
        break;
      case kOpenCLDeviceUnknown:
        LOG(VB_GENERAL, LOG_INFO, "OpenCL Device Type: Unknown");
        break;
    }
    
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Max Compute Units: %1")
        .arg(m_maxComputeUnits));

    LOG(VB_GENERAL, LOG_INFO,
        QString("OpenCL Max Work Item Sizes: %1 / %2 / %3")
        .arg(m_maxWorkItemSizes[0]) .arg(m_maxWorkItemSizes[1])
        .arg(m_maxWorkItemSizes[2]));
    
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Max Work Group Size: %1")
        .arg(m_maxWorkGroupSize));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Device Max Clock Freq: %1 MHz")
        .arg(m_maxClockFrequency));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Address Bits: %1")
        .arg(m_addressBits));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Max Memory Alloc Size: %1 MByte")
        .arg(m_maxMemAllocSize));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Global Memory Size: %1 MByte")
        .arg(m_globalMemSize));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Error Correction Support: %1")
        .arg(m_errCorrection ? "yes" : "no"));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Local Memory Type: %1")
        .arg(m_memType == kOpenCLMemLocal ? "local" : "global"));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Local Memory Size: %1 KByte")
        .arg(m_localMemSize));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Max Const Buffer Size: %1 KByte")
        .arg(m_maxConstBufSize));

    if( m_queueProperties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE )
        LOG(VB_GENERAL, LOG_INFO, "OpenCL Device Queue Properties: "
                                  "CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE");    
    if( m_queueProperties & CL_QUEUE_PROFILING_ENABLE )
        LOG(VB_GENERAL, LOG_INFO, "OpenCL Device Queue Properties: "
                                  "CL_QUEUE_PROFILING_ENABLE");

    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Image Support: %1")
        .arg(m_imageSupport ? "yes" : "no"));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Max Read Image Args: %1")
        .arg(m_maxReadImageArgs));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Max Write Image Args: %1")
        .arg(m_maxWriteImageArgs));

    LOG(VB_GENERAL, LOG_INFO,
        QString("CL_DEVICE_SINGLE_FP_CONFIG: %1%2%3%4%5%6")
        .arg(m_fpConfig & CL_FP_DENORM ? "denorms " : "")
        .arg(m_fpConfig & CL_FP_INF_NAN ? "INF-quietNaNs " : "")
        .arg(m_fpConfig & CL_FP_ROUND_TO_NEAREST ? "round-to-nearest " : "")
        .arg(m_fpConfig & CL_FP_ROUND_TO_ZERO ? "round-to-zero " : "")
        .arg(m_fpConfig & CL_FP_ROUND_TO_INF ? "round-to-inf " : "")
        .arg(m_fpConfig & CL_FP_FMA ? "fma " : ""));
    
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL 2D Max Dims:  %1x%2")
        .arg(m_max2DImageSize[0]) .arg(m_max2DImageSize[1]));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL 3D Max Dims:  %1x%2x%3")
        .arg(m_max3DImageSize[0]) .arg(m_max3DImageSize[1]) 
        .arg(m_max3DImageSize[2]));

    if (!m_extensions.empty())
    {
        for (QStringList::iterator it = m_extensions.begin();
             it != m_extensions.end(); ++it)
            LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Extension: %1").arg(*it));
    }
    else 
    {
        LOG(VB_GENERAL, LOG_INFO, "OpenCL Extension: None");
    }

    if (m_vendorSpecific)
        m_vendorSpecific->Display();

    // CL_DEVICE_PREFERRED_VECTOR_WIDTH_<type>
    LOG(VB_GENERAL, LOG_INFO,
        QString("OpenCL Preferred Vector Widths: CHAR %1, SHORT %2, INT %3, "
                "LONG %4, FLOAT %5, DOUBLE %6")
        .arg(m_vectorWidth[0]) .arg(m_vectorWidth[1]) .arg(m_vectorWidth[2])
        .arg(m_vectorWidth[3]) .arg(m_vectorWidth[4]) .arg(m_vectorWidth[5])); 
}




OpenCLDeviceNvidiaSpecific::OpenCLDeviceNvidiaSpecific(OpenCLDevice *parent) :
    m_parent(parent)
{
    cl_device_id device = m_parent->m_deviceId;

    clGetDeviceInfo(device, CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV,
                    sizeof(cl_uint), &m_computeCapMajor, NULL);
    clGetDeviceInfo(device, CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV,
                    sizeof(cl_uint), &m_computeCapMinor, NULL);

    // this is the same value reported by CL_DEVICE_MAX_COMPUTE_UNITS
    m_multiprocessors = m_parent->m_maxComputeUnits;
    m_cudaCores = ConvertSMVer2Cores(m_computeCapMajor, m_computeCapMinor) *
                  m_multiprocessors;

    clGetDeviceInfo(device, CL_DEVICE_REGISTERS_PER_BLOCK_NV,
                    sizeof(cl_uint), &m_regsPerBlock, NULL);

    clGetDeviceInfo(device, CL_DEVICE_WARP_SIZE_NV, sizeof(cl_uint),
                    &m_warpSize, NULL);

    cl_bool gpu_overlap;
    clGetDeviceInfo(device, CL_DEVICE_GPU_OVERLAP_NV, sizeof(cl_bool),
                    &gpu_overlap, NULL);
    m_gpuOverlap = (gpu_overlap == CL_TRUE);

    cl_bool exec_timeout;
    clGetDeviceInfo(device, CL_DEVICE_KERNEL_EXEC_TIMEOUT_NV,
                    sizeof(cl_bool), &exec_timeout, NULL);
    m_kernelExecTimeout = (exec_timeout == CL_TRUE);

    cl_bool integrated_memory;
    clGetDeviceInfo(device, CL_DEVICE_INTEGRATED_MEMORY_NV,
                    sizeof(cl_bool), &integrated_memory, NULL);
    m_integratedMem = (integrated_memory == CL_TRUE);
}

void OpenCLDeviceNvidiaSpecific::Display(void)
{
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL CUDA Capabilites: %1.%2")
        .arg(m_computeCapMajor) .arg(m_computeCapMinor));

    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Multiprocessor Count: %1")
        .arg(m_multiprocessors));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL CUDA Core Count: %1")
        .arg(m_cudaCores));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Registers per Block: %1")
        .arg(m_regsPerBlock));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Warp Size: %1").arg(m_warpSize));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL GPU Overlap: %1")
        .arg(m_gpuOverlap ? "yes" : "no"));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Kernel Exec Timeout: %1")
        .arg(m_kernelExecTimeout ? "yes" : "no"));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenCL Integrated Memory: %1")
        .arg(m_integratedMem ? "yes" : "no"));
}

OpenCLKernel *OpenCLKernel::Create(OpenCLDevice *device, QString entry,
                                          QString filename)
{
    QFile file(filename);

    LOG(VB_GENERAL, LOG_INFO, QString("Loading OpenCL kernel %1 from %2")
        .arg(entry) .arg(filename));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Create(%1): Could not open program file: %2")
            .arg(entry) .arg(filename));
        return NULL;
    }

    QByteArray programText = file.readAll();
    const char *programChar = programText.constData();
    file.close();

    cl_int ciErrNum;
    cl_program program = clCreateProgramWithSource(device->m_context, 1,
                                                   &programChar, NULL,
                                                   &ciErrNum);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Create(%1): Error %2 trying to create program from source")
            .arg(entry) .arg(ciErrNum));
        return NULL;
    }

    ciErrNum = clBuildProgram(program, 1, &device->m_deviceId, NULL, NULL,
                              NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        // Get the build errors
        char buildLog[16384];
        clGetProgramBuildInfo(program, device->m_deviceId, CL_PROGRAM_BUILD_LOG,
                              sizeof(buildLog), buildLog, NULL);
        LOG(VB_GENERAL, LOG_ERR, QString("Create(%1): Error %2 in kernel: %3")
            .arg(entry) .arg(ciErrNum) .arg(buildLog));
        clReleaseProgram(program);
        return NULL;
    }

    const char *entryPt = entry.toLocal8Bit().constData();
    cl_kernel kernel = clCreateKernel(program, entryPt, &ciErrNum);
    if (!kernel)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error creating kernel %1: %2 (%3)")
            .arg(entryPt) .arg(ciErrNum) .arg(oclErrorString(ciErrNum)));
        clReleaseProgram(program);
        return NULL;
    }

    OpenCLKernel *newKernel = new OpenCLKernel(device, entry, program, kernel);
    return newKernel;
}

OpenCLKernel::~OpenCLKernel()
{
    clReleaseKernel(m_kernel);
    clReleaseProgram(m_program);
}

OpenCLBuffers::OpenCLBuffers(int count) : m_count(count)
{
    m_bufs = new cl_mem[count];
    memset(m_bufs, 0, m_count * sizeof(cl_mem));
}

OpenCLBuffers::~OpenCLBuffers()
{
    if (!m_bufs)
        return;

    for (int i = 0; i < m_count; i++)
        if (m_bufs[i])
            clReleaseMemObject(m_bufs[i]);
    delete [] m_bufs;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

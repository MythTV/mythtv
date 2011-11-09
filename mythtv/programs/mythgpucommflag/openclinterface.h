#ifndef _openclinterface_h
#define _openclinterface_h

#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif

#include <oclUtils.h>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>

typedef enum {
    kOpenCLDeviceUnknown     = 0x0000,
    kOpenCLDeviceCPU         = 0x0001,
    kOpenCLDeviceGPU         = 0x0002,
    kOpenCLDeviceAccelerator = 0x0004,
    kOpenCLDeviceDefault     = 0x0008,
} OpenCLDeviceType;

typedef enum {
    kOpenCLMemLocal,
    kOpenCLMemGlobal,
} OpenCLMemType;

typedef enum {
    kOpenCLVendorNvidia,
    kOpenCLVendorUnknown,
} OpenCLVendorType;

class OpenCLDeviceSpecific;
class OpenCLKernel;
typedef QMap<QString, OpenCLKernel *> OpenCLKernelMap;

class OpenCLDevice
{
  public:
    OpenCLDevice(cl_device_id device, bool needOpenGL = false);
    ~OpenCLDevice();
    void Display(void);
    bool Initialize(void);
    uint64_t GetHash(void);
    bool RegisterKernel(QString entry, QString filename);
    OpenCLKernel *GetKernel(QString name) 
        { return m_kernelMap.value(name, NULL); };

//  private:
    bool m_valid;
    cl_device_id m_deviceId;
    cl_context m_context;
    cl_command_queue m_commandQ;
    cl_context_properties *m_props;
    QString m_name;
    QString m_vendor;
    QString m_driverVersion;
    QString m_deviceVersion;
    QString m_openclCVersion;
    OpenCLDeviceType m_deviceType;
    OpenCLKernelMap  m_kernelMap;

    cl_uint m_maxComputeUnits;
    size_t m_maxWorkItemSizes[3];
    size_t m_maxWorkGroupSize;
    cl_uint m_maxClockFrequency;  // MHz
    cl_uint m_addressBits;
    int m_maxMemAllocSize;    // MByte
    int m_globalMemSize;      // MByte
    bool m_errCorrection;
    OpenCLMemType m_memType;
    int m_localMemSize;       // kByte
    int m_maxConstBufSize;    // kByte
    cl_command_queue_properties m_queueProperties;
    bool m_imageSupport;
    cl_uint m_maxReadImageArgs;
    cl_uint m_maxWriteImageArgs;
    cl_device_fp_config m_fpConfig;
    size_t m_max2DImageSize[2];
    size_t m_max3DImageSize[3];
    QStringList m_extensions;
    OpenCLVendorType m_vendorType;
    OpenCLDeviceSpecific *m_vendorSpecific;
    cl_uint m_vectorWidth[6]; // char, short, int, long, float, double

    bool m_float64;
    bool m_opengl;
};

class OpenCLDeviceSpecific
{
  public:
    OpenCLDeviceSpecific()  {};
    ~OpenCLDeviceSpecific() {};
    virtual void Display(void) = 0;
};

class OpenCLDeviceNvidiaSpecific : public OpenCLDeviceSpecific
{
  public:
    OpenCLDeviceNvidiaSpecific(OpenCLDevice *parent);
    ~OpenCLDeviceNvidiaSpecific() {};
    void Display(void);

//  protected:
    cl_uint m_computeCapMajor;
    cl_uint m_computeCapMinor;
    cl_uint m_multiprocessors;
    cl_uint m_cudaCores;
    cl_uint m_regsPerBlock;
    cl_uint m_warpSize;
    bool m_gpuOverlap;
    bool m_kernelExecTimeout;
    bool m_integratedMem;

  private:
    OpenCLDevice *m_parent;
};

class OpenCLDeviceMap : public QMap<uint64_t, OpenCLDevice *>
{
  public:
    OpenCLDeviceMap();
    ~OpenCLDeviceMap();

    void Display(void);
    OpenCLDevice *GetBestDevice(OpenCLDevice *prev, bool forceSw);

    bool m_valid;
};

class OpenCLKernel
{
  public:
    OpenCLKernel(OpenCLDevice *device, QString name, cl_program program,
                 cl_kernel kernel) :
        m_device(device), m_name(name), m_program(program), m_kernel(kernel) {};
    ~OpenCLKernel();

    static OpenCLKernel *Create(OpenCLDevice *device, QString entry,
                                QString filename);

    OpenCLDevice *m_device;
    QString m_name;
    cl_program m_program;
    cl_kernel m_kernel;
};

typedef struct {
    OpenCLKernel *kernel;
    const char   *entry;
    const char   *filename;
} OpenCLKernelDef;

extern OpenCLDeviceMap *devMap;

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */

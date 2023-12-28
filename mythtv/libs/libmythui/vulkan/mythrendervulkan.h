#ifndef MYTHRENDERVULKAN_H
#define MYTHRENDERVULKAN_H

// C++
#include <array>
#include <optional>

// Qt
#include <QVulkanWindowRenderer>
#include <QVulkanDeviceFunctions>

// MythTV
#include "libmythui/mythuiexp.h"
#include "libmythui/mythrender_base.h"

class MythImage;
class MythDebugVulkan;
class MythRenderVulkan;
class MythWindowVulkan;
class MythShaderVulkan;
class MythTextureVulkan;

using MythVulkan4F = std::array<float,4>;

// workaround 32bit Vulkan defines
#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#define MYTH_NULL_DISPATCH nullptr
#else
#define MYTH_NULL_DISPATCH 0
#endif

class MUI_PUBLIC MythVulkanObject
{
  public:
    static MythVulkanObject* Create(MythRenderVulkan* Render);
    MythVulkanObject(MythRenderVulkan* Render);
    MythVulkanObject(MythVulkanObject* Other);

    bool                    IsValidVulkan() const;
    MythRenderVulkan*       Render ();
    VkDevice                Device ();
    QVulkanDeviceFunctions* Funcs  ();
    MythWindowVulkan*       Window ();

  protected:
    void                    CheckValid();
    bool                    m_vulkanValid  { true    };
    MythRenderVulkan*       m_vulkanRender { nullptr };
    VkDevice                m_vulkanDevice { nullptr };
    QVulkanDeviceFunctions* m_vulkanFuncs  { nullptr };
    MythWindowVulkan*       m_vulkanWindow { nullptr };

  private:
    Q_DISABLE_COPY(MythVulkanObject)
};

class MUI_PUBLIC MythRenderVulkan : public QObject, public QVulkanWindowRenderer, public MythRender
{
    Q_OBJECT

  public:

    static MythRenderVulkan* GetVulkanRender(void);

    MythRenderVulkan();
   ~MythRenderVulkan() override;

    void SetVulkanWindow           (MythWindowVulkan *VulkanWindow);
    MythWindowVulkan* GetVulkanWindow(void);
    void SetFrameExpected          (void);
    bool GetFrameExpected          (void) const;
    bool GetFrameStarted           (void) const;
    void EndFrame                  (void);
    QStringList GetDescription(void) override;

    void preInitResources          (void) override;
    void initResources             (void) override;
    void initSwapChainResources    (void) override;
    void releaseSwapChainResources (void) override;
    void releaseResources          (void) override;
    void startNextFrame            (void) override;
    void physicalDeviceLost        (void) override;
    void logicalDeviceLost         (void) override;

    void            BeginDebugRegion(VkCommandBuffer CommandBuffer, const char *Name,
                                     MythVulkan4F Color);
    void            EndDebugRegion(VkCommandBuffer CommandBuffer);
    bool            CreateImage(QSize Size, VkFormat Format, VkImageTiling Tiling,
                                VkImageUsageFlags Usage, VkMemoryPropertyFlags Properties,
                                VkImage& Image, VkDeviceMemory& ImageMemory);
    void            TransitionImageLayout(VkImage &Image, VkImageLayout OldLayout,
                                          VkImageLayout NewLayout, VkCommandBuffer CommandBuffer = nullptr);
    void            CopyBufferToImage(VkBuffer Buffer, VkImage Image,
                                      uint32_t Width, uint32_t Height,
                                      VkCommandBuffer CommandBuffer = nullptr);
    bool            CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage,
                                 VkMemoryPropertyFlags Properties, VkBuffer& Buffer,
                                 VkDeviceMemory& Memory);
    void            CopyBuffer(VkBuffer Src, VkBuffer Dst, VkDeviceSize Size,
                               VkCommandBuffer CommandBuffer = nullptr);
    VkPipeline      CreatePipeline(MythShaderVulkan* Shader, QRect Viewport,
                                   std::vector<VkDynamicState> Dynamic = { });
    VkCommandBuffer CreateSingleUseCommandBuffer(void);
    void            FinishSingleUseCommandBuffer(VkCommandBuffer &Buffer);
    VkSampler       CreateSampler(VkFilter Min, VkFilter Mag);
    VkPhysicalDeviceFeatures GetPhysicalDeviceFeatures() const;
    VkPhysicalDeviceLimits   GetPhysicalDeviceLimits  () const;

  signals:
    void DoFreeResources    (void);
    void PhysicalDeviceLost (void);
    void LogicalDeviceLost  (void);

  private:
    Q_DISABLE_COPY(MythRenderVulkan)

    std::optional<uint32_t> FindMemoryType(uint32_t Filter, VkMemoryPropertyFlags Properties);

    void BeginFrame                (void);
    void DebugVulkan               (void);

    MythWindowVulkan*  m_window        { nullptr };
    VkDevice           m_device        { nullptr };
    QVulkanFunctions*  m_funcs         { nullptr };
    QVulkanDeviceFunctions *m_devFuncs { nullptr };
    VkPhysicalDeviceFeatures m_phyDevFeatures {  };
    VkPhysicalDeviceLimits   m_phyDevLimits   {  };
    bool               m_frameExpected { false   };
    bool               m_frameStarted  { false   };
    QMap<QString,QString> m_debugInfo;
    MythDebugVulkan*   m_debugMarker   { nullptr };
};

#endif

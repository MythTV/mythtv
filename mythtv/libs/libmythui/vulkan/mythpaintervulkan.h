#ifndef MYTHPAINTERVULKAN_H
#define MYTHPAINTERVULKAN_H

// Qt
#include <QStack>

// MythTV
#include "mythuiexp.h"
#include "mythpainter.h"
#include "mythuianimation.h"
#include "vulkan/mythrendervulkan.h"
#include "vulkan/mythwindowvulkan.h"

class MythDebugVulkan;
class MythUniformBufferVulkan;

#define MAX_TEXTURE_COUNT (1000)

class MUI_PUBLIC MythPainterVulkan : public MythPainter
{
    Q_OBJECT

  public:
    MythPainterVulkan(MythRenderVulkan *VulkanRender, MythWindowVulkan *VulkanWindow);
   ~MythPainterVulkan() override;

    QString GetName           (void) override;
    bool    SupportsAnimation (void) override;
    bool    SupportsAlpha     (void) override;
    bool    SupportsClipping  (void) override;
    void    FreeResources     (void) override;
    void    Begin             (QPaintDevice* /*Parent*/) override;
    void    End               (void) override;
    void    DrawImage         (const QRect &Dest, MythImage *Image, const QRect &Source, int Alpha) override;
    void    PushTransformation(const UIEffects &Fx, QPointF Center = QPointF()) override;
    void    PopTransformation(void) override;

    void    DeleteTextures    (void);
    void    SetMaster         (bool Master);

  public slots:
    void    DoFreeResources   (void);

  protected:
    MythImage* GetFormatImagePriv (void) override;
    void    DeleteFormatImagePriv (MythImage *Image) override;

  private:
    Q_DISABLE_COPY(MythPainterVulkan)

    bool Ready     (void);
    void ClearCache(void);
    MythTextureVulkan* GetTextureFromCache(MythImage *Image);

    bool              m_master           { true    };
    MythWindowVulkan* m_window           { nullptr };
    MythRenderVulkan* m_render           { nullptr };
    VkDevice          m_device           { nullptr };
    QVulkanDeviceFunctions* m_devFuncs   { nullptr };

    VkDescriptorPool  m_projectionDescriptorPool { nullptr };
    VkDescriptorSet   m_projectionDescriptor { nullptr };
    MythUniformBufferVulkan* m_projectionUniform { nullptr };
    VkSampler         m_textureSampler   { nullptr };
    MythShaderVulkan* m_textureShader    { nullptr };
    VkPipelineLayout  m_textureLayout    { nullptr };
    VkPipeline        m_texturePipeline  { nullptr };
    VkDescriptorPool  m_textureDescriptorPool { nullptr };
    bool              m_textureDescriptorsCreated { false };
    std::vector<VkDescriptorSet> m_availableTextureDescriptors;
    VkCommandBuffer   m_textureUploadCmd { nullptr };

    bool              m_frameStarted     { false   };
    QSize             m_lastSize         { 0, 0    };

    std::vector<MythTextureVulkan*>      m_stagedTextures;
    std::vector<MythTextureVulkan*>      m_queuedTextures;
    QMap<MythImage*, MythTextureVulkan*> m_imageToTextureMap;
    std::list<MythImage*>                m_imageExpire;
    QVector<MythTextureVulkan*>          m_texturesToDelete;

    QMatrix4x4         m_projection;
    QStack<QMatrix4x4> m_transforms;

    MythDebugVulkan*   m_debugMarker     { nullptr };
    bool               m_debugAvailable  { true    };
};

#endif

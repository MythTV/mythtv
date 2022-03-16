#ifndef MYTHPAINTERVULKAN_H
#define MYTHPAINTERVULKAN_H

// Qt
#include <QStack>

// MythTV
#include "libmythui/mythuiexp.h"
#include "libmythui/mythpaintergpu.h"
#include "libmythui/mythuianimation.h"
#include "libmythui/vulkan/mythrendervulkan.h"
#include "libmythui/vulkan/mythwindowvulkan.h"

class MythDebugVulkan;
class MythUniformBufferVulkan;

#define MAX_TEXTURE_COUNT (1000)

class MUI_PUBLIC MythPainterVulkan : public MythPainterGPU
{
    Q_OBJECT

  public:
    MythPainterVulkan(MythRenderVulkan* VulkanRender, MythMainWindow* Parent);
   ~MythPainterVulkan() override;

    QString GetName           () override;
    bool    SupportsAnimation () override;
    bool    SupportsAlpha     () override;
    bool    SupportsClipping  () override;
    void    FreeResources     () override;
    void    Begin             (QPaintDevice* /*Parent*/) override;
    void    End               () override;
    void    DrawImage         (QRect Dest, MythImage *Image, QRect Source, int Alpha) override;
    void    PushTransformation(const UIEffects &Fx, QPointF Center = QPointF()) override;
    void    PopTransformation () override;

    void    DeleteTextures    ();

  public slots:
    void    DoFreeResources   ();

  protected:
    MythImage* GetFormatImagePriv () override;
    void    DeleteFormatImagePriv (MythImage *Image) override;

  private:
    Q_DISABLE_COPY(MythPainterVulkan)

    bool Ready     ();
    void ClearCache();
    MythTextureVulkan* GetTextureFromCache(MythImage *Image);

    bool              m_ready  { false   };
    MythVulkanObject* m_vulkan { nullptr };

    VkDescriptorPool  m_projectionDescriptorPool  { MYTH_NULL_DISPATCH };
    VkDescriptorSet   m_projectionDescriptor      { MYTH_NULL_DISPATCH };
    MythUniformBufferVulkan* m_projectionUniform  { nullptr };
    VkSampler         m_textureSampler            { MYTH_NULL_DISPATCH };
    MythShaderVulkan* m_textureShader             { nullptr };
    VkPipeline        m_texturePipeline           { MYTH_NULL_DISPATCH };
    VkDescriptorPool  m_textureDescriptorPool     { MYTH_NULL_DISPATCH };
    bool              m_textureDescriptorsCreated { false };
    std::vector<VkDescriptorSet> m_availableTextureDescriptors;
    VkCommandBuffer   m_textureUploadCmd          { nullptr };
    bool              m_frameStarted              { false   };

    std::vector<MythTextureVulkan*>      m_stagedTextures;
    std::vector<MythTextureVulkan*>      m_queuedTextures;
    QMap<MythImage*, MythTextureVulkan*> m_imageToTextureMap;
    std::list<MythImage*>                m_imageExpire;
    QVector<MythTextureVulkan*>          m_texturesToDelete;

    QMatrix4x4         m_projection;
    QStack<QMatrix4x4> m_transforms;
};

#endif

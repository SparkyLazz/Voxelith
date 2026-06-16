#pragma once

#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4324 4505)
#include <vulkan/vulkan.h>
#pragma warning(pop)

#include <string>

// Named GfxPipeline to avoid collision with the VkPipeline Vulkan handle typedef.
class GfxPipeline {
public:
    GfxPipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    ~GfxPipeline();

    GfxPipeline(const GfxPipeline&)            = delete;
    GfxPipeline& operator=(const GfxPipeline&) = delete;

    VkPipelineLayout layout()   const { return m_layout; }
    VkPipeline       pipeline() const { return m_pipeline; }

private:
    static VkShaderModule loadShaderModule(VkDevice device, const std::string& path);

    VkDevice         m_device      = VK_NULL_HANDLE;
    VkFormat         m_colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat         m_depthFormat = VK_FORMAT_UNDEFINED;
    VkPipelineLayout m_layout      = VK_NULL_HANDLE;
    VkPipeline       m_pipeline    = VK_NULL_HANDLE;
};

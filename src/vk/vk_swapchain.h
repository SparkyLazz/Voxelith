#pragma once

#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4324 4505)
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#pragma warning(pop)

#include <cstdint>
#include <vector>

class VkContext;

class VkSwapchain {
public:
    VkSwapchain(VkContext& ctx, uint32_t width, uint32_t height);
    ~VkSwapchain();

    VkSwapchain(const VkSwapchain&)            = delete;
    VkSwapchain& operator=(const VkSwapchain&) = delete;

    void recreate(uint32_t width, uint32_t height);

    VkSwapchainKHR                  handle()      const { return m_swapchain.swapchain; }
    VkFormat                        colorFormat() const { return m_swapchain.image_format; }
    VkFormat                        depthFormat() const { return m_depthFormat; }
    VkExtent2D                      extent()      const { return m_swapchain.extent; }
    const std::vector<VkImage>&     images()      const { return m_images; }
    const std::vector<VkImageView>& imageViews()  const { return m_imageViews; }
    VkImage                         depthImage()  const { return m_depthImage; }
    VkImageView                     depthView()   const { return m_depthView; }
    uint32_t                        imageCount()  const { return static_cast<uint32_t>(m_images.size()); }

private:
    void build(uint32_t width, uint32_t height);
    void destroy();
    void createDepthResources();
    void destroyDepthResources();

    VkContext&               m_ctx;
    vkb::Swapchain           m_swapchain;
    std::vector<VkImage>     m_images;
    std::vector<VkImageView> m_imageViews;

    VkFormat      m_depthFormat     = VK_FORMAT_D32_SFLOAT;
    VkImage       m_depthImage      = VK_NULL_HANDLE;
    VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
    VkImageView   m_depthView       = VK_NULL_HANDLE;
};

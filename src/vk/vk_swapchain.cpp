#include "vk_swapchain.h"
#include "vk_context.h"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>

static const char* vkFormatToString(VkFormat fmt) {
    switch (fmt) {
        case VK_FORMAT_B8G8R8A8_SRGB:  return "B8G8R8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_SRGB:  return "R8G8B8A8_SRGB";
        case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
        default:                        return "unknown";
    }
}

VkSwapchain::VkSwapchain(VkContext& ctx, uint32_t width, uint32_t height)
    : m_ctx(ctx) {
    build(width, height);
    createDepthResources();
}

VkSwapchain::~VkSwapchain() {
    destroyDepthResources();
    destroy();
}

void VkSwapchain::recreate(uint32_t width, uint32_t height) {
    destroyDepthResources();
    destroy();
    build(width, height);
    createDepthResources();
}

void VkSwapchain::build(uint32_t width, uint32_t height) {
    auto scResult = vkb::SwapchainBuilder{m_ctx.vkbDevice()}
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build();

    if (!scResult) {
        throw std::runtime_error(std::string("vkb::SwapchainBuilder failed: ") +
                                 scResult.error().message());
    }
    m_swapchain = scResult.value();

    auto imgs  = m_swapchain.get_images();
    auto views = m_swapchain.get_image_views();
    if (!imgs || !views) {
        throw std::runtime_error("Failed to get swapchain images/views");
    }

    m_images     = imgs.value();
    m_imageViews = views.value();

    spdlog::info("Swapchain: {}x{}, format {}, {} images",
                 width, height,
                 vkFormatToString(m_swapchain.image_format),
                 m_images.size());
}

void VkSwapchain::destroy() {
    VkDevice dev = m_ctx.device();
    for (auto view : m_imageViews) {
        vkDestroyImageView(dev, view, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();
    vkb::destroy_swapchain(m_swapchain);
}

void VkSwapchain::createDepthResources() {
    VkDevice      dev       = m_ctx.device();
    VmaAllocator  allocator = m_ctx.allocator();
    const VkExtent2D ext    = m_swapchain.extent;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = m_depthFormat;
    imageInfo.extent        = {ext.width, ext.height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(allocator, &imageInfo, &allocInfo,
                       &m_depthImage, &m_depthAllocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateImage failed for depth buffer");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image            = m_depthImage;
    viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format           = m_depthFormat;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

    if (vkCreateImageView(dev, &viewInfo, nullptr, &m_depthView) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImageView failed for depth buffer");
    }

    spdlog::debug("Depth resources created ({}x{}, D32_SFLOAT)", ext.width, ext.height);
}

void VkSwapchain::destroyDepthResources() {
    VkDevice     dev       = m_ctx.device();
    VmaAllocator allocator = m_ctx.allocator();

    if (m_depthView != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, m_depthView, nullptr);
        m_depthView = VK_NULL_HANDLE;
    }
    if (m_depthImage != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, m_depthImage, m_depthAllocation);
        m_depthImage      = VK_NULL_HANDLE;
        m_depthAllocation = VK_NULL_HANDLE;
    }
}

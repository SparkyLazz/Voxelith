#include "vk_context.h"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// helpers (file-local)
// ---------------------------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*) {

    const char* typeStr = vkb::to_string_message_type(type);
    const char* msg     = data->pMessage;

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        spdlog::error("[VK {}] {}", typeStr, msg);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        spdlog::warn("[VK {}] {}", typeStr, msg);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        spdlog::info("[VK {}] {}", typeStr, msg);
    else
        spdlog::trace("[VK {}] {}", typeStr, msg);

    return VK_FALSE;
}

// ---------------------------------------------------------------------------
// VkContext
// ---------------------------------------------------------------------------

VkContext::VkContext(GLFWwindow* window) {
    // Instance
    auto instResult = vkb::InstanceBuilder{}
        .set_app_name("VoxelEditor")
        .set_engine_name("VoxelEditor")
        .require_api_version(1, 3, 0)
        .request_validation_layers(
#ifdef NDEBUG
            false
#else
            true
#endif
        )
        .set_debug_callback(debugCallback)
        .set_debug_messenger_severity(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        .build();

    if (!instResult) {
        throw std::runtime_error(std::string("vkb::InstanceBuilder failed: ") +
                                 instResult.error().message());
    }
    m_instance = instResult.value();

    // Surface
    if (glfwCreateWindowSurface(m_instance.instance, window, nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("glfwCreateWindowSurface failed");
    }

    // Physical device
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    auto physResult = vkb::PhysicalDeviceSelector{m_instance}
        .set_surface(m_surface)
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
        .select();

    if (!physResult) {
        throw std::runtime_error(std::string("Physical device selection failed: ") +
                                 physResult.error().message());
    }

    const auto& phys = physResult.value();
    spdlog::info("GPU: {} (driver {})", phys.name, phys.properties.driverVersion);

    // Logical device
    auto devResult = vkb::DeviceBuilder{phys}.build();
    if (!devResult) {
        throw std::runtime_error(std::string("vkb::DeviceBuilder failed: ") +
                                 devResult.error().message());
    }
    m_device = devResult.value();

    auto gqResult = m_device.get_queue(vkb::QueueType::graphics);
    auto pqResult = m_device.get_queue(vkb::QueueType::present);
    auto gfResult = m_device.get_queue_index(vkb::QueueType::graphics);
    auto pfResult = m_device.get_queue_index(vkb::QueueType::present);

    if (!gqResult || !pqResult || !gfResult || !pfResult) {
        throw std::runtime_error("Failed to get required queues");
    }

    m_graphicsQueue       = gqResult.value();
    m_presentQueue        = pqResult.value();
    m_graphicsQueueFamily = gfResult.value();

    spdlog::info("Queue families \xe2\x80\x94 graphics: {}, present: {}",
                 gfResult.value(), pfResult.value());

    // VMA allocator
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.instance         = m_instance.instance;
    allocatorInfo.physicalDevice   = m_device.physical_device;
    allocatorInfo.device           = m_device.device;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateAllocator failed");
    }
    spdlog::debug("VMA allocator created");
}

VkContext::~VkContext() {
    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
    }

    vkb::destroy_device(m_device);

    if (m_surface != VK_NULL_HANDLE) {
        vkb::destroy_surface(m_instance, m_surface);
    }

    vkb::destroy_instance(m_instance);
}

void VkContext::waitIdle() const {
    if (m_device.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device.device);
    }
}

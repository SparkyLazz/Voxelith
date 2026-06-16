#pragma once

#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4324 4505)
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#pragma warning(pop)

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>

class VkContext {
public:
    explicit VkContext(GLFWwindow* window);
    ~VkContext();

    VkContext(const VkContext&)            = delete;
    VkContext& operator=(const VkContext&) = delete;

    VkInstance       instance()            const { return m_instance.instance; }
    VkPhysicalDevice physicalDevice()      const { return m_device.physical_device.physical_device; }
    VkDevice         device()              const { return m_device.device; }
    VkSurfaceKHR     surface()             const { return m_surface; }
    VkQueue          graphicsQueue()       const { return m_graphicsQueue; }
    VkQueue          presentQueue()        const { return m_presentQueue; }
    uint32_t         graphicsQueueFamily() const { return m_graphicsQueueFamily; }
    VmaAllocator     allocator()           const { return m_allocator; }

    vkb::Instance& vkbInstance() { return m_instance; }
    vkb::Device&   vkbDevice()   { return m_device; }

    void waitIdle() const;

private:
    vkb::Instance m_instance;
    vkb::Device   m_device;
    VkSurfaceKHR  m_surface             = VK_NULL_HANDLE;
    VkQueue       m_graphicsQueue       = VK_NULL_HANDLE;
    VkQueue       m_presentQueue        = VK_NULL_HANDLE;
    uint32_t      m_graphicsQueueFamily = 0;
    VmaAllocator  m_allocator           = VK_NULL_HANDLE;
};

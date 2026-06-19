#pragma once

#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4324 4505)
#include <vulkan/vulkan.h>
#pragma warning(pop)

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>

class VkContext;
class VkSwapchain;

class VkImGui {
public:
    VkImGui(const VkContext& ctx, const VkSwapchain& swapchain,
            GLFWwindow* window, uint32_t imageCount);
    ~VkImGui();

    VkImGui(const VkImGui&)            = delete;
    VkImGui& operator=(const VkImGui&) = delete;

    void beginFrame(float camX, float camY, float camZ);
    void endFrame();
    void recordDraw(VkCommandBuffer cmd);
    void onSwapchainRecreate(uint32_t newImageCount);

private:
    VkDevice         m_device         = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
};

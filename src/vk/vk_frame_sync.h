#pragma once

#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4324 4505)
#include <vulkan/vulkan.h>
#pragma warning(pop)

#include <array>
#include <cstdint>

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

class VkFrameSync {
public:
    VkFrameSync(VkDevice device, uint32_t queueFamily);
    ~VkFrameSync();

    VkFrameSync(const VkFrameSync&)            = delete;
    VkFrameSync& operator=(const VkFrameSync&) = delete;

    VkCommandBuffer currentCmdBuffer()        const { return m_commandBuffers[m_currentFrame]; }
    VkSemaphore     imageAvailableSemaphore() const { return m_frames[m_currentFrame].imageAvailable; }
    VkSemaphore     renderFinishedSemaphore() const { return m_frames[m_currentFrame].renderFinished; }
    VkFence         inFlightFence()           const { return m_frames[m_currentFrame].inFlight; }
    uint32_t        currentFrame()            const { return m_currentFrame; }

    void advance() { m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT; }

private:
    struct FrameData {
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        VkFence     inFlight       = VK_NULL_HANDLE;
    };

    VkDevice      m_device       = VK_NULL_HANDLE;
    uint32_t      m_currentFrame = 0;

    VkCommandPool m_commandPool  = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_commandBuffers{};
    std::array<FrameData,       MAX_FRAMES_IN_FLIGHT> m_frames{};
};

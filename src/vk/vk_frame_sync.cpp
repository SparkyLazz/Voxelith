#include "vk_frame_sync.h"

#include <spdlog/spdlog.h>

#include <stdexcept>

VkFrameSync::VkFrameSync(VkDevice device, uint32_t queueFamily)
    : m_device(device) {

    // Command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamily;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool failed");
    }

    // Command buffers
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateCommandBuffers failed");
    }

    // Per-frame sync objects
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& f : m_frames) {
        if (vkCreateSemaphore(m_device, &semInfo, nullptr, &f.imageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semInfo, nullptr, &f.renderFinished) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &f.inFlight) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create per-frame sync objects");
        }
    }

    spdlog::debug("Frame sync objects created ({} frames in flight)", MAX_FRAMES_IN_FLIGHT);
}

VkFrameSync::~VkFrameSync() {
    if (m_device == VK_NULL_HANDLE) return;

    for (auto& f : m_frames) {
        if (f.imageAvailable != VK_NULL_HANDLE)
            vkDestroySemaphore(m_device, f.imageAvailable, nullptr);
        if (f.renderFinished != VK_NULL_HANDLE)
            vkDestroySemaphore(m_device, f.renderFinished, nullptr);
        if (f.inFlight != VK_NULL_HANDLE)
            vkDestroyFence(m_device, f.inFlight, nullptr);
    }

    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }
}

#include "renderer.h"

#include "vk/vk_context.h"
#include "vk/vk_swapchain.h"
#include "vk/vk_frame_sync.h"
#include "vk/vk_pipeline.h"
#include "vk/vk_mesh.h"
#include "vk/vk_imgui.h"

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <memory>

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct Renderer::Impl {
    // Declaration order = reverse destruction order (imgui last in, first out).
    // Context must outlive everything — declared first, destroyed last.
    std::unique_ptr<VkContext>   context;
    std::unique_ptr<VkSwapchain> swapchain;
    std::unique_ptr<VkFrameSync> frameSync;
    std::unique_ptr<GfxPipeline> pipeline;
    std::unique_ptr<VkMesh>      mesh;
    std::unique_ptr<VkImGui>     imgui;

    GLFWwindow* window          = nullptr;
    bool        resizeRequested = false;
};

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

Renderer::Renderer(GLFWwindow* window) : m_impl(std::make_unique<Impl>()) {
    spdlog::info("SHADER_DIR: {}", SHADER_DIR);

    auto& s  = *m_impl;
    s.window = window;

    s.context = std::make_unique<VkContext>(window);

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    s.swapchain = std::make_unique<VkSwapchain>(*s.context,
                                                static_cast<uint32_t>(fbW),
                                                static_cast<uint32_t>(fbH));

    s.frameSync = std::make_unique<VkFrameSync>(s.context->device(),
                                                s.context->graphicsQueueFamily());

    s.pipeline = std::make_unique<GfxPipeline>(s.context->device(),
                                               s.swapchain->colorFormat(),
                                               s.swapchain->depthFormat());

    s.mesh = std::make_unique<VkMesh>(s.context->allocator(), s.context->device());

    s.imgui = std::make_unique<VkImGui>(*s.context, *s.swapchain,
                                        window, s.swapchain->imageCount());

    spdlog::info("Renderer initialized");
}

Renderer::~Renderer() {
    // Wait for GPU before the unique_ptrs unwind in reverse declaration order:
    // imgui → mesh → pipeline → frameSync → swapchain → context
    if (m_impl && m_impl->context) {
        m_impl->context->waitIdle();
    }
    spdlog::info("Renderer destroyed");
}

void Renderer::notifyResize() {
    m_impl->resizeRequested = true;
}

void Renderer::set_mesh(const std::vector<RendererVertex>& vertices,
                         const std::vector<uint32_t>&        indices) {
    auto& s = *m_impl;
    s.context->waitIdle();
    s.mesh->upload(
        vertices.data(),
        static_cast<VkDeviceSize>(vertices.size() * sizeof(RendererVertex)),
        indices.data(),
        static_cast<uint32_t>(indices.size())
    );
}

// ---------------------------------------------------------------------------
// beginImGuiFrame
// ---------------------------------------------------------------------------

void Renderer::beginImGuiFrame(Vec3 cameraPos) {
    m_impl->imgui->beginFrame(cameraPos.x, cameraPos.y, cameraPos.z);
}

// ---------------------------------------------------------------------------
// drawFrame
// ---------------------------------------------------------------------------

void Renderer::drawFrame(const ViewMatrix& viewIn) {
    auto& s = *m_impl;

    // Skip when minimized — but still end the ImGui frame so NewFrame/Render stay paired.
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(s.window, &fbW, &fbH);
    if (fbW == 0 || fbH == 0) {
        s.imgui->endFrame();
        return;
    }

    if (s.resizeRequested) {
        s.context->waitIdle();
        s.swapchain->recreate(static_cast<uint32_t>(fbW), static_cast<uint32_t>(fbH));
        s.imgui->onSwapchainRecreate(s.swapchain->imageCount());
        s.resizeRequested = false;
    }

    VkDevice        device   = s.context->device();
    VkCommandBuffer cmd      = s.frameSync->currentCmdBuffer();
    VkFence         fence    = s.frameSync->inFlightFence();
    VkSemaphore     imgAvail = s.frameSync->imageAvailableSemaphore();
    VkSemaphore     rendDone = s.frameSync->renderFinishedSemaphore();

    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex    = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        device, s.swapchain->handle(),
        UINT64_MAX, imgAvail, VK_NULL_HANDLE, &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        s.resizeRequested = true;
        s.imgui->endFrame();
        return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        spdlog::error("vkAcquireNextImageKHR failed: {}", static_cast<int>(acquireResult));
        s.imgui->endFrame();
        return;
    }
    if (acquireResult == VK_SUBOPTIMAL_KHR) {
        s.resizeRequested = true;
    }

    vkResetFences(device, 1, &fence);
    vkResetCommandBuffer(cmd, 0);

    const glm::mat4 model   = glm::mat4(1.0f);
    const glm::mat4 view    = glm::make_mat4(viewIn.m);
    const VkExtent2D ext    = s.swapchain->extent();
    const float      aspect = static_cast<float>(ext.width) / static_cast<float>(ext.height);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 5000.0f);
    proj[1][1] *= -1.0f;  // flip Y for Vulkan NDC
    const glm::mat4 mvp = proj * view * model;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Two barriers: color UNDEFINED→COLOR_ATTACHMENT, depth UNDEFINED→DEPTH_ATTACHMENT
    VkImageMemoryBarrier2 barriers[2]{};

    barriers[0].sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barriers[0].srcStageMask     = VK_PIPELINE_STAGE_2_NONE;
    barriers[0].srcAccessMask    = VK_ACCESS_2_NONE;
    barriers[0].dstStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barriers[0].dstAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[0].oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[0].image            = s.swapchain->images()[imageIndex];
    barriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    barriers[1].sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barriers[1].srcStageMask     = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    barriers[1].srcAccessMask    = VK_ACCESS_2_NONE;
    barriers[1].dstStageMask     = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    barriers[1].dstAccessMask    = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barriers[1].oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout        = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barriers[1].image            = s.swapchain->depthImage();
    barriers[1].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

    VkDependencyInfo depInfo{};
    depInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 2;
    depInfo.pImageMemoryBarriers    = barriers;
    vkCmdPipelineBarrier2(cmd, &depInfo);

    VkClearValue clearColor{};
    clearColor.color = {{0.05f, 0.15f, 0.18f, 1.0f}};

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView   = s.swapchain->imageViews()[imageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue  = clearColor;

    VkClearValue clearDepth{};
    clearDepth.depthStencil = {1.0f, 0};

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView   = s.swapchain->depthView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue  = clearDepth;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea           = {{0, 0}, ext};
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachment;
    renderingInfo.pDepthAttachment     = &depthAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(ext.width);
    viewport.height   = static_cast<float>(ext.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, ext};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdPushConstants(cmd, s.pipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(glm::mat4), &mvp);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s.pipeline->pipeline());

    if (s.mesh->indexCount() > 0) {
        const VkDeviceSize offset = 0;
        const VkBuffer     vbuf  = s.mesh->vertexBuffer();
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
        vkCmdBindIndexBuffer(cmd, s.mesh->indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, s.mesh->indexCount(), 1, 0, 0, 0);
    }

    s.imgui->endFrame();
    s.imgui->recordDraw(cmd);

    vkCmdEndRendering(cmd);

    // Barrier: COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR
    VkImageMemoryBarrier2 toPresentBarrier{};
    toPresentBarrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toPresentBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toPresentBarrier.srcAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    toPresentBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_NONE;
    toPresentBarrier.dstAccessMask    = VK_ACCESS_2_NONE;
    toPresentBarrier.oldLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toPresentBarrier.newLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresentBarrier.image            = s.swapchain->images()[imageIndex];
    toPresentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo presentDepInfo{};
    presentDepInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    presentDepInfo.imageMemoryBarrierCount = 1;
    presentDepInfo.pImageMemoryBarriers    = &toPresentBarrier;
    vkCmdPipelineBarrier2(cmd, &presentDepInfo);

    vkEndCommandBuffer(cmd);

    // Submit
    VkSemaphoreSubmitInfo waitSem{};
    waitSem.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSem.semaphore = imgAvail;
    waitSem.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSemaphoreSubmitInfo signalSem{};
    signalSem.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSem.semaphore = rendDone;
    signalSem.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkCommandBufferSubmitInfo cmdSubmit{};
    cmdSubmit.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmit.commandBuffer = cmd;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount   = 1;
    submitInfo.pWaitSemaphoreInfos      = &waitSem;
    submitInfo.commandBufferInfoCount   = 1;
    submitInfo.pCommandBufferInfos      = &cmdSubmit;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos    = &signalSem;

    vkQueueSubmit2(s.context->graphicsQueue(), 1, &submitInfo, fence);

    VkSwapchainKHR swapchainHandle = s.swapchain->handle();

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &rendDone;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapchainHandle;
    presentInfo.pImageIndices      = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(s.context->presentQueue(), &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        s.resizeRequested = true;
    } else if (presentResult != VK_SUCCESS) {
        spdlog::error("vkQueuePresentKHR failed: {}", static_cast<int>(presentResult));
    }

    s.frameSync->advance();
}

#include "vk_imgui.h"
#include "vk_context.h"
#include "vk_swapchain.h"

#pragma warning(push)
#pragma warning(disable : 4100 4127 4201 4244 4245 4267 4305)
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#pragma warning(pop)

#include <spdlog/spdlog.h>

#include <stdexcept>

VkImGui::VkImGui(const VkContext& ctx, const VkSwapchain& swapchain,
                 GLFWwindow* window, uint32_t imageCount)
    : m_device(ctx.device()) {

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // ViewportsEnable deliberately omitted — docking inside main window only.

    ImGui::StyleColorsDark();

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER,                1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000},
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = 1000;
    poolInfo.poolSizeCount = 11;
    poolInfo.pPoolSizes    = poolSizes;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool failed for ImGui");
    }

    ImGui_ImplGlfw_InitForVulkan(window, true);

    // Fields reorganized into PipelineInfoMain in the 2025-09-26 ImGui refactor.
    VkFormat colorFormat = swapchain.colorFormat();

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion      = VK_API_VERSION_1_3;
    initInfo.Instance        = ctx.instance();
    initInfo.PhysicalDevice  = ctx.physicalDevice();
    initInfo.Device          = ctx.device();
    initInfo.QueueFamily     = ctx.graphicsQueueFamily();
    initInfo.Queue           = ctx.graphicsQueue();
    initInfo.DescriptorPool  = m_descriptorPool;
    initInfo.MinImageCount   = 2;
    initInfo.ImageCount      = imageCount;
    initInfo.UseDynamicRendering = true;

    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = 1;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat   = swapchain.depthFormat();

    ImGui_ImplVulkan_Init(&initInfo);

    spdlog::info("ImGui {} initialized", IMGUI_VERSION);
}

VkImGui::~VkImGui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_device != VK_NULL_HANDLE && m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    }
}

void VkImGui::beginFrame(float camX, float camY, float camZ) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::Begin("Stats");
    ImGui::Text("FPS:   %.1f",    ImGui::GetIO().Framerate);
    ImGui::Text("Frame: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Separator();
    ImGui::Text("Camera: (%.2f, %.2f, %.2f)", camX, camY, camZ);
    ImGui::Text("Hold RMB to look. WASD/E/Q move, Shift boosts.");
    ImGui::End();
}

void VkImGui::endFrame() {
    ImGui::Render();
}

void VkImGui::recordDraw(VkCommandBuffer cmd) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void VkImGui::onSwapchainRecreate(uint32_t newImageCount) {
    ImGui_ImplVulkan_SetMinImageCount(newImageCount);
}

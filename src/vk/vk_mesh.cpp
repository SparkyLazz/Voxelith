#include "vk_mesh.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <stdexcept>
#include <vector>

VkMesh::VkMesh(VmaAllocator allocator, VkDevice device)
    : m_allocator(allocator)
    , m_device(device) {

    // Vertex buffer — 8 unique-colored cube corners
    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},  // 0 red
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},  // 1 green
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},  // 2 blue
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},  // 3 yellow
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}},  // 4 magenta
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}},  // 5 cyan
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},  // 6 white
        {{-0.5f,  0.5f,  0.5f}, {0.5f, 0.5f, 0.5f}},  // 7 gray
    };

    const VkDeviceSize vbSize = sizeof(Vertex) * vertices.size();

    VkBufferCreateInfo vbInfo{};
    vbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbInfo.size  = vbSize;
    vbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo vbAllocInfo{};
    vbAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vbAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo vbAllocOut{};
    if (vmaCreateBuffer(m_allocator, &vbInfo, &vbAllocInfo,
                        &m_vertexBuffer, &m_vertexAlloc, &vbAllocOut) != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateBuffer failed for vertex buffer");
    }
    std::memcpy(vbAllocOut.pMappedData, vertices.data(), static_cast<size_t>(vbSize));
    spdlog::debug("Vertex buffer created ({} bytes)", vbSize);

    // Index buffer — 12 triangles (36 indices), CCW winding
    const std::vector<uint16_t> indices = {
        0, 2, 1,  0, 3, 2,   // -Z face
        4, 5, 6,  4, 6, 7,   // +Z face
        0, 4, 7,  0, 7, 3,   // -X face
        1, 2, 6,  1, 6, 5,   // +X face
        0, 1, 5,  0, 5, 4,   // -Y face (bottom)
        3, 7, 6,  3, 6, 2,   // +Y face (top)
    };
    m_indexCount = static_cast<uint32_t>(indices.size());

    const VkDeviceSize ibSize = sizeof(uint16_t) * indices.size();

    VkBufferCreateInfo ibInfo{};
    ibInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ibInfo.size  = ibSize;
    ibInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    VmaAllocationCreateInfo ibAllocInfo{};
    ibAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    ibAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo ibAllocOut{};
    if (vmaCreateBuffer(m_allocator, &ibInfo, &ibAllocInfo,
                        &m_indexBuffer, &m_indexAlloc, &ibAllocOut) != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateBuffer failed for index buffer");
    }
    std::memcpy(ibAllocOut.pMappedData, indices.data(), static_cast<size_t>(ibSize));
    spdlog::debug("Index buffer created ({} indices, {} bytes)", m_indexCount, ibSize);
}

VkMesh::~VkMesh() {
    if (m_allocator == VK_NULL_HANDLE) return;

    if (m_indexBuffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(m_allocator, m_indexBuffer, m_indexAlloc);

    if (m_vertexBuffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(m_allocator, m_vertexBuffer, m_vertexAlloc);
}

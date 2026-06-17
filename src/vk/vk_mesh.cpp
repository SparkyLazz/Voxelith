#include "vk_mesh.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <stdexcept>

VkMesh::VkMesh(VmaAllocator allocator, VkDevice device)
    : m_allocator(allocator)
    , m_device(device) {
    // Starts with no mesh — call upload() to provide geometry.
}

VkMesh::~VkMesh() {
    if (m_allocator == VK_NULL_HANDLE) return;
    if (m_indexBuffer  != VK_NULL_HANDLE)
        vmaDestroyBuffer(m_allocator, m_indexBuffer, m_indexAlloc);
    if (m_vertexBuffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(m_allocator, m_vertexBuffer, m_vertexAlloc);
}

void VkMesh::upload(const void*     vdata, VkDeviceSize vbytes,
                    const uint32_t* idata, uint32_t     icount) {
    // Destroy existing buffers.
    if (m_indexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_indexBuffer, m_indexAlloc);
        m_indexBuffer = VK_NULL_HANDLE;
    }
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_vertexBuffer, m_vertexAlloc);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    m_indexCount = 0;

    if (vbytes == 0 || icount == 0) { return; }

    // Vertex buffer
    VkBufferCreateInfo vbInfo{};
    vbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbInfo.size  = vbytes;
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
    std::memcpy(vbAllocOut.pMappedData, vdata, static_cast<size_t>(vbytes));

    // Index buffer (uint32_t indices)
    const VkDeviceSize ibBytes = static_cast<VkDeviceSize>(icount) * sizeof(uint32_t);
    VkBufferCreateInfo ibInfo{};
    ibInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ibInfo.size  = ibBytes;
    ibInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    VmaAllocationCreateInfo ibAllocInfo{};
    ibAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    ibAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo ibAllocOut{};
    if (vmaCreateBuffer(m_allocator, &ibInfo, &ibAllocInfo,
                        &m_indexBuffer, &m_indexAlloc, &ibAllocOut) != VK_SUCCESS) {
        vmaDestroyBuffer(m_allocator, m_vertexBuffer, m_vertexAlloc);
        m_vertexBuffer = VK_NULL_HANDLE;
        throw std::runtime_error("vmaCreateBuffer failed for index buffer");
    }
    std::memcpy(ibAllocOut.pMappedData, idata, static_cast<size_t>(ibBytes));
    m_indexCount = icount;

    spdlog::debug("[vk_mesh] upload vbytes={} icount={}", vbytes, icount);
}

#pragma once

#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4324 4505)
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#pragma warning(pop)

#include <glm/glm.hpp>

#include <cstdint>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
};

class VkMesh {
public:
    VkMesh(VmaAllocator allocator, VkDevice device);
    ~VkMesh();

    VkMesh(const VkMesh&)            = delete;
    VkMesh& operator=(const VkMesh&) = delete;

    VkBuffer vertexBuffer() const { return m_vertexBuffer; }
    VkBuffer indexBuffer()  const { return m_indexBuffer; }
    uint32_t indexCount()   const { return m_indexCount; }

private:
    VmaAllocator  m_allocator    = VK_NULL_HANDLE;
    VkDevice      m_device       = VK_NULL_HANDLE;

    VkBuffer      m_vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_vertexAlloc  = VK_NULL_HANDLE;
    VkBuffer      m_indexBuffer  = VK_NULL_HANDLE;
    VmaAllocation m_indexAlloc   = VK_NULL_HANDLE;
    uint32_t      m_indexCount   = 0;
};

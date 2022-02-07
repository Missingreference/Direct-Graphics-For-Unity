#include "RenderAPI.h"
#include "PlatformBase.h"

#if SUPPORT_VULKAN

#include <string.h>
#include <map>
#include <vector>
#include <math.h>

// This plugin does not link to the Vulkan loader, easier to support multiple APIs and systems that don't have Vulkan support
#define VK_NO_PROTOTYPES
#include "Unity/IUnityGraphicsVulkan.h"

#define UNITY_USED_VULKAN_API_FUNCTIONS(apply) \
    apply(vkCreateInstance); \
    apply(vkCmdBeginRenderPass); \
    apply(vkCreateBuffer); \
    apply(vkGetPhysicalDeviceMemoryProperties); \
    apply(vkGetBufferMemoryRequirements); \
    apply(vkMapMemory); \
    apply(vkBindBufferMemory); \
    apply(vkBindImageMemory); \
    apply(vkCmdClearColorImage); \
    apply(vkAllocateMemory); \
    apply(vkCreateImageView); \
    apply(vkDestroyBuffer); \
    apply(vkDestroyImage); \
    apply(vkFreeMemory); \
    apply(vkUnmapMemory); \
    apply(vkQueueWaitIdle); \
    apply(vkDeviceWaitIdle); \
    apply(vkCmdCopyBufferToImage); \
    apply(vkCmdCopyImage); \
    apply(vkCmdBlitImage); \
    apply(vkCreateImage); \
    apply(vkGetImageMemoryRequirements); \
    apply(vkFlushMappedMemoryRanges); \
    apply(vkCreatePipelineLayout); \
    apply(vkCreateShaderModule); \
    apply(vkDestroyShaderModule); \
    apply(vkCreateGraphicsPipelines); \
    apply(vkCmdBindPipeline); \
    apply(vkCmdDraw); \
    apply(vkCmdPushConstants); \
    apply(vkCmdBindVertexBuffers); \
    apply(vkDestroyPipeline); \
    apply(vkDestroyPipelineLayout);
    
#define VULKAN_DEFINE_API_FUNCPTR(func) static PFN_##func func
VULKAN_DEFINE_API_FUNCPTR(vkGetInstanceProcAddr);
UNITY_USED_VULKAN_API_FUNCTIONS(VULKAN_DEFINE_API_FUNCPTR);
#undef VULKAN_DEFINE_API_FUNCPTR

static void LoadVulkanAPI(PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkInstance instance)
{
    if (!vkGetInstanceProcAddr && getInstanceProcAddr)
        vkGetInstanceProcAddr = getInstanceProcAddr;

	if (!vkCreateInstance)
		vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");

#define LOAD_VULKAN_FUNC(fn) if (!fn) fn = (PFN_##fn)vkGetInstanceProcAddr(instance, #fn)
    UNITY_USED_VULKAN_API_FUNCTIONS(LOAD_VULKAN_FUNC);
#undef LOAD_VULKAN_FUNC
}

static VKAPI_ATTR void VKAPI_CALL Hook_vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents)
{
    // Change this to 'true' to override the clear color with green
	const bool allowOverrideClearColor = false;
    if (pRenderPassBegin->clearValueCount <= 16 && pRenderPassBegin->clearValueCount > 0 && allowOverrideClearColor)
    {
        VkClearValue clearValues[16] = {};
        memcpy(clearValues, pRenderPassBegin->pClearValues, pRenderPassBegin->clearValueCount * sizeof(VkClearValue));

        VkRenderPassBeginInfo patchedBeginInfo = *pRenderPassBegin;
        patchedBeginInfo.pClearValues = clearValues;
        for (unsigned int i = 0; i < pRenderPassBegin->clearValueCount - 1; ++i)
        {
            clearValues[i].color.float32[0] = 0.0;
            clearValues[i].color.float32[1] = 1.0;
            clearValues[i].color.float32[2] = 0.0;
            clearValues[i].color.float32[3] = 1.0;
        }
        vkCmdBeginRenderPass(commandBuffer, &patchedBeginInfo, contents);
    }
    else
    {
        vkCmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
    }
}

static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
    vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
    VkResult result = vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    if (result == VK_SUCCESS)
        LoadVulkanAPI(vkGetInstanceProcAddr, *pInstance);
 
    return result;
}

static int FindMemoryTypeIndex(VkPhysicalDeviceMemoryProperties const & physicalDeviceMemoryProperties, VkMemoryRequirements const & memoryRequirements, VkMemoryPropertyFlags memoryPropertyFlags)
{
    uint32_t memoryTypeBits = memoryRequirements.memoryTypeBits;

    // Search memtypes to find first index with those properties
    for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < VK_MAX_MEMORY_TYPES; ++memoryTypeIndex)
    {
        if ((memoryTypeBits & 1) == 1)
        {
            // Type is available, does it match user properties?
            if ((physicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)
                return memoryTypeIndex;
        }
        memoryTypeBits >>= 1;
    }

    return -1;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Hook_vkGetInstanceProcAddr(VkInstance device, const char* funcName)
{
    if (!funcName)
        return NULL;

#define INTERCEPT(fn) if (strcmp(funcName, #fn) == 0) return (PFN_vkVoidFunction)&Hook_##fn
    INTERCEPT(vkCreateInstance);
#undef INTERCEPT

    return NULL;
}

static PFN_vkGetInstanceProcAddr UNITY_INTERFACE_API InterceptVulkanInitialization(PFN_vkGetInstanceProcAddr getInstanceProcAddr, void*)
{
    vkGetInstanceProcAddr = getInstanceProcAddr;
    return Hook_vkGetInstanceProcAddr;
}

extern "C" void RenderAPI_Vulkan_OnPluginLoad(IUnityInterfaces* interfaces)
{
    interfaces->Get<IUnityGraphicsVulkan>()->InterceptInitialization(InterceptVulkanInitialization, NULL);
}

struct VulkanBuffer
{
    VkBuffer buffer;
    VkDeviceMemory deviceMemory;
    void* mapped;
    VkDeviceSize sizeInBytes;
    VkDeviceSize deviceMemorySize;
    VkMemoryPropertyFlags deviceMemoryFlags;
};

static VkPipelineLayout CreateTrianglePipelineLayout(VkDevice device)
{
    VkPushConstantRange pushConstantRange;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 64; // single matrix
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    VkPipelineLayout pipelineLayout;
    return vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout) == VK_SUCCESS ? pipelineLayout : VK_NULL_HANDLE;
}

namespace Shader {
// Source of vertex shader (filename: shader.vert)
/*
#version 310 es
layout(location = 0) in highp vec3 vpos;
layout(location = 1) in highp vec4 vcol;
layout(location = 0) out highp vec4 color;
layout(push_constant) uniform PushConstants { mat4 matrix; };
void main() {
    gl_Position = matrix * vec4(vpos, 1.0);
    color = vcol;
}
*/

// Source of fragment shader (filename: shader.frag)
/*
#version 310 es
layout(location = 0) out highp vec4 fragColor;
layout(location = 0) in highp vec4 color;
void main() { fragColor = color; }
*/
// compiled to SPIR-V using:
// %VULKAN_SDK%\bin\glslc -mfmt=num shader.frag shader.vert -c

const uint32_t vertexShaderSpirv[] = {
	0x07230203,0x00010000,0x000d0007,0x00000024,
	0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,
	0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0009000f,0x00000000,0x00000004,0x6e69616d,
	0x00000000,0x0000000a,0x00000016,0x00000020,
	0x00000022,0x00030003,0x00000001,0x00000136,
	0x000a0004,0x475f4c47,0x4c474f4f,0x70635f45,
	0x74735f70,0x5f656c79,0x656e696c,0x7269645f,
	0x69746365,0x00006576,0x00080004,0x475f4c47,
	0x4c474f4f,0x6e695f45,0x64756c63,0x69645f65,
	0x74636572,0x00657669,0x00040005,0x00000004,
	0x6e69616d,0x00000000,0x00060005,0x00000008,
	0x505f6c67,0x65567265,0x78657472,0x00000000,
	0x00060006,0x00000008,0x00000000,0x505f6c67,
	0x7469736f,0x006e6f69,0x00070006,0x00000008,
	0x00000001,0x505f6c67,0x746e696f,0x657a6953,
	0x00000000,0x00030005,0x0000000a,0x00000000,
	0x00060005,0x0000000e,0x68737550,0x736e6f43,
	0x746e6174,0x00000073,0x00050006,0x0000000e,
	0x00000000,0x7274616d,0x00007869,0x00030005,
	0x00000010,0x00000000,0x00040005,0x00000016,
	0x736f7076,0x00000000,0x00040005,0x00000020,
	0x6f6c6f63,0x00000072,0x00040005,0x00000022,
	0x6c6f6376,0x00000000,0x00050048,0x00000008,
	0x00000000,0x0000000b,0x00000000,0x00050048,
	0x00000008,0x00000001,0x0000000b,0x00000001,
	0x00030047,0x00000008,0x00000002,0x00040048,
	0x0000000e,0x00000000,0x00000005,0x00050048,
	0x0000000e,0x00000000,0x00000023,0x00000000,
	0x00050048,0x0000000e,0x00000000,0x00000007,
	0x00000010,0x00030047,0x0000000e,0x00000002,
	0x00040047,0x00000016,0x0000001e,0x00000000,
	0x00040047,0x00000020,0x0000001e,0x00000000,
	0x00040047,0x00000022,0x0000001e,0x00000001,
	0x00020013,0x00000002,0x00030021,0x00000003,
	0x00000002,0x00030016,0x00000006,0x00000020,
	0x00040017,0x00000007,0x00000006,0x00000004,
	0x0004001e,0x00000008,0x00000007,0x00000006,
	0x00040020,0x00000009,0x00000003,0x00000008,
	0x0004003b,0x00000009,0x0000000a,0x00000003,
	0x00040015,0x0000000b,0x00000020,0x00000001,
	0x0004002b,0x0000000b,0x0000000c,0x00000000,
	0x00040018,0x0000000d,0x00000007,0x00000004,
	0x0003001e,0x0000000e,0x0000000d,0x00040020,
	0x0000000f,0x00000009,0x0000000e,0x0004003b,
	0x0000000f,0x00000010,0x00000009,0x00040020,
	0x00000011,0x00000009,0x0000000d,0x00040017,
	0x00000014,0x00000006,0x00000003,0x00040020,
	0x00000015,0x00000001,0x00000014,0x0004003b,
	0x00000015,0x00000016,0x00000001,0x0004002b,
	0x00000006,0x00000018,0x3f800000,0x00040020,
	0x0000001e,0x00000003,0x00000007,0x0004003b,
	0x0000001e,0x00000020,0x00000003,0x00040020,
	0x00000021,0x00000001,0x00000007,0x0004003b,
	0x00000021,0x00000022,0x00000001,0x00050036,
	0x00000002,0x00000004,0x00000000,0x00000003,
	0x000200f8,0x00000005,0x00050041,0x00000011,
	0x00000012,0x00000010,0x0000000c,0x0004003d,
	0x0000000d,0x00000013,0x00000012,0x0004003d,
	0x00000014,0x00000017,0x00000016,0x00050051,
	0x00000006,0x00000019,0x00000017,0x00000000,
	0x00050051,0x00000006,0x0000001a,0x00000017,
	0x00000001,0x00050051,0x00000006,0x0000001b,
	0x00000017,0x00000002,0x00070050,0x00000007,
	0x0000001c,0x00000019,0x0000001a,0x0000001b,
	0x00000018,0x00050091,0x00000007,0x0000001d,
	0x00000013,0x0000001c,0x00050041,0x0000001e,
	0x0000001f,0x0000000a,0x0000000c,0x0003003e,
	0x0000001f,0x0000001d,0x0004003d,0x00000007,
	0x00000023,0x00000022,0x0003003e,0x00000020,
	0x00000023,0x000100fd,0x00010038
};
const uint32_t fragmentShaderSpirv[] = {
    0x07230203,0x00010000,0x000d0006,0x0000000d,
    0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,
    0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0007000f,0x00000004,0x00000004,0x6e69616d,
    0x00000000,0x00000009,0x0000000b,0x00030010,
    0x00000004,0x00000007,0x00030003,0x00000001,
    0x00000136,0x000a0004,0x475f4c47,0x4c474f4f,
    0x70635f45,0x74735f70,0x5f656c79,0x656e696c,
    0x7269645f,0x69746365,0x00006576,0x00080004,
    0x475f4c47,0x4c474f4f,0x6e695f45,0x64756c63,
    0x69645f65,0x74636572,0x00657669,0x00040005,
    0x00000004,0x6e69616d,0x00000000,0x00050005,
    0x00000009,0x67617266,0x6f6c6f43,0x00000072,
    0x00040005,0x0000000b,0x6f6c6f63,0x00000072,
    0x00040047,0x00000009,0x0000001e,0x00000000,
    0x00040047,0x0000000b,0x0000001e,0x00000000,
    0x00020013,0x00000002,0x00030021,0x00000003,
    0x00000002,0x00030016,0x00000006,0x00000020,
    0x00040017,0x00000007,0x00000006,0x00000004,
    0x00040020,0x00000008,0x00000003,0x00000007,
    0x0004003b,0x00000008,0x00000009,0x00000003,
    0x00040020,0x0000000a,0x00000001,0x00000007,
    0x0004003b,0x0000000a,0x0000000b,0x00000001,
    0x00050036,0x00000002,0x00000004,0x00000000,
    0x00000003,0x000200f8,0x00000005,0x0004003d,
    0x00000007,0x0000000c,0x0000000b,0x0003003e,
    0x00000009,0x0000000c,0x000100fd,0x00010038
};
} // namespace Shader

static VkPipeline CreateTrianglePipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, VkPipelineCache pipelineCache)
{
    if (pipelineLayout == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;  
    if (device == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;
    if (renderPass == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;

    bool success = true;
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
     
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].pName = "main";
    
    if (success)
    {
        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = sizeof(Shader::vertexShaderSpirv);
        moduleCreateInfo.pCode = Shader::vertexShaderSpirv;
        success = vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderStages[0].module) == VK_SUCCESS;
    }

    if (success)
    {
        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = sizeof(Shader::fragmentShaderSpirv);
        moduleCreateInfo.pCode = Shader::fragmentShaderSpirv;
        success = vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderStages[1].module) == VK_SUCCESS;
    }

    VkPipeline pipeline;
    if (success)
    {
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.layout = pipelineLayout;
        pipelineCreateInfo.renderPass = renderPass;

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
        inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationState.cullMode = VK_CULL_MODE_NONE;
        rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationState.depthClampEnable = VK_FALSE;
        rasterizationState.rasterizerDiscardEnable = VK_FALSE;
        rasterizationState.depthBiasEnable = VK_FALSE;
        rasterizationState.lineWidth = 1.0f;

        VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
        blendAttachmentState[0].colorWriteMask = 0xf;
        blendAttachmentState[0].blendEnable = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo colorBlendState = {};
        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments = blendAttachmentState;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        const VkDynamicState dynamicStateEnables[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pDynamicStates = dynamicStateEnables;
        dynamicState.dynamicStateCount = sizeof(dynamicStateEnables) / sizeof(*dynamicStateEnables);

        VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
        depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilState.depthTestEnable = VK_TRUE;
        depthStencilState.depthWriteEnable = VK_TRUE;
        depthStencilState.depthBoundsTestEnable = VK_FALSE;
        depthStencilState.stencilTestEnable = VK_FALSE;
        depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // Unity/Vulkan uses reverse Z
        depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
        depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
        depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
        depthStencilState.front = depthStencilState.back;

        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampleState.pSampleMask = NULL;

        // Vertex:
        // float3 vpos;
        // byte4 vcol;
        VkVertexInputBindingDescription vertexInputBinding = {};
        vertexInputBinding.binding = 0;
        vertexInputBinding.stride = 16; 
        vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertexInputAttributes[2];
        vertexInputAttributes[0].binding = 0;
        vertexInputAttributes[0].location = 0;
        vertexInputAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexInputAttributes[0].offset = 0;
        vertexInputAttributes[1].binding = 0;
        vertexInputAttributes[1].location = 1;
        vertexInputAttributes[1].format = VK_FORMAT_R8G8B8A8_UNORM;
        vertexInputAttributes[1].offset = 12;

        VkPipelineVertexInputStateCreateInfo vertexInputState = {};
        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputState.vertexBindingDescriptionCount = 1;
        vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
        vertexInputState.vertexAttributeDescriptionCount = 2;
        vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes;

        pipelineCreateInfo.stageCount = sizeof(shaderStages) / sizeof(*shaderStages);
        pipelineCreateInfo.pStages = shaderStages;
        pipelineCreateInfo.pVertexInputState = &vertexInputState;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pRasterizationState = &rasterizationState;
        pipelineCreateInfo.pColorBlendState = &colorBlendState;
        pipelineCreateInfo.pMultisampleState = &multisampleState;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pDepthStencilState = &depthStencilState;
        pipelineCreateInfo.pDynamicState = &dynamicState;

        success = vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, NULL, &pipeline) == VK_SUCCESS;
    } 

    if (shaderStages[0].module != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, shaderStages[0].module, NULL);
    if (shaderStages[1].module != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, shaderStages[1].module, NULL);

    return success ? pipeline : VK_NULL_HANDLE;
}

class RenderAPI_Vulkan : public RenderAPI
{
public:
    RenderAPI_Vulkan();
    virtual ~RenderAPI_Vulkan() { }

    virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);
    virtual bool GetUsesReverseZ() { return true; }
    virtual void* BeginModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int* outRowPitch);
    virtual void EndModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr);
    virtual void DoCopyTexture(void* sourceTexture, int sourceX, int sourceY, int sourceWidth, int sourceHeight, void* destinationTexture, int destinationX, int destinationY);
    virtual bool CreateTexture(int width, int height, int format, int textureIndex);
    virtual void DestroyTexture(int textureIndex);
    virtual void* GetTexturePointer(int textureIndex);
    virtual void SetTextureColor(float red, float green, float blue, float alpha, void* targetTexture);

private:
    typedef std::vector<VulkanBuffer> VulkanBuffers;
    typedef std::map<unsigned long long, VulkanBuffers> DeleteQueue;

private:
    bool CreateVulkanBuffer(size_t bytes, VulkanBuffer* buffer, VkBufferUsageFlags usage);
    bool CreateVulkanImage(int width, int height, int format, VkImageUsageFlags usage, UnityVulkanImage* outImage);
    void ImmediateDestroyVulkanBuffer(const VulkanBuffer& buffer);
    void SafeDestroy(unsigned long long frameNumber, const VulkanBuffer& buffer);
    void GarbageCollect(bool force = false);

private:
    IUnityGraphicsVulkan* m_UnityVulkan;
    UnityVulkanInstance m_Instance;
    VulkanBuffer m_TextureStagingBuffer;
    VulkanBuffer m_VertexStagingBuffer;
    UnityVulkanImage m_UnityImage;
    std::map<unsigned long long, VulkanBuffers> m_DeleteQueue;
    VkPipelineLayout m_TrianglePipelineLayout;
    VkPipeline m_TrianglePipeline;
    VkRenderPass m_TrianglePipelineRenderPass;

    int m_UsedTextureCount;
    std::vector<UnityVulkanImage*> m_Textures;
};


RenderAPI* CreateRenderAPI_Vulkan()
{
    return new RenderAPI_Vulkan();
}

RenderAPI_Vulkan::RenderAPI_Vulkan()
    : m_UnityVulkan(NULL)
    , m_TextureStagingBuffer()
    , m_VertexStagingBuffer()
    , m_UnityImage()
    , m_TrianglePipelineLayout(VK_NULL_HANDLE)
    , m_TrianglePipeline(VK_NULL_HANDLE)
    , m_TrianglePipelineRenderPass(VK_NULL_HANDLE)
    , m_UsedTextureCount(0)
{
}

void RenderAPI_Vulkan::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
    switch (type)
    {
    case kUnityGfxDeviceEventInitialize:
        m_UnityVulkan = interfaces->Get<IUnityGraphicsVulkan>();
        m_Instance = m_UnityVulkan->Instance();

        // Make sure Vulkan API functions are loaded
        LoadVulkanAPI(m_Instance.getInstanceProcAddr, m_Instance.instance);

        UnityVulkanPluginEventConfig config_1;
        config_1.graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_DontCare;
        config_1.renderPassPrecondition = kUnityVulkanRenderPass_EnsureInside;
        config_1.flags = kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission | kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState;
        m_UnityVulkan->ConfigureEvent(1, &config_1);

        // alternative way to intercept API
        m_UnityVulkan->InterceptVulkanAPI("vkCmdBeginRenderPass", (PFN_vkVoidFunction)Hook_vkCmdBeginRenderPass);
        break;
    case kUnityGfxDeviceEventShutdown:

        if (m_Instance.device != VK_NULL_HANDLE)
        {
            GarbageCollect(true);
            if (m_TrianglePipeline != VK_NULL_HANDLE)
            {
                vkDestroyPipeline(m_Instance.device, m_TrianglePipeline, NULL);
                m_TrianglePipeline = VK_NULL_HANDLE;
            }
            if (m_TrianglePipelineLayout != VK_NULL_HANDLE)
            {
                vkDestroyPipelineLayout(m_Instance.device, m_TrianglePipelineLayout, NULL);
                m_TrianglePipelineLayout = VK_NULL_HANDLE;
            }

            for (int i = 0; i < m_UsedTextureCount; i++)
            {
                if (m_Textures[i] != nullptr)
                {
                    DestroyTexture(i);
                }
            }
        }

        m_UsedTextureCount = 0;
        m_UnityVulkan = NULL;
        m_TrianglePipelineRenderPass = VK_NULL_HANDLE;
        m_Instance = UnityVulkanInstance();

        break;
    }
}

bool RenderAPI_Vulkan::CreateVulkanBuffer(size_t sizeInBytes, VulkanBuffer* buffer, VkBufferUsageFlags usage)
{
    if (sizeInBytes == 0)
        return false;

    VkBufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = NULL;
    bufferCreateInfo.pQueueFamilyIndices = &m_Instance.queueFamilyIndex;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = sizeInBytes;

    *buffer = VulkanBuffer();

    if (vkCreateBuffer(m_Instance.device, &bufferCreateInfo, NULL, &buffer->buffer) != VK_SUCCESS)
        return false;

    VkPhysicalDeviceMemoryProperties physicalDeviceProperties;
    vkGetPhysicalDeviceMemoryProperties(m_Instance.physicalDevice, &physicalDeviceProperties);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(m_Instance.device, buffer->buffer, &memoryRequirements);

    const int memoryTypeIndex = FindMemoryTypeIndex(physicalDeviceProperties, memoryRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if (memoryTypeIndex < 0)
    {
        ImmediateDestroyVulkanBuffer(*buffer);
        return false;
    }

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = NULL;
    memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;

    if (vkAllocateMemory(m_Instance.device, &memoryAllocateInfo, NULL, &buffer->deviceMemory) != VK_SUCCESS)
    {
        ImmediateDestroyVulkanBuffer(*buffer);
        return false;
    }

    if (vkMapMemory(m_Instance.device, buffer->deviceMemory, 0, VK_WHOLE_SIZE, 0, &buffer->mapped) != VK_SUCCESS)
    {
        ImmediateDestroyVulkanBuffer(*buffer);
        return false;
    }

    if (vkBindBufferMemory(m_Instance.device, buffer->buffer, buffer->deviceMemory, 0) != VK_SUCCESS)
    {
        ImmediateDestroyVulkanBuffer(*buffer);
        return false;
    }

    buffer->sizeInBytes = sizeInBytes;
    buffer->deviceMemoryFlags = physicalDeviceProperties.memoryTypes[memoryTypeIndex].propertyFlags;
    buffer->deviceMemorySize = memoryAllocateInfo.allocationSize;

    return true;
}

bool RenderAPI_Vulkan::CreateVulkanImage(int width, int height, int format, VkImageUsageFlags usage, UnityVulkanImage* outImage)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = static_cast<VkFormat>(format);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    /*
    VkBufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = NULL;
    bufferCreateInfo.pQueueFamilyIndices = &m_Instance.queueFamilyIndex;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = sizeInBytes;
    */
    *outImage = UnityVulkanImage();

    if (vkCreateImage(m_Instance.device, &imageInfo, NULL, &outImage->image) != VK_SUCCESS)
        return false;

    VkPhysicalDeviceMemoryProperties physicalDeviceProperties;
    vkGetPhysicalDeviceMemoryProperties(m_Instance.physicalDevice, &physicalDeviceProperties);

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(m_Instance.device, outImage->image, &memoryRequirements);
    //vkGetBufferMemoryRequirements(m_Instance.device, buffer->buffer, &memoryRequirements);

    const int memoryTypeIndex = FindMemoryTypeIndex(physicalDeviceProperties, memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryTypeIndex < 0)
    {
        //ImmediateDestroyVulkanBuffer(*buffer);
        return false;
    }

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = NULL;
    memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;

    if (vkAllocateMemory(m_Instance.device, &memoryAllocateInfo, NULL, &outImage->memory.memory) != VK_SUCCESS)
    {
        //ImmediateDestroyVulkanBuffer(*buffer);
        return false;
    }

    /*
    if (vkMapMemory(m_Instance.device, outImage->memory.memory, 0, VK_WHOLE_SIZE, 0, &outImage->memory.mapped) != VK_SUCCESS)
    {
        //ImmediateDestroyVulkanBuffer(*buffer);
        return false;
    }
    */

    //if (vkBindBufferMemory(m_Instance.device, buffer->buffer, buffer->deviceMemory, 0) != VK_SUCCESS)
    if (vkBindImageMemory(m_Instance.device, outImage->image, outImage->memory.memory, 0) != VK_SUCCESS)
    {
        //ImmediateDestroyVulkanBuffer(*buffer);
        return false;
    }

    //outImage->memory.size = sizeInBytes;
    outImage->memory.flags = physicalDeviceProperties.memoryTypes[memoryTypeIndex].propertyFlags;
    outImage->memory.size = memoryAllocateInfo.allocationSize;

    return true;
}

void RenderAPI_Vulkan::ImmediateDestroyVulkanBuffer(const VulkanBuffer& buffer)
{
    if (buffer.buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_Instance.device, buffer.buffer, NULL);

    if (buffer.mapped && buffer.deviceMemory != VK_NULL_HANDLE)
        vkUnmapMemory(m_Instance.device, buffer.deviceMemory);

    if (buffer.deviceMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_Instance.device, buffer.deviceMemory, NULL);
}

/*TODO
void RenderAPI_Vulkan::ImmediateDestroyVulkanImage(const VkImage& image)
{
    if (buffer.buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_Instance.device, buffer.buffer, NULL);

    if (buffer.mapped && buffer.deviceMemory != VK_NULL_HANDLE)
        vkUnmapMemory(m_Instance.device, buffer.deviceMemory);

    if (buffer.deviceMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_Instance.device, buffer.deviceMemory, NULL);
}
*/

void RenderAPI_Vulkan::SafeDestroy(unsigned long long frameNumber, const VulkanBuffer& buffer)
{
    m_DeleteQueue[frameNumber].push_back(buffer);
}

void RenderAPI_Vulkan::GarbageCollect(bool force /*= false*/)
{
    UnityVulkanRecordingState recordingState;
    if (force)
        recordingState.safeFrameNumber = ~0ull;
    else
        if (!m_UnityVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
            return;

    DeleteQueue::iterator it = m_DeleteQueue.begin();
    while (it != m_DeleteQueue.end())
    {
        if (it->first <= recordingState.safeFrameNumber)
        {
            for (size_t i = 0; i < it->second.size(); ++i)
                ImmediateDestroyVulkanBuffer(it->second[i]);
            m_DeleteQueue.erase(it++);
        }
        else
            ++it;
    }
}

void* RenderAPI_Vulkan::BeginModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int* outRowPitch)
{
    *outRowPitch = textureWidth * 4;
    const size_t stagingBufferSizeRequirements = *outRowPitch * textureHeight;

    UnityVulkanRecordingState recordingState;
    if (!m_UnityVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
        return NULL;

    SafeDestroy(recordingState.currentFrameNumber, m_TextureStagingBuffer);
    //m_TextureStagingBuffer = VulkanBuffer();
    //if (!CreateVulkanBuffer(stagingBufferSizeRequirements, &m_TextureStagingBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
    //    return NULL;

    return m_TextureStagingBuffer.mapped;
}

void RenderAPI_Vulkan::EndModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr)
{
    return;
    // cannot do resource uploads inside renderpass
    m_UnityVulkan->EnsureOutsideRenderPass();

    UnityVulkanImage image;
    if (!m_UnityVulkan->AccessTexture(textureHandle, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image))
        return;

    UnityVulkanRecordingState recordingState;
    if (!m_UnityVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
        return;

    VkBufferImageCopy region;
    region.bufferImageHeight = 0;
    region.bufferRowLength = 0;
    region.bufferOffset = 0;
    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;
    region.imageExtent.width = textureWidth;
    region.imageExtent.height = textureHeight;
    region.imageExtent.depth = 1;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.mipLevel = 0;
    vkCmdCopyBufferToImage(recordingState.commandBuffer, m_TextureStagingBuffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void RenderAPI_Vulkan::DoCopyTexture(void* sourceTexture, int sourceX, int sourceY, int sourceWidth, int sourceHeight, void* destinationTexture, int destinationX, int destinationY)
{
    // cannot do resource uploads inside renderpass
    m_UnityVulkan->EnsureOutsideRenderPass();

    UnityVulkanImage sourceImage;
    if (!m_UnityVulkan->AccessTexture(sourceTexture, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &sourceImage))
        return;
    UnityVulkanImage destinationImage;
    if (!m_UnityVulkan->AccessTexture(destinationTexture, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &destinationImage))
        return;

    UnityVulkanRecordingState recordingState;
    if (!m_UnityVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
        return;

    VkImageCopy region;
    region.srcOffset.x = sourceX;
    region.srcOffset.y = sourceY;
    region.srcOffset.z = 0;
    region.extent.width = sourceWidth;
    region.extent.height = sourceHeight;
    region.extent.depth = 1;
    region.dstOffset.x = destinationX;
    region.dstOffset.y = destinationY;
    region.dstOffset.z = 0;
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;
    region.srcSubresource.mipLevel = 0;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;
    region.dstSubresource.mipLevel = 0;
    
    vkCmdCopyImage(recordingState.commandBuffer, sourceImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL , destinationImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

bool RenderAPI_Vulkan::CreateTexture(int width, int height, int format, int textureIndex)
{
    // cannot do resource uploads inside renderpass
    //m_UnityVulkan->EnsureOutsideRenderPass();

    if(textureIndex == m_UsedTextureCount)
    {
        m_Textures.push_back(new UnityVulkanImage());
        m_UsedTextureCount++;
    }
    else
    {
        m_Textures[textureIndex] = new UnityVulkanImage();
    }

    return CreateVulkanImage(width, height, format, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, m_Textures[textureIndex]);
}

void RenderAPI_Vulkan::DestroyTexture(int textureIndex)
{
    vkDestroyImage(m_Instance.device, m_Textures[textureIndex]->image, nullptr);
    vkFreeMemory(m_Instance.device, m_Textures[textureIndex]->memory.memory, nullptr);
    delete m_Textures[textureIndex];
    m_Textures[textureIndex] = nullptr;
}

void* RenderAPI_Vulkan::GetTexturePointer(int textureIndex)
{
    return (void*)&m_Textures[textureIndex]->image;
}

void RenderAPI_Vulkan::SetTextureColor(float red, float green, float blue, float alpha, void* targetTexture)
{
    // cannot do resource uploads inside renderpass
    m_UnityVulkan->EnsureOutsideRenderPass();

    UnityVulkanImage targetImage;
    if (!m_UnityVulkan->AccessTexture(targetTexture, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &targetImage))
        return;

    UnityVulkanRecordingState recordingState;
    if (!m_UnityVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
        return;

    VkImageSubresourceRange imageSubresourceRange;
    imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageSubresourceRange.baseMipLevel = 0;
    imageSubresourceRange.levelCount = 1;
    imageSubresourceRange.baseArrayLayer = 0;
    imageSubresourceRange.layerCount = 1;

    VkClearColorValue color = { red, green, blue, alpha };

    vkCmdClearColorImage(recordingState.commandBuffer, targetImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &imageSubresourceRange);
}

#endif // #if SUPPORT_VULKAN

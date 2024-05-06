#pragma once
#include "common-stuff.h"
#include "pipeline-helpers.h"

typedef struct{
  VkRenderPass value;
  enum  {
    CREATE_RENDER_PASS_TOP_FAIL_CODE = -0x7fff,
    CREATE_RENDER_PASS_FAILED,
    CREATE_RENDER_PASS_OK = 0,
  } code;
} OptRenderPass;

OptRenderPass create_render_pass(VkDevice device, VkFormat img_format, VkFormat depth_stencil_format);

OptRenderPass clear_render_pass(OptRenderPass render_pass, VkDevice device);



//Just a binary file reader, but returns u32 type slice
DEF_SLICE(u32);
/* uint32_t *read_spirv_in_stk_allocr(StackAllocator *stk_allocr, */
/*                                    size_t *p_stk_offset, */
/*                                    const char *file_name, */
/*                                    size_t *p_file_size); */

u32Slice read_binary_u32(AllocInterface allocr,
			 const char* file_name);


typedef struct{
  VkShaderModule value;
  enum  {
    CREATE_SHADER_MODULE_FROM_FILE_ERROR = -0x7fff,
    CREATE_SHADER_MODULE_FROM_FILE_READ_FILE_ERROR,
    CREATE_SHADER_MODULE_FROM_FILE_OK = 0,
  } code;
} OptShaderModule;

OptShaderModule create_shader_module_from_file(AllocInterface allocr,
					       VkDevice device,
					       const char* shader_file_name);

struct GraphicsPipelineCreationInfos {
  VkPipelineDynamicStateCreateInfo dynamic_state;
  VkPipelineColorBlendStateCreateInfo color_blend_state;
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
  VkPipelineMultisampleStateCreateInfo multisample_state;
  VkPipelineRasterizationStateCreateInfo rasterization_state;
  VkPipelineViewportStateCreateInfo viewport_state;
  VkPipelineTessellationStateCreateInfo tessellation_state;
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
  //VkPipelineVertexInputStateCreateInfo vertex_input_state;
  VkPipelineCreateFlags flags;
};
typedef struct GraphicsPipelineCreationInfos
GraphicsPipelineCreationInfos;

GraphicsPipelineCreationInfos
default_graphics_pipeline_creation_infos(void);

typedef struct ShaderNames ShaderNames;
struct ShaderNames {
  const char* vert;
  const char* frag;
  const char* geom;
};

typedef struct {
  GraphicsPipelineCreationInfos create_infos;
  VertexBindingSlice vert_bindings;
  VertexAttributeSlice vert_attrs;
  VkPipelineLayout pipe_layout;
  ShaderNames shaders;
  VkRenderPass compatible_render_pass;
  uint32_t subpass_index;
} CreateGraphicsPipelineParam;

typedef struct{
  VkPipeline value;
  enum  {
    CREATE_GRAPHICS_PIPELINE_TOP_FAIL_CODE = -0x7fff,
    CREATE_GRAPHICS_PIPELINE_FAILED,
    CREATE_GRAPHICS_PIPELINE_SHADER_MODULES_CREATION_FAILED,
    CREATE_GRAPHICS_PIPELINE_VERT_FRAG_SHADER_NAME_ERR,
    CREATE_GRAPHICS_PIPELINE_OK = 0,
  } code;
} OptPipeline;

OptPipeline create_graphics_pipeline(AllocInterface allocr, VkDevice device,
				     CreateGraphicsPipelineParam param);

OptPipeline clear_graphics_pipeline(OptPipeline pipeline, VkDevice device);

DEF_SLICE(VkSemaphore);
typedef struct{
  VkSemaphoreSlice value;
  enum {
    CREATE_SEMAPHORES_TOP_FAIL_CODE = -0x7fff,
    CREATE_SEMAPHORES_FAILED ,
    CREATE_SEMAPHORES_ALLOC_FAIL,
    CREATE_SEMAPHORES_OK = 0,
  } code;
} OptSemaphores;

OptSemaphores create_semaphores(AllocInterface allocr,
				VkDevice device,
				u32 count);

OptSemaphores clear_semaphores(AllocInterface allocr,
			       OptSemaphores semas,
			       VkDevice device);

DEF_SLICE(VkFence);
typedef struct{
  VkFenceSlice value;
  enum  {
    CREATE_FENCES_TOP_FAIL_CODE = -0x7fff,
    CREATE_FENCES_FAILED,
    CREATE_FENCES_ALLOC_FAILED,
    CREATE_FENCES_OK = 0,
  } code;
} OptFences;

typedef struct {
  uint32_t fences_count;
  VkDevice device;

  VkFence **p_fences;
} CreateFencesParam;
OptFences create_fences(AllocInterface allocr,
			VkDevice device,
			u32 count);
OptFences clear_fences(AllocInterface allocr,
		       OptFences fences,
		       VkDevice device);
DEF_SLICE(VkCommandBuffer);
typedef struct {
  VkDevice device;
  VkCommandPool cmd_pool;
  uint32_t cmd_buffer_count;

  VkCommandBuffer **p_cmd_buffers;
} CreatePrimaryCommandBuffersParam;
typedef struct{
  VkCommandBufferSlice value;
  
  enum {
    CREATE_PRIMARY_COMMAND_BUFFERS_TOP_FAIL_CODE = -0x7fff,
    CREATE_PRIMARY_COMMAND_BUFFERS_FAILED,
    CREATE_PRIMARY_COMMAND_BUFFERS_ALLOC_FAILED,
    CREATE_PRIMARY_COMMAND_BUFFERS_INVALID_POOL,
    CREATE_PRIMARY_COMMAND_BUFFERS_OK = 0,
  } code;
} OptPrimaryCommandBuffers;

OptPrimaryCommandBuffers create_primary_command_buffers(AllocInterface allocr,
							VkDevice device,
							OptCommandPool cmd_pool,
							u32 count);
OptPrimaryCommandBuffers clear_primary_command_buffers(AllocInterface allocr,
						       OptPrimaryCommandBuffers cmd_buffers);
//TODO Might need to later restructure this part of code when later using stuff
enum BeginRenderingOperationsCodes {
  BEGIN_RENDERING_OPERATIONS_FAILED = -0x7fff,
  BEGIN_RENDERING_OPERATIONS_BEGIN_CMD_BUFFER_FAIL,
  BEGIN_RENDERING_OPERATIONS_WAIT_FOR_FENCE_FAIL,
  BEGIN_RENDERING_OPERATIONS_OK = 0,
  BEGIN_RENDERING_OPERATIONS_TRY_RECREATE_SWAPCHAIN,
  BEGIN_RENDERING_OPERATIONS_MINIMIZED,
};

typedef struct {
  VkDevice device;
  VkSwapchainKHR swapchain;
  VkRenderPass render_pass;
  VkFramebuffer *framebuffers;
  VkExtent2D framebuffer_render_extent;
  VkCommandBuffer cmd_buffer;
  VkSemaphore present_done_semaphore;
  VkFence render_done_fence;
  VkClearValue clear_value;

  uint32_t *p_img_inx;

} BeginRenderingOperationsParam;
enum BeginRenderingOperationsCodes begin_rendering_operations(BeginRenderingOperationsParam param);

enum EndRenderingOperationsCodes {
  END_RENDERING_OPERATIONS_FAILED = -0x7fff,
  END_RENDERING_OPERATIONS_GRAPHICS_QUEUE_SUBMIT_FAIL,
  END_RENDERING_OPERATIONS_END_CMD_BUFFER_FAIL,
  END_RENDERING_OPERATIONS_OK = 0,
  END_RENDERING_OPERATIONS_TRY_RECREATING_SWAPCHAIN,
};

typedef struct {
  VkDevice device;
  VkCommandBuffer cmd_buffer;

  VkSemaphore render_done_semaphore;
  VkSemaphore present_done_semaphore;
  VkFence render_done_fence;

  VkQueue graphics_queue;
  VkQueue present_queue;
  VkSwapchainKHR swapchain;
  uint32_t img_index;

} EndRenderingOperationsParam;
enum EndRenderingOperationsCodes end_rendering_operations(EndRenderingOperationsParam param);

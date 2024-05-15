
#include "shaders/glsl.h"
#include "compute-test.h"
#include "pipeline-helpers.h"

#include "vectors.h"
#include "compute-test.h"

DEF_SLICE(Vec2);
typedef struct TestComputeData TestComputeData;
struct TestComputeData {
  VkDescriptorSetLayout desc_layout;
  VkDescriptorPool desc_pool;
  VkDescriptorSet desc_set;
  VkPipelineLayout pipe_layout;
  VkPipeline pipeline;
  VkCommandPool cmd_pool;
  VkCommandBuffer cmd_buffer;

  MemoryItemSlice items;
  VkFormatSlice img_formats;
  u32Slice item_usages;
  u32 item_count;
};

ComputeJobInitOutput init_test_compute(void* any_init_param, ComputeJobParam* params){

  ComputeJobInitOutput output = {0};

  TestComputeData* data = alloc_mem(params->allocr, sizeof*data, alignof(TestComputeData));
  if(data == nullptr){
    return output;
  }
  output.internal_data = data;
  
  //Init descriptor layout
  data->desc_layout = create_descriptor_set_layout
    (params->device, 0,
     {
       .binding = 0,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
     });

  //Create descriptor pool
  data->desc_pool = make_pool
    (params->allocr, params->device,
     {.count = 1,
      .descs = MAKE_ARRAY_SLICE(DescSize,
				{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				 .descriptorCount = 1 })
     });

  if((data->desc_layout == VK_NULL_HANDLE) ||
     (data->desc_pool == VK_NULL_HANDLE)){
    return output;
  }

  //Create pipeline layout
  {
    VkPipelineLayoutCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &data->desc_layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &(VkPushConstantRange){
	.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	.offset = 0,
	.size = sizeof(u32),
      }
    };
    VkResult res = vkCreatePipelineLayout(params->device, &info, get_glob_vk_alloc(),
					  &data->pipe_layout);
    if(res != VK_SUCCESS){
      data->pipe_layout = VK_NULL_HANDLE;
      return output;
    }
  }
  
  //Allocate descriptors
  data->desc_set = alloc_set(params->device, data->desc_pool, data->desc_layout);
  if(VK_NULL_HANDLE == data->desc_set )
    return output;

  //Create shaders
  OptShaderModule shader = create_shader_module_from_file
    (params->allocr,
     params->device,
     "./shaders/glsl.comp.spv");
  
  //Create compute pipeline
  if(shader.code < 0){
    return output;
  }
  {

    VkComputePipelineCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .layout = data->pipe_layout,
      .stage = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	.stage = VK_SHADER_STAGE_COMPUTE_BIT,
	.module = shader.value,
	.pName = "main"
      },
    };
    VkResult res = vkCreateComputePipelines(params->device, VK_NULL_HANDLE,
					    1, &info,
					    get_glob_vk_alloc(),
					    &data->pipeline);

    if(VK_SUCCESS != res){
      data->pipeline = VK_NULL_HANDLE;
    }
    
    vkDestroyShaderModule(params->device, shader.value, get_glob_vk_alloc());
  }
  

  //Create compute command pool and buffers
  {
    VkResult res = VK_SUCCESS;

    res = vkCreateCommandPool(params->device, &(VkCommandPoolCreateInfo){
	.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
	.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
	.queueFamilyIndex = params->compute_queue->family,
      }, get_glob_vk_alloc(), &data->cmd_pool);

    if(res != VK_SUCCESS){
      data->cmd_pool = VK_NULL_HANDLE;
      return output;
    }
    res = vkAllocateCommandBuffers(params->device, &(VkCommandBufferAllocateInfo){
	.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	.commandPool = data->cmd_pool,
	.commandBufferCount = 1,
	.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      }, &data->cmd_buffer);
    if(res != VK_SUCCESS){
      data->cmd_buffer = VK_NULL_HANDLE;
      return output;
      
    }
  }
  //Allocate the input/output buffers


  //Just one , number of data is provided as any_init_param pointing to a size_t variable holding count

  Vec2Slice* init_data = any_init_param;
  if((nullptr == init_data) || (init_data->count == 0))
    return output;

  data->item_count = init_data->count;
    
  OptBuffer actual_buffer = alloc_buffer
    (params->gpu_allocr, params->device,
     Vec2_slice_bytes(*init_data), (AllocBufferParams){
      .props_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    });

  if(actual_buffer.code < 0){
    free_buffer(params->gpu_allocr, actual_buffer, params->device);
    return output;
  }

  //Write descriptors
  write_storage_descriptor(params->device, data->desc_set,
			   actual_buffer.value, 0);

  //Try to allocate cpu buffer for everything upfront, if fail, free all
  data->items = SLICE_ALLOC(MemoryItem, 1, params->allocr);
  data->img_formats = SLICE_ALLOC(VkFormat, 1, params->allocr);
  data->item_usages = SLICE_ALLOC(u32, 1, params->allocr);

  bool it_failed = ((nullptr == data->items.data) ||
		    (nullptr == data->img_formats.data) ||
		    (nullptr == data->item_usages.data));
  
  data->items.data[0] = (MemoryItem){
    .type = MEMORY_ITEM_TYPE_GPU_BUFFER,
    .gpu_buffer = actual_buffer.value
  };
  
  //Write the initial data
  if(!it_failed && immediate_command_begin(params->device, *params->compute_queue)){
    MemoryItemDarray stage = init_MemoryItem_darray(params->allocr);
    
    u32 count = copy_memory_items(&stage, MAKE_ARRAY_SLICE
				  (CopyUnit,
				   {.src  = {
				       .type = MEMORY_ITEM_TYPE_CPU_BUFFER,
				       .cpu_buffer = SLICE_REINTERPRET(Vec2, u8, *init_data),
				     },
				    .dst = data->items.data[0]
				   }
				   ), params->device, (CopyMemoryParam){
				    .allocr = params->allocr,
				    .gpu_allocr = params->gpu_allocr,
				    .cmd_buf = params->compute_queue->im_buffer,
				    .src_queue_family = params->compute_queue->family,
				    .dst_queue_family = params->compute_queue->family,
				    .src_stage_mask = VK_PIPELINE_STAGE_2_NONE,
				    .dst_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				  }
				  );
					
    if(!immediate_command_end(params->device, *params->compute_queue) || (count != 1)){
      it_failed = true;
    }
    finish_copying_items(&stage, params->device, params->gpu_allocr);
  }
  else{
    it_failed = true;
  }

  if(it_failed){
    SLICE_FREE(data->items, params->allocr);
    SLICE_FREE(data->img_formats, params->allocr);
    SLICE_FREE(data->item_usages, params->allocr);
    free_buffer(params->gpu_allocr, actual_buffer, params->device);
    return output;
  }

  //Everything worked, return with proper data

  output.main_items = &data->items;
  output.main_item_usages = &data->item_usages;
  output.main_item_img_formats = &data->img_formats;


  data->item_usages.data[0] = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  

  return output;
}

void clean_test_compute(void* pdata, ComputeJobParam* params ){
  if(pdata == nullptr)
    return;
  TestComputeData* data = pdata;

  

  for_slice(data->items, i){
    free_memory_item(data->items.data[i], params->allocr,
		     params->gpu_allocr, params->device);
  }
  
  SLICE_FREE(data->items, params->allocr);
  SLICE_FREE(data->img_formats, params->allocr);
  SLICE_FREE(data->item_usages, params->allocr);

  
  
  if(VK_NULL_HANDLE != data->cmd_pool){
    vkDestroyCommandPool(params->device, data->cmd_pool, get_glob_vk_alloc());
  }

  if(VK_NULL_HANDLE != data->pipeline){
    vkDestroyPipeline(params->device, data->pipeline, get_glob_vk_alloc());
  }

  if(VK_NULL_HANDLE != data->pipe_layout){
    vkDestroyPipelineLayout(params->device, data->pipe_layout, get_glob_vk_alloc());
  }

  
  if(VK_NULL_HANDLE != data->desc_pool){
    vkDestroyDescriptorPool(params->device, data->desc_pool, get_glob_vk_alloc());
  }
  
  if(VK_NULL_HANDLE != data->desc_layout){
    vkDestroyDescriptorSetLayout(params->device, data->desc_layout, get_glob_vk_alloc());
  }

  free_mem(params->allocr, data);
}

VkCommandBuffer run_test_compute(void* pdata, ComputeJobParam* params){

  TestComputeData* data = pdata;
  if((nullptr == pdata) || (VK_NULL_HANDLE == data->cmd_buffer)){
    return VK_NULL_HANDLE;
  }

  VkResult res = VK_SUCCESS;

  res = vkBeginCommandBuffer(data->cmd_buffer, &(VkCommandBufferBeginInfo){
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    });

  if(VK_SUCCESS != res)
    return VK_NULL_HANDLE; 

  vkCmdBindPipeline(data->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		    data->pipeline);

  
  
  vkCmdPushConstants(data->cmd_buffer,
		     data->pipe_layout,
		     VK_SHADER_STAGE_COMPUTE_BIT,
		     0, sizeof(u32), &data->item_count);
		     
  
  vkCmdBindDescriptorSets(data->cmd_buffer,
			  VK_PIPELINE_BIND_POINT_COMPUTE,
			  data->pipe_layout, 0,
			  1, &data->desc_set,
			  0, nullptr);
  vkCmdDispatch(data->cmd_buffer,
		_align_up(data->item_count, COMP_X_DIM), 1, 1);
  
  res = vkEndCommandBuffer(data->cmd_buffer);
  if(VK_SUCCESS != res)
    return VK_NULL_HANDLE;

  return data->cmd_buffer;
}

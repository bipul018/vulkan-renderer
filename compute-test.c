#include "shaders/glsl.h"
#include "compute-test.h"
#include "pipeline-helpers.h"

#include "vectors.h"
#include "compute-test.h"

//DEF_SLICE(Vec2);
typedef struct TestComputeData TestComputeData;
struct TestComputeData {
  VkDescriptorSetLayout desc_layout;
  VkDescriptorPool desc_pool;
  
  VkDescriptorSet desc_set1;
  VkDescriptorSet desc_set2;
  VkDescriptorSet desc_set3;
  
  VkPipelineLayout pipe_layout1;
  VkPipeline pipeline1;

  VkPipelineLayout pipe_layout2;
  VkPipeline pos_pipeline;
  VkPipeline vel_pipeline;
  
  VkCommandPool cmd_pool;
  VkCommandBuffer cmd_buffer;

  OptBuffer vel_buffer;
  OptBuffer acc_buffer;
  MemoryItemSlice items;
  VkFormatSlice img_formats;
  u32Slice item_usages;

  TestComputePubData* pub_data;
  MemoryItemDarray copying_stages;
  timespec last_frame_time;
  u32 tex_item_count;
  u32 pts_item_count;
};

ComputeJobInitOutput init_test_compute(void* any_init_param, ComputeJobParam* params){

  ComputeJobInitOutput output = {0};

  TestComputeData* data = alloc_mem(params->allocr, sizeof*data, alignof(TestComputeData));
  if(data == nullptr){
    return output;
  }
  output.internal_data = data;
  data->vel_buffer.code = ALLOC_BUFFER_TOP_ERROR;
  data->acc_buffer.code = ALLOC_BUFFER_TOP_ERROR;
  
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
     {.count = 3,
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
					  &data->pipe_layout1);
    if(res != VK_SUCCESS){
      data->pipe_layout1 = VK_NULL_HANDLE;
      return output;
    }
  }

  {
    VkPipelineLayoutCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	.setLayoutCount = 2,
	.pSetLayouts = (VkDescriptorSetLayout[]){data->desc_layout, data->desc_layout},
	.pushConstantRangeCount = 1,
	.pPushConstantRanges = &(VkPushConstantRange){
	  .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	  .offset = 0,
	  .size = sizeof(u32)+sizeof(float),
	}
    };
    VkResult res = vkCreatePipelineLayout(params->device, &info, get_glob_vk_alloc(),
					  &data->pipe_layout2);
    if(res != VK_SUCCESS){
      data->pipe_layout1 = VK_NULL_HANDLE;
      return output;
    }
  }
  
  //Allocate descriptors
  data->desc_set1 = alloc_set(params->device, data->desc_pool, data->desc_layout);
  if(VK_NULL_HANDLE == data->desc_set1 )
    return output;

  data->desc_set2 = alloc_set(params->device, data->desc_pool, data->desc_layout);
  if(VK_NULL_HANDLE == data->desc_set2 )
    return output;

  data->desc_set3 = alloc_set(params->device, data->desc_pool, data->desc_layout);
  if(VK_NULL_HANDLE == data->desc_set3 )
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
      .layout = data->pipe_layout1,
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
					    &data->pipeline1);

    if(VK_SUCCESS != res){
      data->pipeline1 = VK_NULL_HANDLE;
    }
    
    vkDestroyShaderModule(params->device, shader.value, get_glob_vk_alloc());
  }

  shader = create_shader_module_from_file
    (params->allocr,
     params->device,
     "./shaders/naccl.comp.spv");
  
  //Create compute pipeline
  if(shader.code < 0){
    return output;
  }
  {

    VkComputePipelineCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .layout = data->pipe_layout2,
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
					    &data->vel_pipeline);

    if(VK_SUCCESS != res){
      data->vel_pipeline = VK_NULL_HANDLE;
    }
    
    vkDestroyShaderModule(params->device, shader.value, get_glob_vk_alloc());
  }
  
  shader = create_shader_module_from_file
    (params->allocr,
     params->device,
     "./shaders/nbody.comp.spv");
  
  //Create compute pipeline
  if(shader.code < 0){
    return output;
  }
  {

    VkComputePipelineCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .layout = data->pipe_layout2,
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
					    &data->pos_pipeline);

    if(VK_SUCCESS != res){
      data->pos_pipeline = VK_NULL_HANDLE;
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

  //Expects an array of 2 such slices
  //First slice is texture coords
  //Second slice is vert coords
  //Third slice is velocities
  TestComputeInitParam* init_data = any_init_param;
  if(nullptr == init_data)
    return output;
  data->pub_data = init_data->pub_data;
  
  Vec2Slice tex_data = init_data->init_tex_items;
  Vec2Slice pos_data = init_data->init_pos_items;
  Vec2Slice vel_data = init_data->init_vel_items;

  
  if((tex_data.count == 0) || (pos_data.count == 0) ||
     (vel_data.count == 0) || (pos_data.count != vel_data.count))
    return output;

  data->tex_item_count = tex_data.count;
  data->pts_item_count = pos_data.count;

  
  data->vel_buffer = alloc_buffer
    (params->gpu_allocr, params->device,
     Vec2_slice_bytes(vel_data), (AllocBufferParams){
      .props_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    });
  if(data->vel_buffer.code < 0)
    return output;
  
  data->acc_buffer = alloc_buffer
    (params->gpu_allocr, params->device,
     Vec2_slice_bytes(vel_data), (AllocBufferParams){
      .props_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    });
  if(data->acc_buffer.code < 0)
    return output;
  
    
  OptBuffer actual_buffers[2] = {0};
  bool gpu_buffer_fail = false;
  {
    actual_buffers[0] = alloc_buffer
      (params->gpu_allocr, params->device,
       Vec2_slice_bytes(tex_data), (AllocBufferParams){
	.props_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
	VK_BUFFER_USAGE_TRANSFER_DST_BIT |
	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
      });
    if(actual_buffers[0].code < 0)
      gpu_buffer_fail = true;
  }
  {
    actual_buffers[1] = alloc_buffer
      (params->gpu_allocr, params->device,
       Vec2_slice_bytes(pos_data), (AllocBufferParams){
	.props_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
	VK_BUFFER_USAGE_TRANSFER_DST_BIT |
	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
      });
    if(actual_buffers[1].code < 0)
      gpu_buffer_fail = true;
  }

  if(gpu_buffer_fail){
    for_range(u32, i, 0, 2)
      actual_buffers[i] = free_buffer(params->gpu_allocr, actual_buffers[i], params->device);
    return output;
  }
  data->copying_stages = init_MemoryItem_darray(params->allocr);
  
  //Write descriptors
  write_storage_descriptor(params->device, data->desc_set1,
			   actual_buffers[0].value, 0);
  write_storage_descriptor(params->device, data->desc_set2,
			   actual_buffers[1].value, 0);
  write_storage_descriptor(params->device, data->desc_set3,
			   data->vel_buffer.value, 0);

  //Try to allocate cpu buffer for everything upfront, if fail, free all
  data->items = SLICE_ALLOC(MemoryItem, 2, params->allocr);
  data->img_formats = SLICE_ALLOC(VkFormat, 2, params->allocr);
  data->item_usages = SLICE_ALLOC(u32, 2, params->allocr);

  bool it_failed = ((nullptr == data->items.data) ||
		    (nullptr == data->img_formats.data) ||
		    (nullptr == data->item_usages.data));
  
  //Write the initial data
  if(!it_failed && immediate_command_begin(params->device, *params->compute_queue)){
    for_slice(data->items, i){
      data->items.data[i] = (MemoryItem){
	.type = MEMORY_ITEM_TYPE_GPU_BUFFER,
	.gpu_buffer = {
	  .obj = actual_buffers[i].value,
	  .offset = 0,
	  .length = actual_buffers[i].value.size
	}
      };
    }
  
    MemoryItemDarray stage = init_MemoryItem_darray(params->allocr);
    
    u32 count = copy_memory_items(&stage, MAKE_ARRAY_SLICE
				  (CopyUnit,
				   {.src  = {
				       .type = MEMORY_ITEM_TYPE_CPU_BUFFER,
				       .cpu_buffer = SLICE_REINTERPRET(Vec2, u8, tex_data),
				     },
				    .dst = data->items.data[0]
				   },
				   
				   {.src  = {
				       .type = MEMORY_ITEM_TYPE_CPU_BUFFER,
				       .cpu_buffer = SLICE_REINTERPRET(Vec2, u8, pos_data),
				     },
				    .dst = data->items.data[1]
				   },
				   
				   {.src  = {
				       .type = MEMORY_ITEM_TYPE_CPU_BUFFER,
				       .cpu_buffer = SLICE_REINTERPRET(Vec2, u8, vel_data),
				     },
				    .dst = {
				      .type = MEMORY_ITEM_TYPE_GPU_BUFFER,
				      .gpu_buffer = {
					.obj = data->vel_buffer.value,
					.offset = 0,
					.length = data->vel_buffer.value.size,
				      }
				    }
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
					
    if(!immediate_command_end(params->device, *params->compute_queue) || (count != 3)){
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
    
    for_range(u32, i, 0, 3)
      actual_buffers[i] = free_buffer(params->gpu_allocr, actual_buffers[i], params->device);
    return output;
  }

  //Everything worked, return with proper data

  output.main_items = &data->items;
  output.main_item_usages = &data->item_usages;
  output.main_item_img_formats = &data->img_formats;


  data->item_usages.data[0] = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  data->item_usages.data[1] = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  
  data->last_frame_time = start_monotonic_timer();
  
  return output;
}

void clean_test_compute(void* pdata, ComputeJobParam* params ){
  if(pdata == nullptr)
    return;
  TestComputeData* data = pdata;

  finish_copying_items(&data->copying_stages, params->device, params->gpu_allocr);

  for_slice(data->items, i){
    free_memory_item(data->items.data[i], params->allocr,
		     params->gpu_allocr, params->device);
  }
  
  SLICE_FREE(data->items, params->allocr);
  SLICE_FREE(data->img_formats, params->allocr);
  SLICE_FREE(data->item_usages, params->allocr);

  free_buffer(params->gpu_allocr, data->vel_buffer, params->device);
  free_buffer(params->gpu_allocr, data->acc_buffer, params->device);
  
  if(VK_NULL_HANDLE != data->cmd_pool){
    vkDestroyCommandPool(params->device, data->cmd_pool, get_glob_vk_alloc());
  }
  
  
  if(VK_NULL_HANDLE != data->pos_pipeline){
    vkDestroyPipeline(params->device, data->pos_pipeline, get_glob_vk_alloc());
  }

  if(VK_NULL_HANDLE != data->vel_pipeline){
    vkDestroyPipeline(params->device, data->vel_pipeline, get_glob_vk_alloc());
  }

  if(VK_NULL_HANDLE != data->pipeline1){
    vkDestroyPipeline(params->device, data->pipeline1, get_glob_vk_alloc());
  }

  if(VK_NULL_HANDLE != data->pipe_layout2){
    vkDestroyPipelineLayout(params->device, data->pipe_layout2, get_glob_vk_alloc());
  }

  if(VK_NULL_HANDLE != data->pipe_layout1){
    vkDestroyPipelineLayout(params->device, data->pipe_layout1, get_glob_vk_alloc());
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
  
  finish_copying_items(&data->copying_stages, params->device, params->gpu_allocr);
  
  double delta = timer_sec(end_monotonic_timer(&data->last_frame_time));

  VkResult res = VK_SUCCESS;

  res = vkBeginCommandBuffer(data->cmd_buffer, &(VkCommandBufferBeginInfo){
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    });

  if(VK_SUCCESS != res)
    return VK_NULL_HANDLE;

  if(data->pub_data->do_rewrite_pos || data->pub_data->do_rewrite_vel){
    
    CopyUnitSlice copies = MAKE_ARRAY_SLICE(CopyUnit, {0}, {0});
    copies.count = 0;
    if(data->pub_data->do_rewrite_pos){
      copies.data[copies.count++] = (CopyUnit){
	.dst = {.type = MEMORY_ITEM_TYPE_GPU_BUFFER,
		.gpu_buffer = {.obj = data->items.data[1].gpu_buffer.obj,
			       .offset = data->pub_data->rewrite_pos_offset,
			       .length = data->pub_data->rewrite_pos_source.count
		},
	},
	.src = {.type = MEMORY_ITEM_TYPE_CPU_BUFFER,
		.cpu_buffer = data->pub_data->rewrite_pos_source,}
      };
    }
    if(data->pub_data->do_rewrite_vel){
      copies.data[copies.count++] = (CopyUnit){
	.dst = {.type = MEMORY_ITEM_TYPE_GPU_BUFFER,
		.gpu_buffer = {.obj = data->vel_buffer.value,
			       .offset = data->pub_data->rewrite_vel_offset,
			       .length = data->pub_data->rewrite_vel_source.count,}},
	.src = {.type = MEMORY_ITEM_TYPE_CPU_BUFFER,
		.cpu_buffer = data->pub_data->rewrite_vel_source,
	}
      };
    }
    for_slice(copies, i){
      assert(copies.data[i].src.type == MEMORY_ITEM_TYPE_CPU_BUFFER && "Some error, src should be a cpu buffer");
      assert(copies.data[i].dst.type == MEMORY_ITEM_TYPE_GPU_BUFFER && "Some error, dst should be a gpu buffer");
    }
    
    u32 count = copy_memory_items(&data->copying_stages, copies,
				  params->device, (CopyMemoryParam){
				    .allocr = params->allocr,
				    .gpu_allocr = params->gpu_allocr,
				    .cmd_buf = data->cmd_buffer,
				    .src_queue_family = params->compute_queue->family,
				    .dst_queue_family = params->compute_queue->family,
				    .src_stage_mask = VK_PIPELINE_STAGE_2_NONE,
				    .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				  }
				  );
    data->pub_data->do_rewrite_pos = data->pub_data->do_rewrite_vel = false;
  }
  
  if(!data->pub_data->paused){
    vkCmdBindPipeline(data->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		      data->pipeline1);

  
  
    vkCmdPushConstants(data->cmd_buffer,
		       data->pipe_layout1,
		       VK_SHADER_STAGE_COMPUTE_BIT,
		       0, sizeof(u32), &data->tex_item_count);
		     
  
    vkCmdBindDescriptorSets(data->cmd_buffer,
			    VK_PIPELINE_BIND_POINT_COMPUTE,
			    data->pipe_layout1, 0,
			    1, &data->desc_set1,
			    0, nullptr);
    vkCmdDispatch(data->cmd_buffer,
		  _align_up(_min(data->tex_item_count, data->pub_data->max_used), COMP_X_DIM), 1, 1);

  
    vkCmdBindPipeline(data->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		      data->pos_pipeline);

    struct{
      u32 size;
      float delt;
    } push_data = {0};
    push_data.size = data->pts_item_count;
    push_data.delt = delta;
    assert(sizeof(push_data) == (sizeof(float) + sizeof(u32)));
    
    vkCmdPushConstants(data->cmd_buffer,
		       data->pipe_layout2,
		       VK_SHADER_STAGE_COMPUTE_BIT,
		       0, sizeof(push_data), &push_data);
  
    vkCmdBindDescriptorSets(data->cmd_buffer,
			    VK_PIPELINE_BIND_POINT_COMPUTE,
			    data->pipe_layout2, 0,
			    2, (VkDescriptorSet[]){data->desc_set2, data->desc_set3},
			    0, nullptr);
    vkCmdDispatch(data->cmd_buffer,
		  _align_up(_min(data->tex_item_count, data->pub_data->max_used), COMP_X_DIM), 1, 1);

    //TODO :: insert a pipeline barrier here
    copy_memory_items(nullptr, (CopyUnitSlice){0}, params->device,
		      (CopyMemoryParam){
			.allocr = params->allocr,
			.gpu_allocr = params->gpu_allocr,
			.cmd_buf = data->cmd_buffer,
			.src_queue_family = params->compute_queue->family,
			.dst_queue_family = params->compute_queue->family,
			.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		      });
  
    vkCmdBindPipeline(data->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		      data->vel_pipeline);

    vkCmdDispatch(data->cmd_buffer,
		  _align_up(data->pts_item_count, COMP_X_DIM), 1, 1);

  }
  
  
  res = vkEndCommandBuffer(data->cmd_buffer);
  if(VK_SUCCESS != res)
    return VK_NULL_HANDLE;

  return data->cmd_buffer;
}

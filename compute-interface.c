#include "compute-interface.h"  
//Init func
ComputeJob init_compute_job(GPUAllocr* gpu_allocr, AllocInterface allocr,
			    VulkanDevice* device, u32 max_frames,
			    void* job_init_param,
			    InitJobFnT* init_job_fn,
			    RunJobFnT* run_job_fn,
			    CleanJobFnT* clean_job_fn){
  ComputeJob output = {
    .allocr = allocr,
    .gpu_allocr = gpu_allocr,
    .device = device,
    .init_job_fn = init_job_fn,
    .run_job_fn = run_job_fn,
    .clean_job_fn = clean_job_fn,
    .first_time = true,
  };

  ComputeJobInitOutput init_output = init_job_fn(job_init_param, &(ComputeJobParam){
      .allocr = allocr,
      .gpu_allocr = gpu_allocr,
      .device = device->device,
      .compute_queue = &device->compute,
    });

  if((nullptr == init_output.main_item_img_formats) ||
     (nullptr == init_output.main_items) ||
     (nullptr == init_output.main_item_usages)){
    output.code = INIT_COMPUTE_JOB_JOB_INIT_FAIL;
    return output;
  }
  output.main_items = init_output.main_items;
  output.main_item_usages = init_output.main_item_usages;
  output.main_item_img_formats = init_output.main_item_img_formats;
  output.job_data = init_output.internal_data;

  //Create command pools
  OptCommandPool compute_pool = create_command_pool(device->device, device->compute.family);
  OptCommandPool graphics_pool = create_command_pool(device->device, device->graphics.family);

  if((compute_pool.code < 0) || (graphics_pool.code < 0)){
    compute_pool = clear_command_pool(compute_pool, device->device);
    graphics_pool = clear_command_pool(graphics_pool, device->device);
    output.code = INIT_COMPUTE_JOB_CMD_POOL_FAIL;
    return output;
  }

  output.compute_pool = compute_pool.value;
  output.graphics_pool = graphics_pool.value;

  //Create command buffers
  VkCommandBufferAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  VkResult res = VK_SUCCESS;
  alloc_info.commandPool = compute_pool.value;
  res = vkAllocateCommandBuffers(device->device, &alloc_info, &output.release_cmd_buf);

  if(VK_SUCCESS != res){
    output.code = INIT_COMPUTE_JOB_CMD_BUF_FAIL;
    return output;
  }

  res = vkAllocateCommandBuffers(device->device, &alloc_info, &output.acquire_cmd_buf);

  if(VK_SUCCESS != res){
    output.code = INIT_COMPUTE_JOB_CMD_BUF_FAIL;
    return output;
  }

  alloc_info.commandPool = graphics_pool.value;
  res = vkAllocateCommandBuffers(device->device, &alloc_info, &output.copy_main_cmd_buf);

  if(VK_SUCCESS != res){
    output.code = INIT_COMPUTE_JOB_CMD_BUF_FAIL;
    return output;
  }

  output.frame_copy_cmd_buf = SLICE_ALLOC(VkCommandBuffer, max_frames, allocr);
  if(output.frame_copy_cmd_buf.data == nullptr){
    output.code = INIT_COMPUTE_JOB_CMD_BUF_FAIL;
    return output;
  }
  alloc_info.commandBufferCount = max_frames;
  res = vkAllocateCommandBuffers(device->device, &alloc_info, output.frame_copy_cmd_buf.data);

  if(VK_SUCCESS != res){
    output.code = INIT_COMPUTE_JOB_CMD_BUF_FAIL;
    return output;
  }
  
  //Create sync objects
  

  res = vkCreateFence(device->device, & (VkFenceCreateInfo){
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    },
    get_glob_vk_alloc(), &output.signal_fence);
  if(VK_SUCCESS != res){
    output.code = INIT_COMPUTE_JOB_FENCE_FAIL;
    return output;
  }

  VkSemaphoreCreateInfo sema_create_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  res = vkCreateSemaphore(device->device, &sema_create_info,
			  get_glob_vk_alloc(), &output.release_sema);
  if(VK_SUCCESS != res){
    output.code = INIT_COMPUTE_JOB_SINGLE_SEMAPHORE_FAIL;
    return output;
  }

  res = vkCreateSemaphore(device->device, &sema_create_info,
			  get_glob_vk_alloc(), &output.acquire_sema);
  if(VK_SUCCESS != res){
    output.code = INIT_COMPUTE_JOB_SINGLE_SEMAPHORE_FAIL;
    return output;
  }

  res = vkCreateSemaphore(device->device, &sema_create_info,
			  get_glob_vk_alloc(), &output.signal_sema);
  if(VK_SUCCESS != res){
    output.code = INIT_COMPUTE_JOB_SINGLE_SEMAPHORE_FAIL;
    return output;
  }
  
  OptSemaphores copy_wait = create_semaphores(allocr, device->device, max_frames);
  if(copy_wait.code < 0){
    clear_semaphores(allocr, copy_wait, device->device);
    output.code = INIT_COMPUTE_JOB_COPY_WAIT_SEMA_FAIL;
    return output;
  }
  output.copy_wait_sema = copy_wait.value;

  OptSemaphores copy_signal = create_semaphores(allocr, device->device, max_frames);
  if(copy_signal.code < 0){
    clear_semaphores(allocr, copy_signal, device->device);
    output.code = INIT_COMPUTE_JOB_COPY_SIGNAL_SEMA_FAIL;
    return output;
  }
  output.copy_signal_sema = copy_signal.value;

  OptSemaphores render_signal = create_semaphores(allocr, device->device, max_frames);
  if(copy_signal.code < 0){
    clear_semaphores(allocr, render_signal, device->device);
    output.code = INIT_COMPUTE_JOB_RENDER_SEMA_FAIL;
    return output;
  }
  output.render_signal_sema = render_signal.value;
  
  //Create gpu resources
  output.buffered_items = SLICE_ALLOC(MemoryItem, output.main_items->count, allocr);
  output.render_side_items = SLICE_ALLOC(MemoryItemSlice, max_frames, allocr);
  if(!output.buffered_items.data || !output.render_side_items.data){
    output.code = INIT_COMPUTE_JOB_RESOURCE_BUFFER_ALLOC_FAIL;
    return output;
  }

  memset(output.buffered_items.data, 0, MemoryItem_slice_bytes(output.buffered_items));
  memset(output.render_side_items.data, 0, MemoryItemSlice_slice_bytes(output.render_side_items));
  for_slice(output.render_side_items, i){
    MemoryItemSlice items = SLICE_ALLOC(MemoryItem, output.main_items->count, allocr);
    if(!items.data){
      output.code = INIT_COMPUTE_JOB_RESOURCE_BUFFER_ALLOC_FAIL;
      return output;
    }
    memset(items.data, 0, MemoryItem_slice_bytes(items));
    output.render_side_items.data[i] = items;
  }
  //TODO move this assertion where it makes sense
  assert(output.main_items->count == output.main_item_img_formats->count);
  assert(output.main_items->count == output.main_item_usages->count);
  for_slice(*output.main_items, i){
    switch(output.main_items->data[i].type){
    case MEMORY_ITEM_TYPE_IMAGE:{

      AllocImageParams params = {
	.image_format = output.main_item_img_formats->data[i],
	.optimal_layout = true,
	.props_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	.usage = output.main_item_usages->data[i] |
	VK_IMAGE_USAGE_TRANSFER_DST_BIT
      };
      size_t width = output.main_items->data[i].image.obj.width;
      size_t height = output.main_items->data[i].image.obj.height;

      
      //Create max_frames for render_side_items
      for_slice(output.render_side_items, j){
	OptImage render_img = alloc_image(gpu_allocr, device->device, width, height, params);
	if(render_img.code < 0){
	  free_image(gpu_allocr, render_img, device->device);
	  output.code = INIT_COMPUTE_JOB_RESOURCE_ALLOC_FAIL;
	  return output;
	}
	output.render_side_items.data[j].data[i] =
	  (MemoryItem){
	  .type = MEMORY_ITEM_TYPE_IMAGE,
	  .image = {
	    //The layout item should be in
	    .layout = output.main_items->data[i].image.layout,
	    .obj = render_img.value,
	  },
	};
      }
      
      //Create 1 for buffered_items
      params.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      OptImage buff_img = alloc_image(gpu_allocr, device->device, width, height, params);
      if(buff_img.code < 0){
	free_image(gpu_allocr, buff_img, device->device);
	output.code = INIT_COMPUTE_JOB_RESOURCE_ALLOC_FAIL;
	return output;
      }
      output.buffered_items.data[i] =
	(MemoryItem){
	.type = MEMORY_ITEM_TYPE_IMAGE,
	.image = {
	  //The layout item should be in 
	  .layout = output.main_items->data[i].image.layout,
	  .obj = buff_img.value,
	},
      };
    } break;
    case MEMORY_ITEM_TYPE_GPU_BUFFER:{
      AllocBufferParams params = {
	.props_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	.usage = output.main_item_usages->data[i] | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      };
      size_t size = output.main_items->data[i].gpu_buffer.size;
      
      //Create max_frames for render_side_items
      for_slice(output.render_side_items, j){
	OptBuffer render_buff = alloc_buffer(gpu_allocr, device->device, size, params);
	if(render_buff.code < 0){
	  free_buffer(gpu_allocr, render_buff, device->device);
	  output.code = INIT_COMPUTE_JOB_RESOURCE_ALLOC_FAIL;
	  return output;
	}
	output.render_side_items.data[j].data[i] =
	  (MemoryItem){
	  .type = MEMORY_ITEM_TYPE_GPU_BUFFER,
	  .gpu_buffer = render_buff.value,
	};
      }
      
      //Create 1 for buffered_items
      params.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      OptBuffer buff_buff = alloc_buffer(gpu_allocr, device->device, size, params);
      if(buff_buff.code < 0){
	free_buffer(gpu_allocr, buff_buff, device->device);
	output.code = INIT_COMPUTE_JOB_RESOURCE_ALLOC_FAIL;
	return output;
      }
      output.buffered_items.data[i] =
	(MemoryItem){
	.type = MEMORY_ITEM_TYPE_GPU_BUFFER,
	.gpu_buffer = buff_buff.value,
      };
    } break;
    default:
      assert(false);
    }
  }
  //Create and initialize the invalidation array
  output.is_res_valid = SLICE_ALLOC(bool, max_frames, allocr);
  if(nullptr == output.is_res_valid.data){
    output.code = INIT_COMPUTE_JOB_INVALIDATION_ARRAY_FAIL;
    return output;
  }

  for_slice(output.is_res_valid, i){
    output.is_res_valid.data[i] = true;
  }
  
  //INIT_COMPUTE_JOB_INVALIDATION_ARRAY_FAIL,
  
  //Record copying commands

  //This should always be empty
  MemoryItemDarray dummy_stage = init_MemoryItem_darray(allocr);

  //TODO :: Free this at the end
  CopyUnitSlice copy_items = SLICE_ALLOC(CopyUnit, output.main_items->count, allocr);
  if(copy_items.count != output.main_items->count){
    output.code = INIT_COMPUTE_JOB_COPY_START_FAIL;
    return output;
  }
  VkCommandBufferBeginInfo cmd_begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
  //Failure cases are out of memory


#define RecordCmd(cmd_buffer, src_data, dst_data, src_queue, dst_queue)	\
  do{									\
    res = vkBeginCommandBuffer((cmd_buffer), &cmd_begin_info);		\
    if(res != VK_SUCCESS){						\
      output.code = INIT_COMPUTE_JOB_COPY_RECORD_FAIL;			\
      goto end_part;							\
    }									\
									\
    for_slice(copy_items, i){						\
      copy_items.data[i].src = (src_data).data[i];			\
      copy_items.data[i].dst = (dst_data).data[i];			\
    }									\
									\
    if(copy_memory_items(&dummy_stage, copy_items, device->device,	\
			 (CopyMemoryParam){				\
			   .allocr = allocr,				\
			   .gpu_allocr = gpu_allocr,			\
			   .cmd_buf = (cmd_buffer),			\
			   .src_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, \
			   .dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, \
			   .src_queue_family = (src_queue),		\
			   .dst_queue_family = (dst_queue),		\
			 }) != copy_items.count){			\
      output.code = INIT_COMPUTE_JOB_COPY_RECORD_FAIL;			\
      goto end_part;							\
    }									\
    assert(dummy_stage.count == 0);					\
    res = vkEndCommandBuffer((cmd_buffer));				\
    if(VK_SUCCESS != res){						\
      output.code = INIT_COMPUTE_JOB_COPY_RECORD_FAIL;			\
      goto end_part;							\
    }									\
  }while(0)

//Release ownership from compute to graphics
  RecordCmd(output.release_cmd_buf, *output.main_items, *output.main_items,
	    device->compute.family, device->graphics.family);

  //Acquire ownership from compute to graphics, then copy items to buffered counterpart
  //  finally release ownership back to compute
  RecordCmd(output.copy_main_cmd_buf, *output.main_items, output.buffered_items,
	    device->graphics.family, device->graphics.family);

  //Acquire ownership from graphics to compute and signal the compute job
  RecordCmd(output.acquire_cmd_buf, *output.main_items, *output.main_items,
	    device->graphics.family, device->compute.family);

  //For each frame in graphics now, copy from graphics buffered side to per frame buffered side
  assert(output.frame_copy_cmd_buf.count == output.render_side_items.count);
  for_slice(output.frame_copy_cmd_buf, index){
    RecordCmd(output.frame_copy_cmd_buf.data[index], output.buffered_items,
	      output.render_side_items.data[index],
	      device->graphics.family, device->graphics.family);
  }
  
 end_part:
  SLICE_FREE(copy_items, allocr);
    
  #undef RecordCmd
  //TOOD Overall cleanup code here, also check for all returns, above
  return output;
}

//clean func

//Helper function to compose semaphores to give to queue submit
typedef VkSemaphoreSubmitInfo SemaInfo;
DEF_DARRAY(SemaInfo, 1);

SemaInfo make_bin_sema(VkSemaphore sema){
  return (SemaInfo){
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,	
    .semaphore = sema,
    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
  };
}

//Check and run func, needs to be called after begin_rendering operations
//Returns if a semaphore needs to be added to the wait list for current rendering frame
RunComputeOutput check_run_compute_job(AllocInterface allocr,
				       ComputeJob* job_data,
				       u32 curr_frame){

  VkResult res = VK_SUCCESS;

  SemaInfoDarray wait_sema = init_SemaInfo_darray(allocr);
  SemaInfoDarray signal_sema = init_SemaInfo_darray(allocr);
    
#define PQUEUE_INFO(cmd_buffer, ...)				\
  &(VkSubmitInfo2){						\
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,			\
      .commandBufferInfoCount = 1,				\
      .pCommandBufferInfos =					\
      &(VkCommandBufferSubmitInfo){				\
	.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,	\
	.commandBuffer = (cmd_buffer),				\
      },							\
      .signalSemaphoreInfoCount = signal_sema.count,		\
      .pSignalSemaphoreInfos = signal_sema.data,		\
      .waitSemaphoreInfoCount = wait_sema.count,		\
      .pWaitSemaphoreInfos = wait_sema.data,			\
      __VA_ARGS__						\
      }

  //TODO :: Maybe some failure states can be recovered
#define fail_action()						\
  do{resize_SemaInfo_darray(&signal_sema, 0);			\
    resize_SemaInfo_darray(&wait_sema, 0);			\
    return (RunComputeOutput){.was_failed = true};}while(0)	\

    
  if(VK_SUCCESS == vkGetFenceStatus(job_data->device->device, job_data->signal_fence)){
    //Setup a whole new batch of ownership transfer

    //For now assume all command buffers have been recorded already for simplicity
    
    //Release the ownership of compute side resources 
    wait_sema.count = 0;
    signal_sema.count = 0;
    push_SemaInfo_darray(&signal_sema, make_bin_sema(job_data->release_sema));
						     
    if(signal_sema.count < 1)
      fail_action();
    res = vkQueueSubmit2(job_data->device->compute.vk_obj, 1,
			 PQUEUE_INFO(job_data->release_cmd_buf), VK_NULL_HANDLE);
    if(VK_SUCCESS != res)
      fail_action();

    //Copy all shared resources from original to buffered
    wait_sema.count = 0;
    signal_sema.count = 0;
    bool all_pushed = true;
    //If is_res_valid is true, wait on actual waiting semaphores,
    //   else wait on the signaling semaphores,
    //   except for the first time wait on none ?
    if(!job_data->first_time){
      for_slice(job_data->is_res_valid, i){
	if(job_data->is_res_valid.data[i]){
	  all_pushed = push_SemaInfo_darray(&wait_sema, make_bin_sema(job_data->copy_wait_sema.data[i])) && all_pushed;
	}
	else {
	  all_pushed = push_SemaInfo_darray(&wait_sema, make_bin_sema(job_data->copy_signal_sema.data[i])) && all_pushed;
	}
      }
    }
    all_pushed = push_SemaInfo_darray(&wait_sema, make_bin_sema(job_data->release_sema)) && all_pushed;
    
    for_slice(job_data->copy_signal_sema, i){
      all_pushed = push_SemaInfo_darray(&signal_sema,
			   make_bin_sema(job_data->copy_signal_sema.data[i])) && all_pushed;
    }
    all_pushed = push_SemaInfo_darray(&signal_sema, make_bin_sema(job_data->acquire_sema)) && all_pushed;
    if(!all_pushed)
      fail_action();
    res = vkQueueSubmit2(job_data->device->graphics.vk_obj, 1,
			 PQUEUE_INFO(job_data->copy_main_cmd_buf), VK_NULL_HANDLE);
    if(VK_SUCCESS != res)
      fail_action();

    //Acquire the ownership back from the graphics queue
    wait_sema.count = 0;
    signal_sema.count = 0;
    push_SemaInfo_darray(&wait_sema,make_bin_sema(job_data->acquire_sema));
    push_SemaInfo_darray(&signal_sema, make_bin_sema(job_data->signal_sema));
    if((wait_sema.count != 1) ||
       (signal_sema.count != 1))
      fail_action();
    res = vkQueueSubmit2(job_data->device->compute.vk_obj, 1,
			 PQUEUE_INFO(job_data->acquire_cmd_buf), VK_NULL_HANDLE);
    if(VK_SUCCESS != res)
      fail_action();

    //Run the compue job
    VkCommandBuffer job_cmd = job_data->run_job_fn(job_data->job_data, &(ComputeJobParam){
	.allocr = job_data->allocr,
	.gpu_allocr = job_data->gpu_allocr,
	.device = job_data->device->device,
	.compute_queue = &job_data->device->compute
      });
    if(VK_NULL_HANDLE == job_cmd)
      fail_action();
    wait_sema.count = 0;
    signal_sema.count = 0;
    push_SemaInfo_darray(&wait_sema, make_bin_sema(job_data->signal_sema));
    if(1 != wait_sema.count)
      fail_action();
    res = vkResetFences(job_data->device->device, 1, &job_data->signal_fence);
    if(VK_SUCCESS != res)
      fail_action();
    res = vkQueueSubmit2(job_data->device->compute.vk_obj, 1,
			 PQUEUE_INFO(job_cmd), job_data->signal_fence);
    if(VK_SUCCESS != res)
      fail_action();
    //Now for the rest, we don't just yet submit these to queue, just reset their validity
    memset(job_data->is_res_valid.data, 0, bool_slice_bytes(job_data->is_res_valid));
  }
  // Check if need to submit another command to graphics queue
  if(!job_data->is_res_valid.data[curr_frame]){
    //Submit another cmd buffer

    wait_sema.count = 0;
    signal_sema.count = 0;
    push_SemaInfo_darray(&wait_sema, make_bin_sema(job_data->copy_signal_sema.data[curr_frame]));
    push_SemaInfo_darray(&signal_sema, make_bin_sema(job_data->copy_wait_sema.data[curr_frame]));
    push_SemaInfo_darray(&signal_sema, make_bin_sema(job_data->render_signal_sema.data[curr_frame]));
    if((wait_sema.count != 1) || (signal_sema.count != 2))
      fail_action();
    res = vkQueueSubmit2(job_data->device->graphics.vk_obj, 1,
			 PQUEUE_INFO(job_data->frame_copy_cmd_buf.data[curr_frame]),
			 VK_NULL_HANDLE);
    if(res != VK_SUCCESS)
      fail_action();
    job_data->is_res_valid.data[curr_frame] = true;
    job_data->first_time = false;
    return (RunComputeOutput){
      .sema = job_data->render_signal_sema.data[curr_frame],
      .was_failed = false,
    };
  }
  return(RunComputeOutput){
    .sema = VK_NULL_HANDLE,
    .was_failed = false,
  };
  
#undef fail_action
#undef PQUEUE_INFO
}

void clean_compute_job(ComputeJob* job){
  assert(job != nullptr);

  vkDeviceWaitIdle(job->device->device);
  switch(job->code){
  case INIT_COMPUTE_JOB_OK:

  case INIT_COMPUTE_JOB_COPY_RECORD_FAIL:
  case INIT_COMPUTE_JOB_COPY_START_FAIL:

  case INIT_COMPUTE_JOB_INVALIDATION_ARRAY_FAIL:
    SLICE_FREE(job->is_res_valid, job->allocr);
    
  case INIT_COMPUTE_JOB_RESOURCE_ALLOC_FAIL:
    for_slice(job->render_side_items, i){
      for_slice(job->render_side_items.data[i], j){
	job->render_side_items.data[i].data[j] =
	  free_memory_item(job->render_side_items.data[i].data[j],
			   job->allocr, job->gpu_allocr,
			   job->device->device);

      }
    }
    for_slice(job->buffered_items, i){
      job->buffered_items.data[i] = free_memory_item(job->buffered_items.data[i],
						     job->allocr, job->gpu_allocr,
						     job->device->device);
    }
    
  case INIT_COMPUTE_JOB_RESOURCE_BUFFER_ALLOC_FAIL:
    for_slice(job->render_side_items, i){
      SLICE_FREE(job->render_side_items.data[i], job->allocr);
    }
    SLICE_FREE(job->render_side_items, job->allocr);
    SLICE_FREE(job->buffered_items, job->allocr);

    
    
  case INIT_COMPUTE_JOB_RENDER_SEMA_FAIL:
    clear_semaphores(job->allocr, (OptSemaphores){.value = job->render_signal_sema}, job->device->device);
    
  case INIT_COMPUTE_JOB_COPY_SIGNAL_SEMA_FAIL:
    clear_semaphores(job->allocr, (OptSemaphores){.value = job->copy_signal_sema}, job->device->device);
    
  case INIT_COMPUTE_JOB_COPY_WAIT_SEMA_FAIL:
    clear_semaphores(job->allocr, (OptSemaphores){.value = job->copy_wait_sema}, job->device->device);    
    
  case INIT_COMPUTE_JOB_SINGLE_SEMAPHORE_FAIL:
    if(0 != job->release_sema)
      vkDestroySemaphore(job->device->device, job->release_sema, get_glob_vk_alloc());
    if(0 != job->acquire_sema)
      vkDestroySemaphore(job->device->device, job->acquire_sema, get_glob_vk_alloc());
    if(0 != job->signal_sema)
      vkDestroySemaphore(job->device->device, job->signal_sema, get_glob_vk_alloc());
    
    vkDestroyFence(job->device->device, job->signal_fence, get_glob_vk_alloc());
  case INIT_COMPUTE_JOB_FENCE_FAIL:
  case INIT_COMPUTE_JOB_CMD_BUF_FAIL:
    SLICE_FREE(job->frame_copy_cmd_buf, job->allocr);
    clear_command_pool((OptCommandPool){.value = job->compute_pool}, job->device->device);
    clear_command_pool((OptCommandPool){.value = job->graphics_pool}, job->device->device);
  case INIT_COMPUTE_JOB_CMD_POOL_FAIL:
  case INIT_COMPUTE_JOB_JOB_INIT_FAIL:
    job->clean_job_fn(job->job_data, &(ComputeJobParam){
	.allocr = job->allocr,
	.gpu_allocr = job->gpu_allocr,
	.device = job->device->device,
	.compute_queue = &job->device->compute
      });
  case INIT_COMPUTE_JOB_TOP_FAIL_CODE:
    //TODO Decide if main_items has to be freed here or somewhere else
    *job = (ComputeJob){0};
    job->code = INIT_COMPUTE_JOB_TOP_FAIL_CODE;
  }
  
}

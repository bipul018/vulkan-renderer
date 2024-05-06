#include "pipeline-helpers.h"


DEF_DARRAY(DescSize,1);
//Will have to handle inline uniform blocks itself
VkDescriptorPool make_pool(AllocInterface allocr, VkDevice device, DescSetSlice sets){
  DescSizeDarray da_list = init_DescSize_darray(allocr);
  DescSizeDarray* ptr = &da_list;
  //Count unique descriptor types
  u32 max_sets = 0;
  u32 inline_bindings = 0;
  for_slice(sets, i){
    max_sets += sets.data[i].count;
    for_slice(sets.data[i].descs, j){

      if(sets.data[i].descs.data[j].type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK){
	inline_bindings += sets.data[i].count;
      }
      
      s32 inx = -1;
      for_slice(da_list, k){
	if(da_list.data[k].type == sets.data[i].descs.data[j].type){
	  inx = k;
	  goto found;
	}
      }
      
      if(push_DescSize_darray(&da_list, (DescSize){
	    .type = sets.data[i].descs.data[j].type,
	  })){ 
	inx = da_list.count -1;
      }
      else{
	(void)resize_DescSize_darray(&da_list, 0);
	return VK_NULL_HANDLE;
      }
      found:
      da_list.data[inx].descriptorCount += sets.data[i].count *
	sets.data[i].descs.data[j].descriptorCount;
    }
  }

  VkDescriptorPool pool = VK_NULL_HANDLE;
  VkDescriptorPoolCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = max_sets,
    .poolSizeCount = da_list.count,//need to see flags
    .pPoolSizes = da_list.data,
    .pNext = &(VkDescriptorPoolInlineUniformBlockCreateInfo){
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO,
      .maxInlineUniformBlockBindings = inline_bindings,
    },
  };
  if(inline_bindings == 0){
    create_info.pNext = nullptr;
  }

  if(VK_SUCCESS != vkCreateDescriptorPool(device, &create_info, get_glob_vk_alloc(), &pool)){
    pool = VK_NULL_HANDLE;
  }
  
  (void)resize_DescSize_darray(&da_list, 0);
  return pool;
}

VkDescriptorSet alloc_set(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout set_layout){
  VkDescriptorSet set;
  VkResult res = vkAllocateDescriptorSets
    (device, &(VkDescriptorSetAllocateInfo){
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts=&set_layout,
    }, &set);
  if(res == VK_SUCCESS)
    return set;
  return VK_NULL_HANDLE;
}

void write_uniform_descriptor(VkDevice device, VkDescriptorSet set, BufferObj buffer, int binding){
  vkUpdateDescriptorSets(device, 1, &(VkWriteDescriptorSet){
      	   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	   .dstSet = set,
	   .dstBinding = binding,
	   .dstArrayElement = 0,
	   .descriptorCount = 1,
	   .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	   .pBufferInfo = &(VkDescriptorBufferInfo){
	     .buffer = buffer.vk_obj,
	     .offset = 0,
	     .range = buffer.size,
	   },
    }, 0, nullptr);
}
void write_sampler(VkDevice device, VkDescriptorSet set, VkSampler sampler, int binding){
  vkUpdateDescriptorSets(device, 1, &(VkWriteDescriptorSet){
      	   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	   .dstSet = set,
	   .dstBinding = binding,
	   .dstArrayElement = 0,
	   .descriptorCount = 1,
	   .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
	   .pImageInfo = &(VkDescriptorImageInfo){
	     .sampler = sampler,
	   },
    }, 0, nullptr);
}
//Expects image layout to be shader read only optimal
void write_image(VkDevice device, VkDescriptorSet set, VkImageView image, int binding){
  vkUpdateDescriptorSets(device, 1, &(VkWriteDescriptorSet){
      	   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	   .dstSet = set,
	   .dstBinding = binding,
	   .dstArrayElement = 0,
	   .descriptorCount = 1,
	   .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
	   .pImageInfo = &(VkDescriptorImageInfo){
	     .imageView = image,
	     .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	   },
    }, 0, nullptr);
}
void write_inline_descriptor(VkDevice device, VkDescriptorSet set, u8Slice data, int binding){

  vkUpdateDescriptorSets
    (device, 1,
     &(VkWriteDescriptorSet){
       .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .dstSet = set,
       .dstBinding = binding,
       .dstArrayElement = 0,
       .descriptorCount = data.count,
       .descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
       .pNext = &(VkWriteDescriptorSetInlineUniformBlock){
	 .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK,
	 .dataSize = data.count,
	 .pData = data.data
       },
     }, 0, nullptr);
}


//Vertex input helpers

VertexInputDesc init_vertex_input(AllocInterface allocr){
  return(VertexInputDesc){
    .bindings = init_VertexBinding_darray(allocr),
    .attrs = init_VertexAttribute_darray(allocr)
  };
}
void clear_vertex_input(VertexInputDesc* vert_desc){
  (void)resize_VertexBinding_darray(&vert_desc->bindings, 0);
  (void)resize_VertexAttribute_darray(&vert_desc->attrs, 0);
  vert_desc->current_binding = 0;
}

//If not added yet binding, pushes, returns false if failed to push,
//Else changes the stride/rate of the binding
bool vertex_input_set_binding(VertexInputDesc* vert_desc, u32 binding, u32 stride, bool vertex_rate){
  if(nullptr == vert_desc)
    return false;
  //Search for binding entry
  
  for_slice(vert_desc->bindings, i){
    if(vert_desc->bindings.data[i].binding == binding){
      vert_desc->bindings.data[i].stride = stride;
      vert_desc->bindings.data[i].inputRate =
	(vertex_rate?VK_VERTEX_INPUT_RATE_VERTEX:VK_VERTEX_INPUT_RATE_INSTANCE);
      vert_desc->current_binding = binding;
      return true;
    }
  }

  if(!push_VertexBinding_darray(&vert_desc->bindings,(VertexBinding){
	.binding = binding,
	.stride = stride,
	.inputRate = (vertex_rate?VK_VERTEX_INPUT_RATE_VERTEX:VK_VERTEX_INPUT_RATE_INSTANCE)
      })){
    return false;
  }
  vert_desc->current_binding = binding;
  return true;
}
//If not added yet the location, pushes, returns false if failed to push,
//Else changes the offset/format/binding of the location
bool vertex_input_add_attribute(VertexInputDesc* vert_desc,u32 location, u32 offset, VkFormat format){
  if(nullptr == vert_desc){
    return false;
  }
  for_slice(vert_desc->attrs, i){
    if(vert_desc->attrs.data[i].location == location){
      vert_desc->attrs.data[i].offset = offset;
      vert_desc->attrs.data[i].format = format;
      vert_desc->attrs.data[i].binding = vert_desc->current_binding;
      return true;
    }
  }
  if(!push_VertexAttribute_darray(&vert_desc->attrs, (VertexAttribute){
	.binding = vert_desc->current_binding,
	.location = location,
	.format = format,
	.offset = offset
      })){
    return false;
  }
  return true;
}



typedef VkImageMemoryBarrier2 ImgBarr;
DEF_DARRAY(ImgBarr,1);

CopyResult copy_items_to_gpu(AllocInterface allocr, GPUAllocr* gpu_allocr, VkDevice device, CopyInputSlice items, u32 queue_index, VkQueue queue){

  CopyResult result = {0};
  result.value.stage_bufs = init_BufferObj_darray(allocr);
  
  if(VK_SUCCESS != vkCreateFence(device, &(VkFenceCreateInfo){
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
      }, get_glob_vk_alloc(), &result.value.fence)){
    result.code = COPY_ITEMS_TO_GPU_FENCE_CREATE_FAIL;
    return result;
  }

  //First collect all layout transitions needed
  ImgBarrDarray transitions = init_ImgBarr_darray(allocr);
  for_slice(items, i){
    if(!items.data[i].is_buffer){
      if(!push_ImgBarr_darray(&transitions, (ImgBarr){
	    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
	    .srcStageMask = VK_ACCESS_NONE,
	    .srcAccessMask = VK_ACCESS_NONE,
	    /* .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, */
	    /* .dstAccessMask = VK_ACCESS_SHADER_READ_BIT, */
	    .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
	    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
	    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    //.newLayout = VK_IMAGE_LAYOUT_GENERAL,
	    .srcQueueFamilyIndex = queue_index,
	    .dstQueueFamilyIndex = queue_index,//g_device.graphics_family_inx,
	    .image = items.data[i].image.vk_obj,
	    .subresourceRange = {
	      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	      .layerCount = VK_REMAINING_ARRAY_LAYERS,
	      .levelCount = VK_REMAINING_MIP_LEVELS,
	    }
	  })){
	resize_ImgBarr_darray(&transitions, 0);
	result.code = COPY_ITEMS_TO_GPU_ALLOC_INT_FAIL;
	goto free_transitions;
      }
    }
  }
  
  
  if(VK_SUCCESS != vkCreateCommandPool(device,&(VkCommandPoolCreateInfo){
	.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
	.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
	.queueFamilyIndex = queue_index,
      }, get_glob_vk_alloc(), &result.value.cmd_pool)){
    result.code = COPY_ITEMS_TO_GPU_COMMAND_POOL_FAIL;
    goto free_transitions;
  }
  VkCommandBuffer cmd_buf = VK_NULL_HANDLE;
  if(VK_SUCCESS != vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo){
	.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	.commandPool = result.value.cmd_pool,
	.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	.commandBufferCount = 1,
      }, &cmd_buf)){
    result.code = COPY_ITEMS_TO_GPU_COMMAND_BUFFER_FAIL;
    goto free_transitions;
  }
  if(VK_SUCCESS != vkBeginCommandBuffer
     (cmd_buf, &(VkCommandBufferBeginInfo){
       .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
       .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
     })){
    result.code = COPY_ITEMS_TO_GPU_RECORD_BEGIN_FAIL;
    goto free_transitions;
  }


  vkCmdPipelineBarrier2(cmd_buf, &(VkDependencyInfo){
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .dependencyFlags = 0,
      .imageMemoryBarrierCount = transitions.count,
      .pImageMemoryBarriers = transitions.data
    });

  //Create buffers, map  and push into copied_items
  for_slice(items, i){
    //Check if staging needed, that is done by trying to map memory
    BufferObj obj = {0};
    void* mapping = nullptr;
    bool was_staged = true;
    if(items.data[i].is_buffer){
      mapping = create_mapping_of_buffer(device, *gpu_allocr, items.data[i].buffer);
      if(mapping != nullptr){
	obj = items.data[i].buffer;
	was_staged = false;
      }
    }

    if(mapping == nullptr){
      OptBuffer stage = alloc_buffer(gpu_allocr, device, items.data[i].src.count,
					(AllocBufferParams){
					  .props_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
					  .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
					});
      if(stage.code < 0){
	free_buffer(gpu_allocr, stage, device);
	result.code = COPY_ITEMS_TO_GPU_ALLOC_STAGING_FAIL;
	goto free_transitions;
	//return result;
      }
      mapping = create_mapping_of_buffer(device, *gpu_allocr, stage.value);
      obj = stage.value;
      //push
      if(!push_BufferObj_darray(&result.value.stage_bufs, stage.value)){
	flush_mapping_of_buffer(device, *gpu_allocr, stage.value);
	free_buffer(gpu_allocr, stage, device);
	result.code = COPY_ITEMS_TO_GPU_ALLOC_RESULT_FAIL;
	goto free_transitions;
	//return result;
      }
    }
    memcpy(mapping, items.data[i].src.data, items.data[i].src.count);
    flush_mapping_of_buffer(device, *gpu_allocr, obj);

    //now write copy commands if needed
    if(was_staged){
      if(items.data[i].is_buffer){
	vkCmdCopyBuffer(cmd_buf, obj.vk_obj, items.data[i].buffer.vk_obj, 1,
			&(VkBufferCopy){
			  .size = items.data[i].src.count
			});
      }
      else{
	vkCmdCopyBufferToImage(cmd_buf, obj.vk_obj, items.data[i].image.vk_obj,
			       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			       &(VkBufferImageCopy){
				 .imageSubresource = {
				   .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				   .layerCount = 1
				 },
				 .imageExtent = {
				   .depth = 1,
				   .width = items.data[i].image.width,
				   .height = items.data[i].image.height,
				 }
			       });
      }
    }
  }
  //Change the layouts again
  for_slice(transitions, i){
    transitions.data[i].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    transitions.data[i].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    transitions.data[i].dstStageMask = VK_PIPELINE_STAGE_NONE;
    transitions.data[i].dstAccessMask = VK_ACCESS_NONE;
    transitions.data[i].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitions.data[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }


  vkCmdPipelineBarrier2(cmd_buf, &(VkDependencyInfo){
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .dependencyFlags = 0,
      .imageMemoryBarrierCount = transitions.count,
      .pImageMemoryBarriers = transitions.data
    });

 free_transitions:
  //Free transitions here ??
  (void)resize_ImgBarr_darray(&transitions, 0);

  if(result.code < 0){
    return result;
  }
  
  //End submission
  if(VK_SUCCESS != vkEndCommandBuffer(cmd_buf)){
    result.code = COPY_ITEMS_TO_GPU_RECORD_END_FAIL;
    return result;
  }

  //Transition the fence state
  if(VK_SUCCESS != vkResetFences(device, 1, &result.value.fence)){
    result.code = COPY_ITEMS_TO_GPU_RESET_FENCE_FAIL;
    return result;
  }

  if(VK_SUCCESS != vkQueueSubmit2
     (queue, 1, &(VkSubmitInfo2){
       .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
       .commandBufferInfoCount = 1,
       .pCommandBufferInfos = &(VkCommandBufferSubmitInfo){
	 .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
	 .commandBuffer = cmd_buf,
       }
     }, result.value.fence)){
    result.code = COPY_ITEMS_TO_GPU_SUBMIT_FAIL;
    return result;
  }

  return result;
}

CopyResult copy_items_to_gpu_wait(VkDevice device, GPUAllocr* gpu_allocr, CopyResult copy_result){
  switch(copy_result.code){
  case COPY_ITEMS_TO_GPU_OK:
    (void)vkWaitForFences(device, 1, &copy_result.value.fence, VK_TRUE, UINT64_MAX);
  case COPY_ITEMS_TO_GPU_SUBMIT_FAIL:
  case COPY_ITEMS_TO_GPU_RESET_FENCE_FAIL:
  case COPY_ITEMS_TO_GPU_RECORD_END_FAIL:
  case COPY_ITEMS_TO_GPU_ALLOC_RESULT_FAIL:
  case COPY_ITEMS_TO_GPU_ALLOC_STAGING_FAIL:
    
    for_slice(copy_result.value.stage_bufs, i){
      free_buffer(gpu_allocr, (OptBuffer){
	  .value = copy_result.value.stage_bufs.data[i]
	}, device);
    }
    (void)resize_BufferObj_darray(&copy_result.value.stage_bufs, 0);
  case COPY_ITEMS_TO_GPU_RECORD_BEGIN_FAIL:
  case COPY_ITEMS_TO_GPU_COMMAND_BUFFER_FAIL:
    vkDestroyCommandPool(device, copy_result.value.cmd_pool, get_glob_vk_alloc());
  case COPY_ITEMS_TO_GPU_COMMAND_POOL_FAIL:

  case COPY_ITEMS_TO_GPU_ALLOC_INT_FAIL:
    vkDestroyFence(device, copy_result.value.fence, get_glob_vk_alloc());
  case COPY_ITEMS_TO_GPU_FENCE_CREATE_FAIL:
  case COPY_ITEMS_TO_GPU_TOP_FAIL_CODE:
    copy_result = (CopyResult){.code = COPY_ITEMS_TO_GPU_TOP_FAIL_CODE};
  }
  return copy_result;
}

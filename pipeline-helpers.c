#include "pipeline-helpers.h"
#include "util_structures.h"
#include "vulkan/vulkan_core.h"


VkDescriptorSetLayout create_descriptor_set_layout_(VkDevice device,
						   VkDescriptorSetLayoutCreateFlags flags,
						   DescBindingSlice bindings){

  VkDescriptorSetLayoutCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pBindings = bindings.data,
    .bindingCount = bindings.count,
    .flags = flags
  };
  VkDescriptorSetLayout desc_layout = VK_NULL_HANDLE;
  VkResult res = vkCreateDescriptorSetLayout(device, &create_info,
					     get_glob_vk_alloc(),
					     &desc_layout);
  if(res != VK_SUCCESS)
    return VK_NULL_HANDLE;
  return desc_layout;
}

DEF_DARRAY(DescSize,1);
//Will have to handle inline uniform blocks itself
VkDescriptorPool make_pool_(AllocInterface allocr, VkDevice device, DescSetSlice sets){
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
void write_storage_descriptor(VkDevice device, VkDescriptorSet set, BufferObj buffer, int binding){
  vkUpdateDescriptorSets(device, 1, &(VkWriteDescriptorSet){
      	   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	   .dstSet = set,
	   .dstBinding = binding,
	   .dstArrayElement = 0,
	   .descriptorCount = 1,
	   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
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
void write_storage_image(VkDevice device, VkDescriptorSet set, VkImageView image, int binding){
  vkUpdateDescriptorSets(device, 1, &(VkWriteDescriptorSet){
      	   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	   .dstSet = set,
	   .dstBinding = binding,
	   .dstArrayElement = 0,
	   .descriptorCount = 1,
	   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
	   .pImageInfo = &(VkDescriptorImageInfo){
	     .imageView = image,
	     .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
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



MemoryItem free_memory_item(MemoryItem item, AllocInterface allocr, GPUAllocr* gpu_allocr, VkDevice device){
  switch(item.type){
  case MEMORY_ITEM_TYPE_CPU_BUFFER:{
    SLICE_FREE(item.cpu_buffer, allocr);
  }break;
  case MEMORY_ITEM_TYPE_GPU_BUFFER:{
    free_buffer(gpu_allocr, (OptBuffer){.value = item.gpu_buffer}, device);
  }break;
  case MEMORY_ITEM_TYPE_IMAGE:{
    free_image(gpu_allocr, (OptImage){.value = item.image.obj}, device);
  }break;
  case MEMORY_ITEM_TYPE_NONE:
    break;
  }
  return (MemoryItem){0};
}


typedef VkImageMemoryBarrier2 ImgBarr;
DEF_DARRAY(ImgBarr,1);
typedef VkBufferMemoryBarrier2 BuffBarr;
DEF_DARRAY(BuffBarr, 1);

static u32 submit_barriers(VkCommandBuffer cmd_buf, ImgBarrDarray* img_barrs,
			    BuffBarrDarray* buff_barrs, CopyUnitSlice items,
			    bool is_src, bool is_before,
			    VkPipelineStageFlags2 stage_mask,
			    VkPipelineStageFlags2 another_stage_mask,
			    u32 src_queue_family,
			    u32 dst_queue_family){
  u32 barrier_count = 0;
  u32 last_inx = 0;
  bool fail_recent = false;
  while(true){
    img_barrs->count = 0;
    buff_barrs->count = 0;
    for(;last_inx < items.count; last_inx++){
      MemoryItem unit;
      MemoryItem other;
      if(is_src){
	unit = items.data[last_inx].src;
	other = items.data[last_inx].dst;
      }
      else{
	unit = items.data[last_inx].dst;
	other = items.data[last_inx].src;
      }
      //Skip cases are if this is cpu buffer, or both src and dst are none 
      if((unit.type == MEMORY_ITEM_TYPE_CPU_BUFFER) ||
	 ((unit.type == other.type) && (unit.type == MEMORY_ITEM_TYPE_NONE)))
	continue;
      //there is no copy operation is when either of the types are none
      //or the both types are same along with vulkan handle
      bool no_copy = ((unit.type == MEMORY_ITEM_TYPE_NONE) ||
		      (other.type == MEMORY_ITEM_TYPE_NONE));
      if(unit.type == other.type){
	if(unit.type == MEMORY_ITEM_TYPE_GPU_BUFFER){
	  no_copy = no_copy || (unit.gpu_buffer.vk_obj == other.gpu_buffer.vk_obj);
	}
	else{
	  no_copy = no_copy || (unit.image.obj.vk_obj == other.image.obj.vk_obj);
	}
      }
      //A special case of no copy is when src = none, then we process dst in src's turn
      //  either way, we don't process for dst turn, or if it is not is_before anyway
      bool no_cpy_alt = no_copy && unit.type == MEMORY_ITEM_TYPE_NONE;
      if(no_cpy_alt){
	_swap(unit, other);
      }
      if(no_copy && (!is_before || !is_src))
	continue;
      BuffBarr common = {
	    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
	    .srcQueueFamilyIndex = src_queue_family,
	    .srcStageMask = stage_mask,
	    .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
	    .dstQueueFamilyIndex = dst_queue_family,};

      //So the cases to process are,
      // A> when no_copy is false,
      // B> when no_copy is true but no_cpy_alt is false
      // C> when both are true
      if(!no_copy){
	//TODO :: Here we need to account for before or after
	if(is_src){
	  common.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	  common.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
	}
	else{
	  common.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
	  common.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	}
	if(!is_before){
	  if(is_src){
	    _swap(common.srcQueueFamilyIndex, common.dstQueueFamilyIndex);
	  }
	  else{
	    common.srcQueueFamilyIndex = common.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  }
	  _swap(common.srcStageMask, common.dstStageMask);
	  _swap(common.srcAccessMask, common.dstAccessMask);
	}
      }
      else{
	//In this case, there is never after, but there is + dst stage mask
	common.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT |
	  VK_ACCESS_2_MEMORY_WRITE_BIT;
	common.dstStageMask = another_stage_mask,
	common.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT |
	  VK_ACCESS_2_MEMORY_WRITE_BIT;
	if(!no_cpy_alt){
	  //When src is none, we don't do any queue ownership transfer
	  common.srcQueueFamilyIndex = dst_queue_family;
	}
      }
      
      if(unit.type == MEMORY_ITEM_TYPE_IMAGE){
	ImgBarr barr = {
	  .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
	  .srcStageMask = common.srcStageMask,
	  .srcAccessMask = common.srcAccessMask,
	  .dstStageMask = common.dstStageMask,
	  .dstAccessMask = common.dstAccessMask,
	  .oldLayout = unit.image.layout,
	  .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	  .srcQueueFamilyIndex = common.srcQueueFamilyIndex,
	  .dstQueueFamilyIndex = common.dstQueueFamilyIndex,
	  .image = unit.image.obj.vk_obj,
	  .subresourceRange = {
	    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	    .layerCount = VK_REMAINING_ARRAY_LAYERS,
	    .levelCount = VK_REMAINING_MIP_LEVELS,
	  }
	};
	if(!no_copy){
	  if(!is_src){
	    barr.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	    barr.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	  }
	  if(!is_before){
	    if(is_src){
	      _swap(barr.oldLayout, barr.newLayout);
	    }
	    else{
	      barr.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	      barr.newLayout = unit.image.layout;
	    }
	  }
	}
	else{
	  barr.oldLayout = items.data[last_inx].src.image.layout;
	  barr.newLayout = items.data[last_inx].dst.image.layout;
	}
	bool push_res = push_ImgBarr_darray(img_barrs, barr);
	if(push_res){
	  fail_recent = false;
	}
	else{
	  fail_recent = true;
	  if(fail_recent){
	    barrier_count++;
	    vkCmdPipelineBarrier2(cmd_buf, &(VkDependencyInfo){
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.dependencyFlags = 0,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barr,
	      });
	  }
	  else{
	    goto submit1;
	  }
	}
	
      }
      if(unit.type == MEMORY_ITEM_TYPE_GPU_BUFFER){
	BuffBarr barr = common;
	barr.buffer = unit.gpu_buffer.vk_obj;
	barr.offset = 0;
	barr.size = unit.gpu_buffer.size;

	bool push_res = push_BuffBarr_darray(buff_barrs, barr);
	if(push_res){
	  fail_recent = false;
	}
	else{
	  fail_recent = true;
	  if(fail_recent){
	    barrier_count++;
	    vkCmdPipelineBarrier2(cmd_buf, &(VkDependencyInfo){
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.dependencyFlags = 0,
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers = &barr,
	      });
	  }
	  else{
	    //In this case, set all barreris from both arrays,
	    goto submit1;
	    //  reset the size to 0 for both,
	    //  and start from beginning
	  }
	}
      }
    
    }
  submit1:
    //Here submit all
    barrier_count += img_barrs->count + buff_barrs->count;
    vkCmdPipelineBarrier2(cmd_buf, &(VkDependencyInfo){
	.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
	.dependencyFlags = 0,
	.imageMemoryBarrierCount = img_barrs->count,
	.pImageMemoryBarriers = img_barrs->data,
	.bufferMemoryBarrierCount = buff_barrs->count,
	.pBufferMemoryBarriers = buff_barrs->data
      });
    if(last_inx >= items.count){
      break;
    }
  }
  return barrier_count;
}

u32 copy_memory_items(MemoryItemDarray* stage_buffers, CopyUnitSlice items,
		      VkDevice device, CopyMemoryParam param){

  //TODO :: Insert a general memory barrier in case of nothing to copy and exit
  // Or if there was no barrier to insert in the first one,
  //Then too, need to do similar
  
  ImgBarrDarray img_barrs = init_ImgBarr_darray(param.allocr);
  BuffBarrDarray buff_barrs = init_BuffBarr_darray(param.allocr);

  u32 init_barrs = submit_barriers(param.cmd_buf, &img_barrs,
				   &buff_barrs, items,
				   true, true,
				   param.src_stage_mask,
				   param.dst_stage_mask,
				   param.src_queue_family,
				   param.dst_queue_family);
  
  //So now all src pipeline barriers have been installed, similarly do for destinations
  
  init_barrs += submit_barriers(param.cmd_buf, &img_barrs,
				&buff_barrs, items,
				false, true,
				param.src_stage_mask,
				param.dst_stage_mask,
				param.src_queue_family,
				param.dst_queue_family);
  if(init_barrs == 0){
    //TODO Insert a single simple pipeline memory barrier here and return
    vkCmdPipelineBarrier2(param.cmd_buf, &(VkDependencyInfo){
	.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
	.dependencyFlags = 0,
	.memoryBarrierCount = 1,
	.pMemoryBarriers = &(VkMemoryBarrier2){
	  .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
	  .srcStageMask = param.src_stage_mask,
	  .dstStageMask = param.dst_stage_mask,
	  .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT |
	  VK_ACCESS_2_MEMORY_WRITE_BIT,
	  .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT |
	  VK_ACCESS_2_MEMORY_WRITE_BIT,
	}
      });
    
    resize_ImgBarr_darray(&img_barrs, 0);
    resize_BuffBarr_darray(&buff_barrs, 0);
    return 0;
  }
  //Now start copying
  u32 i = 0;
  //for_slice(items, i){
  for_range(, i, 0, items.count){
    //copy_count = i;

    //Need stage buffer or not
    bool need_stage = false;
    //If needed, was the stage buffer for source or destination 
    bool src_stage = false;

    //All CPU -> CPU transfers are directly done by memcpy immediately
    MemoryItem src = items.data[i].src;
    MemoryItem dst = items.data[i].dst;
    
    if((src.type == MEMORY_ITEM_TYPE_NONE) ||
       (dst.type == MEMORY_ITEM_TYPE_NONE))
      continue;
    bool same = src.type == dst.type;

    if(same && src.type == MEMORY_ITEM_TYPE_CPU_BUFFER){
      //Only memcpy
      memcpy(dst.cpu_buffer.data, src.cpu_buffer.data,
	     _min(dst.cpu_buffer.count, src.cpu_buffer.count));
      continue;
    }

    if(same && (src.type == MEMORY_ITEM_TYPE_GPU_BUFFER) &&
       (src.gpu_buffer.vk_obj == dst.gpu_buffer.vk_obj))
      continue;
    
    if(same && (src.type == MEMORY_ITEM_TYPE_IMAGE) &&
       (src.image.obj.vk_obj == dst.image.obj.vk_obj))
      continue;


    //Here need to allocate gpu stage memory, might fail, but then set it to src/dst
    if((src.type == MEMORY_ITEM_TYPE_CPU_BUFFER) ||
       (dst.type == MEMORY_ITEM_TYPE_CPU_BUFFER)){

      size_t count = ((src.type == MEMORY_ITEM_TYPE_CPU_BUFFER)?
		      src.cpu_buffer.count: dst.cpu_buffer.count);
      
      OptBuffer stage = alloc_buffer(param.gpu_allocr, device, count,
				     (AllocBufferParams){
				       .props_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				       .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
				     });

      if(stage.code < 0){
	stage = free_buffer(param.gpu_allocr, stage, device);
	//Need to break out of this with error
	break;
      }
      else{
	//Try to push
	if(!push_MemoryItem_darray(stage_buffers, (MemoryItem){
	      .type = MEMORY_ITEM_TYPE_GPU_BUFFER,
	      .gpu_buffer = stage.value
	    })){
	  stage = free_buffer(param.gpu_allocr, stage, device);
	  break;
	}
	else{
	  if(dst.type == MEMORY_ITEM_TYPE_CPU_BUFFER){
	    //Push the dst cpu buffer too
	    if(!push_MemoryItem_darray(stage_buffers, dst)){
	      break;
	    }
	    dst.type = MEMORY_ITEM_TYPE_GPU_BUFFER;
	    dst.gpu_buffer = stage.value;
	  }
	  else{
	    void* mapping = create_mapping_of_buffer(device, *param.gpu_allocr, stage.value);
	    assert(mapping != nullptr);
	    memcpy(mapping, src.cpu_buffer.data, src.cpu_buffer.count);
	    flush_mapping_of_buffer(device, *param.gpu_allocr, stage.value);
	    
	    src.type = MEMORY_ITEM_TYPE_GPU_BUFFER;
	    src.gpu_buffer = stage.value;
	  }
	}
      }
    }

    //Now the cpu buffers have been 'converted' into gpu staging buffers

    //It must be image if not gpu buffer
    if(src.type == MEMORY_ITEM_TYPE_GPU_BUFFER){
      if(dst.type == MEMORY_ITEM_TYPE_GPU_BUFFER){
	vkCmdCopyBuffer(param.cmd_buf, src.gpu_buffer.vk_obj,
			dst.gpu_buffer.vk_obj, 1,
			&(VkBufferCopy){
			  .srcOffset = 0,
			  .dstOffset = 0,
			    .size = _min(src.gpu_buffer.size,
					 dst.gpu_buffer.size)
			});
      }
      else{
	assert(dst.type == MEMORY_ITEM_TYPE_IMAGE);
	vkCmdCopyBufferToImage(param.cmd_buf, src.gpu_buffer.vk_obj,
			       dst.image.obj.vk_obj,
			       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			       1, &(VkBufferImageCopy){
				 .imageExtent = {
				   .depth = 1,
				   .width = dst.image.obj.width,
				   .height = dst.image.obj.height,
				 },
				 .imageSubresource = {
				   .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				   .layerCount = 1,
				 },
			       });
      }
    }
    else{
      assert(src.type == MEMORY_ITEM_TYPE_IMAGE);
      if(dst.type == MEMORY_ITEM_TYPE_GPU_BUFFER){
	vkCmdCopyImageToBuffer(param.cmd_buf, src.image.obj.vk_obj,
			       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			       dst.gpu_buffer.vk_obj,
			       1, &(VkBufferImageCopy){
				 .imageExtent = {
				   .depth = 1,
				   .width = src.image.obj.width,
				   .height = src.image.obj.height,
				 },
				 .imageSubresource = {
				   .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				   .layerCount = 1,
				 },
			       });
      }
      else{
	assert(src.type == MEMORY_ITEM_TYPE_IMAGE);
	//Only single planar 2D color images can be copied here,
	//If multiplanar images {i think cubemaps?} are used, do it separately
	vkCmdCopyImage(param.cmd_buf, src.image.obj.vk_obj,
		       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		       dst.image.obj.vk_obj,
		       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		       1, &(VkImageCopy){
			 .extent = {
			   .depth = 1,
			   .width = _min(src.image.obj.width, dst.image.obj.width),
			   .height = _min(src.image.obj.height, dst.image.obj.height),
			 },
			 .srcSubresource = {
			   .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			   .layerCount = 1,
			 },
			 .dstSubresource = {
			   .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			   .layerCount = 1,
			 },
		       });
      }
    }
  }

  
  submit_barriers(param.cmd_buf, &img_barrs,
		  &buff_barrs, items,
		  true, false,
		  param.dst_stage_mask,
		  param.src_stage_mask,
		  param.src_queue_family,
		  param.dst_queue_family);
  
  //So now all src pipeline barriers have been installed, similarly do for destinations
  
  submit_barriers(param.cmd_buf, &img_barrs,
		  &buff_barrs, items,
		  false, false,
		  param.dst_stage_mask,
		  param.src_stage_mask,
		  param.src_queue_family,
		  param.dst_queue_family);
  
  resize_ImgBarr_darray(&img_barrs, 0);
  resize_BuffBarr_darray(&buff_barrs, 0);
  
  return i;
}

void finish_copying_items(MemoryItemDarray* stage_buffers, VkDevice device, GPUAllocr* gpu_allocr){

  if(stage_buffers == nullptr){
    return;
  }

  for_slice((*stage_buffers), i){

    MemoryItem pop_item = stage_buffers->data[i];
    
    //Check if next is a cpu buffer
    if(((i+1) < stage_buffers->count) &&
       (stage_buffers->data[i+1].type == MEMORY_ITEM_TYPE_CPU_BUFFER)){

      void* mapping = create_mapping_of_buffer(device, *gpu_allocr,
					       stage_buffers->data[i].gpu_buffer);
      assert(mapping != nullptr);
      memcpy(stage_buffers->data[i+1].cpu_buffer.data, mapping,
	     _min(stage_buffers->data[i+1].cpu_buffer.count,
		  stage_buffers->data[i].gpu_buffer.size));
      flush_mapping_of_buffer(device, *gpu_allocr,
			      stage_buffers->data[i].gpu_buffer);
      i++;
    }

    free_buffer(gpu_allocr, (OptBuffer){.value = pop_item.gpu_buffer}, device);
  }

  resize_MemoryItem_darray(stage_buffers, 0);
}

#include "device-mem-stuff.h"
#include <string.h>
//TODO :: Need to make a partitioning scheme for cases when the alignment requirement is
//        too high, so need to also partition the front bit
typedef struct OnePage OnePage;
struct OnePage {
  OnePage* next;
  //u8* base;
  VkDeviceMemory handle;
  size_t size;
  size_t free;
};
typedef struct DaAllocr DaAllocr;
struct DaAllocr {
  OnePage* pages;
  MemoryDesc* allocd;
  MemoryDesc* freed;
};

typedef struct MemoryDesc MemoryDesc;
struct MemoryDesc {
  //id is the valid vulkan handle to the buffer that was created out of chosen memory
  // or maybe later it will be a union of buffer and image handle, 
  void* id;
  //void* id;
  MemoryDesc* next;
  OnePage* page;
  size_t offset;
  size_t size;
};
//Just make the damn allocator out of linked lists now
//Firstly, with no ids

//This results in a MemoryDesc* to later bind to
//This fxn is wrapped by create buffer or create image fxn which creates and stores
//appropriate handle

//Needs maybe parameter as struct
static MemoryDesc* alloc_da_memory(DaAllocr* allocr, size_t size, size_t align, AllocInterface mem_allocr, u32 mem_inx, VkDevice device){
//void* alloc_da_allocr(void* da_allocr, size_t size, size_t align){
//DaAllocr* allocr = da_allocr;
  //Search for memory in freed,
  MemoryDesc** memptr = &allocr->freed;
  while(*memptr != nullptr){
    size_t aligned_off = _align_up((*memptr)->offset, align);
    size_t aligned_size = (*memptr)->size - (aligned_off - (*memptr)->offset);

    if(aligned_size < size){
      memptr = &(*memptr)->next;
      continue;
    }
    //Found a suitable candidate

    //If found , first see if no partition needed
    //Else , partition it, and readjust list
    //Insert into allocd

    if((aligned_size - size) > sizeof(void*)){
      //Partition it
      MemoryDesc* new_mem = alloc_mem(mem_allocr,
				      sizeof(MemoryDesc),
				      alignof(MemoryDesc));
      if(nullptr == new_mem){
	return nullptr;
      }
      //Insert new memory onth allocd
      *new_mem = **memptr;
      new_mem->size = size + aligned_off - new_mem->offset;
      //new_mem->id = new_mem->page->base + aligned_off;
      new_mem->next = allocr->allocd;
      allocr->allocd = new_mem;

      //Now adjust for old mem
      (*memptr)->offset = new_mem->offset + new_mem->size;
      (*memptr)->size -= new_mem->size;

      //Need to insert it into proper place, need to remove it and reinsert it
      if(allocr->freed != *memptr){
	MemoryDesc* old_mem = *memptr;
	*memptr = old_mem->next;
	MemoryDesc** new_place = &(allocr->freed);
	while(new_place != memptr){
	  if((*new_place)->size >= old_mem->size){
	    break;
	  }
	  new_place = &(*new_place)->next;
	}
	//Insert into new_place
	old_mem->next = (*new_place);
	*new_place = old_mem;
      }
      //Bookeeping
      new_mem->page->free -= new_mem->size;
      //visualize_em(allocr);
      return new_mem;
      //return new_mem->id;
    }
    else{
      //No partition
      MemoryDesc* old_mem = *memptr;
      *memptr = old_mem->next;
      //old_mem->id = old_mem->page->base + aligned_off;
      old_mem->next = allocr->allocd;
      allocr->allocd = old_mem;
      //Bookkeeping
      old_mem->page->free -= old_mem->size;
      //visualize_em(allocr);
      return old_mem;
      //return old_mem->id;
    }
  }

  //If no memory found, alloc another page
  //insert into freed, and return recursively or partition and return after adjust
  
  OnePage* new_page = alloc_mem(mem_allocr, sizeof(OnePage),
				alignof(OnePage));
  if(nullptr == new_page)
    return nullptr;

  size_t page_size = SIZE_KB(64);
  page_size = _align_up(_align_up(size, page_size), align);
  *new_page = (OnePage){.size = page_size, .free = page_size};


  //Allocate a new entry for 'freed'
  MemoryDesc* new_entry = alloc_mem(mem_allocr, sizeof(MemoryDesc),
				    alignof(MemoryDesc));

  if(nullptr == new_entry){
    free_mem(mem_allocr, new_page);
    return nullptr;
  }
  (*new_entry) = (MemoryDesc){
    .page = new_page,
    .offset = 0,
    .size = page_size
  };

  
  //Here do vulkan allocation
  VkMemoryAllocateInfo mem_alloc_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = page_size,
    .memoryTypeIndex = mem_inx,
  };
  VkResult res = vkAllocateMemory(device, &mem_alloc_info, get_glob_vk_alloc(), &(new_page->handle));

  //new_page->base = malloc(page_size);
  //if(nullptr == new_page->base){
  if(VK_SUCCESS != res){
    free_mem(mem_allocr, new_page);
    free_mem(mem_allocr, new_entry);
    return nullptr;
  }

  new_page->next = allocr->pages;
  allocr->pages = new_page;

  //Now need to sift new_entry till a bigger is found
  memptr = &allocr->freed;
  while(*memptr != nullptr){
    if((*memptr)->size >= new_entry->size){
      break;
    }
    memptr = &(*memptr)->next;
  }
  new_entry->next = (*memptr);
  *memptr = new_entry;

  return alloc_da_memory(allocr, size, align, mem_allocr, mem_inx, device);
}
//MemoryDesc* alloc_da_memory(DaAllocr* allocr, size_t size, size_t align, AllocInterface mem_allocr, u32 mem_inx, VkDevice device){

//Currently assumes all handles are unique
//Later may be modified to free memories also
bool free_da_memory(DaAllocr* allocr, AllocInterface mem_allocr, void* handle){
  //DaAllocr* allocr = da_allocr;
  //Search for memory in allocd
  MemoryDesc** memptr = &(allocr->allocd);
  while(nullptr != *memptr){
    if((*memptr)->id == handle){
      //goto found_it;
      break;
    }
    memptr = &(*memptr)->next;
  }

  if(nullptr == (*memptr))
    return false;

  //found_it:

  MemoryDesc* found_mem = *memptr;
  *memptr = found_mem->next;

  //Bookkeeping
  found_mem->page->free += found_mem->size;

  //Try to merge now, only 1 merge per freeing, dont care anything else for now
  MemoryDesc** try_merge = &(allocr->freed);
  while(nullptr != *try_merge){
    //Should be from same page of memory
    if((*try_merge)->page != found_mem->page)
      goto continue_on;

    if(((*try_merge)->offset + (*try_merge)->size) == found_mem->offset){
      found_mem->offset = (*try_merge)->offset;
    }
    else if((*try_merge)->offset == (found_mem->offset + found_mem->size)){
    }
    else{
      goto continue_on;
    }
    found_mem->size += (*try_merge)->size;

    //Free try_merge
    MemoryDesc* tmp = *try_merge;
    *try_merge = tmp->next;
    free_mem(mem_allocr, tmp);

    //Reinsert the found_mem, which has to be done anyway
    try_merge = &(allocr->freed); //This should cause full merge
    continue;
    //break;
  continue_on:
    try_merge = &(*try_merge)->next;
    continue;
  }

  //Insert found mem now
  MemoryDesc** inserting = &(allocr->freed);
  while((*inserting) != nullptr){
    if((*inserting)->size >= found_mem->size){
      break;
    }
    inserting = &(*inserting)->next;
  }

  found_mem->next = (*inserting);
  *inserting = found_mem;
  //visualize_em(allocr);
  return true;
}
//Now a wrapper that takes memory property and type bits to determine
//  closest DaAllocr to use to allocate memory, and returns a MemoryDesc* pointer
//DEF_SLICE(u32);
struct MemoryInxPair{
  MemoryDesc* mem;
  u32 inx;
};
DEF_SLICE(DaAllocr);
struct MemoryInxPair find_alloc_mem(GPUAllocr* gpu_allocr,
			   u32 memory_type_flags,
			   u32 mem_props,
			   size_t size, size_t align,
			   VkDevice device){

  if(nullptr == gpu_allocr->gpu_allocrs)
    return (struct MemoryInxPair){0};

  for_slice(init_DaAllocr_slice(gpu_allocr->gpu_allocrs,
			   gpu_allocr->props.memoryTypeCount), inx){

    const bool is_type = ((1<<inx) & memory_type_flags) != 0;
    const bool is_props = mem_props == (mem_props &
				 gpu_allocr->props.memoryTypes[inx].propertyFlags);
    if(is_type && is_props){
      MemoryDesc* allocd =
	alloc_da_memory(gpu_allocr->gpu_allocrs + inx,
			size, align, gpu_allocr->cpu_allocr,
			inx, device);
      if(allocd != nullptr){
	return (struct MemoryInxPair){.mem = allocd, .inx = inx};
      }
    }
  }
  return (struct MemoryInxPair){0};
}
//Then another wrapper over that, one image and another buffer version
//  which actually queries memory property and calls above fxns, then also creates those
//  vk objects and returns them after registering

OptBuffer alloc_buffer(GPUAllocr* gpu_allocr, VkDevice device, size_t size, AllocBufferParams param){
  OptBuffer buffer = {0};
  VkResult result = VK_SUCCESS;
  result = vkCreateBuffer(device,
			  &(VkBufferCreateInfo){
			    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			    .usage = param.usage,
			    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			    .size = size
			  },
			  get_glob_vk_alloc(),
			  &buffer.value.vk_obj);
  if(result != VK_SUCCESS){
    buffer.code = ALLOC_BUFFER_BUFFER_CREATE_FAIL;
    return buffer;
  }
  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(device, buffer.value.vk_obj, &mem_reqs);
  struct MemoryInxPair allocd = find_alloc_mem(gpu_allocr,
					       mem_reqs.memoryTypeBits,
					       param.props_flags,
					       (size_t)mem_reqs.size,
					       (size_t)mem_reqs.alignment,
					       device);
  if(nullptr == allocd.mem){
    buffer.code = ALLOC_BUFFER_OUT_OF_MEMORY;
    return buffer;
  }
  //Calculate the actual offset again using alignment
  size_t aligned_off = _align_up(allocd.mem->offset, mem_reqs.alignment);
  
  buffer.value.backing = allocd.mem;
  buffer.value.size = mem_reqs.size;
  buffer.value.mem_inx = allocd.inx;
  allocd.mem->id = buffer.value.vk_obj;

  //Now bind memory
  result = vkBindBufferMemory(device, buffer.value.vk_obj,
			      allocd.mem->page->handle,
			      (VkDeviceSize)aligned_off);
  if(result != VK_SUCCESS){
    buffer.code = ALLOC_BUFFER_MEMORY_BIND_FAIL;
    return buffer;
  }
  return buffer;
}


OptImage alloc_image(GPUAllocr* gpu_allocr, VkDevice device, size_t width, size_t height, AllocImageParams params){
  OptImage image = {0};
  VkResult result = VK_SUCCESS;

  VkImageCreateInfo img_create = {
    .sType =  VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    //.flags = sparse binding, cube compatible, etc
    .imageType = VK_IMAGE_TYPE_2D,
    .format = params.image_format, //This might have to be taken as parameter
    //This has to be taken as parameter, a vkextent3d with width, height, depth fields
    .extent = {.depth = 1, .width = width, .height = height},
    //These three have to be 1 if tiling is linear, maybe some checks and take parameters
    .mipLevels = 1,
    .arrayLayers = 1,
    //Maybe dont think about this till multisampling is used
    .samples = VK_SAMPLE_COUNT_1_BIT,
    //This will have to be taken as parameter/decide by usage and initial layout automatically
    //linear tiling also may have some padding, also linear has a lot of restrictions
    .tiling = (params.optimal_layout?VK_IMAGE_TILING_OPTIMAL:VK_IMAGE_TILING_LINEAR), 
       //Possible values for usage : transfer cannot be used alone
    /* VK_IMAGE_USAGE_TRANSFER_SRC_BIT = 0x00000001, */
    /* VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002, */
    /* VK_IMAGE_USAGE_SAMPLED_BIT = 0x00000004, */
    /* VK_IMAGE_USAGE_STORAGE_BIT = 0x00000008, */
    /* VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 0x00000010, */
    /* VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 0x00000020, */
    /* VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT = 0x00000040, */
    /* VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT = 0x00000080, */
    .usage = params.usage,
    //A given format has a set of valid usages,
    //Checked by vkGetPhysicalDeviceFormatProperties
    //Look at page 1025 again, something about format feature flags is to be accounted for
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    //This has to be taken in, but as a bool to set to preinitialized or undefined
    //preinitialized is 'useful' only with linear tiling
    .initialLayout = (params.pre_initialized?VK_IMAGE_LAYOUT_PREINITIALIZED:VK_IMAGE_LAYOUT_UNDEFINED),
  };
  //TODO :: later make, for both buffer and image a proper format selection
  //mechanism out of usages by properly quering the hardware properties
  //  with custom generic enum values
  //Something about vk format properties
  /*
    Has 3 format feature flags members, for linear optimal and buffer
    
    
    
   */
  
  result = vkCreateImage(device, &img_create, get_glob_vk_alloc(), &image.value.vk_obj);

  if(result != VK_SUCCESS){
    image.code = ALLOC_IMAGE_IMAGE_CREATE_FAIL;
    return image;
  }
  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(device, image.value.vk_obj, &mem_reqs);
  struct MemoryInxPair allocd = find_alloc_mem(gpu_allocr,
					       mem_reqs.memoryTypeBits,
					       params.props_flags,
					       (size_t)mem_reqs.size,
					       (size_t)mem_reqs.alignment,
					       device);
  if(nullptr == allocd.mem){
    image.code = ALLOC_IMAGE_OUT_OF_MEMORY;
    return image;
  }
  size_t aligned_off = _align_up(allocd.mem->offset, mem_reqs.alignment);
  image.value.backing = allocd.mem;
  //image.value.size = mem_reqs.size;
  image.value.width = width;
  image.value.height = height;
  image.value.mem_inx = allocd.inx;
  allocd.mem->id = image.value.vk_obj;

  //Now bind memory
  result = vkBindImageMemory(device, image.value.vk_obj,
			      allocd.mem->page->handle,
			      aligned_off);
  if(result != VK_SUCCESS){
    image.code = ALLOC_IMAGE_MEMORY_BIND_FAIL;
    return image;
  }
  return image;
}



//For free counterpart, given a memory type flags only,
//  tries to find similarly suitable DaAllocr
//  and calls the fn above
/* bool find_free_mem(GPUAllocr* gpu_allocr, */
/* 		    u32 memory_type_flags, */
/* 		    void* handle){ */
/*   for_slice(init_u32_slice(gpu_allocr->allocrs, */
/* 			   gpu_allocr->props.memoryTypeCount), inx){ */
/*     const bool is_type = ((1<<inx) & memory_type_flags) != 0; */
/*     if(is_type){ */
/*       bool res = free_da_memory(gpu_allocr->allocrs + inx, handle); */
/*       if(res) */
/* 	return true; */
/*     } */
/*   } */
/*   return false; */
/* } */
//This is wrapped by buffer/image creation fxns,
OptBuffer free_buffer(GPUAllocr* gpu_allocr, OptBuffer buffer, VkDevice device){
  switch(buffer.code){
  case ALLOC_BUFFER_OK:
  case ALLOC_BUFFER_MEMORY_BIND_FAIL:{
    /* VkMemoryRequirements mem_reqs; */
    /* vkGetBufferMemoryRequirements(device, buffer.value.vk_obj, &mem_reqs); */
    free_da_memory(gpu_allocr->gpu_allocrs + buffer.value.mem_inx,
		   gpu_allocr->cpu_allocr,
		   (void*)buffer.value.vk_obj);
    /* find_free_mem(gpu_allocr, */
    /* 		  mem_reqs.memoryTypeBits, */
    /* 		  (void*)buffer.value); */
        //call to free memory here
  }
  case ALLOC_BUFFER_OUT_OF_MEMORY:{
    vkDestroyBuffer(device, buffer.value.vk_obj, get_glob_vk_alloc());
    //destroy buffer here
  }
  case ALLOC_BUFFER_BUFFER_CREATE_FAIL:
  case ALLOC_BUFFER_TOP_ERROR:
    buffer = (OptBuffer){0};
  }
  return buffer;
}

OptImage free_image(GPUAllocr* gpu_allocr, OptImage image, VkDevice device){
  switch(image.code){
  case ALLOC_IMAGE_OK:
  case ALLOC_IMAGE_MEMORY_BIND_FAIL:{
    /* VkMemoryRequirements mem_reqs; */
    /* vkGetImageMemoryRequirements(device, image.value.vk_obj, &mem_reqs); */
    free_da_memory(gpu_allocr->gpu_allocrs + image.value.mem_inx,
		   gpu_allocr->cpu_allocr,
		   (void*)image.value.vk_obj);
    /* find_free_mem(gpu_allocr, */
    /* 		  mem_reqs.memoryTypeBits, */
    /* 		  (void*)image.value); */
        //call to free memory here
  }
  case ALLOC_IMAGE_OUT_OF_MEMORY:{
    vkDestroyImage(device, image.value.vk_obj, get_glob_vk_alloc());
    //destroy image here
  }
  case ALLOC_IMAGE_IMAGE_CREATE_FAIL:
  case ALLOC_IMAGE_TOP_ERROR:
    image = (OptImage){0};
  }
  return image;
}


//Also we will need a init and free for the whole thing
GPUAllocr init_allocr(VkPhysicalDevice phy_dev, AllocInterface allocr){
  GPUAllocr gpu_allocr = {0};
  gpu_allocr.cpu_allocr = allocr;
  vkGetPhysicalDeviceMemoryProperties(phy_dev, &(gpu_allocr.props));
  
  VkPhysicalDeviceProperties device_props;
  vkGetPhysicalDeviceProperties(phy_dev, &device_props);

  gpu_allocr.nc_atom_size = device_props.limits.nonCoherentAtomSize;

  //Now allocate array
  gpu_allocr.gpu_allocrs = alloc_mem(allocr,
				     sizeof(DaAllocr) * gpu_allocr.props.memoryTypeCount,
				     alignof(DaAllocr));
  if(nullptr != gpu_allocr.gpu_allocrs){
    memset(gpu_allocr.gpu_allocrs, 0,
	   sizeof(DaAllocr) * gpu_allocr.props.memoryTypeCount);
  }
  return gpu_allocr;
  
}

//Won't destroy buffers or images for now
GPUAllocr deinit_allocr(GPUAllocr allocr, VkDevice device){
  for_range(size_t, i, 0, allocr.props.memoryTypeCount){
    
    //Free all pages' device memory
    //Free all linked list nodes
    OnePage* head = allocr.gpu_allocrs[i].pages;
    while(head != nullptr){
      vkFreeMemory(device, head->handle, get_glob_vk_alloc());
      OnePage* next =head->next;
      free_mem(allocr.cpu_allocr, head);
      head = next;
    }

    MemoryDesc* allocd = allocr.gpu_allocrs[i].allocd;
    while(allocd != nullptr){
      MemoryDesc* next = allocd->next;
      free_mem(allocr.cpu_allocr, allocd);
      allocd = next;
    }

    MemoryDesc* freed = allocr.gpu_allocrs[i].freed;
    while(freed != nullptr){
      MemoryDesc* next = freed->next;
      free_mem(allocr.cpu_allocr, freed);
      freed = next;
    }
  }
  allocr = (GPUAllocr){0};
  return allocr;
}

//TODO :: There is a granularity requirement ?


void* create_mapping_of_buffer(VkDevice device, GPUAllocr allocr, BufferObj buffer){
  //Check memory properties for host visible
  //Then map

  VkMemoryRequirements mem_reqs; 
  vkGetBufferMemoryRequirements(device, buffer.vk_obj, &mem_reqs);
  if((allocr.props.memoryTypes[buffer.mem_inx].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){

    void* mapping = nullptr;
    if(VK_SUCCESS == vkMapMemory(device, buffer.backing->page->handle,
				 (VkDeviceSize)0, VK_WHOLE_SIZE, 0, &mapping)){
      size_t aligned_off = _align_up(buffer.backing->offset, mem_reqs.alignment);
      return (u8*)mapping + aligned_off;
    }
  }
  return nullptr;

  //If not host cohorent, start of range should be round down to atom size multiple
  //And also the end

  //Then do invalidate ranges if not host cohorent

  //size = alloc size
  //offset = ??
}

void flush_mapping_of_buffer(VkDevice device, GPUAllocr allocr, BufferObj buffer){
  if((allocr.props.memoryTypes[buffer.mem_inx].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){

    size_t off = buffer.backing->offset;
    size_t end = buffer.backing->offset + buffer.size;
    off = _align_down(off, allocr.nc_atom_size);
    end = _align_up(end, allocr.nc_atom_size);
        
    vkFlushMappedMemoryRanges(device, 1,
			      &(VkMappedMemoryRange){
				.memory = buffer.backing->page->handle,
				.offset = off,
				.size = (end - off),
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE });

    vkUnmapMemory(device, buffer.backing->page->handle);
  }
    
}


void* create_mapping_of_image(VkDevice device, GPUAllocr allocr, ImageObj image){
  //Check memory properties for host visible
  //Then map

  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(device, image.vk_obj, &mem_reqs);
  if((allocr.props.memoryTypes[image.mem_inx].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){

    void* mapping = nullptr;
    if(VK_SUCCESS == vkMapMemory(device, image.backing->page->handle,
				 (VkDeviceSize)0, VK_WHOLE_SIZE, 0, &mapping)){
      
      size_t aligned_off = _align_up(image.backing->offset, mem_reqs.alignment);
      return (u8*)mapping + image.backing->offset;
    }
  }
  return nullptr;

  //If not host cohorent, start of range should be round down to atom size multiple
  //And also the end

  //Then do invalidate ranges if not host cohorent

  //size = alloc size
  //offset = ??
}

void flush_mapping_of_image(VkDevice device, GPUAllocr allocr, ImageObj image){
  if((allocr.props.memoryTypes[image.mem_inx].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){

    size_t off = image.backing->offset;
    size_t end = image.backing->offset + image.backing->size;
    off = _align_down(off, allocr.nc_atom_size);
    end = _align_up(end, allocr.nc_atom_size);
        
    vkFlushMappedMemoryRanges(device, 1,
			      &(VkMappedMemoryRange){
				.memory = image.backing->page->handle,
				.offset = off,
				.size = (end - off),
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE });

    vkUnmapMemory(device, image.backing->page->handle);
  }
    
}


//New version ends here

/* int create_and_allocate_buffer( */
/*     const VkAllocationCallbacks* alloc_callbacks, */
/*     GPUAllocator* p_allocr, VkDevice device, */
/*     CreateAndAllocateBufferParam param) { */

/*     VkBufferCreateInfo create_info = { */
/*         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, */
/*         .usage = param.usage, */
/*         .sharingMode = param.share_mode, */
/*         .size = param.size, */
/*     }; */

/*     if (vkCreateBuffer(device, &create_info, alloc_callbacks, */
/*                        &param.p_buffer->buffer) != VK_SUCCESS) */
/*         return CREATE_AND_ALLOCATE_BUFFER_CREATE_BUFFER_FAILED; */

/*     // VkDeviceBufferMemoryRequirements info_struct = { */
/*     //     .sType = */
/*     //     VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS, */
/*     //     .pCreateInfo = &create_info, */
/*     // }; */
/*     // VkMemoryRequirements2 mem_reqs = { */
/*     //     .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, */
/*     // }; */
/*     // vkGetDeviceBufferMemoryRequirements(device, &info_struct, */
/*     //                                     &mem_reqs); */

/*     VkMemoryRequirements mem_reqs; */
/*     vkGetBufferMemoryRequirements(device, param.p_buffer->buffer, */
/*                                   &mem_reqs); */

/*     param.p_buffer->base_align = mem_reqs.alignment; */
/*     param.p_buffer->total_amt = mem_reqs.size; */


/*     // if ((mem_reqs.memoryRequirements.memoryTypeBits & */
/*     if ((mem_reqs.memoryTypeBits & p_allocr->memory_type) == 0) */
/*         return CREATE_AND_ALLOCATE_BUFFER_INCOMPATIBLE_MEMORY; */

/*     if (gpu_allocr_allocate_buffer(p_allocr, device, param.p_buffer) < */
/*         0) */
/*         return CREATE_AND_ALLOCATE_BUFFER_BUFFER_ALLOC_FAILED; */

/*     return CREATE_AND_ALLOCATE_BUFFER_OK; */
/* } */

/* int allocate_device_memory( */
/*     const VkAllocationCallbacks* alloc_callbacks, */
/*     AllocateDeviceMemoryParam param) { */

/*     VkPhysicalDeviceMemoryProperties mem_props = { 0 }; */

/*     vkGetPhysicalDeviceMemoryProperties(param.phy_device, &mem_props); */

/*     int32_t mem_inx = -1; */
/*     for (int i = 0; i < mem_props.memoryTypeCount; ++i) { */
/*         if (((1 << i) & param.memory_type_flag_bits) && */
/*             (param.memory_properties == */
/*                 (param.memory_properties & */
/*                     mem_props.memoryTypes[i].propertyFlags))) { */
/*             mem_inx = i; */
/*             break; */
/*         } */
/*     } */
/*     if (mem_inx == -1) */
/*         return ALLOCATE_DEVICE_MEMORY_NOT_AVAILABLE_TYPE; */

/*     VkPhysicalDeviceProperties device_props; */
/*     vkGetPhysicalDeviceProperties(param.phy_device, &device_props); */

/*     param.p_gpu_allocr->atom_size = */
/*         device_props.limits.nonCoherentAtomSize; */
/*     param.p_gpu_allocr->memory_type = mem_inx; */


/*     VkMemoryAllocateInfo alloc_info = { */
/*         .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, */
/*         .memoryTypeIndex = mem_inx, */
/*         .allocationSize = param.allocation_size, */
/*     }; */

/*     if (vkAllocateMemory(param.device, &alloc_info, alloc_callbacks, */
/*                          &(param.p_gpu_allocr->memory_handle)) != */
/*         VK_SUCCESS) */
/*         return ALLOCATE_DEVICE_MEMORY_FAILED; */

/*     param.p_gpu_allocr->mem_size = param.allocation_size; */
/*     param.p_gpu_allocr->curr_offset = 0; */

/*     if (param.memory_properties & */
/*         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) { */
/*         if (vkMapMemory( */
/*             param.device, param.p_gpu_allocr->memory_handle, */
/*             param.p_gpu_allocr->curr_offset, */
/*             param.p_gpu_allocr->mem_size, 0, */
/*             &(param.p_gpu_allocr->mapped_memory)) != VK_SUCCESS) */

/*             param.p_gpu_allocr->mapped_memory = NULL; */
/*     } else { */
/*         param.p_gpu_allocr->mapped_memory = NULL; */
/*     } */

/*     return ALLOCATE_DEVICE_MEMORY_OK; */
/* } */

/* void free_device_memory(const VkAllocationCallbacks* alloc_callbacks, */
/*                         FreeDeviceMemoryParam param, int err_codes) { */

/*     switch (err_codes) { */
/*     case ALLOCATE_DEVICE_MEMORY_OK: */

/*         if (param.p_gpu_allocr->mapped_memory) { */
/*             vkUnmapMemory(param.device, */
/*                           param.p_gpu_allocr->memory_handle); */
/*         } */
/*         vkFreeMemory(param.device, param.p_gpu_allocr->memory_handle, */
/*                      alloc_callbacks); */

/*     case ALLOCATE_DEVICE_MEMORY_FAILED: */
/*     case ALLOCATE_DEVICE_MEMORY_NOT_AVAILABLE_TYPE: */
/*         param.p_gpu_allocr->memory_handle = VK_NULL_HANDLE; */
/*         param.p_gpu_allocr->mapped_memory = NULL; */
/*         param.p_gpu_allocr->mem_size = 0; */
/*         param.p_gpu_allocr->curr_offset = 0; */
/*     } */
/* } */

/* int gpu_allocr_allocate_buffer(GPUAllocator* gpu_allocr, */
/*                                VkDevice device, */
/*                                GpuAllocrAllocatedBuffer* */
/*                                buffer_info) { */

/*     buffer_info->base_align = max(1, buffer_info->base_align); */
/*     size_t next_offset = */
/*         align_up_(gpu_allocr->curr_offset, buffer_info->base_align) + */
/*         buffer_info->total_amt; */
/*     if (next_offset > gpu_allocr->mem_size) */
/*         return GPU_ALLOCR_ALLOCATE_BUFFER_NOT_ENOUGH_MEMORY; */

/*     if (vkBindBufferMemory( */
/*         device, buffer_info->buffer, gpu_allocr->memory_handle, */
/*         next_offset - buffer_info->total_amt) != VK_SUCCESS) { */
/*         return GPU_ALLOCR_ALLOCATE_BUFFER_FAILED; */
/*     } */
/*     if (gpu_allocr->mapped_memory) */
/*         buffer_info->mapped_memory = */
/*             (uint8_t*)(gpu_allocr->mapped_memory) + next_offset - */
/*             buffer_info->total_amt; */
/*     { */
/*     gpu_allocr->curr_offset = next_offset; */
/*     return GPU_ALLOCR_ALLOCTE_BUFFER_OK; */
/* } */

/* // Only for host visible and properly mapped memories */
/* VkResult gpu_allocr_flush_memory(VkDevice device, GPUAllocator allocr, */
/*                                  void* mapped_memory, size_t amount) { */

/*     VkDeviceSize offset = */
/*         (uint8_t*)mapped_memory - (uint8_t*)allocr.mapped_memory; */

/*     VkDeviceSize final = offset + amount; */
/*     offset = align_down_(offset, allocr.atom_size); */
/*     final = offset + align_up_(final - offset, allocr.atom_size); */
/*     final = min(final, allocr.mem_size); */


/*     return vkFlushMappedMemoryRanges( */
/*         device, 1, */
/*         &(VkMappedMemoryRange){ */
/*             .memory = allocr.memory_handle, */
/*             .offset = offset, */
/*             .size = final - offset, */
/*             .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE }); */
/* } */

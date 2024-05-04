#pragma once

#include "common-stuff.h"

typedef struct OnePage OnePage;
typedef struct MemoryDesc MemoryDesc;
typedef struct DaAllocr DaAllocr;
typedef struct GPUAllocr GPUAllocr;
struct GPUAllocr {
  //TODO:: Later make this dynamically allocated, so DaAllocr can be moved inside the .c only
  //DaAllocr allocrs[VK_MAX_MEMORY_TYPES];
  DaAllocr* gpu_allocrs;
  AllocInterface cpu_allocr;
  VkPhysicalDeviceMemoryProperties props;
  u32 nc_atom_size;
};

GPUAllocr init_allocr(VkPhysicalDevice phy_dev, AllocInterface allocr);
GPUAllocr deinit_allocr(GPUAllocr allocr, VkDevice device);

typedef struct BufferObj BufferObj;
struct BufferObj {
  VkBuffer vk_obj;
  size_t size;
  MemoryDesc* backing;
  u32 mem_inx;
};

//For now, everything will be exclusive sharing mode
typedef struct {
  //VkSharingMode share_mode;
  VkBufferUsageFlags usage;
  u32 props_flags;
} AllocBufferParams;

typedef struct{
  BufferObj value;
  enum {
    ALLOC_BUFFER_TOP_ERROR = -0x7ff,
    ALLOC_BUFFER_MEMORY_BIND_FAIL,
    ALLOC_BUFFER_OUT_OF_MEMORY,
    ALLOC_BUFFER_BUFFER_CREATE_FAIL,
    ALLOC_BUFFER_OK = 0,
  } code;
} OptBuffer;
OptBuffer alloc_buffer(GPUAllocr* gpu_allocr, VkDevice device, size_t size, AllocBufferParams params);
OptBuffer free_buffer(GPUAllocr* gpu_allocr, OptBuffer buffer, VkDevice device);

typedef struct ImageObj ImageObj;
struct ImageObj {
  VkImage vk_obj;
  size_t width;
  size_t height;
  MemoryDesc* backing;
  u32 mem_inx;
};

//For now, everything will be exclusive sharing mode
typedef struct {
  //VkSharingMode share_mode;
  VkImageUsageFlags usage;
  VkFormat image_format;
  bool pre_initialized;
  bool optimal_layout;
  u32 props_flags;
} AllocImageParams;

typedef struct{
  ImageObj value;
  enum {
    ALLOC_IMAGE_TOP_ERROR = -0x7ff,
    ALLOC_IMAGE_MEMORY_BIND_FAIL,
    ALLOC_IMAGE_OUT_OF_MEMORY,
    ALLOC_IMAGE_IMAGE_CREATE_FAIL,
    ALLOC_IMAGE_OK = 0,
  } code;
} OptImage;
//Only 2d images for now
OptImage alloc_image(GPUAllocr* gpu_allocr, VkDevice device, size_t width, size_t height, AllocImageParams params);
OptImage free_image(GPUAllocr* gpu_allocr, OptImage buffer, VkDevice device);

//TODO :: now needed to map memory at least, later make it so that it can map or use staging buffer to copy automatically

//Make a map and flush+unmap pair of funxtions for buffer only now
//For now it will map only if host visible, later will automatically utilize staging buffers

void* create_mapping_of_buffer(VkDevice device, GPUAllocr allocr, BufferObj buffer);
void flush_mapping_of_buffer(VkDevice, GPUAllocr allocr, BufferObj buffer);

void* create_mapping_of_image(VkDevice device, GPUAllocr allocr, ImageObj image);
void flush_mapping_of_image(VkDevice, GPUAllocr allocr, ImageObj image);

//Old gpu allocator stuff
/* struct GPUAllocator { */
/*     VkDeviceMemory memory_handle; */

/*     // Will be mapped iff host visible */
/*     void *mapped_memory; */
/*     VkDeviceSize mem_size; */
/*     VkDeviceSize curr_offset; */
/*     size_t atom_size; */
/*     uint32_t memory_type; */
/* }; */

/* typedef struct GPUAllocator GPUAllocator; */

/* enum AllocateDeviceMemoryCodes { */
/*     ALLOCATE_DEVICE_MEMORY_FAILED = -0x7fff, */
/*     ALLOCATE_DEVICE_MEMORY_NOT_AVAILABLE_TYPE, */
/*     ALLOCATE_DEVICE_MEMORY_OK = 0, */
/* }; */

/* typedef struct { */
/*     VkPhysicalDevice phy_device; */
/*     VkDevice device; */
/*     // like host visible, local */
/*     uint32_t memory_properties; */
/*     uint32_t memory_type_flag_bits; */
/*     size_t allocation_size; */

/*     GPUAllocator *p_gpu_allocr; */
/* } AllocateDeviceMemoryParam; */

/* int allocate_device_memory(const VkAllocationCallbacks *alloc_callbacks, */
/*                            AllocateDeviceMemoryParam param); */

/* typedef struct { */
/*     VkDevice device; */

/*     GPUAllocator *p_gpu_allocr; */
/* } FreeDeviceMemoryParam; */

/* void free_device_memory(const VkAllocationCallbacks *alloc_callbacks, */
/*                         FreeDeviceMemoryParam param, int err_codes); */

/* enum GpuAllocrAllocateBufferCodes { */
/*     GPU_ALLOCR_ALLOCATE_BUFFER_FAILED = -0x7fff, */
/*     GPU_ALLOCR_ALLOCATE_BUFFER_NOT_ENOUGH_MEMORY, */
/*     GPU_ALLOCR_ALLOCTE_BUFFER_OK = 0 */
/* }; */

/* typedef struct { */
/*     VkBuffer buffer; */
/*     size_t total_amt; */
/*     size_t base_align; */
/*     void *mapped_memory; */
/* } GpuAllocrAllocatedBuffer; */

/* int gpu_allocr_allocate_buffer( */
/*   GPUAllocator *gpu_allocr, VkDevice device, */
/*   GpuAllocrAllocatedBuffer *buffer_info); */

/* // Only for host visible and properly mapped memories */
/* VkResult gpu_allocr_flush_memory(VkDevice device, GPUAllocator allocr, */
/*                                  void *mapped_memory, size_t amount); */

/* //Assumes a compatible memory is allocated */
/* enum CreateAndAllocateBufferCodes { */
/*     CREATE_AND_ALLOCATE_BUFFER_DUMMY_ERR_CODE = -0x7fff, */
/*     CREATE_AND_ALLOCATE_BUFFER_BUFFER_ALLOC_FAILED, */
/*     CREATE_AND_ALLOCATE_BUFFER_INCOMPATIBLE_MEMORY, */
/*     CREATE_AND_ALLOCATE_BUFFER_CREATE_BUFFER_FAILED, */


/*     CREATE_AND_ALLOCATE_BUFFER_OK = 0, */
/* }; */


/* typedef struct { */
/*     GpuAllocrAllocatedBuffer *p_buffer; */
/*     VkSharingMode share_mode; */
/*     VkBufferUsageFlags usage; */
/*     size_t size; */

/* }CreateAndAllocateBufferParam; */
/* int create_and_allocate_buffer(const VkAllocationCallbacks * alloc_callbacks, */
/*                                GPUAllocator *p_allocr, */
/*                                VkDevice device, */
/*   CreateAndAllocateBufferParam param); */

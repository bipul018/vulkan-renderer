#pragma once

#include "common-stuff.h"
#include "device-mem-stuff.h"
#include "util_structures.h"
#include "vulkan/vulkan_core.h"

DEF_SLICE(u8);

//Simple helper functions to create descriptor set layouts more easily
typedef VkDescriptorSetLayoutBinding DescBinding;
DEF_SLICE(DescBinding);

VkDescriptorSetLayout create_descriptor_set_layout_(VkDevice device,
						   VkDescriptorSetLayoutCreateFlags flags,
						   DescBindingSlice bindings);

#define create_descriptor_set_layout(device, flags, ...)	\
  create_descriptor_set_layout_(device, flags, MAKE_ARRAY_SLICE(DescBinding, __VA_ARGS__))

//Descriptor helpers
typedef VkDescriptorPoolSize DescSize;
DEF_SLICE(DescSize);
typedef struct DescSet DescSet;
struct DescSet {
  DescSizeSlice descs;
  size_t count;
};
DEF_SLICE(DescSet);
//Need to provide inline descriptor entries one per binding in a set
VkDescriptorPool make_pool_(AllocInterface allocr, VkDevice device, DescSetSlice sets);
#define make_pool(allocr, device, ...) \
  make_pool_(allocr, device, MAKE_ARRAY_SLICE(DescSet, __VA_ARGS__))
VkDescriptorSet alloc_set(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout set_layout);
void write_uniform_descriptor(VkDevice device, VkDescriptorSet set, BufferObj buffer, int binding);
void write_storage_descriptor(VkDevice device, VkDescriptorSet set, BufferObj buffer, int binding);
void write_sampler(VkDevice device, VkDescriptorSet set, VkSampler sampler, int binding);
//Expects image layout to be shader read only optimal
void write_image(VkDevice device, VkDescriptorSet set, VkImageView image, int binding);
void write_inline_descriptor(VkDevice device, VkDescriptorSet set, u8Slice data, int binding);
void write_storage_image(VkDevice device, VkDescriptorSet set, VkImageView img, int binding);


//Vertex input helpers
typedef VkVertexInputBindingDescription VertexBinding;
typedef VkVertexInputAttributeDescription VertexAttribute;

DEF_SLICE(VertexBinding);
DEF_SLICE(VertexAttribute);
DEF_DARRAY(VertexBinding,1);
DEF_DARRAY(VertexAttribute,1);

typedef struct VertexInputDesc VertexInputDesc;
struct VertexInputDesc {
  VertexBindingDarray bindings;
  VertexAttributeDarray attrs;
  u32 current_binding;
};

VertexInputDesc init_vertex_input(AllocInterface allocr);
void clear_vertex_input(VertexInputDesc*);

//If not added yet binding, pushes, returns false if failed to push,
//Else changes the stride/rate of the binding
bool vertex_input_set_binding(VertexInputDesc*, u32 binding, u32 stride, bool vertex_rate);
//If not added yet the location, pushes, returns false if failed to push,
//Else changes the offset/format/binding of the location
bool vertex_input_add_attribute(VertexInputDesc*, u32 location, u32 offset, VkFormat format); 

enum MemoryItemType{
  MEMORY_ITEM_TYPE_NONE,
  MEMORY_ITEM_TYPE_CPU_BUFFER,
  MEMORY_ITEM_TYPE_GPU_BUFFER,
  MEMORY_ITEM_TYPE_IMAGE,
};

//Some generic copying mechanisms
typedef struct MemoryItem MemoryItem;
struct MemoryItem {
  enum MemoryItemType type;
  union{
    u8Slice cpu_buffer;
    struct{
      BufferObj obj;
      size_t offset;
      size_t length;
    } gpu_buffer;
    struct{
      ImageObj obj;
      VkImageLayout layout;
    } image;    
  };
};
DEF_DARRAY(MemoryItem,1);

//A helper fxn to free memory item assuming creation was ok
MemoryItem free_memory_item(MemoryItem item, AllocInterface allocr, GPUAllocr* gpu_allocr, VkDevice device);

//when both are present, it means do copying, where dst's ownership is taken to be dst
//but for src ownership is first taken into dst, copied and back to src
//src == dst and dst = none mean same
//Here, no copying has to occur, 

//For changing ownership only, keep dst none
//For changing image layout only, no copies, set dst also same as src,
//Then set dst's layout value to that needed
//Access is always memory/buffer/image after write for src, after read for dst
typedef struct CopyUnit CopyUnit;
struct CopyUnit {
  MemoryItem src;
  MemoryItem dst;
};

DEF_SLICE(CopyUnit);
DEF_DARRAY(BufferObj,1);

//Buffers used for staging will be filled in stage_items
//stage_buffers should have been initialized, not necessarily by allocr
//For now only color aspect images are copied
typedef struct  {
  u32 src_queue_family;
  u32 dst_queue_family;
  VkPipelineStageFlags2 src_stage_mask;
  VkPipelineStageFlags2 dst_stage_mask;
  AllocInterface allocr;
  VkCommandBuffer cmd_buf;
  GPUAllocr* gpu_allocr;
} CopyMemoryParam;

//Only single planar 2D color images can be copied here,
//If multiplanar images {i think cubemaps?} are used, do it separately
//During copying to/from cpu/gpu buffer from/to image, it is assumed that buffer contains the
//required size to accomodate the whole image of the format it was created with
// and is tightly packed , as there is no way for this fxn to determine the size of buffer
u32 copy_memory_items(MemoryItemDarray* stage_buffers, CopyUnitSlice items,
		       VkDevice device, CopyMemoryParam param);

//The way this is constructed, you can have multiple results of copy_memory_items
//  for the same command buffer into  a single stage_buffers, and call it at end once

//Need to be externally synchronized, only after this will any cpu side dst buffers obtain
// the data actually copied
//Will free stage_buffers darray
void finish_copying_items(MemoryItemDarray* stage_buffers, VkDevice device, GPUAllocr* gpu_allocr);
  

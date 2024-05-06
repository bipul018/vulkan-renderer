#pragma once

#include "common-stuff.h"
#include "device-mem-stuff.h"

DEF_SLICE(u8);

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
VkDescriptorPool make_pool(AllocInterface allocr, VkDevice device, DescSetSlice sets);
VkDescriptorSet alloc_set(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout set_layout);
void write_uniform_descriptor(VkDevice device, VkDescriptorSet set, BufferObj buffer, int binding);
void write_sampler(VkDevice device, VkDescriptorSet set, VkSampler sampler, int binding);
//Expects image layout to be shader read only optimal
void write_image(VkDevice device, VkDescriptorSet set, VkImageView image, int binding);
void write_inline_descriptor(VkDevice device, VkDescriptorSet set, u8Slice data, int binding);

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

//For copying around memory
DEF_DARRAY(BufferObj,1);
typedef struct CopyResult CopyResult;
struct CopyResult {
  struct {
    VkCommandPool cmd_pool;
    VkFence fence;
    BufferObjDarray stage_bufs;
  } value;
  enum {
    COPY_ITEMS_TO_GPU_TOP_FAIL_CODE = -0x7fff,
    COPY_ITEMS_TO_GPU_SUBMIT_FAIL,
    COPY_ITEMS_TO_GPU_RESET_FENCE_FAIL,
    COPY_ITEMS_TO_GPU_RECORD_END_FAIL,
    COPY_ITEMS_TO_GPU_ALLOC_RESULT_FAIL,
    COPY_ITEMS_TO_GPU_ALLOC_STAGING_FAIL,
    COPY_ITEMS_TO_GPU_RECORD_BEGIN_FAIL,
    COPY_ITEMS_TO_GPU_COMMAND_BUFFER_FAIL,
    COPY_ITEMS_TO_GPU_COMMAND_POOL_FAIL,
    COPY_ITEMS_TO_GPU_ALLOC_INT_FAIL,
    COPY_ITEMS_TO_GPU_FENCE_CREATE_FAIL,
    COPY_ITEMS_TO_GPU_OK = 0,
  } code;
};

//As input,
typedef struct CopyInput CopyInput;
struct CopyInput {
  u8Slice src;
  union{
    BufferObj buffer;
    ImageObj image;
  };
  bool is_buffer;
};
DEF_SLICE(CopyInput);

CopyResult copy_items_to_gpu(AllocInterface allocr, GPUAllocr* gpu_allocr, VkDevice device, CopyInputSlice items, u32 queue_index, VkQueue queue);

CopyResult copy_items_to_gpu_wait(VkDevice device, GPUAllocr* gpu_allocr, CopyResult copy_result);

  

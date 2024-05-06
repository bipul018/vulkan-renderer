#pragma once
//This is for pipeline category 1
#include "common-stuff.h"
#include "vectors.h"
#include "render-stuff.h"
//Vertex input structs or typedefs, one per binding

//Descriptor structs or typedefs, one per binding
/* typedef Vec2 Pipe1Desc1_0; */
/* typedef float Pipe1Des0_0; */
/* typedef struct{ */
/*   VkSampler sampler; */
/*   VkImageView texture2d; */
/* } Pipe1Desc2_0; */

u32 get_pipe1_translate_set(void);
u32 get_pipe1_rotate_set(void);
u32 get_pipe1_texture_set(void);

//For giving bindings list for set n
const DescSizeSlice get_pipe1_translate_bindings(void);
const DescSizeSlice get_pipe1_rotate_bindings(void);
const DescSizeSlice get_pipe1_texture_bindings(void);

//For giving layouts for set n
VkDescriptorSetLayout get_pipe1_translate_layout(VkDevice device);
VkDescriptorSetLayout get_pipe1_rotate_layout(VkDevice device);
VkDescriptorSetLayout get_pipe1_texture_layout(VkDevice device);

//Create pipeline layout, one per set : also store this as a global static handle
VkPipelineLayout get_pipe1_pipe_layout(VkDevice device);

void clear_layouts(VkDevice device);

//Create pipeline, takes in shader names, maybe some other options, but is not globally stored

//Ignores the pipe_layout parameter and fills it itself,
//Also it fills the vertex input attributes acc input_desc
OptPipeline create_pipeline1(AllocInterface allocr, VkDevice device, VertexInputDesc vert_desc,ShaderNames shaders, VkRenderPass render_pass, u32 subpass);

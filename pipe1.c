#include "pipe1.h"
#include "shaders/glsl.h"
//Define global layout storages
VkDescriptorSetLayout g_layouts[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};

VkPipelineLayout g_pipe_layout = VK_NULL_HANDLE;

u32 get_pipe1_transform_set(void){
  return TRANSFORM_UNI_SET;
}
u32 get_pipe1_texture_data_set(void){
  return TEXTURE_DATA_UNI_SET;
}
u32 get_pipe1_transform_binding_no(void){
  return TRANSFORM_UNI_BINDING;
}
u32 get_pipe1_texture_binding_no(void){
  return TEXTURE_UNI_BINDING;
}
u32 get_pipe1_sampler_binding_no(void){
  return SAMPLER_UNI_BINDING;
}

VkDescriptorSetLayout get_pipe1_transform_layout(VkDevice device){
  if(g_layouts[get_pipe1_transform_set()] != VK_NULL_HANDLE)
    return g_layouts[get_pipe1_transform_set()];

  g_layouts[get_pipe1_transform_set()] =
    create_descriptor_set_layout
    (device, 0,
     {
       .binding = get_pipe1_transform_binding_no(),
       .descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
       .descriptorCount= 2*sizeof(Vec4) + sizeof(Vec2),
       .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
     });
  
  return g_layouts[get_pipe1_transform_set()];
}
const DescSizeSlice get_pipe1_transform_bindings(void){

  static DescSize res[] = {
    {.type = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
     .descriptorCount = 2*sizeof(Vec4) + sizeof(Vec2)}
  };

  return SLICE_FROM_ARRAY(DescSize, res);
}

/* VkDescriptorSetLayout get_pipe1_rotate_layout(VkDevice device){ */
/*   if(g_layouts[get_pipe1_rotate_set()] != VK_NULL_HANDLE) */
/*     return g_layouts[get_pipe1_rotate_set()]; */

/*   g_layouts[get_pipe1_rotate_set()] = create_descriptor_set_layout */
/*     (device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR, */
/*      { */
/*        .binding = 0, */
/*        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, */
/*        .descriptorCount = 1, */
/*        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, */
/*      }); */
  
/*   return g_layouts[get_pipe1_rotate_set()]; */
/* } */
/* //This returns 0 as push descriptors aren't supposed to be allocated */
/* const DescSizeSlice get_pipe1_rotate_bindings(void){ */
  
/*   static const DescSize res[] = { */
/*     {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, */
/*      .descriptorCount = 1} */
/*   }; */

/*   return SLICE_FROM_ARRAY(DescSize, res); */
/* } */

VkDescriptorSetLayout get_pipe1_texture_data_layout(VkDevice device){
  if(g_layouts[get_pipe1_texture_data_set()] != VK_NULL_HANDLE)
    return g_layouts[get_pipe1_texture_data_set()];
  g_layouts[get_pipe1_texture_data_set()] = create_descriptor_set_layout
    (device, 0,
     {.binding = get_pipe1_sampler_binding_no(),
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
      .descriptorCount= 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
     },
     {.binding = get_pipe1_texture_binding_no(),
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .descriptorCount= 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
     });

  return g_layouts[get_pipe1_texture_data_set()];
}
const DescSizeSlice get_pipe1_texture_data_bindings(void){
  static DescSize descs[] = {
    {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1},
    {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1}
  };

  return SLICE_FROM_ARRAY(DescSize, descs);
}

//Create pipeline layout, one per set : also store this as a global static handle
VkPipelineLayout get_pipe1_pipe_layout(VkDevice device){
  if(g_pipe_layout != VK_NULL_HANDLE)
    return g_pipe_layout;

  if((get_pipe1_transform_layout(device) == VK_NULL_HANDLE) ||
     //(get_pipe1_texture_layout(device) == VK_NULL_HANDLE) ||
     (get_pipe1_texture_data_layout(device) == VK_NULL_HANDLE))
    return VK_NULL_HANDLE;
  
  VkPipelineLayoutCreateInfo pipe_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pushConstantRangeCount = 0,
    .setLayoutCount = _countof(g_layouts),
    .pSetLayouts = g_layouts,
  };
  if(VK_SUCCESS != vkCreatePipelineLayout(device, &pipe_layout_info,
					  get_glob_vk_alloc(), &g_pipe_layout)){
    g_pipe_layout = VK_NULL_HANDLE;    
  }
  return g_pipe_layout;
}

void clear_layouts(VkDevice device){
  if(VK_NULL_HANDLE != g_pipe_layout)
    vkDestroyPipelineLayout(device, g_pipe_layout, get_glob_vk_alloc());
  g_pipe_layout = VK_NULL_HANDLE;
  for_range(int, i, 0, _countof(g_layouts)){
    if(VK_NULL_HANDLE != g_layouts[i])
      vkDestroyDescriptorSetLayout(device, g_layouts[i], get_glob_vk_alloc());
    g_layouts[i] = VK_NULL_HANDLE;
  }
}
OptPipeline create_pipeline1(AllocInterface allocr, VkDevice device, VertexInputDesc vert_desc,ShaderNames shaders, VkRenderPass render_pass, u32 subpass){
  CreateGraphicsPipelineParam params = {
    .create_infos = default_graphics_pipeline_creation_infos(),
    .pipe_layout = get_pipe1_pipe_layout(device),
    .shaders = shaders,
    .compatible_render_pass = render_pass,
    .subpass_index = subpass,
    .vert_bindings = {.data = vert_desc.bindings.data, .count = vert_desc.bindings.count},
    .vert_attrs = {.data = vert_desc.attrs.data, .count = vert_desc.attrs.count}
  };
  return create_graphics_pipeline (allocr, device, params);
}

OptPipeline create_pipeline1_point(AllocInterface allocr, VkDevice device, VertexInputDesc vert_desc,ShaderNames shaders, VkRenderPass render_pass, u32 subpass){
  GraphicsPipelineCreationInfos infos = default_graphics_pipeline_creation_infos();
  infos.input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  CreateGraphicsPipelineParam params = {
    .create_infos = infos,
    .pipe_layout = get_pipe1_pipe_layout(device),
    .shaders = shaders,
    .compatible_render_pass = render_pass,
    .subpass_index = subpass,
    .vert_bindings = {.data = vert_desc.bindings.data, .count = vert_desc.bindings.count},
    .vert_attrs = {.data = vert_desc.attrs.data, .count = vert_desc.attrs.count}
  };
  return create_graphics_pipeline (allocr, device, params);
}






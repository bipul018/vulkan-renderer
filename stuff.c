#include "stuff.h"
#include "device-mem-stuff.h"
#include "render-stuff.h"

#include <stdio.h>
#include <string.h>

static VulkanDevice g_device;
static AllocInterface g_allocr;
static GPUAllocr gpu_allocr;

static u32 frame_count = 0;
static OptPipeline main_pipeline = {0};
static OptBuffer vertex_buffer = {0};
static VkPipelineLayout pipe_layout = {0};
static bool inited_properly = false;

typedef struct Vec2 Vec2;
struct Vec2 {
  float x;
  float y;
};

typedef struct VertexInput VertexInput;
struct VertexInput {
  Vec2 v2;
};
VkDescriptorSetLayout desc_layout = VK_NULL_HANDLE;
VkDescriptorPool desc_pool = VK_NULL_HANDLE;

//0 for sampler, 1 for sampled image
VkDescriptorSetLayout tex_layout = VK_NULL_HANDLE;
VkDescriptorSet tex_set[] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
OptImage tex_img = {.code = ALLOC_IMAGE_IMAGE_CREATE_FAIL};
VkImageView tex_view = VK_NULL_HANDLE;
VkSampler tex_sampler = VK_NULL_HANDLE;
Vec2 translate = {0.f, 0.f};

DEF_SLICE(VkDescriptorSet);
VkDescriptorSetSlice desc_sets = {0};

//for now image resources
static bool init_textures(void){

  //image for now will be rgba , will be packing it manually, 2x2 sized for now
  //so lower 8 bits will be r
  u32 image[] = {
    (255 << 0) | (0 << 8) | (0 << 16) | (255 << 24),
    (0 << 0) | (255 << 8) | (0 << 16) | (255 << 24), 
    (0 << 0) | (0 << 8) | (255 << 16) | (255 << 24), 
    (255 << 0) | (255 << 8) | (255 << 16) | (255 << 24), 
  };

  tex_img = alloc_image(&gpu_allocr, g_device.device, 2, 2,
			(AllocImageParams){
			  .image_format = VK_FORMAT_R8G8B8A8_UNORM,
			  .optimal_layout = false,
			  .pre_initialized = true,
			  .props_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|
				 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			  .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
			});

  if(tex_img.code < 0)
    return false;

  //Copy image now
  void* loc = create_mapping_of_image(g_device.device, gpu_allocr, tex_img.value);
  if(loc == nullptr)
    return false;
  memcpy(loc, image, sizeof(image));
  flush_mapping_of_image(g_device.device, gpu_allocr, tex_img.value);
  //Maybe have to perform a layout transition or, copy image to another image 

  //TODO look how mipmapping is achieved
  
  //Create image view
  VkResult res = vkCreateImageView(g_device.device,
				   &(VkImageViewCreateInfo){
				     .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				     .image = tex_img.value.vk_obj,
				     .viewType = VK_IMAGE_VIEW_TYPE_2D,
				     .format = VK_FORMAT_R8G8B8A8_UNORM,
				     .components = { 0 },//0 = identity swizzle
				     .subresourceRange = {
				       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				     },
				   }, get_glob_vk_alloc(), &tex_view);
  if(res != VK_SUCCESS){
    tex_view = VK_NULL_HANDLE;
    return false;
  }

  //Create sampler now
  VkSamplerCreateInfo sampler_info = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_NEAREST,
    .minFilter = VK_FILTER_NEAREST,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    //Now there are mip LOD bias and anisotropy things,
    // and compare operations
    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    .unnormalizedCoordinates = VK_FALSE,
  };
  res = vkCreateSampler(g_device.device, &sampler_info, get_glob_vk_alloc(), &tex_sampler);
  if(res != VK_SUCCESS){
    tex_sampler = VK_NULL_HANDLE;
    return false;
  }
  return true;
}

static void clear_textures(void){
  if(tex_sampler != VK_NULL_HANDLE)
    vkDestroySampler(g_device.device, tex_sampler, get_glob_vk_alloc());
  if(tex_view != VK_NULL_HANDLE)
    vkDestroyImageView(g_device.device, tex_view, get_glob_vk_alloc());
  free_image(&gpu_allocr, tex_img, g_device.device);
      
}

//before that descriptors
static void init_descriptors0(void){
  VkDescriptorSetLayoutBinding bindings[] = {
    {.binding = 0,
	.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
	.descriptorCount= sizeof(Vec2),
     .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    }
  };

  VkDescriptorSetLayoutCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pBindings = bindings,
    .bindingCount = _countof(bindings),
  };

  if(VK_SUCCESS != vkCreateDescriptorSetLayout(g_device.device,
					       &create_info,
					       get_glob_vk_alloc(),
					       &desc_layout)){
    desc_layout = VK_NULL_HANDLE;
    return;
  }
  VkDescriptorSetLayoutBinding tex_bindings[] = {
    {.binding = 0,
     .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
     .descriptorCount= 1,
     .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    },
    {.binding = 1,
     .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
     .descriptorCount= 1,
     .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    },
  };

  VkDescriptorSetLayoutCreateInfo tex_create_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pBindings = tex_bindings,
    .bindingCount = _countof(tex_bindings),
  };

  if(VK_SUCCESS != vkCreateDescriptorSetLayout(g_device.device,
					       &tex_create_info,
					       get_glob_vk_alloc(),
					       &tex_layout)){
    tex_layout = VK_NULL_HANDLE;
    return;
  }
    

  if(VK_SUCCESS != vkCreateDescriptorPool
     (g_device.device, &(VkDescriptorPoolCreateInfo){
       .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
       .maxSets = frame_count,
       .poolSizeCount = 3,//need to see flags
       .pPoolSizes = (VkDescriptorPoolSize[]){
	 {.type = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
	  .descriptorCount = 8*frame_count},//frame_count},
	 {.type = VK_DESCRIPTOR_TYPE_SAMPLER,
	  .descriptorCount = 1},
	 {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
	  .descriptorCount = 1},
       },
       .pNext = &(VkDescriptorPoolInlineUniformBlockCreateInfo){
	 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO,
	 .maxInlineUniformBlockBindings = frame_count
       },
     },
       get_glob_vk_alloc(), &desc_pool)){
    desc_pool = VK_NULL_HANDLE;
    return;
  }

  //Now need to some how  create and bind inline descriptors ?
  desc_sets = SLICE_ALLOC(VkDescriptorSet, frame_count, g_allocr);
  if(desc_sets.data != nullptr){
    for_slice(desc_sets, i){
      VkResult res = vkAllocateDescriptorSets
	(g_device.device, &(VkDescriptorSetAllocateInfo){
	  .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
	  .descriptorPool = desc_pool,
	  .descriptorSetCount = 1,
	  .pSetLayouts=&desc_layout,
	}, desc_sets.data + i);

      if(res != VK_SUCCESS){
	desc_sets.data[i] = VK_NULL_HANDLE;
      }
    }
  }
}

//Write descriptors per frame
static void write_descriptors(u32 frame_inx){

  vkUpdateDescriptorSets
    (g_device.device, 1,
     &(VkWriteDescriptorSet){
       .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .dstSet = desc_sets.data[frame_inx],
       .dstBinding = 0,
       .dstArrayElement = 0,
       .descriptorCount = sizeof(translate),
       .descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
       .pNext = &(VkWriteDescriptorSetInlineUniformBlock){
	 .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK,
	 .dataSize = sizeof(translate),
	 .pData = &translate
       },
     }, 0, nullptr);
  
			 
}


static void clear_descriptors0(void){
  SLICE_FREE(desc_sets, g_allocr);

  
  if(VK_NULL_HANDLE != desc_pool){
    vkDestroyDescriptorPool(g_device.device, desc_pool, get_glob_vk_alloc());
  }

  if(VK_NULL_HANDLE != tex_layout){
    vkDestroyDescriptorSetLayout(g_device.device, tex_layout, get_glob_vk_alloc());
  }
  
  if(desc_layout != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(g_device.device, desc_layout, get_glob_vk_alloc());
}

//Now need to make textures happen


void init_stuff(VulkanDevice device, AllocInterface allocr, VkRenderPass render_pass, u32 max_frames){
  g_device = device;
  g_allocr = allocr;
  frame_count = max_frames;

  gpu_allocr = init_allocr(g_device.phy_device, g_allocr);

  init_descriptors0();
  //Pipeline layout needed
  VkPipelineLayoutCreateInfo pipe_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pushConstantRangeCount = 0,
    .setLayoutCount = 1,
    .pSetLayouts = &desc_layout,
  };

  pipe_layout = VK_NULL_HANDLE;
  

  if(VK_SUCCESS != vkCreatePipelineLayout(g_device.device, &pipe_layout_info,
					  get_glob_vk_alloc(), &pipe_layout)){
    return;
  }
  
  //vert.spv and frag.spv
  GraphicsPipelineCreationInfos pipe_info = default_graphics_pipeline_creation_infos();
  //setup vertex input state create info
  VkVertexInputBindingDescription bindings[] = {
    {.binding = 0, .stride = sizeof(VertexInput), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}
  };

  VkVertexInputAttributeDescription attributes[] = {
    {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset =0}
  };
  pipe_info.vertex_input_state.vertexBindingDescriptionCount = _countof(bindings);
  pipe_info.vertex_input_state.pVertexBindingDescriptions = bindings;
  pipe_info.vertex_input_state.vertexAttributeDescriptionCount = _countof(attributes);
  pipe_info.vertex_input_state.pVertexAttributeDescriptions = attributes;
  
  main_pipeline = create_graphics_pipeline
    (g_allocr, g_device.device,
     (CreateGraphicsPipelineParam){
       .create_infos = pipe_info,
       .pipe_layout = pipe_layout,
       .vert_shader_file = "shaders/vert-sh.spv",
       .frag_shader_file = "shaders/frag-sh.spv",
       .compatible_render_pass = render_pass,
       .subpass_index = 0
     });


  printf("Now creating buffers\n");

  VertexInput inputs[] = {
    {0.f, 0.f},
    {0.5f, 0.f},
    {0.5f, 0.5f},
    {0.5f, 0.5f},
    {0.f, 0.5f},
    {0.f, 0.f}
  };

  vertex_buffer = alloc_buffer(&gpu_allocr, g_device.device, sizeof(inputs),
			       (AllocBufferParams){
				 .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				 .props_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|
				 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				 //.share_mode = VK_SHARING_MODE_EXCLUSIVE,
			       });
  if(vertex_buffer.code >= 0){
    void* mapping = create_mapping_of_buffer(g_device.device, gpu_allocr, vertex_buffer.value);
    if(mapping != nullptr){
      memcpy(mapping, inputs, sizeof(inputs));
      flush_mapping_of_buffer(g_device.device, gpu_allocr, vertex_buffer.value);
    }
    printf("Copied the buffer data\n");


  }


  if(desc_layout != VK_NULL_HANDLE){
    printf("Descriptor layout was created \n");
  }
  //amend this inited properly
  printf("Exiting init_stuff\n");
  inited_properly = (main_pipeline.code >= 0) && (vertex_buffer.code >= 0)
    && init_textures();
  if(inited_properly){
    printf("Was inited properly\n");
  }
}

void clean_stuff(void){
  clear_textures();
  free_buffer(&gpu_allocr, vertex_buffer, g_device.device);
  clear_graphics_pipeline(main_pipeline, g_device.device);
  if(VK_NULL_HANDLE != pipe_layout){
    vkDestroyPipelineLayout(g_device.device, pipe_layout, get_glob_vk_alloc());
  }
  clear_descriptors0();
  gpu_allocr = deinit_allocr(gpu_allocr, g_device.device);  
}
void event_stuff(MSG msg){

}

void update_stuff(int width, int height){

  static Vec2 da_translate = {0,0};

  translate.x = (da_translate.x - width/2) / (width/2);
  translate.y = (da_translate.y - height/2) / (height/2);
  if(da_translate.x >= width){
    da_translate.x = 0;
  }
  da_translate.x += 1;
  da_translate.y += 0.5;
}

void render_stuff(u32 frame_inx, VkCommandBuffer cmd_buf){
  if(!inited_properly)
    return;
  write_descriptors(frame_inx);

  //bind descriptor set man
  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  pipe_layout, 0, 1, desc_sets.data + frame_inx, 0, nullptr);
  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS , main_pipeline.value);

  vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vertex_buffer.value.vk_obj,
			 (VkDeviceSize[]){0});


  vkCmdDraw(cmd_buf, 6, 1, 0, 0);
}


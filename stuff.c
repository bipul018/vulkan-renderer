#include "stuff.h"
#include "device-mem-stuff.h"
#include "render-stuff.h"
#include "pipe1.h"
#include "pipeline-helpers.h"
  //TODO :: Make the default all fail code = -1 for everything
#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR_;
#define vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR_

static VulkanDevice g_device;
static AllocInterface g_allocr;
static GPUAllocr gpu_allocr;

static u32 frame_count = 0;
static OptPipeline main_pipeline = {.code = CREATE_GRAPHICS_PIPELINE_TOP_FAIL_CODE};
static OptBuffer vertex_buffer = {0};
//static VkPipelineLayout pipe_layout = {0};
static bool inited_properly = false;

//VkDescriptorSetLayout desc_layout = VK_NULL_HANDLE;
//VkDescriptorSetLayout push_desc_layout = VK_NULL_HANDLE;
VkDescriptorPool desc_pool = VK_NULL_HANDLE;

//0 for sampler, 1 for sampled image
//VkDescriptorSetLayout tex_layout = VK_NULL_HANDLE;
VkDescriptorSet tex_set = VK_NULL_HANDLE;
OptImage tex_img = {.code = ALLOC_IMAGE_IMAGE_CREATE_FAIL};
VkImageView tex_view = VK_NULL_HANDLE;
VkSampler tex_sampler = VK_NULL_HANDLE;
Vec2 translate = {0.f, 0.f};

DEF_SLICE(VkDescriptorSet);
VkDescriptorSetSlice desc_sets = {0};

VkDescriptorSetSlice rot_sets = {0};

float angle = 0;
//Frame buffers for angle
DEF_SLICE(OptBuffer);
OptBufferSlice angle_bufs = {0};

//Now going to make a descriptor for uniform, normal, that is rather pushed

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//for now image resources
static bool init_textures(void){
  
  u32 image_backup[] = {
    (255 << 0) | (0 << 8) | (0 << 16) | (255 << 24),
    (0 << 0) | (255 << 8) | (0 << 16) | (255 << 24),
    (0 << 0) | (0 << 8) | (255 << 16) | (255 << 24),
    (255 << 0) | (255 << 8) | (255 << 16) | (255 << 24),
  };

  int width, height, channels = 4;
  unsigned char *img = stbi_load("res/aphoto.jpg", &width, &height, &channels, 4);
  unsigned char *image = img;
  if(img == NULL) {
    printf("Error in loading the image\n");
    image = (unsigned char *)image_backup;
    width = height = 2;
    channels = 4;
  }
  else{
    printf("Image res/starry-night.jpg was loaded with width %d, height %d, channels %d\n",
	   width, height, channels);
    channels = 4;
  }
  tex_img = alloc_image(&gpu_allocr, g_device.device, width, height,
				   (AllocImageParams){
				     .image_format = VK_FORMAT_R8G8B8A8_UNORM,
				     .optimal_layout = true,
				     .pre_initialized = false,
				     .props_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				     .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
				     VK_IMAGE_USAGE_SAMPLED_BIT,
				   });
  //TODO :: copy image data to image object with layout transition
  if(tex_img.code >= 0){
    CopyResult img_cpy = copy_items_to_gpu(g_allocr, &gpu_allocr, g_device.device,
					   MAKE_ARRAY_SLICE(CopyInput, {
					       .src = init_u8_slice(image,
								     width*height*channels),
					       .image = tex_img.value,
					       .is_buffer = false,
					     }), g_device.graphics_family_inx,
					 g_device.graphics_queue);
    copy_items_to_gpu_wait(g_device.device, &gpu_allocr, img_cpy);
  }
  
  //TODO look how mipmapping is achieved  
  if(img != nullptr)
    free(img);
  
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
				       .layerCount = VK_REMAINING_ARRAY_LAYERS,
				       .levelCount = VK_REMAINING_MIP_LEVELS,
				     },
				   }, get_glob_vk_alloc(), &tex_view);
  if(res != VK_SUCCESS){
    tex_view = VK_NULL_HANDLE;

    return false;
  }

  //Create sampler now
  VkSamplerCreateInfo sampler_info = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
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
static bool init_descriptors0(void){

  //TODO:: call the layout creating functions

  //TODO:: make pool for the non push descriptors

  desc_pool = make_pool(g_allocr, g_device.device, MAKE_ARRAY_SLICE
			(DescSet,
			 {.count = frame_count, .descs = get_pipe1_translate_bindings()},
			 {.count = 1, .descs = get_pipe1_texture_bindings()}));
			 //{.count = frame_count, .descs = get_pipe1_rotate_bindings()}));
  if(desc_pool == VK_NULL_HANDLE)
    return false;
  //TOOD later maybe just let pass 'push descriptor' flags

  bool failed = false;
  desc_sets = SLICE_ALLOC(VkDescriptorSet, frame_count, g_allocr);
  if(desc_sets.data != nullptr){
    for_slice(desc_sets, i){

      //TODO:: inline uniforms allocate using the fxn
      desc_sets.data[i] = alloc_set(g_device.device, desc_pool, get_pipe1_translate_layout(g_device.device));
      if(desc_sets.data[i] == VK_NULL_HANDLE){
	failed = true;
      }
    }
  }
  if(failed)
    return false;
  failed = false;
  /* rot_sets = SLICE_ALLOC(VkDescriptorSet, frame_count, g_allocr); */
  /* if(rot_sets.data == nullptr){ */
  /*   failed = true; */
  /* } */
  /* for_slice(rot_sets, i){ */
  /*   rot_sets.data[i] = alloc_set(g_device.device, desc_pool, get_pipe1_rotate_layout(g_device.device)); */
  /*   if(rot_sets.data[i] == VK_NULL_HANDLE){ */
  /*     failed = true; */
  /*   } */
  /* } */

  /* if(failed) */
  /*   return false; */
  //allocate sets for sampler and sampled image
  //TODO:: allocate image thing sets using the fxn

  tex_set = alloc_set(g_device.device, desc_pool, get_pipe1_texture_layout(g_device.device));
  
  
  if(VK_NULL_HANDLE == tex_set){
    return false;
  }
  //TODO write the descriptors for texture here
  write_sampler(g_device.device, tex_set, tex_sampler, 0);
  write_image(g_device.device, tex_set, tex_view, 1);

  return true;
}

//Write descriptors per frame
static void write_descriptors(u32 frame_inx){

  //TODO :: write using inline descriptor thing
  write_inline_descriptor(g_device.device, desc_sets.data[frame_inx],
			  init_u8_slice((void*)&translate, sizeof(translate)), 0);
  /* write_uniform_descriptor(g_device.device, rot_sets.data[frame_inx], */
  /* 			   angle_bufs.data[frame_inx].value, 0); */
}


static void clear_descriptors0(void){
  SLICE_FREE(desc_sets, g_allocr);
  
  
  SLICE_FREE(desc_sets, g_allocr);
  
  if(VK_NULL_HANDLE != desc_pool){
    vkDestroyDescriptorPool(g_device.device, desc_pool, get_glob_vk_alloc());
  }
  
}

//Now need to make textures happen


void init_stuff(VulkanDevice device, AllocInterface allocr, VkRenderPass render_pass, u32 max_frames){
  g_device = device;
  g_allocr = allocr;
  frame_count = max_frames;

  gpu_allocr = init_allocr(g_device.phy_device, g_allocr);
  bool tex_inited = init_textures();
  bool desc_inited = init_descriptors0();
  //Pipeline layout needed

  //TODO :: use pipeline creation directly, no layout creation here, using updated version

  VertexInputDesc vert_desc = init_vertex_input(g_allocr);

  bool was_success = true;
  was_success = was_success && vertex_input_set_binding
    (&vert_desc, 0, sizeof(Vec2), true);
  was_success = was_success && vertex_input_add_attribute
    (&vert_desc, 0, 0, VK_FORMAT_R32G32_SFLOAT);

  if(was_success){
    main_pipeline = create_pipeline1(g_allocr, g_device.device, vert_desc,
				     (ShaderNames){
				       .vert = "shaders/vert-sh.spv",
				       .frag = "shaders/frag-sh.spv"
				     }, render_pass, 0);
  }
  clear_vertex_input(&vert_desc);

  printf("Now creating buffers\n");

  Vec2 inputs[] = {
    {0.f, 0.f},
    {0.5f, 0.f},
    {0.5f, 0.5f},
    {0.5f, 0.5f},
    {0.f, 0.5f},
    {0.f, 0.f}
  };

  //TODO :: maybe make it device local and write using staging buffer 
  
  vertex_buffer = alloc_buffer(&gpu_allocr, g_device.device, sizeof(inputs),
			       (AllocBufferParams){
				 .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ,
				 .props_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				 .make_transfer_dst = true,
			       });

  if(vertex_buffer.code >= 0){
    CopyResult vert_cpy = copy_items_to_gpu
      (g_allocr, &gpu_allocr, g_device.device,
       MAKE_ARRAY_SLICE(CopyInput,
			{.src = init_u8_slice((void*)inputs, sizeof(inputs)),
			 .buffer = vertex_buffer.value,
			 .is_buffer = true,}),
			g_device.graphics_family_inx, g_device.graphics_queue);
    copy_items_to_gpu_wait(g_device.device, &gpu_allocr, vert_cpy);
  }
  
  printf("Copied the buffer data\n");

  angle_bufs = SLICE_ALLOC(OptBuffer, frame_count, g_allocr);
  bool all_created = true;
  if(nullptr != angle_bufs.data){
    for_slice(angle_bufs, i){
      angle_bufs.data[i] = alloc_buffer(&gpu_allocr, g_device.device, sizeof(angle),
					(AllocBufferParams){
					  .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					  .props_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					});
      if(angle_bufs.data[i].code < 0){
	all_created = false;
      }
    }
  }
  else{
    all_created = false;
  }

  //amend this inited properly
  printf("Exiting init_stuff\n");
  inited_properly = (main_pipeline.code >= 0) && (vertex_buffer.code >= 0)
    && tex_inited && desc_inited && all_created && was_success ;

  if(inited_properly){
    printf("Was inited properly\n");
  }
}

void clean_stuff(void){

  for_slice(angle_bufs, i){
    free_buffer(&gpu_allocr, angle_bufs.data[i], g_device.device);
  }
  SLICE_FREE(angle_bufs, g_allocr);
  clear_textures();
  free_buffer(&gpu_allocr, vertex_buffer, g_device.device);
  clear_graphics_pipeline(main_pipeline, g_device.device);
  clear_descriptors0();
  //TODO clear layouts
  clear_layouts(g_device.device);
  gpu_allocr = deinit_allocr(gpu_allocr, g_device.device);  
}
void event_stuff(MSG msg){
  
}

void update_stuff(int width, int height, u32 frame_inx){

  angle += 0.005;
  
  static Vec2 da_translate = {0,0};

  translate.x = (da_translate.x - width/2) / (width/2);
  translate.y = (da_translate.y - height/2) / (height/2);
  if(da_translate.x >= (width - 90)){
    da_translate.x = 0;
  }
  if(da_translate.y >= (height - 90)){
    da_translate.y = 0;
  }
  da_translate.x += 1;
  da_translate.y += 0.5;

  CopyResult vert_cpy = copy_items_to_gpu
    (g_allocr, &gpu_allocr, g_device.device,
     MAKE_ARRAY_SLICE(CopyInput,
		      {.src = init_u8_slice((void*)&angle, sizeof(angle)),
		       .buffer = angle_bufs.data[frame_inx].value,
		       .is_buffer = true,}),
     g_device.graphics_family_inx, g_device.graphics_queue);
  copy_items_to_gpu_wait(g_device.device, &gpu_allocr, vert_cpy);
  
  /* void* ptr = create_mapping_of_buffer(g_device.device, gpu_allocr, angle_bufs.data[frame_inx].value); */
  /* if(ptr == nullptr){ */
  /*   return; */
  /* } */
  /* *(float*)ptr = angle; */
  /* flush_mapping_of_buffer(g_device.device, gpu_allocr, angle_bufs.data[frame_inx].value); */

}

void render_stuff(u32 frame_inx, VkCommandBuffer cmd_buf){
  if(!inited_properly)
    return;
  write_descriptors(frame_inx);

  //Push desriptor set
  //Create a buffer
  
  

  vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			    get_pipe1_pipe_layout(g_device.device), get_pipe1_rotate_set(), 1,
			    &(VkWriteDescriptorSet){
			      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			      .dstBinding = 0,
			      .dstArrayElement = 0,
			      .descriptorCount = 1,
			      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			      .pBufferInfo = &(VkDescriptorBufferInfo){
				.buffer = angle_bufs.data[frame_inx].value.vk_obj,
				.offset = 0,
				.range = sizeof(angle)
			      },
			    });
  /* vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, */
  /* 			  get_pipe1_pipe_layout(g_device.device), get_pipe1_rotate_set(), */
  /* 			  1, rot_sets.data + frame_inx, 0, nullptr); */
  
  //bind descriptor set man
  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  get_pipe1_pipe_layout(g_device.device), get_pipe1_translate_set(), 1, desc_sets.data + frame_inx, 0, nullptr);
  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  get_pipe1_pipe_layout(g_device.device), get_pipe1_texture_set(), 1, &tex_set, 0, nullptr);
    
  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS , main_pipeline.value);

  vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vertex_buffer.value.vk_obj,
			 (VkDeviceSize[]){0});


  vkCmdDraw(cmd_buf, 6, 1, 0, 0);
}


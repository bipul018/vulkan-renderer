#include "stuff.h"
#include "common-stuff.h"
#include "device-mem-stuff.h"
#include "render-stuff.h"
#include "pipe1.h"
#include "pipeline-helpers.h"
#include "vulkan/vulkan_core.h"
  //TODO :: Make the default all fail code = -1 for everything
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <winuser.h>
#include "compute-test.h"


extern PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR_;
#define vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR_

static VulkanDevice* g_device;
static AllocInterface g_allocr;
static GPUAllocr gpu_allocr;

static u32 frame_count = 0;
static OptPipeline main_pipeline = {.code = CREATE_GRAPHICS_PIPELINE_TOP_FAIL_CODE};
//static OptBuffer vertex_buffer = {0};
//static OptBuffer texture_buffer = {0};
//static VkPipelineLayout pipe_layout = {0};
static bool inited_properly = false;

Vec2 some_sq_seqs(size_t n){
  int sqLen = ceil(sqrt(n)); // dim of square in which n move lies
  int i = n - (sqLen - 1)*(sqLen - 1); // index since the last full square
  int d = sqLen*sqLen - (sqLen - 1)*(sqLen - 1); // difference between current sq and last full sq
  Vec2 pos;
  // pattern is increasing until center of sequence for the given sq and constant 
  // or constant until center of sequence and then decreasing
  // odd and even current sq dim determines the pattern for x and y
  if (sqLen % 2 == 0){
    pos.x = (i <= (d+1)/2)?(sqLen-1):(sqLen - 1 - abs(i - ((d+1)/2)));
    pos.y = (i >= (d+1)/2)?(sqLen-1):(i-1);
  }
  else{
    pos.x = (i >= (d+1)/2)?(sqLen-1):(i-1);
    pos.y = (i <= (d+1)/2)?(sqLen-1):(sqLen - 1 - abs(i - ((d+1)/2)));
  }
  return pos;
}

/* static const Vec2 some_sq_seqs[] = { */
/*   {0,0}, {1, 0}, {1, 1}, {0,1}, {0, 2}, {1, 2}, {2, 2}, {2, 1}, {2, 0}, {3, 0}, {3, 1}, {3, 2}, {3, 3}, {2, 3}, {1, 3}, {0, 3} */
/* }; */

static ComputeJob compute_job = {0};

#define MAX_INPUT_OBJ 40000 //3600 //324
static u32 input_count = MAX_INPUT_OBJ;
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

DEF_SLICE(Vec2);

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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

//for now image resources
static bool init_textures(void){
  
  u32 image_backup[] = {
    (255 << 0) | (0 << 8) | (0 << 16) | (255 << 24),
    (0 << 0) | (255 << 8) | (0 << 16) | (255 << 24),
    (0 << 0) | (0 << 8) | (255 << 16) | (255 << 24),
    (255 << 0) | (255 << 8) | (255 << 16) | (255 << 24),
  };

  int width, height, channels = 4;
  unsigned char *img = stbi_load("./res/starry-night.jpg", &width, &height, &channels, 4);
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
  
  tex_img = alloc_image(&gpu_allocr, g_device->device, width, height,
			(AllocImageParams){
			  .image_format = VK_FORMAT_R8G8B8A8_UNORM,
			  .optimal_layout = true,
			  .pre_initialized = false,
			  .props_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			  .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			  VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			  VK_IMAGE_USAGE_SAMPLED_BIT,
			});
  

  if(tex_img.code >= 0){
    if(immediate_command_begin(g_device->device, g_device->graphics)){
      MemoryItemDarray stage = init_MemoryItem_darray(g_allocr);
      u32 count = copy_memory_items(&stage, MAKE_ARRAY_SLICE
				    (CopyUnit, {.src  = {
					.type = MEMORY_ITEM_TYPE_CPU_BUFFER,
					.cpu_buffer = init_u8_slice(image, width*height*channels)},
						.dst = {
						  .type = MEMORY_ITEM_TYPE_IMAGE,
						  .image = {
						    .obj = tex_img.value,
						    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						  }
						}
				    }), g_device->device, (CopyMemoryParam){
				      .allocr = g_allocr,
				      .gpu_allocr = &gpu_allocr,
				      .cmd_buf = g_device->graphics.im_buffer,
				      .src_queue_family = g_device->graphics.family,
				      .dst_queue_family = g_device->graphics.family,
				      .src_stage_mask = VK_PIPELINE_STAGE_2_NONE,
				      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				    }
				    );
					
      if(!immediate_command_end(g_device->device, g_device->graphics) ||
	 (count != 1)){

	//TODO :: handle this and flow the error back
	assert(false);
      }
      finish_copying_items(&stage, g_device->device, &gpu_allocr);
    }
  }
  
  //TODO look how mipmapping is achieved  
  if(img != nullptr)
    free(img);
  
  //Create image view
  VkResult res = vkCreateImageView(g_device->device,
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
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    //Now there are mip LOD bias and anisotropy things,
    // and compare operations
    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    .unnormalizedCoordinates = VK_FALSE,
  };
  res = vkCreateSampler(g_device->device, &sampler_info, get_glob_vk_alloc(), &tex_sampler);
  if(res != VK_SUCCESS){
    tex_sampler = VK_NULL_HANDLE;

    return false;
  }

  return true;
}

static void clear_textures(void){
  if(tex_sampler != VK_NULL_HANDLE)
    vkDestroySampler(g_device->device, tex_sampler, get_glob_vk_alloc());
  if(tex_view != VK_NULL_HANDLE)
    vkDestroyImageView(g_device->device, tex_view, get_glob_vk_alloc());
  free_image(&gpu_allocr, tex_img, g_device->device);
      
}

//before that descriptors
static bool init_descriptors0(void){

  //TODO:: call the layout creating functions

  //TODO:: make pool for the non push descriptors

  desc_pool = make_pool
    (g_allocr, g_device->device, 
     {.count = frame_count, .descs = get_pipe1_translate_bindings()},
     {.count = 1, .descs = get_pipe1_texture_bindings()}
     );
  //{.count = frame_count, .descs = get_pipe1_rotate_bindings()}));
  if(desc_pool == VK_NULL_HANDLE)
    return false;
  //TOOD later maybe just let pass 'push descriptor' flags

  bool failed = false;
  desc_sets = SLICE_ALLOC(VkDescriptorSet, frame_count, g_allocr);
  if(desc_sets.data != nullptr){
    for_slice(desc_sets, i){

      //TODO:: inline uniforms allocate using the fxn
      desc_sets.data[i] = alloc_set(g_device->device, desc_pool, get_pipe1_translate_layout(g_device->device));
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

  tex_set = alloc_set(g_device->device, desc_pool, get_pipe1_texture_layout(g_device->device));
  
  
  if(VK_NULL_HANDLE == tex_set){
    return false;
  }
  //TODO write the descriptors for texture here
  write_sampler(g_device->device, tex_set, tex_sampler, 0);
  write_image(g_device->device, tex_set, tex_view, 1);

  return true;
}

//Write descriptors per frame
static void write_descriptors(u32 frame_inx){

  //TODO :: write using inline descriptor thing
  write_inline_descriptor(g_device->device, desc_sets.data[frame_inx],
			  init_u8_slice((void*)&translate, sizeof(translate)), 0);
  /* write_uniform_descriptor(g_device.device, rot_sets.data[frame_inx], */
  /* 			   angle_bufs.data[frame_inx].value, 0); */
}


static void clear_descriptors0(void){
  SLICE_FREE(desc_sets, g_allocr);
  
  
  SLICE_FREE(desc_sets, g_allocr);
  
  if(VK_NULL_HANDLE != desc_pool){
    vkDestroyDescriptorPool(g_device->device, desc_pool, get_glob_vk_alloc());
  }
  
}

//Now need to make textures happen

//void do_compute_stuff(VulkanDevice device, AllocInterface allocr, GPUAllocr* gpu_allocr);

void init_stuff(VulkanDevice* ptr_device, AllocInterface allocr, VkRenderPass render_pass, u32 max_frames){
  g_device = ptr_device;
  g_allocr = allocr;
  frame_count = max_frames;

  gpu_allocr = init_allocr(g_device->phy_device, g_allocr);

  //do_compute_stuff(device, allocr, &gpu_allocr);
  
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
  
  was_success = was_success && vertex_input_set_binding
    (&vert_desc, 1, sizeof(Vec2), true);
  was_success = was_success && vertex_input_add_attribute
    (&vert_desc, 1, 0, VK_FORMAT_R32G32_SFLOAT);


  if(was_success){
    main_pipeline = create_pipeline1_point(g_allocr, g_device->device,
					   vert_desc,
					   (ShaderNames){
					     .vert = "./shaders/glsl.vert.spv",
					     .frag = "./shaders/glsl.frag.spv"
					   }, render_pass, 0);
  }
  clear_vertex_input(&vert_desc);


  angle_bufs = SLICE_ALLOC(OptBuffer, frame_count, g_allocr);
  bool all_created = true;
  if(nullptr != angle_bufs.data){
    for_slice(angle_bufs, i){
      angle_bufs.data[i] = alloc_buffer(&gpu_allocr, g_device->device, sizeof(angle),
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
  
  printf("Now creating buffers\n");
  const float span = 0.001f;
  const float padding = 0.001f;
  Vec2 base_inputs[] = {
    {0.f, 0.f},
    {span , 0.f},
    {span , span },
    {span , span },
    {0.f, span },
    {0.f, 0.f}
  };
  
  Vec2 base_tex_inputs[] = {
    {0.f, 0.f},
    {span, 0.f},
    {span, span},
    {span, span},
    {0.f, span},
    {0.f, 0.f}
  };
  
  static Vec2 inputs[MAX_INPUT_OBJ * _countof(base_inputs)] = {0};
  static Vec2 tex_inputs[MAX_INPUT_OBJ * _countof(base_tex_inputs)] = {0};
  static Vec2 vels[MAX_INPUT_OBJ * _countof(base_inputs)] = {0};
  //size_t compute_size = 6 * MAX_INPUT_OBJ;
  Vec2Slice compute_data[] = {
    SLICE_FROM_ARRAY(Vec2, tex_inputs),
    SLICE_FROM_ARRAY(Vec2, inputs),
    SLICE_FROM_ARRAY(Vec2, vels)
  };
  
  for_range(u32, i, 0, MAX_INPUT_OBJ){
    u32 base_inx = i * _countof(base_inputs);
    memcpy(inputs + base_inx, base_inputs, sizeof(base_inputs));
    memcpy(tex_inputs + base_inx, base_tex_inputs, sizeof(base_tex_inputs));

    for_range(u32, j, 0, _countof(base_inputs)){
      inputs[base_inx + j] = vec2_add
	(inputs[base_inx + j],
	 vec2_scale_fl
	 (some_sq_seqs(i+1), 
	  span + padding));
      tex_inputs[base_inx + j] = vec2_add
	(tex_inputs[base_inx + j],
	 vec2_scale_fl
	 (some_sq_seqs(i+1), 
	  span));
    }
  }
  for_slice(compute_data[2], i){
    compute_data[1].data[i] = (Vec2){.x = 0.1f*(rand() / (RAND_MAX - 1.f))-0.05f,
				     .y = 0.1f*(rand() / (RAND_MAX - 1.f))-0.05f,};
    
    compute_data[2].data[i] = (Vec2){.x = 0.2f*(rand() / (RAND_MAX - 1.f))-0.1f,
				     .y = 0.2f*(rand() / (RAND_MAX - 1.f))-0.1f,};
  }
  

  
  compute_job = init_compute_job(&gpu_allocr, allocr, g_device, 3,
				    &compute_data,
				    init_test_compute,
				    run_test_compute,
				    clean_test_compute);

  
  //amend this inited properly
  printf("Exiting init_stuff\n");
  inited_properly = (main_pipeline.code >= 0) //&& (vertex_buffer.code >= 0)
    // && (texture_buffer.code >= 0) 
    && tex_inited && desc_inited && all_created && was_success
    && (compute_job.code >= 0);

  if(inited_properly){
    printf("Was inited properly\n");
  }

  printf("Going to try initing compute stuff\n");

}

void clean_stuff(void){

  clean_compute_job(&compute_job);

  for_slice(angle_bufs, i){
    free_buffer(&gpu_allocr, angle_bufs.data[i], g_device->device);
  }
  SLICE_FREE(angle_bufs, g_allocr);
  clear_textures();
  // free_buffer(&gpu_allocr, texture_buffer, g_device->device);
  //free_buffer(&gpu_allocr, vertex_buffer, g_device->device);
  clear_graphics_pipeline(main_pipeline, g_device->device);
  clear_descriptors0();
  //TODO clear layouts
  clear_layouts(g_device->device);
  gpu_allocr = deinit_allocr(gpu_allocr, g_device->device); 
}

bool take_ss = false;

u32 ss_frame = 0;

void event_stuff(MSG msg){
    if(msg.message == WM_KEYDOWN){
    switch(msg.wParam){
    case ' ':{
      printf("Keyup on space");
      take_ss = true;
      //Now create another image, copy swapchain image into this image
      //Then make into a file after copying into a buffer
    } break;
    case VK_RIGHT:{
    if(input_count < MAX_INPUT_OBJ)
      input_count++;

    }break;
    case VK_LEFT:{
      if(input_count > 1)
	input_count--;
    }break;
    }
  }
  
}

void update_stuff(int width, int height, u32 frame_inx){

  //angle += 0.005;
  
  static Vec2 da_translate = {0,0};

  /* translate.x = (da_translate.x - width/2.f) / (width/2.f); */
  /* translate.y = (da_translate.y - height/2.f) / (height/2.f); */
  /* if(da_translate.x >= (width - 90)){ */
  /*   da_translate.x = 0; */
  /* } */
  /* if(da_translate.y >= (height - 90)){ */
  /*   da_translate.y = 0; */
  /* } */
  /* da_translate.x += 1; */
  /* da_translate.y += 0.5; */

  
  
  void* ptr = create_mapping_of_buffer(g_device->device, gpu_allocr, angle_bufs.data[frame_inx].value);
  if(ptr == nullptr){
    return;
  }
  *(float*)ptr = angle;
  flush_mapping_of_buffer(g_device->device, gpu_allocr, angle_bufs.data[frame_inx].value);

}

VkSemaphore render_stuff(u32 frame_inx, VkCommandBuffer cmd_buf){
  if(!inited_properly)
    return VK_NULL_HANDLE;

  RunComputeOutput compute_res = check_run_compute_job(g_allocr,
						       &compute_job,
						       frame_inx);
  if(compute_res.was_failed)
    return VK_NULL_HANDLE;
  write_descriptors(frame_inx);

  //Push desriptor set
  //Create a buffer

  VkDependencyInfo before_info = {0};
  VkDependencyInfo after_info = {0};

  bool was_computed = false;


  
  /* if(take_ss && (frame_inx == 0)){ */
  /*   was_computed = run_compute_stuff(tex_img.value, tex_view, &before_info, &after_info); */
  /*   take_ss = false; */
  /* } */

  
  vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			    get_pipe1_pipe_layout(g_device->device), get_pipe1_rotate_set(), 1,
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
			  get_pipe1_pipe_layout(g_device->device), get_pipe1_translate_set(), 1, desc_sets.data + frame_inx, 0, nullptr);
  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  get_pipe1_pipe_layout(g_device->device), get_pipe1_texture_set(), 1, &tex_set, 0, nullptr);
    
  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS , main_pipeline.value);

  VkBuffer vert_buffers[] = {
    //vertex_buffer.value.vk_obj,
    compute_job.render_side_items.data[frame_inx].data[1].gpu_buffer.vk_obj,
    compute_job.render_side_items.data[frame_inx].data[0].gpu_buffer.vk_obj,
    //texture_buffer.value.vk_obj
  };
  
  vkCmdBindVertexBuffers(cmd_buf, 0, 2, vert_buffers,
			 (VkDeviceSize[]){0,0});


  vkCmdDraw(cmd_buf, 6 * input_count, 1, 0, 0);


  return compute_res.sema;
}


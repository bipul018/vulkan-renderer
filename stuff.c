#include "stuff.h"
#include "device-mem-stuff.h"
#include "render-stuff.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR_;
#define vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR_

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
VkDescriptorSetLayout push_desc_layout = VK_NULL_HANDLE;
VkDescriptorPool desc_pool = VK_NULL_HANDLE;

//0 for sampler, 1 for sampled image
VkDescriptorSetLayout tex_layout = VK_NULL_HANDLE;
VkDescriptorSet tex_set = VK_NULL_HANDLE;
OptImage tex_img = {.code = ALLOC_IMAGE_IMAGE_CREATE_FAIL};
VkImageView tex_view = VK_NULL_HANDLE;
VkSampler tex_sampler = VK_NULL_HANDLE;
Vec2 translate = {0.f, 0.f};

DEF_SLICE(VkDescriptorSet);
VkDescriptorSetSlice desc_sets = {0};

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

  OptBuffer stage_buff = alloc_buffer(&gpu_allocr, g_device.device, width * height * channels,
				   (AllocBufferParams){
				     .props_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				     .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				   });
			

  if((tex_img.code < 0) || (stage_buff.code < 0)){
    free_buffer(&gpu_allocr, stage_buff, g_device.device);
    return false;
  }

  //Copy image now
  void* loc = create_mapping_of_buffer(g_device.device, gpu_allocr, stage_buff.value);
  if(loc == nullptr){
    free_buffer(&gpu_allocr, stage_buff, g_device.device);
    return false;
  }
   
  memcpy(loc, image, width * height * channels);
  flush_mapping_of_buffer(g_device.device, gpu_allocr, stage_buff.value);

  if(img != nullptr)
    free(img);
  
  //Maybe have to perform a layout transition or, copy image to another image

  //Do layout transition now, with copying from one image to another
  //VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

  //TODO :: Make the default all fail code = -1 for everything
  
  //Make it so it's easy to break out of, as zig might get confused over gotos
  OptCommandPool cmd_pool = {.code = CREATE_COMMAND_POOL_FAILED};
  OptPrimaryCommandBuffers cmd_buf = {.code = CREATE_PRIMARY_COMMAND_BUFFERS_TOP_FAIL_CODE};
  OptFences fence = {.code = CREATE_FENCES_TOP_FAIL_CODE};
  do{

    //Create a cmd pool, then allocate a single cmd buffer, then create a fence
    //Then begin cmd recording, then submit it with the fence, wait for it, destroy all

    cmd_pool = create_command_pool(g_device.device, g_device.graphics_family_inx);
    cmd_buf = create_primary_command_buffers
      (g_allocr, g_device.device, cmd_pool, 1);
    fence = create_fences(g_allocr, g_device.device, 1);
    
    if((cmd_buf.code < 0) && (fence.code < 0)){
      break;
    }

    if(VK_SUCCESS != vkResetFences(g_device.device, 1, fence.value.data)){
      break;
    }
    if(VK_SUCCESS != vkBeginCommandBuffer
       (cmd_buf.value.data[0], &(VkCommandBufferBeginInfo){
	 .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	 .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
       }))
      break;
    /*
      The three dependency flags
      VK_DEPENDENCY_BY_REGION_BIT
      VK_DEPENDENCY_DEVICE_GROUP_BIT 
      VK_DEPENDENCY_VIEW_LOCAL_BIT >> Not used when outside of render pass
    */
    //p 405
    
    vkCmdPipelineBarrier2(cmd_buf.value.data[0], &(VkDependencyInfo){
	.sType =  VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
	.dependencyFlags = 0,
	.imageMemoryBarrierCount = 1,
	.pImageMemoryBarriers = &(VkImageMemoryBarrier2)
	  {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
	   .srcStageMask = VK_PIPELINE_STAGE_HOST_BIT,
	   .srcAccessMask = VK_ACCESS_NONE,
	   /* .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, */
	   /* .dstAccessMask = VK_ACCESS_SHADER_READ_BIT, */
	   .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
	   .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
	   .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	   .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	   //.newLayout = VK_IMAGE_LAYOUT_GENERAL,
	   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,//g_device.graphics_family_inx,
	   .image = tex_img.value.vk_obj,
	   .subresourceRange = {
	     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	     .layerCount = VK_REMAINING_ARRAY_LAYERS,
	     .levelCount = VK_REMAINING_MIP_LEVELS,
	   },},
	});


    vkCmdCopyBufferToImage(cmd_buf.value.data[0], stage_buff.value.vk_obj,
			   tex_img.value.vk_obj,
			   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			   &(VkBufferImageCopy){
			     .imageSubresource = {
			       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			       .layerCount = 1,
			     },
			     .imageOffset = {0},
			     .imageExtent = {.width=width,.height=height,.depth=1},
			     .bufferOffset=0,
			     .bufferRowLength=0,
			     .bufferImageHeight=0
			   });
			       
    
    /* vkCmdCopyImage(cmd_buf.value.data[0], stage_img.value.vk_obj, */
    /* 		   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex_img.value.vk_obj, */
    /* 		   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1, */
    /* 		   &(VkImageCopy){ */
    /* 		     .srcSubresource = { */
    /* 		       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, */
    /* 		       .layerCount = 1, */
    /* 		     }, */
    /* 		     .dstSubresource = { */
    /* 		       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, */
    /* 		       .layerCount = 1, */
    /* 		     }, */
    /* 		     .srcOffset = {0}, */
    /* 		     .dstOffset = {0}, */
    /* 		     .extent = {.width = 2, .height = 2, .depth = 1} */
    /* 		   }); */

    vkCmdPipelineBarrier2(cmd_buf.value.data[0], &(VkDependencyInfo){
	.sType =  VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
	.dependencyFlags = 0,
	.imageMemoryBarrierCount = 1,
	.pImageMemoryBarriers = &(VkImageMemoryBarrier2){
	  .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
	  .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
	  .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
	  /* .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, */
	  /* .dstAccessMask = VK_ACCESS_SHADER_READ_BIT, */
	  .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
	  .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
	  .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	  .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	  //.newLayout = VK_IMAGE_LAYOUT_GENERAL,
	  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,//g_device.graphics_family_inx,
	  .image = tex_img.value.vk_obj,
	  .subresourceRange = {
	    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	    .layerCount = VK_REMAINING_ARRAY_LAYERS,
	    .levelCount = VK_REMAINING_MIP_LEVELS,
	  },
	}
      });;

    
    if(VK_SUCCESS != vkEndCommandBuffer(cmd_buf.value.data[0])){
      break;
    }

    if(VK_SUCCESS != vkQueueSubmit2
       (g_device.graphics_queue, 1, &(VkSubmitInfo2){
	 .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
	 .commandBufferInfoCount = 1,
	   .pCommandBufferInfos = &(VkCommandBufferSubmitInfo){
	     .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
	     .commandBuffer = cmd_buf.value.data[0],
	   }
       }, fence.value.data[0]))
      break;
    VkResult res = vkWaitForFences(g_device.device, 1, fence.value.data, VK_TRUE, UINT64_MAX);
    if(res != VK_SUCCESS){
      break;
    }
  }while(false);
  vkDeviceWaitIdle(g_device.device);
  //Delete everything
  clear_fences(g_allocr, fence, g_device.device);
  clear_primary_command_buffers(g_allocr, cmd_buf);
  clear_command_pool(cmd_pool, g_device.device);
  

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
				       .layerCount = VK_REMAINING_ARRAY_LAYERS,
				       .levelCount = VK_REMAINING_MIP_LEVELS,
				     },
				   }, get_glob_vk_alloc(), &tex_view);
  if(res != VK_SUCCESS){
    tex_view = VK_NULL_HANDLE;
    free_buffer(&gpu_allocr, stage_buff, g_device.device);
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
    free_buffer(&gpu_allocr, stage_buff, g_device.device);
    return false;
  }
  free_buffer(&gpu_allocr, stage_buff, g_device.device);
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
    return false;
  }

  VkDescriptorSetLayoutBinding push_bindings[] = {
      {.binding = 0,
       .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
       .descriptorCount = sizeof(float),
       .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      }
    };
  VkDescriptorSetLayoutCreateInfo push_create_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pBindings = push_bindings,
    .bindingCount = _countof(push_bindings),
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
  };
  if(VK_SUCCESS != vkCreateDescriptorSetLayout(g_device.device,
					       &push_create_info,
					       get_glob_vk_alloc(),
					       &push_desc_layout)){
    push_desc_layout = VK_NULL_HANDLE;
    return false;
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
    return false;
  }
    

  if(VK_SUCCESS != vkCreateDescriptorPool
     (g_device.device, &(VkDescriptorPoolCreateInfo){
       .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
       .maxSets = frame_count + 1, //One for sampler and image combo
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
    return false;
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
  //allocate sets for sampler and sampled image
  VkResult res = vkAllocateDescriptorSets
    (g_device.device, &(VkDescriptorSetAllocateInfo){
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = desc_pool,
      .descriptorSetCount = 1,
      .pSetLayouts=&tex_layout,
    }, &tex_set);
  if(VK_SUCCESS != res){
    tex_set = VK_NULL_HANDLE;
    return false;
  }
  else{
    //write the descriptors for texture here
    
    vkUpdateDescriptorSets
      (g_device.device, 2,
       (VkWriteDescriptorSet[]){
	 {
	   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	   .dstSet = tex_set,
	   .dstBinding = 0,
	   .dstArrayElement = 0,
	   .descriptorCount = 1,
	   .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
	   .pImageInfo = &(VkDescriptorImageInfo){
	     .sampler = tex_sampler,
	   },
	 },
	 {
	   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	   .dstSet = tex_set,
	   .dstBinding = 1,
	   .dstArrayElement = 0,
	   .descriptorCount = 1,
	   .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
	   .pImageInfo = &(VkDescriptorImageInfo){
	     .imageView = tex_view,
	     .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	   },
	 }
       }, 0, nullptr);
  }

  return true;
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

  if(push_desc_layout != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(g_device.device, push_desc_layout, get_glob_vk_alloc());
  
  if(desc_layout != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(g_device.device, desc_layout, get_glob_vk_alloc());
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
  VkPipelineLayoutCreateInfo pipe_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pushConstantRangeCount = 0,
    .setLayoutCount = 3,
    .pSetLayouts = (VkDescriptorSetLayout[]){push_desc_layout, desc_layout, tex_layout},
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

  angle_bufs = SLICE_ALLOC(OptBuffer, frame_count, g_allocr);
  bool all_created = true;
  if(nullptr != angle_bufs.data){
    for_slice(angle_bufs, i){
      angle_bufs.data[i] = alloc_buffer(&gpu_allocr, g_device.device, sizeof(angle),
					(AllocBufferParams){
					  .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					  .props_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
					});
      if(angle_bufs.data[i].code < 0){
	all_created = false;
      }
    }
  }
  else{
    all_created = false;
  }

  if(push_desc_layout == VK_NULL_HANDLE){
    printf("Push descriptor layout wasn't created\n");
  }
  
  if(desc_layout != VK_NULL_HANDLE){
    printf("Descriptor layout was created \n");
  }
  //amend this inited properly
  printf("Exiting init_stuff\n");
  inited_properly = (main_pipeline.code >= 0) && (vertex_buffer.code >= 0)
    && tex_inited && desc_inited && all_created;
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
  if(VK_NULL_HANDLE != pipe_layout){
    vkDestroyPipelineLayout(g_device.device, pipe_layout, get_glob_vk_alloc());
  }
  clear_descriptors0();
  gpu_allocr = deinit_allocr(gpu_allocr, g_device.device);  
}
void event_stuff(MSG msg){
  
}

void update_stuff(int width, int height){

  angle += 0.01;
  
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
}

void render_stuff(u32 frame_inx, VkCommandBuffer cmd_buf){
  if(!inited_properly)
    return;
  write_descriptors(frame_inx);

  //Push desriptor set
  //Create a buffer
  
  
  void* ptr = create_mapping_of_buffer(g_device.device, gpu_allocr, angle_bufs.data[frame_inx].value);
  if(ptr == nullptr){
    return;
  }
  *(float*)ptr = angle;
  flush_mapping_of_buffer(g_device.device, gpu_allocr, angle_bufs.data[frame_inx].value);

  vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			    pipe_layout, 0, 1,
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
  
  //bind descriptor set man
  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  pipe_layout, 1, 1, desc_sets.data + frame_inx, 0, nullptr);
  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  pipe_layout, 2, 1, &tex_set, 0, nullptr);
    
  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS , main_pipeline.value);

  vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vertex_buffer.value.vk_obj,
			 (VkDeviceSize[]){0});


  vkCmdDraw(cmd_buf, 6, 1, 0, 0);
}


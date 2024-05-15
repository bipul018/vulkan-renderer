#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "common-stuff.h"
#include "vulkan/vulkan_core.h"
#include "window-stuff.h"
#include "render-stuff.h"
#include "device-mem-stuff.h"
#include "LooplessSizeMove.h"
#include "stuff.h"
#include "features.h"

//Vulkan debug messenger callback
VkBool32 vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      msg,
    void*                                            pUserData){

  static int num = 0;

  const bool enable_printf = false;
  
  /* if((messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) && */
  /*    (!enable_printf ||  */
  /*     ((0 != strcmp("WARNING-DEBUG-PRINTF", msg->pMessageIdName))))) */
  /*   return VK_FALSE; */
  


  const char* severity = "";
  switch(messageSeverity){
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    severity = "ERROR";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    severity = "WARNING";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    severity = "INFO";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    severity = "VERBOSE";
    break;
  default:
    break;
  }
  
  _set_printf_count_output(1);
  int msg_len = 1;
  printf("[%s] %nMessage Number >> %d\n", severity,&msg_len, num);
  printf("%*sMessage Id Name >> %s\n",msg_len, "", msg->pMessageIdName);
  printf("%*sVulkan Message Id >> %d\n", msg_len, "", msg->messageIdNumber);
  printf("%*sMessage >> %s\n\n", msg_len, "", msg->pMessage);

  num++;

  if(messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT){
    assert(false);
  }
  
  return VK_FALSE;
}

PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR_ = nullptr;

//Create win32 surface and window and stuff

LRESULT CALLBACK wnd_proc(HWND h_wnd, UINT msg, WPARAM wparam, LPARAM lparam){
  if(msg == WM_DESTROY){
    PostQuitMessage(0);
    return 0;
  }
  
  return DefWindowProcA(h_wnd, msg, wparam, lparam);
}


int main(){
  AllocInterface allocr = gen_std_allocator();
  
  VulkanLayer inst_layers[] = {
    //{.layer_name = "VK_LAYER_LUNARG_api_dump", .required = true},
    {.layer_name = "VK_LAYER_KHRONOS_validation", .required = true},
  };

  VulkanExtension inst_exts[] = {
    {.extension_name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME, .required = true},
    {.extension_name = VK_KHR_SURFACE_EXTENSION_NAME, .required = true},
    {.extension_name = VK_KHR_WIN32_SURFACE_EXTENSION_NAME, .required = true},
  };

  //Setup debug messenger
  debug_callbacks.pfnUserCallback = vk_debug_callback;
  OptInstance inst = create_instance(allocr,
				     SLICE_FROM_ARRAY(VulkanLayer, inst_layers),
				     SLICE_FROM_ARRAY(VulkanExtension, inst_exts),
				     instance_features_ptr);

  if(inst.code < 0)
    goto instance;
  printf("Creation of instance successfull\n");

  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst.value, "vkCreateDebugUtilsMessengerEXT");
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst.value, "vkDestroyDebugUtilsMessengerEXT");

  VkDebugUtilsMessengerEXT dbg_messenger = {0};
  if(VK_SUCCESS != vkCreateDebugMessenger(inst.value, &debug_callbacks,
					  get_glob_vk_alloc(), &dbg_messenger)){
    printf("Couldn't setup debug messenger\n");
  }
  
  OptVulkanWindow window = create_vulkan_window
    ((CreateVulkanWindowParams){
      .window_name = "Hello Vulkan",
      .wnd_proc = wnd_proc,
      .width = 500,
      .height = 500
    }, inst.value, WS_OVERLAPPEDWINDOW);

  if(window.code < 0)
    goto window;
  printf("Creation of window successfull\n");


  //create device
  VkFormat depth_stencil_format;
  VkSurfaceFormatKHR img_format;
  u32 min_imgs = 0;

  VulkanLayer dev_layers[] = {
    {.layer_name = "VK_LAYER_KHRONOS_validation", .required = true},
  };

  VulkanExtension dev_extensions[] = {
    {.extension_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME, .required = true},
    {.extension_name = VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, .required = true},
    {.extension_name = "VK_KHR_push_descriptor", .required = true},
  };

  //Features to enable
  void* device_feats = MAKE_DEVICE_FEATURES();

  GET_FEATURE_1_3(device_feats, inlineUniformBlock) = VK_TRUE;
  GET_FEATURE_1_3(device_feats, synchronization2) = VK_TRUE;
  //GET_FEATURE_1_0(device_feats, fillModeNonSolid) = VK_TRUE;

  OptVulkanDevice device = create_device(allocr,
					 (CreateDeviceParam){
					   .chosen_surface = window.value.surface,
					   .vk_instance = inst.value,
					   .p_img_format = &img_format,
					   .p_depth_stencil_format = &depth_stencil_format,
					   .p_min_img_count = &min_imgs
					 },
					 SLICE_FROM_ARRAY(VulkanLayer, dev_layers),
					 SLICE_FROM_ARRAY(VulkanExtension, dev_extensions),
					 device_feats);
  if(device.code < 0)
    goto device;

  printf("Device Created\n");

  //Create the fnx
  vkCmdPushDescriptorSetKHR_ = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(device.value.device, "vkCmdPushDescriptorSetKHR");

  if(vkCmdPushDescriptorSetKHR_ == nullptr){
    printf("Warning, couldnot get device proc address\n");
  }
  
  //Create renderpass
  OptRenderPass render_pass = create_render_pass(device.value.device,
						 img_format.format,
						 depth_stencil_format);
  if(render_pass.code < 0)
    goto render_pass;
  
  struct SwapchainEntities old_swapchain = {0};
  OptSwapchainOut swap = create_swapchain(allocr,(CreateSwapchainParam){
      .device = device.value,
      .surface = window.value.surface,
      .win_width = window.value.width,
      .win_height = window.value.height,
      .p_min_img_count = &min_imgs,
      .p_surface_format = &img_format,
      .old_swapchain = old_swapchain.swapchain,
    });

  OptSwapchainImages swap_imgs = create_swapchain_images(device.value.device,
							 swap, img_format.format);
  OptDepthbufferOut swap_depth = create_depthbuffers(device.value,
						     swap, depth_stencil_format);
  OptFramebuffers swap_frames = create_framebuffers(device.value.device,
						    render_pass.value,
						    swap,
						    swap_imgs.value.img_views,
						    swap_depth.value.depthbuffer_views);
  OptSwapchainEntities swap_entity = compose_swapchain_entities(swap,
								swap_imgs,
								swap_depth,
							      swap_frames);
  VkExtent2D swap_extent = swap.value.extent;
  if(swap_entity.code < 0)
    goto swap_entity;

  printf("Swapchain created\n");
  //create command buffers, fences, semaphores
  const size_t max_frames_in_flight = 3;
  size_t curr_frame = 0;

  //My Goal, one day just using compute pipelines, srite to a texture and then
  //Either display using win32 direct, or through sdl blit 
  
  //Frame fences, render finished semaphores, present done semaphores
  OptFences frame_fences = create_fences(allocr, device.value.device, max_frames_in_flight);
  OptSemaphores render_finised_semas = create_semaphores(allocr, device.value.device,
							 max_frames_in_flight);
  OptSemaphores present_done_semas = create_semaphores(allocr, device.value.device,
						      max_frames_in_flight);

  if((frame_fences.code < 0) || (render_finised_semas.code < 0) || (present_done_semas.code < 0))
    goto sync_objs;

  printf("Sync objects created\n");

  //On graphics queue, error passed through to command buffers
  OptCommandPool cmd_pool = create_command_pool(device.value.device,
						device.value.graphics.family);

  OptPrimaryCommandBuffers cmd_bufs = create_primary_command_buffers
    (allocr, device.value.device, cmd_pool, max_frames_in_flight);
  if(cmd_bufs.code < 0)
    goto cmd_things;

  printf("Command things created\n");

  //Here pipelines and descriptors and gpu memories are initialized
  // later move everything above away maybe eh

  init_stuff(&device.value, allocr, render_pass.value, max_frames_in_flight);
  printf("Inited stuff\n");
  
  //Now rendering loop needed
  MSG msg = {0};
  while(true){
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0){
      TranslateMessage(&msg);
      SizingCheck(&msg);
      //Probably check here for keys if dont give a fuck about callbacks
      event_stuff(msg);
      DispatchMessage(&msg);
    }
    if(msg.message == WM_QUIT){
      break;
    }
    update_stuff(window.value.width, window.value.height, curr_frame);
    //Here do vulkan rendering stuff
    u32 img_inx = 0;
    int begin_code = begin_rendering_operations
      ((BeginRenderingOperationsParam){
	.device = device.value.device,
	.swapchain = swap_entity.value.swapchain,
	.render_pass = render_pass.value,
	.framebuffers = swap_entity.value.framebuffers,
	.framebuffer_render_extent = swap_extent,
	.cmd_buffer = cmd_bufs.value.data[curr_frame],
	.present_done_semaphore = present_done_semas.value.data[curr_frame],
	.render_done_fence = frame_fences.value.data[curr_frame],
	.clear_value = (VkClearValue){
	  .color = {0.9f, 0.9f, 0.9f, 1.0f}
	},
	.p_img_inx = &img_inx});


    if(begin_code <  0) continue;
    // try recreate swapchain or minimized then recreate swapchain
    if((begin_code == BEGIN_RENDERING_OPERATIONS_TRY_RECREATE_SWAPCHAIN) ||
       (begin_code == BEGIN_RENDERING_OPERATIONS_MINIMIZED))
      goto recreate;
    //set viewport and scissor commands
    vkCmdSetViewport(cmd_bufs.value.data[curr_frame],
		     0, 1,
		     &(VkViewport){
		       .x = 0.f,
		       .y = 0.f,
		       .width = swap_extent.width,
		       .height = swap_extent.height,
		       .minDepth = 0.f,
		       .maxDepth = 1.f
		     });
    vkCmdSetScissor(cmd_bufs.value.data[curr_frame],
		    0, 1,
		    &(VkRect2D){
		      .offset = {.x = 0, .y= 0},
		      .extent = swap_extent
		    });

    VkSemaphore extra_sema = render_stuff(curr_frame, cmd_bufs.value.data[curr_frame]);

    VkSemaphoreSlice rndr_wait = {0};
    
    rndr_wait = MAKE_ARRAY_SLICE(VkSemaphore, present_done_semas.value.data[curr_frame], extra_sema);

    if(VK_NULL_HANDLE == extra_sema)
      rndr_wait.count--;
    
    //end_rendering
    int end_code = end_rendering_operations
      ((EndRenderingOperationsParam){
	.device = device.value.device,
	.cmd_buffer = cmd_bufs.value.data[curr_frame],
	.render_done_fence = frame_fences.value.data[curr_frame],
	.render_done_semaphore = render_finised_semas.value.data[curr_frame],
	.render_wait_semaphores = rndr_wait,
	//.present_done_semaphore = present_done_semas.value.data[curr_frame],
	.graphics_queue = device.value.graphics.vk_obj,
	.present_queue = device.value.present.vk_obj,
	.swapchain = swap_entity.value.swapchain,
	.img_index = img_inx
      });
    //again check if try recreating swapchain,
    if(end_code == END_RENDERING_OPERATIONS_TRY_RECREATING_SWAPCHAIN)
      goto recreate;
	  
    //increase frame numbering , min of max frames and surface images
    curr_frame = (curr_frame + 1) % _min(max_frames_in_flight, min_imgs);
    continue;
  recreate:
    curr_frame = curr_frame;
    RECT rc;
    GetClientRect(window.value.window_handle, &rc);
    window.value.width = rc.right;
    window.value.height = rc.bottom;
    
    OptSwapchainEntities new_entities = recreate_swapchain
      (allocr, (RecreateSwapchainParam){
	.device = device.value,
	.new_win_width = window.value.width,
	.new_win_height = window.value.height,
	.surface = window.value.surface,
	.framebuffer_render_pass = render_pass.value,
	.depth_img_format = depth_stencil_format,
	.p_surface_format = &img_format,
	.p_min_img_count = &min_imgs,
	.p_swap_extent = &swap_extent
      },&old_swapchain,swap_entity.value);
    if(new_entities.code >= 0){
      swap_entity = new_entities;
      swap_frames = extract_swap_frames_out(swap_entity);
      swap_depth = extract_swap_depth_out(swap_entity);
      swap_imgs = extract_swap_images_out(swap_entity);
      swap = extract_swapchain_out(swap_entity);
    }
  }

  vkDeviceWaitIdle(device.value.device);
  clean_stuff();

 cmd_things:
  clear_primary_command_buffers(allocr, cmd_bufs);
  clear_command_pool(cmd_pool, device.value.device);

 sync_objs:
  clear_semaphores(allocr, present_done_semas, device.value.device);
  clear_semaphores(allocr, render_finised_semas, device.value.device);
  clear_fences(allocr, frame_fences, device.value.device);
		   
						    
 swap_entity:
  clear_framebuffers(swap_frames, device.value.device, swap);
  clear_depthbuffers(swap_depth, device.value.device, swap);
  clear_swapchain_images(swap_imgs, device.value.device, swap);
  clear_swapchain(swap, device.value.device);
  {
    OptSwapchainEntities old_opt_swap = {.value = old_swapchain};
    //For the initial case when no swapchain was recreated
    if(old_opt_swap.value.swapchain == 0){
      old_opt_swap.code = COMPOSE_SWAPCHAIN_SWAPCHAIN_ERR;
    }
    OptSwapchainOut old_swap = extract_swapchain_out(old_opt_swap);
    clear_framebuffers(extract_swap_frames_out(old_opt_swap),
		       device.value.device, old_swap);
    clear_depthbuffers(extract_swap_depth_out(old_opt_swap),
		       device.value.device, old_swap);
    clear_swapchain_images(extract_swap_images_out(old_opt_swap),
			   device.value.device, old_swap);
    clear_swapchain(old_swap, device.value.device);
  }
 render_pass:
  render_pass = clear_render_pass(render_pass, device.value.device);
 device:
  device = clear_device(device);
 window:
  window = clear_vulkan_window(window, inst.value);
  vkDestroyDebugMessenger(inst.value,dbg_messenger , get_glob_vk_alloc());
 instance:
  inst = clear_instance(inst);

  return 0;
}

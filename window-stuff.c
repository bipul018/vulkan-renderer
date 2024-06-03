#include "window-stuff.h"
#include <libloaderapi.h>
#include <stdbool.h>
//#include <xkeycheck.h>
#include "render-stuff.h"
#include <stdio.h>
#include <winuser.h>


//Create win32 surface and window and stuff

static LRESULT CALLBACK win32_wnd_proc(HWND h_wnd, UINT msg, WPARAM wparam, LPARAM lparam){
  if(msg == WM_CLOSE){
    //Kind of hack, to prevent having to send custom data and letting msg pump get all close messages
    PostThreadMessageA(GetCurrentThreadId(), WM_CLOSE, 0, (LPARAM)h_wnd);
    return 0;
  }

  //TODO:: Later add mechanism of calling user callbacks also
  
  return DefWindowProcA(h_wnd, msg, wparam, lparam);
}


OptVulkanWindow create_vulkan_window(CreateVulkanWindowParams params, VkInstance vk_instance, int style_flags){
  OptVulkanWindow window = {0};

  //This won't ever be unregistered i think
  static LPSTR wnd_class_name = NULL;
  if(wnd_class_name == NULL){
    wnd_class_name =  "Window Class" __FILE__ STRINGIFY(__LINE__) ;
    WNDCLASS wnd_class = {
      .hInstance = GetModuleHandle(NULL),
      .lpszClassName = wnd_class_name,
      .lpfnWndProc = win32_wnd_proc,
    };
    RegisterClass(&wnd_class);
  }
  
  if(nullptr == params.window_name){
    params.window_name = "Sample Vulkan Window";
  }
  /* if(nullptr == params.window_class){ */
  /*   params.window_class = "Sample Vulkan Class"; */
  /* } */
  if(0 == params.width){
    params.width = CW_USEDEFAULT;
  }
  if(0 == params.height){
    params.height = CW_USEDEFAULT;
  }
  params.posy += 15;
  /* if(nullptr == params.wnd_proc){ */
  /*   params.wnd_proc = DefWindowProc; */
  /* } */
  
  /* WNDCLASS wnd_class = { */
  /*   .hInstance = GetModuleHandle(NULL), */
  /*   .lpszClassName = params.window_class, */
  /*   .lpfnWndProc = params.wnd_proc, */
  /* }; */
  /* RegisterClass(&wnd_class); */
  /* window.value.class_name = params.window_class; */

  //WIndow flags
  //WS_OVERLAPPEDWINDOW | WS_MAXIMIZE,
  window.value.window_handle = CreateWindowEx(0, wnd_class_name,
					      params.window_name,
					      style_flags,
					      params.posx, params.posy,
					      params.width, params.height,
					      NULL, NULL,
					      GetModuleHandle(NULL),
					      params.proc_data);

  if(nullptr == window.value.window_handle){
    window.code = CREATE_VULKAN_WINDOW_WINDOW_CREATE_FAIL;
    return window;
  }
  VkWin32SurfaceCreateInfoKHR create_info = {
    .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
    .hinstance = GetModuleHandle(NULL),
    .hwnd = window.value.window_handle,
  };
  VkResult res = VK_SUCCESS;
  res = vkCreateWin32SurfaceKHR(vk_instance, &create_info,
				get_glob_vk_alloc(),
				&window.value.surface);
  if(VK_SUCCESS != res){
    window.code = CREATE_VULKAN_WINDOW_SURFACE_CREATE_FAIL;
    return window;
  }
  
  return window;
}

void show_vulkan_window(VulkanWindow* vk_window){
  ShowWindow(vk_window->window_handle, SW_SHOW);
  RECT rc;
  GetClientRect(vk_window->window_handle, &rc);

  vk_window->width = rc.right;
  vk_window->height = rc.bottom;
}

OptVulkanWindow clear_vulkan_window(OptVulkanWindow vk_window, VkInstance vk_instance){
  switch(vk_window.code){
  case CREATE_VULKAN_WINDOW_OK:
    //destroy surface
    vkDestroySurfaceKHR(vk_instance, vk_window.value.surface, get_glob_vk_alloc());
  case CREATE_VULKAN_WINDOW_SURFACE_CREATE_FAIL:
    //destroy window
    DestroyWindow(vk_window.value.window_handle);
  case CREATE_VULKAN_WINDOW_WINDOW_CREATE_FAIL:
    //unregister class
    /* UnregisterClass(vk_window.value.class_name, */
    /* 		    GetModuleHandle(NULL)); */
    
  case CREATE_VULKAN_WINDOW_TOP_FAIL_CODE:
    vk_window = (OptVulkanWindow){0};
  }
  return vk_window;
}

OptSwapchainOut create_swapchain(AllocInterface allocr, CreateSwapchainParam param){
  VkResult result = VK_SUCCESS;
  OptSwapchainOut output = {0};


  OptSwapchainDetails swap_dets = choose_swapchain_details
    (allocr, (ChooseSwapchainDetsParam){
      .phy_device = param.device.phy_device,
      .surface = param.surface,
      .return_extent = true,
      .return_transform = true,
      .return_present_mode = true,
      .return_format_n_count = true
    });

  if(swap_dets.code < 0){
    output.code = CREATE_SWAPCHAIN_CHOOSE_DETAILS_FAIL;
    return output;
  }
  output.value.allocr = allocr;
    
  //param.p_curr_swapchain_data->swapchain_life = 0;

  VkSurfaceFormatKHR surface_format = swap_dets.value.img_format;
  VkPresentModeKHR present_mode = swap_dets.value.present_mode;
  uint32_t min_img_count = swap_dets.value.min_img_count;
  VkSurfaceTransformFlagBitsKHR transform_flags = swap_dets.value.transform_flags;
  VkExtent2D curr_extent = swap_dets.value.surface_extent;
    
  // Choose swap extent
  //  ++++
  //   TODO:: make dpi aware, ..., maybe not needed
  //  ++++
  // Now just use set width and height, also currently not checked
  // anything from capabilities Also be aware of _max and _min extent
  // set to numeric _max of uint32_t
  //TODO :: make this not -1 but max compare
  if (curr_extent.width != -1 &&
      curr_extent.height != -1) {
      
    output.value.extent = curr_extent;
  } else {
    output.value.extent.width = param.win_width;
    output.value.extent.height = param.win_height;
  }

  if (!output.value.extent.width ||
      !output.value.extent.height) {
    output.code = CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE;
    return output;
  }

  // An array of queue family indices used
  uint32_t indices_array[] = {
    param.device.graphics.family,
    param.device.present.family,
  };

  VkSwapchainCreateInfoKHR create_info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = param.surface,
    .minImageCount = min_img_count,
    .imageFormat = surface_format.format,
    .imageColorSpace = surface_format.colorSpace,
    .imageExtent = output.value.extent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .preTransform = transform_flags,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = present_mode,
    .clipped = VK_TRUE, // This should be false only if pixels
    // have to re read
    .oldSwapchain = param.old_swapchain,
    .imageSharingMode = VK_SHARING_MODE_CONCURRENT,
    // Here , for exclusive sharing mode it is  optional; else for
    // concurrent, there has to be at least two different queue
    // families, and all should be specified to share the images
    // amoong
    .queueFamilyIndexCount = _countof(indices_array),
    .pQueueFamilyIndices = indices_array
  };

  if (indices_array[0] == indices_array[1]) {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  result = vkCreateSwapchainKHR(param.device.device, &create_info,
				get_glob_vk_alloc(),
				&output.value.swapchain);

  if (result != VK_SUCCESS){
    output.code = CREATE_SWAPCHAIN_FAILED;
    return output;
  }

  result = vkGetSwapchainImagesKHR(
				   param.device.device, output.value.swapchain,
				   &(output.value.img_count), NULL);

  if ((result != VK_SUCCESS) ||
      (output.value.img_count == 0)){
      
    output.code = CREATE_SWAPCHAIN_IMAGE_COUNT_LOAD_FAIL;
    return output;
  }

  bool format_changed =
    (param.p_surface_format->format != surface_format.format) ||
    (param.p_surface_format->colorSpace !=
     surface_format.colorSpace);
  bool min_count_changed =
    *(param.p_min_img_count) != min_img_count;

  *(param.p_surface_format) = surface_format;
  *(param.p_min_img_count) = min_img_count;
  //param.p_curr_swapchain_data->swapchain_life =
  //        (1 << param.p_curr_swapchain_data->img_count) - 1;

  if (format_changed && min_count_changed)
    output.code = CREATE_SWAPCHAIN_FORMAT_AND_COUNT_CHANGED;
  else if (format_changed)
    output.code = CREATE_SWAPCHAIN_SURFACE_FORMAT_CHANGED;
  else if (min_count_changed)
    output.code = CREATE_SWAPCHAIN_MIN_IMG_COUNT_CHANGED;

  return output;
}


OptSwapchainOut clear_swapchain(OptSwapchainOut swapchain, VkDevice device){

  switch (swapchain.code) {

  case CREATE_SWAPCHAIN_FORMAT_AND_COUNT_CHANGED:
  case CREATE_SWAPCHAIN_MIN_IMG_COUNT_CHANGED:
  case CREATE_SWAPCHAIN_SURFACE_FORMAT_CHANGED:
  case CREATE_SWAPCHAIN_OK:
  case CREATE_SWAPCHAIN_IMAGE_COUNT_LOAD_FAIL:
    vkDestroySwapchainKHR(device,
			  swapchain.value.swapchain,
			  get_glob_vk_alloc());

  case CREATE_SWAPCHAIN_FAILED:
  case CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE:
  case CREATE_SWAPCHAIN_CHOOSE_DETAILS_FAIL:
  case CREATE_SWAPCHAIN_TOP_FAIL_CODE:
    //param.p_swapchain_data->swapchain_life = 0;
    swapchain = (OptSwapchainOut){0};
  }
  return swapchain;
}

DEF_SLICE(VkImage);
DEF_SLICE(VkImageView);

OptSwapchainImages create_swapchain_images(VkDevice device, OptSwapchainOut swapchain_res,
					   VkFormat format){
  VkResult result = VK_SUCCESS;
  OptSwapchainImages output = {0};
  if(swapchain_res.code < 0){
    output.code = CREATE_SWAPCHAIN_IMAGES_INVALID_SWAPCHAIN;
    return output;
  }

  VkImageSlice images = SLICE_ALLOC(VkImage, swapchain_res.value.img_count,
				    swapchain_res.value.allocr);
  /* output.value.images = alloc_mem(swapchain_res.value.allocr, */
  /* 				  swapchain_res.value.img_count * sizeof(VkImage), */
  /* 				  alignof(VkImage)); */
  
  //if (!param.p_curr_swapchain_data->images){
  if(nullptr == images.data){
    output.code = CREATE_SWAPCHAIN_IMAGES_IMAGE_ALLOC_FAIL;
    return output;
  }
  output.value.images = images.data;
  
  vkGetSwapchainImagesKHR(device,
			  swapchain_res.value.swapchain,
			  &(swapchain_res.value.img_count),
			  images.data);

  VkImageViewSlice img_views = SLICE_ALLOC(VkImageView, swapchain_res.value.img_count,
					   swapchain_res.value.allocr);
  /* output.value.img_views = alloc_mem(swapchain_res.value.allocr, */
  /* 				     swapchain_res.value.img_count * sizeof(VkImageView), */
  /* 				     alignof(VkImageView)); */
  if(nullptr == img_views.data){
    //if (!param.p_curr_swapchain_data->img_views){
    output.code = CREATE_SWAPCHAIN_IMAGES_IMG_VIEW_ALLOC_FAIL;
    return output;
  }
  output.value.img_views = img_views.data;

  memset(img_views.data, 0,
	 img_views.count * sizeof(VkImageView));
  
  /* memset(output.value.img_views, 0, */
  /* 	 swapchain_res.value.img_count * */
  /* 	 sizeof(VkImageView)); */
  /* for (size_t i = 0; i < param.p_curr_swapchain_data->img_count; */
  /*      ++i) { */
  for_slice(img_views, i){
    VkImageViewCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = images.data[i],//param.p_curr_swapchain_data->images[i],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,

      .components =
      (VkComponentMapping) {
	.r = VK_COMPONENT_SWIZZLE_IDENTITY,
	.g = VK_COMPONENT_SWIZZLE_IDENTITY,
	.b = VK_COMPONENT_SWIZZLE_IDENTITY,
	.a = VK_COMPONENT_SWIZZLE_IDENTITY},

      .subresourceRange =
      (VkImageSubresourceRange) {.aspectMask =
				 VK_IMAGE_ASPECT_COLOR_BIT,
				 .baseMipLevel = 0,
				 .levelCount = 1,
				 .baseArrayLayer = 0,
				 .layerCount = 1}

    };

    result = vkCreateImageView(device, &create_info, get_glob_vk_alloc(),
			       img_views.data + i);
    if (result != VK_SUCCESS)
      break;
  }

  if (result != VK_SUCCESS){
    output.code = CREATE_SWAPCHAIN_IMAGES_IMG_VIEW_CREATE_FAIL;
  }

  return output;
}



OptSwapchainImages clear_swapchain_images(OptSwapchainImages images, VkDevice device,
					  OptSwapchainOut swapchain_res){

  switch(images.code){
  case CREATE_SWAPCHAIN_IMAGES_OK:
  case CREATE_SWAPCHAIN_IMAGES_IMG_VIEW_CREATE_FAIL:{
    VkImageViewSlice img_views = init_VkImageView_slice(images.value.img_views,
							swapchain_res.value.img_count);
    /* for (int i = 0; i < param.p_swapchain_data->img_count; */
    /* 	 ++i) { */
    for_slice(img_views, i){
      if(img_views.data[i] != 0){
	vkDestroyImageView(device,
			   img_views.data[i],
			   get_glob_vk_alloc());
      }
    }
    //free_mem(img_views
    SLICE_FREE(img_views, swapchain_res.value.allocr);
    //free(param.p_swapchain_data->img_views);
  }
  case CREATE_SWAPCHAIN_IMAGES_IMG_VIEW_ALLOC_FAIL:


    //free(param.p_swapchain_data->images);
    free_mem(swapchain_res.value.allocr, images.value.images);
  case CREATE_SWAPCHAIN_IMAGES_IMAGE_ALLOC_FAIL:
    //param.p_swapchain_data->images = NULL;
    //param.p_swapchain_data->img_count = 0;
  case CREATE_SWAPCHAIN_IMAGES_INVALID_SWAPCHAIN:
  case CREATE_SWAPCHAIN_IMAGES_TOP_FAIL_CODE:
    images = (OptSwapchainImages){0};
  }
  return images;
}

OptDepthbufferOut create_depthbuffers(VulkanDevice device,
				      OptSwapchainOut swapchain_res,
				      VkFormat depth_img_format){
  VkResult result = VK_SUCCESS;
  OptDepthbufferOut output = {0};
  if(swapchain_res.code < 0){
    output.code = CREATE_DEPTHBUFFERS_INVALID_SWAPCHAIN;
    return output;
  }

  VkImageSlice depthimgs = SLICE_ALLOC(VkImage,
				       swapchain_res.value.img_count,
				       swapchain_res.value.allocr);
					 
  /* *param.p_depthbuffers = */
  /*         malloc(param.depth_count * sizeof(VkImage)); */
  if (nullptr == depthimgs.data){
    output.code = CREATE_DEPTHBUFFERS_IMAGE_MEM_ALLOC_FAIL;
    return output;
  }
  output.value.depthbuffers = depthimgs.data;
    
  VkImageCreateInfo img_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .extent =
    (VkExtent3D) {
      .depth = 1,
      .width = swapchain_res.value.extent.width,
      .height = swapchain_res.value.extent.height,
    },
    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .mipLevels = 1,
    .arrayLayers = 1,
    .format = depth_img_format,
    .imageType = VK_IMAGE_TYPE_2D,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  //for (size_t i = 0; i < param.depth_count; ++i) {
  for_slice(depthimgs, i){
    //(*param.p_depthbuffers)[i] = VK_NULL_HANDLE;
    depthimgs.data[i] = VK_NULL_HANDLE;
  }
  for_slice(depthimgs, i){
    result = vkCreateImage(device.device, &img_create_info,
			   get_glob_vk_alloc(),
			   depthimgs.data + i);
    if (result != VK_SUCCESS){
      output.code = CREATE_DEPTHBUFFERS_IMAGE_FAIL;

    }
  }
  if(output.code == CREATE_DEPTHBUFFERS_IMAGE_FAIL){
    return output;
  }

  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(device.device, depthimgs.data[0], &mem_reqs);

  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(device.phy_device, &mem_props);

  uint32_t mem_type_inx;
  for (mem_type_inx = 0; mem_type_inx < mem_props.memoryTypeCount;
       ++mem_type_inx) {
    if ((mem_reqs.memoryTypeBits & (1 << mem_type_inx)) &&
	(mem_props.memoryTypes[mem_type_inx].propertyFlags &
	 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      break;
  }

  if (mem_type_inx == mem_props.memoryTypeCount){
    output.code = CREATE_DEPTHBUFFERS_DEVICE_MEM_INAPPROPRIATE;
    return output;
  }

  VkMemoryAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .memoryTypeIndex = mem_type_inx,
    .allocationSize =
    _align_up(mem_reqs.size, mem_reqs.alignment) *
    depthimgs.count
  };

  result = vkAllocateMemory(device.device, &alloc_info,
			    get_glob_vk_alloc(), &output.value.depth_memory);
  if (result != VK_SUCCESS){
    output.code = CREATE_DEPTHBUFFERS_DEVICE_MEM_ALLOC_FAIL;
    return output;
  }

  //for (size_t i = 0; i < param.depth_count; ++i) {
  for_slice(depthimgs, i){
    result = vkBindImageMemory(device.device, depthimgs.data[i],
			       output.value.depth_memory,
			       i *
			       _align_up(mem_reqs.size,
					 mem_reqs.alignment));
    if (result != VK_SUCCESS){
      output.code = CREATE_DEPTHBUFFERS_IMAGE_DEVICE_MEM_BIND_FAIL;
    }
  }

  VkImageViewSlice depth_views = SLICE_ALLOC(VkImageView,
					     depthimgs.count,
					     swapchain_res.value.allocr);
  /* *param.p_depthbuffer_views = */
  /*       malloc(param.depth_count * sizeof(VkImageView)); */
  if (nullptr == depth_views.data){
    output.code = CREATE_DEPTHBUFFERS_IMAGE_VIEW_MEM_ALLOC_FAIL;
    return output;
  }
  output.value.depthbuffer_views = depth_views.data;

  //for (size_t i = 0; i < param.depth_count; ++i) {
  for_slice(depth_views, i){
    depth_views.data[i] = VK_NULL_HANDLE;
  }
  for_slice(depth_views, i){

    VkImageViewCreateInfo view_create = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = depth_img_format,
      .components = {
	0
      },
      .image = depthimgs.data[i],// (*param.p_depthbuffers)[i],
      .subresourceRange = {
	.baseArrayLayer = 0,
	.levelCount = 1,
	.baseMipLevel = 0,
	.layerCount = 1,
	.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
      }
    };

    result = vkCreateImageView(device.device, &view_create,
			       get_glob_vk_alloc(),
			       depth_views.data + i);
    if (result != VK_SUCCESS){
      output.code = CREATE_DEPTHBUFFERS_IMAGE_VIEW_FAIL;
    }
  }

  return output;
}

/* void clear_depthbuffers(const VkAllocationCallbacks *alloc_callbacks, */
/*                         ClearDepthbuffersParam param, int err_code) { */

OptDepthbufferOut clear_depthbuffers(OptDepthbufferOut depth_bufs,
				     VkDevice device, OptSwapchainOut swapchain_res){

  switch (depth_bufs.code) {
  case CREATE_DEPTHBUFFERS_OK:
  case CREATE_DEPTHBUFFERS_IMAGE_VIEW_FAIL:{
    //if (param.p_depthbuffer_views[0])
    VkImageViewSlice depth_views = init_VkImageView_slice
      (depth_bufs.value.depthbuffer_views,
       swapchain_res.value.img_count);
    for_slice(depth_views, i){
      if(VK_NULL_HANDLE != depth_views.data[i]){
	vkDestroyImageView(device, depth_views.data[i],
			   get_glob_vk_alloc());
      }
    }
    SLICE_FREE(depth_views, swapchain_res.value.allocr);
    //free(*param.p_depthbuffer_views);
  }
  case CREATE_DEPTHBUFFERS_IMAGE_VIEW_MEM_ALLOC_FAIL:
  case CREATE_DEPTHBUFFERS_IMAGE_DEVICE_MEM_BIND_FAIL:
  case CREATE_DEPTHBUFFERS_DEVICE_MEM_ALLOC_FAIL:
    vkFreeMemory(device, depth_bufs.value.depth_memory,
		 get_glob_vk_alloc());

  case CREATE_DEPTHBUFFERS_DEVICE_MEM_INAPPROPRIATE:
  case CREATE_DEPTHBUFFERS_IMAGE_FAIL:{
    VkImageSlice depth_imgs = init_VkImage_slice
      (depth_bufs.value.depthbuffers,
       swapchain_res.value.img_count);
    for_slice(depth_imgs, i){
      if(VK_NULL_HANDLE != depth_imgs.data[i])
	vkDestroyImage(device,
		       depth_imgs.data[i],
		       get_glob_vk_alloc());
    }
    SLICE_FREE(depth_imgs, swapchain_res.value.allocr);
  }
  case CREATE_DEPTHBUFFERS_IMAGE_MEM_ALLOC_FAIL:
  case CREATE_DEPTHBUFFERS_INVALID_SWAPCHAIN:
  case CREATE_DEPTHBUFFERS_TOP_FAIL_CODE:
    depth_bufs = (OptDepthbufferOut){0};
    break;
  }
  return depth_bufs;
}


/* int create_framebuffers(StackAllocator *stk_allocr, size_t stk_offset, */
/*                         const VkAllocationCallbacks *alloc_callbacks, */
/*                         CreateFramebuffersParam param) { */
DEF_SLICE(VkFramebuffer);
//DEF_SLICE(VkFence);
OptFramebuffers create_framebuffers(VkDevice device, VkRenderPass compatible_render_pass,
				    OptSwapchainOut swapchain_res,
				    VkImageView* img_views, VkImageView* depth_views){

  VkResult result = VK_SUCCESS;
  OptFramebuffers output = {0};

  if((swapchain_res.code < 0) || (nullptr == img_views) ||
     (nullptr == depth_views) || (VK_NULL_HANDLE == compatible_render_pass)){
    output.code = CREATE_FRAMEBUFFERS_INVALID_INPUT;
    return output;
  }

  VkFramebufferSlice framebuffers = SLICE_ALLOC(VkFramebuffer,
						swapchain_res.value.img_count,
						swapchain_res.value.allocr);
  /* param.p_framebuffers[0] = */
  /*         malloc(param.framebuffer_count * sizeof(VkFramebuffer)); */

  if(nullptr == framebuffers.data){
    output.code = CREATE_FRAMEBUFFERS_FB_ALLOC_FAILED;
    return output;
  }
  output.value.framebuffers = framebuffers.data;

  /* memset(param.p_framebuffers[0], 0, */
  /*        param.framebuffer_count * sizeof(VkFramebuffer)); */

  for_slice(framebuffers, i){
    framebuffers.data[i] = VK_NULL_HANDLE;
  }
    
  //for (int i = 0; i < param.framebuffer_count; ++i) {
  for_slice(framebuffers, i){
    VkImageView img_attachments[] = {img_views[i],
				     depth_views[i]};
    VkFramebufferCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = compatible_render_pass,
      .attachmentCount = _countof(img_attachments),
      .pAttachments = img_attachments,
      .width = swapchain_res.value.extent.width,
      .height = swapchain_res.value.extent.height,
      .layers = 1,
    };
    result = vkCreateFramebuffer(device, &create_info,
				 get_glob_vk_alloc(),
				 framebuffers.data + i);

    if (result != VK_SUCCESS){
      output.code = CREATE_FRAMEBUFFERS_FB_FAILED;
    }
  }

  if (output.code != 0){
    return output;
  }

  OptFences fences = create_fences(swapchain_res.value.allocr,
				   device, swapchain_res.value.img_count);
  /* create_fences(stk_allocr, stk_offset, alloc_callbacks, */
  /*                         (CreateFencesParam) {.device = param.device.device, */
  /*                                 .fences_count = param.p_curr_swapchain_data->img_count, */
  /*                                 .p_fences = &(param.p_curr_swapchain_data->img_fences)}); */

  output.value.fences = fences.value.data;

  if(fences.code < 0){
    if (fences.code == CREATE_FENCES_ALLOC_FAILED){
      output.code = CREATE_FRAMEBUFFERS_FENCE_ALLOC_FAILED;
    }
    else if (fences.code == CREATE_FENCES_FAILED){
      output.code =  CREATE_FRAMEBUFFERS_FENCE_CREATE_FAILED;
    }
    else{
      printf("This should absolutely not happen, so TODO handle this.\n");
    }
  }
  return output;
}

/* void clear_framebuffers(const VkAllocationCallbacks *alloc_callbacks, */
/*                         ClearFramebuffersParam param, int err_codes) ; */

OptFramebuffers clear_framebuffers(OptFramebuffers frames,
				   VkDevice device,
				   OptSwapchainOut swapchain_res){
  switch (frames.code) {
  case CREATE_FRAMEBUFFERS_OK:
      
  case CREATE_FRAMEBUFFERS_FENCE_CREATE_FAILED:
  case CREATE_FRAMEBUFFERS_FENCE_ALLOC_FAILED:{

    OptFences fences = {
      .value = init_VkFence_slice(frames.value.fences, swapchain_res.value.img_count)
    };
    if (frames.code == CREATE_FRAMEBUFFERS_FENCE_ALLOC_FAILED)
      fences.code = CREATE_FENCES_ALLOC_FAILED;
    else if (frames.code == CREATE_FRAMEBUFFERS_FENCE_CREATE_FAILED)
      fences.code = CREATE_FENCES_FAILED;
    fences = clear_fences(swapchain_res.value.allocr, fences, device);
  }	  
  case CREATE_FRAMEBUFFERS_FB_FAILED:{
    VkFramebufferSlice framebuffers = init_VkFramebuffer_slice
      (frames.value.framebuffers, swapchain_res.value.img_count);
    for_slice(framebuffers, i){
      if(VK_NULL_HANDLE != framebuffers.data[i]){

	vkDestroyFramebuffer(device,
			     framebuffers.data[i],
			     get_glob_vk_alloc());
      }
    }

    SLICE_FREE(framebuffers, swapchain_res.value.allocr);
      
  }
  case CREATE_FRAMEBUFFERS_FB_ALLOC_FAILED:
    

  case CREATE_FRAMEBUFFERS_INVALID_INPUT:
  case CREATE_FRAMEBUFFERS_TOP_FAIL_CODE:
    frames = (OptFramebuffers){0};
  }
  return frames;
}

OptSwapchainEntities compose_swapchain_entities(OptSwapchainOut swapchain_res,
						OptSwapchainImages swapchain_imgs,
						OptDepthbufferOut depthbuffers,
						OptFramebuffers framebuffers){
  OptSwapchainEntities output = {0};

  if(swapchain_res.code < 0)
    return (OptSwapchainEntities){.code = COMPOSE_SWAPCHAIN_SWAPCHAIN_ERR};

  output.value.swapchain = swapchain_res.value.swapchain;
  output.value.img_count = swapchain_res.value.img_count;
  output.value.allocr = swapchain_res.value.allocr;

  if(swapchain_imgs.code < 0)
    return (OptSwapchainEntities){.code = COMPOSE_SWAPCHAIN_IMAGES_ERR};

  output.value.images = swapchain_imgs.value.images;
  output.value.img_views = swapchain_imgs.value.img_views;

  if(depthbuffers.code < 0)
    return (OptSwapchainEntities){.code = COMPOSE_SWAPCHAIN_DEPTHBUFFER_ERR};

  output.value.depth_imgs = depthbuffers.value.depthbuffers;
  output.value.depth_views = depthbuffers.value.depthbuffer_views;
  output.value.device_mem = depthbuffers.value.depth_memory;

  if(framebuffers.code < 0)
    return (OptSwapchainEntities){.code = COMPOSE_SWAPCHAIN_FRAMEBUFFER_ERR};

  output.value.img_fences = framebuffers.value.fences;
  output.value.framebuffers = framebuffers.value.framebuffers;

  return output;
}

OptSwapchainOut extract_swapchain_out(OptSwapchainEntities swap_entity){
  OptSwapchainOut output = { 0 };

  output.value.swapchain = swap_entity.value.swapchain;
  output.value.img_count = swap_entity.value.img_count;
  output.value.allocr = swap_entity.value.allocr;

  if(swap_entity.code < 0){
    output.code = CREATE_SWAPCHAIN_FAILED;
  }
  return output;
  
}
OptSwapchainImages extract_swap_images_out(OptSwapchainEntities swap_entity){
  OptSwapchainImages output = {0};
  
  VkImage *images;
  VkImageView *img_views;

  output.value.images = swap_entity.value.images;
  output.value.img_views = swap_entity.value.img_views;

  if(swap_entity.code < 0){
    output.code = CREATE_SWAPCHAIN_IMAGES_INVALID_SWAPCHAIN;
  }
  return output;
}
OptDepthbufferOut extract_swap_depth_out(OptSwapchainEntities swap_entity){
  OptDepthbufferOut output = {0};
  if(swap_entity.code < 0){
    output.code = CREATE_DEPTHBUFFERS_INVALID_SWAPCHAIN;
  }

  output.value.depthbuffers = swap_entity.value.depth_imgs;
  output.value.depthbuffer_views = swap_entity.value.depth_views;
  output.value.depth_memory = swap_entity.value.device_mem;

  return output;
}
OptFramebuffers extract_swap_frames_out(OptSwapchainEntities swap_entity){
  OptFramebuffers output = {0};
  if(swap_entity.code < 0){
    output.code = CREATE_FRAMEBUFFERS_INVALID_INPUT;
  }
  output.value.framebuffers = swap_entity.value.framebuffers;
  output.value.fences = swap_entity.value.img_fences;

  return output;
}
				  
  

/* int recreate_swapchain(StackAllocator *stk_allocr, size_t stk_offset, */
/*                        VkAllocationCallbacks *alloc_callbacks, */
/*                        RecreateSwapchainParam param) { */

/* OptSwapchainEntities recreate_swapchain(AllocInterface allocr, */
/* 					RecreateSwapchainParam param, */
/* 					struct SwapchainEntities* p_old_swapchain_data){ */
OptSwapchainEntities recreate_swapchain(AllocInterface allocr,
					RecreateSwapchainParam param,
					struct SwapchainEntities* p_old_swapchain_data,
					struct SwapchainEntities curr_swapchain_data){
  if (p_old_swapchain_data->swapchain) {
    vkDeviceWaitIdle(param.device.device);

    OptSwapchainOut swapchain_stuff = {
      .value = {
	.swapchain = p_old_swapchain_data->swapchain,
	.img_count = p_old_swapchain_data->img_count,
	//.extent =
	.allocr = p_old_swapchain_data->allocr
      }
    };
    OptSwapchainImages swapchain_imgs = {
      .value = {
	.images = p_old_swapchain_data->images,
	.img_views = p_old_swapchain_data->img_views,
      }
    };
    OptDepthbufferOut depths = {
      .value = {
	.depth_memory = p_old_swapchain_data->device_mem,
	.depthbuffers = p_old_swapchain_data->depth_imgs,
	.depthbuffer_views = p_old_swapchain_data->depth_views,
      }
    };
    OptFramebuffers frames = {
      .value = {
	.framebuffers = p_old_swapchain_data->framebuffers,
	.fences = p_old_swapchain_data->img_fences
      }
    };

	
    frames = clear_framebuffers(frames, param.device.device, swapchain_stuff);
    depths = clear_depthbuffers(depths, param.device.device, swapchain_stuff);
    swapchain_imgs = clear_swapchain_images(swapchain_imgs, param.device.device,
					    swapchain_stuff);
    swapchain_stuff = clear_swapchain(swapchain_stuff, param.device.device);
  }

  //param.p_old_swapchain_data[0] = param.p_new_swapchain_data[0];
  *p_old_swapchain_data = curr_swapchain_data;


  OptSwapchainOut swapchain_stuff =
    create_swapchain(allocr,
		     (CreateSwapchainParam){
		       .device = param.device,
		       .win_width = param.new_win_width,
		       .win_height = param.new_win_height,
		       .surface = param.surface,
		       .old_swapchain = curr_swapchain_data.swapchain,
		       .p_surface_format = param.p_surface_format,
		       .p_min_img_count = param.p_min_img_count
		     });
  OptSwapchainImages swapchain_imgs =
    create_swapchain_images(param.device.device,
			    swapchain_stuff,
			    param.p_surface_format->format);

  OptDepthbufferOut depth_bufs =
    create_depthbuffers(param.device,
			swapchain_stuff,
			param.depth_img_format);

  OptFramebuffers frames =
    create_framebuffers(param.device.device, param.framebuffer_render_pass,
			swapchain_stuff, swapchain_imgs.value.img_views,
			depth_bufs.value.depthbuffer_views);

  OptSwapchainEntities swapchain =
    compose_swapchain_entities(swapchain_stuff, swapchain_imgs,
			       depth_bufs, frames);

  //TODO :: Might have to handle this later elegantly
  if(swapchain.code == CREATE_SWAPCHAIN_SURFACE_FORMAT_CHANGED){
    printf("ERROR : surface format is not supposed to change : inside recreate swapchain\n");
  }
  if(swapchain.code < 0){
    clear_framebuffers(frames,
		       param.device.device,
		       swapchain_stuff);
    clear_depthbuffers(depth_bufs,
		       param.device.device,
		       swapchain_stuff);
    clear_swapchain_images(swapchain_imgs,
			   param.device.device,
			   swapchain_stuff);
    clear_swapchain(swapchain_stuff,
		    param.device.device);

    //param.p_old_swapchain_data[0] = param.p_new_swapchain_data[0];
    *p_old_swapchain_data = curr_swapchain_data;
  
    swapchain = (OptSwapchainEntities){.code = swapchain.code};
    *p_old_swapchain_data = (struct SwapchainEntities){0};
  }
  if(nullptr != param.p_swap_extent){
    *param.p_swap_extent = swapchain_stuff.value.extent;
  }
  return swapchain;
}

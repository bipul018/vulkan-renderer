#pragma once
#include "common-stuff.h"
#include <winnt.h>

//So this will be broken
//Part 1 : creates swapchain only
//Part 2 : takes result of part 1, allocates and creates image views 
//Part 3 : takes result of part 1, allocates and creates depth
//Part 4 : takes result of part 1[ 2 3 ]+ renderpass and creates framebuffers + fences
//Part 5 : Takes result of part 1 2 3 4 and creates swapchain entity
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>
typedef struct VulkanWindow VulkanWindow;
struct VulkanWindow {
  VkSurfaceKHR surface;
  LPSTR class_name;
  HANDLE window_handle;
  int width;
  int height;
};

//Should be created before device, for now, but destroyed later
typedef struct{
  VulkanWindow value;
  enum{
    CREATE_VULKAN_WINDOW_TOP_FAIL_CODE = -0x7fff,
    CREATE_VULKAN_WINDOW_SURFACE_CREATE_FAIL,
    CREATE_VULKAN_WINDOW_WINDOW_CREATE_FAIL,
    CREATE_VULKAN_WINDOW_OK = 0,
  } code;
} OptVulkanWindow;

typedef struct CreateVulkanWindowParams CreateVulkanWindowParams;
struct CreateVulkanWindowParams {
  LPSTR window_name;
  int width;
  int height;
  int posx;
  int posy;
  WNDPROC wnd_proc;
  LPSTR window_class;
  void* proc_data;
};

OptVulkanWindow create_vulkan_window(CreateVulkanWindowParams params, VkInstance vk_instance, int flags);

OptVulkanWindow clear_vulkan_window(OptVulkanWindow vk_window, VkInstance vk_instance);

typedef struct{
  struct{
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
    uint32_t img_count;
    AllocInterface allocr;
  } value;
  enum  {
    CREATE_SWAPCHAIN_TOP_FAIL_CODE = -0x7fff,

    //These are phased out codes
    /* CREATE_SWAPCHAIN_FENCE_CREATE_FAILED, */
    /* CREATE_SWAPCHAIN_FENCE_ALLOC_FAILED, */
    /* CREATE_SWAPCHAIN_DEPTH_IMAGE_VIEW_CREATE_FAIL, */
    /* CREATE_SWAPCHAIN_DEPTH_IMAGE_VIEW_ALLOC_FAIL, */
    /* CREATE_SWAPCHAIN_DEPTH_IMAGE_DEVICE_MEM_ALLOC_FAIL, */
    /* CREATE_SWAPCHAIN_DEPTH_IMAGE_CREATE_FAIL, */
    /* CREATE_SWAPCHAIN_DEPTH_IMAGE_ALLOC_FAIL, */
    /* CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL, */
    /* CREATE_SWAPCHAIN_IMAGE_VIEW_ALLOC_FAIL, */
    /* CREATE_SWAPCHAIN_IMAGE_ALLOC_FAIL, */
    
    CREATE_SWAPCHAIN_IMAGE_COUNT_LOAD_FAIL,
    CREATE_SWAPCHAIN_FAILED,
    CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE,
    CREATE_SWAPCHAIN_CHOOSE_DETAILS_FAIL,
    CREATE_SWAPCHAIN_OK = 0,
    CREATE_SWAPCHAIN_SURFACE_FORMAT_CHANGED,
    CREATE_SWAPCHAIN_MIN_IMG_COUNT_CHANGED,
    CREATE_SWAPCHAIN_FORMAT_AND_COUNT_CHANGED,
  } code;
} OptSwapchainOut;
 

typedef struct {
  struct VulkanDevice device;
  int win_width;
  int win_height;
  VkSurfaceKHR surface;
  VkSwapchainKHR old_swapchain;


  //These two values, if changes upon query, will return a success code that is not 0
  //These two being null shouldnot be a problem though, 
  VkSurfaceFormatKHR *p_surface_format;
  uint32_t *p_min_img_count;

} CreateSwapchainParam;

OptSwapchainOut create_swapchain(AllocInterface allocr, CreateSwapchainParam param);

OptSwapchainOut clear_swapchain(OptSwapchainOut swapchain, VkDevice device);

typedef struct {
  struct{
    VkImage *images;
    VkImageView *img_views;
  } value;

  enum {
    CREATE_SWAPCHAIN_IMAGES_TOP_FAIL_CODE = -0x7fff,
    CREATE_SWAPCHAIN_IMAGES_IMG_VIEW_CREATE_FAIL,
    CREATE_SWAPCHAIN_IMAGES_IMG_VIEW_ALLOC_FAIL,
    CREATE_SWAPCHAIN_IMAGES_IMAGE_ALLOC_FAIL,
    CREATE_SWAPCHAIN_IMAGES_INVALID_SWAPCHAIN,
    CREATE_SWAPCHAIN_IMAGES_OK = 0,
  } code;

} OptSwapchainImages;
OptSwapchainImages create_swapchain_images(VkDevice device, OptSwapchainOut swapchain_res, VkFormat format);
OptSwapchainImages clear_swapchain_images(OptSwapchainImages images, VkDevice device,
				   OptSwapchainOut swapchain_res);

//May later modify it to create a sequence of any arbitrary image
typedef struct {
  VkDevice device;
  VkPhysicalDevice phy_device;
  OptSwapchainOut swapchain_res;
  VkFormat depth_img_format;
} CreateDepthbuffersParam;

typedef struct{
  struct{
    VkImage *depthbuffers;
    VkImageView *depthbuffer_views;
    VkDeviceMemory depth_memory;
  } value;
  enum {
    CREATE_DEPTHBUFFERS_TOP_FAIL_CODE = -0x7fff,
    CREATE_DEPTHBUFFERS_IMAGE_VIEW_FAIL,
    CREATE_DEPTHBUFFERS_IMAGE_VIEW_MEM_ALLOC_FAIL,
    CREATE_DEPTHBUFFERS_IMAGE_DEVICE_MEM_BIND_FAIL,
    CREATE_DEPTHBUFFERS_DEVICE_MEM_ALLOC_FAIL,
    CREATE_DEPTHBUFFERS_DEVICE_MEM_INAPPROPRIATE,
    CREATE_DEPTHBUFFERS_IMAGE_FAIL,
    CREATE_DEPTHBUFFERS_IMAGE_MEM_ALLOC_FAIL,
    
    CREATE_DEPTHBUFFERS_INVALID_SWAPCHAIN,
    CREATE_DEPTHBUFFERS_OK = 0,
  } code;
} OptDepthbufferOut;

OptDepthbufferOut create_depthbuffers(VulkanDevice device,
				      OptSwapchainOut swapchain_res,
				      VkFormat depth_img_format);

OptDepthbufferOut clear_depthbuffers(OptDepthbufferOut depth_bufs,
				     VkDevice device, OptSwapchainOut swapchain_res);

typedef struct{
  struct{
    VkFramebuffer *framebuffers;
    VkFence *fences;
  } value;
  
  enum  {
    CREATE_FRAMEBUFFERS_TOP_FAIL_CODE = -0x7fff,
    
    CREATE_FRAMEBUFFERS_FENCE_CREATE_FAILED,
    CREATE_FRAMEBUFFERS_FENCE_ALLOC_FAILED,

    CREATE_FRAMEBUFFERS_FB_FAILED,
    CREATE_FRAMEBUFFERS_FB_ALLOC_FAILED,
    CREATE_FRAMEBUFFERS_INVALID_INPUT,
    CREATE_FRAMEBUFFERS_OK = 0,
  } code;
} OptFramebuffers;

OptFramebuffers create_framebuffers(VkDevice device, VkRenderPass compatible_render_pass,
				    OptSwapchainOut swapchain_res,
				    VkImageView* img_views, VkImageView* depth_views);

OptFramebuffers clear_framebuffers(OptFramebuffers framebuffers,
				   VkDevice device,
				   OptSwapchainOut swapchain_res);
struct SwapchainEntities {
  VkSwapchainKHR swapchain;

  uint32_t img_count;
  // All following have this img_count elements if successfully
  // created
  
  //Allocator interface used to allocate everything below
  AllocInterface allocr;
  VkImage *images;
  VkImageView *img_views;

  VkDeviceMemory device_mem;
  VkImage *depth_imgs;
  VkImageView *depth_views;

  //Properly record all images status from these fences
  VkFence *img_fences;
  VkFramebuffer *framebuffers;
};

//Tries to compose a swapchain entity if all requirements satisfy,
//Doesn't clear them if fails, so don't destroy input arguments
//Also that's why it doesnot have a direct clear function, like others
typedef struct{
  struct SwapchainEntities value;
  enum {
    COMPOSE_SWAPCHAIN_TOP_FAIL_CODE = -0x7fff,
    COMPOSE_SWAPCHAIN_FRAMEBUFFER_ERR,
    COMPOSE_SWAPCHAIN_DEPTHBUFFER_ERR,
    COMPOSE_SWAPCHAIN_IMAGES_ERR,
    COMPOSE_SWAPCHAIN_SWAPCHAIN_ERR,
    COMPOSE_SWAPCHAIN_OK = 0,
  } code;
} OptSwapchainEntities;

OptSwapchainEntities compose_swapchain_entities(OptSwapchainOut swapchain_res,
						OptSwapchainImages swapchain_imgs,
						OptDepthbufferOut depthbuffers,
						OptFramebuffers framebuffers);

//Performs a simple decompositino of a swapchain entity, considering if it is
//completely successfull or completely unsuccessfull
//Also doesn't extract extent information
OptSwapchainOut extract_swapchain_out(OptSwapchainEntities swap_entity);
OptSwapchainImages extract_swap_images_out(OptSwapchainEntities swap_entity);
OptDepthbufferOut extract_swap_depth_out(OptSwapchainEntities swap_entity);
OptFramebuffers extract_swap_frames_out(OptSwapchainEntities swap_entity);
				  
typedef struct {
  struct VulkanDevice device;
  int new_win_width;
  int new_win_height;
  VkSurfaceKHR surface;
  //Needed for recreating framebuffer {maybe not }
  VkRenderPass framebuffer_render_pass;
  VkFormat depth_img_format;

  //Input and output parameters
  VkSurfaceFormatKHR *p_surface_format;
  uint32_t *p_min_img_count;
  VkExtent2D* p_swap_extent;

} RecreateSwapchainParam;

//Returns the same result as compose swapchain
//Always frees the old swapchain, then assigns the current swapchain to old swapchain
//Tries to create a new swapchain, frees all intermediates if not successfull
//Also doesn't assign current to old if swapchain creation was unsuccessfull
OptSwapchainEntities recreate_swapchain(AllocInterface allocr,
					RecreateSwapchainParam param,
					struct SwapchainEntities* p_old_swapchain_data,
					struct SwapchainEntities curr_swapchain_data);

#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <stdlib.h>

#define UTIL_INCLUDE_ALL 1
#include <util_headers.h>


typedef struct VulkanLayer VulkanLayer;
struct VulkanLayer {
  const char *layer_name;
  bool required;
  bool available;
};
DEF_SLICE(VulkanLayer);
typedef struct VulkanExtension VulkanExtension;
struct VulkanExtension {
  const char *extension_name;
  struct VulkanLayer *layer;
  bool required;
  bool available;
};
DEF_SLICE(VulkanExtension);

//A global vk allocation callbacks pointer getter function for now
const VkAllocationCallbacks* get_glob_vk_alloc();

//Everything is a optional, kind of, with 'value' and 'code'
//But also may take in pointer if kindof unrelated
//0 value of code will be ok normal, >0 will be okay with some message, <0 are errors

//Fallthrough basis of error handling will be attempted,
//so unless error is handled, optional is never unpacked

//The 'clear' functions will return a nulled optional to assign back if needed

typedef struct{
  VkInstance value;
  enum {
    CREATE_INSTANCE_TOP_FAIL_CODE = -0x7fff,
    CREATE_INSTANCE_FAILED ,
    CREATE_INSTANCE_EXTENSION_NOT_FOUND,
    CREATE_INSTANCE_EXTENSION_CHECK_ALLOC_ERROR,
    CREATE_INSTANCE_LAYER_NOT_FOUND,
    CREATE_INSTANCE_LAYER_CHECK_ALLOC_ERROR,
    CREATE_INSTANCE_OK = 0
  } code;
} OptInstance;

//Modifies the order of layers and extensions so that the used up ones are front
OptInstance create_instance(AllocInterface allocr,
			    VulkanLayerSlice instance_layers,
			    VulkanExtensionSlice instance_extensons,
			    void* p_next_chain);
OptInstance clear_instance(OptInstance vk_instance);

typedef struct {
  VkPhysicalDevice phy_device;
  VkSurfaceKHR surface;
  //Optional, if false, then skips assigning results{used to check validity of GPU}  
  bool return_present_mode;
  bool return_extent;
  bool return_format_n_count;
  bool return_transform;
} ChooseSwapchainDetsParam;

typedef struct{
  struct{
    VkSurfaceFormatKHR img_format;
    VkPresentModeKHR present_mode;
    uint32_t min_img_count;
    VkSurfaceTransformFlagBitsKHR transform_flags;
    VkExtent2D surface_extent;
  } value;
  enum  {
    CHOOSE_SWAPCHAIN_DETAILS_TOP_FAIL_CODE = -0x7fff,
    CHOOSE_SWAPCHAIN_DETAILS_INT_ALLOC_ERR,
    CHOOSE_SWAPCHAIN_DETAILS_NO_PRESENT_MODES,
    CHOOSE_SWAPCHAIN_DETAILS_NO_SURFACE_FORMAT,
    CHOOSE_SWAPCHAIN_DETAILS_CAPABILITY_GET_FAIL,
    CHOOSE_SWAPCHAIN_DETAILS_OK = 0
  } code;
} OptSwapchainDetails;

OptSwapchainDetails choose_swapchain_details(AllocInterface allocr, ChooseSwapchainDetsParam param);

typedef struct VulkanQueue VulkanQueue;
struct VulkanQueue {
  //TODO Might need a mutex for multithreaded sync later
  VkQueue vk_obj;
  VkCommandPool im_pool;
  VkCommandBuffer im_buffer;
  VkFence im_fence;
  u32 family;
};

//TODO :: Make a macro to always inittialize the top fail code to -0x7fff and use it everywhere
//And use a macro to default initialize a opt object by setting it's code value to -0x7fff
typedef struct{
  VulkanQueue value;
  enum{
    CREATE_VULKAN_QUEUE_TOP_FAIL_CODE = -0x7fff,
    CREATE_VULKAN_QUEUE_FENCE_CREATE_FAIL,
    CREATE_VULKAN_QUEUE_CMD_BUFFER_FAIL,
    CREATE_VULKAN_QUEUE_CMD_POOL_FAIL,
    CREATE_VULKAN_QUEUE_GET_FAIL,
    CREATE_VULKAN_QUEUE_OK = 0,
  } code;
}OptVulkanQueue;

OptVulkanQueue create_vulkan_queue(VkDevice device, u32 family_inx, u32 queue_inx);
OptVulkanQueue clear_vulkan_queue(OptVulkanQueue queue, VkDevice device);

typedef struct VulkanDevice VulkanDevice;
struct VulkanDevice {
  VkPhysicalDevice phy_device;
  VkDevice device;

  VulkanQueue graphics;
  VulkanQueue present;
  VulkanQueue compute;
};

typedef struct {
  VkInstance vk_instance;
  //TODO:: Need to decouple this surface from device, some external way of
  // managing features needed
  
  VkFormat* p_depth_stencil_format;
  VkSurfaceFormatKHR* p_img_format;
  uint32_t* p_min_img_count;
} CreateDeviceParam;

typedef struct{
  //struct{
    struct VulkanDevice value;
    //} value;
  enum {
    CREATE_DEVICE_TOP_FAIL_CODE = -0x7fff,
    CREATE_DEVICE_QUEUE_ITEM_FAIL,
    CREATE_DEVICE_FAILED ,
    CREATE_DEVICE_PHY_DEVICE_INT_ALLOC_FAIL,
    CREATE_DEVICE_NO_SUITABLE_GPU,
    CREATE_DEVICE_PHY_DEVICE_TEST_ALLOC_FAIL,
    CREATE_DEVICE_NO_GPU,
    CREATE_DEVICE_OK = 0,
  } code;
} OptVulkanDevice;
//TODO For now no feature availability check is done, fix that
OptVulkanDevice create_device(const AllocInterface allocr,
			      CreateDeviceParam param,
			      VulkanLayerSlice device_layers,
			      VulkanExtensionSlice device_extensions,
			      void* p_next_chain);

OptVulkanDevice clear_device(OptVulkanDevice vk_device);

typedef struct{
  VkCommandPool value;
  enum  {
    CREATE_COMMAND_POOL_FAILED = -0x7fff,
    CREATE_COMMAND_POOL_OK = 0
  } code;
} OptCommandPool;

OptCommandPool create_command_pool(VkDevice device,
				   uint32_t queue_family_inx);

OptCommandPool clear_command_pool(OptCommandPool cmd_pool, VkDevice device);

// Helpers for immediate command submission on a queue
//Only call end iff begin was a success
bool immediate_command_begin(VkDevice device, VulkanQueue queue);
bool immediate_command_end(VkDevice device, VulkanQueue queue);


//Choose an image format if supported, returns VK_FORMAT_UNDEFINED if fails
DEF_SLICE(VkFormat);
VkFormat choose_image_format(VkPhysicalDevice phy_device,
			     VkFormatSlice format_candidates,
                             VkImageTiling img_tiling, VkFormatFeatureFlags format_flags);

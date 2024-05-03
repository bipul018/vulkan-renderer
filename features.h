#pragma once
#include "common-stuff.h"


//A header only file specifying instance and device features to enable, pass this on to the
//instance and device creation functions / or use these there directly??

//instance features
VkDebugUtilsMessengerCreateInfoEXT debug_callbacks = {
  .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
  .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
  VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT|
  VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
  .messageType= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|
  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
};
VkValidationFeaturesEXT validation_features = {
  .sType=VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
  .enabledValidationFeatureCount = 1,
  .pEnabledValidationFeatures =
  (VkValidationFeaturesEXT []){
    VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT
  },
  .pNext = &debug_callbacks
};

void* instance_features_ptr = &validation_features;


//device features helpers, for now only core features, no extensions
#define MAKE_DEVICE_FEATURES()						\
  &(VkPhysicalDeviceFeatures2){						\
  .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,		\
    .pNext = &(VkPhysicalDeviceVulkan11Features){			\
  .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,	\
  .pNext = &(VkPhysicalDeviceVulkan12Features){				\
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,	\
    .pNext = &(VkPhysicalDeviceVulkan13Features){			\
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,	\
    }}}}

#define FEATURE_STRUCT_1_0(feature_chain)	\
  ((VkPhysicalDeviceFeatures2*)(feature_chain))

#define FEATURE_STRUCT_1_1(feature_chain)	\
  ((VkPhysicalDeviceVulkan11Features*)FEATURE_STRUCT_1_0(feature_chain)->pNext)

#define FEATURE_STRUCT_1_2(feature_chain)	\
  ((VkPhysicalDeviceVulkan12Features*)FEATURE_STRUCT_1_1(feature_chain)->pNext)

#define FEATURE_STRUCT_1_3(feature_chain)	\
  ((VkPhysicalDeviceVulkan13Features*)FEATURE_STRUCT_1_2(feature_chain)->pNext)

#define GET_FEATURE_1_0(feature_chain, feature)\
  FEATURE_STRUCT_1_0(feature_chain)->features.feature

#define GET_FEATURE_1_1(feature_chain, feature) \
  FEATURE_STRUCT_1_1(feature_chain)->feature

#define GET_FEATURE_1_2(feature_chain, feature)	\
  FEATURE_STRUCT_1_2(feature_chain)->feature

#define GET_FEATURE_1_3(feature_chain, feature)	\
  FEATURE_STRUCT_1_3(feature_chain)->feature








#pragma once
#include "device-mem-stuff.h"
#include "common-stuff.h"
#include "render-stuff.h"

DEF_SLICE(bool);
DEF_SLICE(MemoryItem);
DEF_SLICE(MemoryItemSlice);

typedef struct ComputeJobParam ComputeJobParam;
struct ComputeJobParam {
  VkDevice device;
  VulkanQueue* compute_queue;
  GPUAllocr* gpu_allocr;
  AllocInterface allocr;
};

typedef struct ComputeJobInitOutput ComputeJobInitOutput;
struct ComputeJobInitOutput {
  MemoryItemSlice* main_items;
  VkFormatSlice* main_item_img_formats;
  u32Slice* main_item_usages;
  void* internal_data;
};

//A fxn type to initialize the compute job, assumes failure if nullptr in any of the list returned
typedef ComputeJobInitOutput InitJobFnT(void* init_params, ComputeJobParam* params);

//A fxn pointer to the fxn that sets up and runs the compute job
// has to return a command buffer created upon compute queue family, given in init
typedef VkCommandBuffer RunJobFnT(void* pdata, ComputeJobParam* params);

//A fxn type to clean the compute job
typedef void CleanJobFnT(void* pdata, ComputeJobParam* params);

//The things are allocated by external thing,
//Internal things are in void* {maybe}
typedef struct ComputeJob ComputeJob;
struct ComputeJob {
  AllocInterface allocr;
  GPUAllocr* gpu_allocr;
  VulkanDevice* device;
  void* job_data;

  InitJobFnT* init_job_fn;
  RunJobFnT* run_job_fn;
  CleanJobFnT* clean_job_fn;
  
  VkCommandPool compute_pool;
  VkCommandPool graphics_pool;
  
  //All items should be either image or gpu buffer
  MemoryItemSlice* main_items;
  //If for index i in main_items, there is an image, main_item_img_format has on index i
  // the same image format used in creating the original image
  VkFormatSlice* main_item_img_formats;
  u32Slice* main_item_usages;
  
  MemoryItemSlice buffered_items;
  MemoryItemSliceSlice render_side_items;

  //Command buffers on compute side to release/acquire ownership
  VkCommandBuffer release_cmd_buf;
  VkCommandBuffer acquire_cmd_buf;

  //Command buffer on graphics side to copy from main to their double buffered counterparts
  VkCommandBuffer copy_main_cmd_buf;
  //Command buffer on graphics side, associated per frame to copy from double buffered
  // to per frame resources
  VkCommandBufferSlice frame_copy_cmd_buf;

  //Fence to be signaled by compute job
  VkFence signal_fence;
  //Semaphore for signaling release of ownership
  VkSemaphore release_sema;
  //Semaphore to signal to take back ownership
  VkSemaphore acquire_sema;
  //Semaphore the compute job has to wait upon before starting
  VkSemaphore signal_sema;

  //These are for copying from main to buffered counterparts
  //frame_count semaphores that copying from src to buffered counterpart wait on
  VkSemaphoreSlice copy_wait_sema;
  //frame_count semaphores that copying from src to buffered counterpart signal
  VkSemaphoreSlice copy_signal_sema;

  //frame_count semaphores, one for each to signal the rendering command buffer
  VkSemaphoreSlice render_signal_sema;

  //A 'cache invalidation' like signal, one per frame
  boolSlice is_res_valid;

  bool first_time;

  enum{
    INIT_COMPUTE_JOB_TOP_FAIL_CODE = -0x7fff,
    INIT_COMPUTE_JOB_COPY_RECORD_FAIL,
    INIT_COMPUTE_JOB_COPY_START_FAIL,
    INIT_COMPUTE_JOB_INVALIDATION_ARRAY_FAIL,
    INIT_COMPUTE_JOB_RESOURCE_ALLOC_FAIL,
    INIT_COMPUTE_JOB_RESOURCE_BUFFER_ALLOC_FAIL,
    INIT_COMPUTE_JOB_RENDER_SEMA_FAIL,
    INIT_COMPUTE_JOB_COPY_SIGNAL_SEMA_FAIL,
    INIT_COMPUTE_JOB_COPY_WAIT_SEMA_FAIL,
    INIT_COMPUTE_JOB_SINGLE_SEMAPHORE_FAIL,
    INIT_COMPUTE_JOB_FENCE_FAIL,
    INIT_COMPUTE_JOB_CMD_BUF_FAIL,
    INIT_COMPUTE_JOB_CMD_POOL_FAIL,
    INIT_COMPUTE_JOB_JOB_INIT_FAIL,
    INIT_COMPUTE_JOB_OK = 0,
  }code;
};

ComputeJob init_compute_job(GPUAllocr* gpu_allocr, AllocInterface allocr,
			    VulkanDevice* device, u32 max_frames,
			    void* job_init_param,
			    InitJobFnT* init_job_fn,
			    RunJobFnT* run_job_fn,
			    CleanJobFnT* clean_job_fn);

typedef struct {
  VkSemaphore sema;
  bool was_failed;
} RunComputeOutput;
RunComputeOutput check_run_compute_job(AllocInterface allocr,
				       ComputeJob* job_data,
				       u32 curr_frame);

void clean_compute_job(ComputeJob* job);

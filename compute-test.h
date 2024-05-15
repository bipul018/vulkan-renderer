#pragma once
#include "device-mem-stuff.h"
#include "common-stuff.h" 
#include "compute-interface.h"

ComputeJobInitOutput init_test_compute(void* any_init_param, ComputeJobParam* params);
VkCommandBuffer run_test_compute(void* pdata, ComputeJobParam* params);
void clean_test_compute(void* pdata, ComputeJobParam* params );



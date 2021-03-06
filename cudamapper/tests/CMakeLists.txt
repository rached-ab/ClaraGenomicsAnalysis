#
# Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

set(TARGET_NAME cudamappertests)

set(SOURCES
    main.cpp
    Test_CudamapperIndexBatcher.cu
    Test_CudamapperIndexCache.cu
    Test_CudamapperIndexDescriptor.cpp
    Test_CudamapperIndexGPU.cu
    Test_CudamapperMatcherGPU.cu
    Test_CudamapperMinimizer.cpp
    Test_CudamapperOverlapper.cpp
    Test_CudamapperOverlapperTriggered.cu
    Test_CudamapperUtilsKmerFunctions.cpp
   )

get_property(cudamapper_data_include_dir GLOBAL PROPERTY cudamapper_data_include_dir)
include_directories(${cudamapper_data_include_dir})

set(LIBS cudamapper)

cga_add_tests(${TARGET_NAME} "${SOURCES}" "${LIBS}")

#include "cuda_grid_map.h"

#include <string>
#include <iostream>

#include <stb_image_write.h>
#include "cuda_utils.h"

CudaGridMap::CudaGridMap(unsigned int width, unsigned int height, cudaChannelFormatDesc channel_description)
{
    m_width = width;
    m_height = height;
    m_channel_description = channel_description;

	//Allocate arrays.
	HANDLE_ERROR(cudaMallocArray(&m_vectors.cuda_array, &channel_description, width, height, cudaArraySurfaceLoadStore));

	//Create resource descriptions.
	cudaResourceDesc res_desc;
	memset(&res_desc, 0, sizeof(res_desc));
	res_desc.resType = cudaResourceTypeArray;

	//Create CUDA Surface objects
	res_desc.res.array.array = m_vectors.cuda_array;
	HANDLE_ERROR(cudaCreateSurfaceObject(&m_vectors.surface_object, &res_desc));
}

CudaGridMap::~CudaGridMap()
{
	HANDLE_ERROR(cudaDestroySurfaceObject(m_vectors.surface_object));
	HANDLE_ERROR(cudaFreeArray(m_vectors.cuda_array));
}

std::array<unsigned int, 2> CudaGridMap::getGridDims() const
{
    return { m_width, m_height };
}

cudaSurfaceObject_t CudaGridMap::getCudaSurfaceObject() const
{
    return m_vectors.surface_object;
}

cudaArray* CudaGridMap::getCudaArray() const
{
    return m_vectors.cuda_array;
}

std::array<CudaGridMap, 3> CudaGridMap::create3LayerPyramid(unsigned int width, unsigned int height, cudaChannelFormatDesc channel_description)
{
    bool cannot_construct_pyramid = ((width % 4) != 0) || ((height % 4) != 0);
    if (cannot_construct_pyramid)
    {
        throw std::runtime_error{ "Can't construct pyramid. Dimensions must be divisible by 4." };
    }

    CudaGridMap high_resolution_map(width, height, channel_description);
    CudaGridMap medium_resolution_map(width / 2, height / 2, channel_description);
    CudaGridMap low_resolution_map(width / 4, height / 4, channel_description);
    
    return { high_resolution_map, medium_resolution_map, low_resolution_map };
}
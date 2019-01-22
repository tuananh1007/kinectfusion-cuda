#include "pch.h"

#include <cuda_runtime.h>
#include <cublas.h>

#include "rigid_transform_3d.h"
#include "icp.h"
#include "cuda_wrapper.cuh"
#include "icp.cuh"
#include "cuda_utils.h"


class IcpTests : public ::testing::Test
{
protected:
    const double pi = 3.14159265358979323846;
    int width = 2;
    int height = 2;
    int n_iterations = 2;
    cudaChannelFormatDesc format_description = cudaCreateChannelDesc(32, 32, 32, 32, cudaChannelFormatKindFloat);
    std::vector<unsigned int> iters_per_layer = { 1, 2, 3 };
    std::vector<void *> cuda_pointers_to_free;
    std::streambuf* oldCoutStreamBuf = std::cout.rdbuf();
    
    // 90 degree turn to the right
    glm::mat3x3 rotation_mat = glm::mat3x3(
        glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(1.0f,  0.0f, 0.0f),
        glm::vec3(0.0f,  0.0f, 1.0f));

    virtual void TearDown()
    {
        for (const auto ptr : cuda_pointers_to_free)
        {
            cudaFree(ptr);
        }

        std::cout.rdbuf(oldCoutStreamBuf);
    }
};

TEST_F(IcpTests, TestInitialization)
{
    RigidTransform3D transform;

    ICP icp(transform, iters_per_layer, 4, 4, 1.0, 1.0);
}

TEST_F(IcpTests, TestComputeCorrespondence)
{
    std::array<glm::vec3, 4> vertices = { { { 1.0,  1.0, 3.0 },
                                            { 2.0,  2.0, 6.0 },
                                            {-1.0,  1.0, 3.0 },
                                            {-3.0, -3.0, 4.0 } } };
    glm::mat3x3 intrinsics(
        glm::vec3(2.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 2.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f));
    
    glm::vec3 translation_vec(0.0);
    
    std::array<std::array<int, 2>, 4> true_pixel_coordinates = { { { 0,  1 },
                                                                   { 0,  1 },
                                                                   { 0,  0 },
                                                                   { 2, -1 } } };
    
    std::array<glm::vec2, 4> coordinates;
    for (int i = 0; i < 4; i++)
    {
        coordinates[i] = computeCorrespondenceTestWrapper(vertices[i], rotation_mat, translation_vec, intrinsics);
    }

    for (int i = 0; i < 4; i++)
    {
        ASSERT_EQ(true_pixel_coordinates[i][0], coordinates[i][0]);
        ASSERT_EQ(true_pixel_coordinates[i][1], coordinates[i][1]);
    }
}

TEST_F(IcpTests, TestNormalsAreTooDifferent)
{
    // Vectors in x-y-plane
    glm::vec3 target_normal(1.0, 0.0, 0.0);

    // The following vectors will be turned by 90 degrees to the right
    glm::vec3 normal_close_enough(0.0, 1.0, 0.0);
    glm::vec3 normal_too_different(0.0, -1.0, 0.0);
    
    float angle_thresh = pi / 2;

    bool close = normalsAreTooDifferentTestWrapper(normal_close_enough, target_normal, rotation_mat, angle_thresh);
    bool far_off = normalsAreTooDifferentTestWrapper(normal_too_different, target_normal, rotation_mat, angle_thresh);

    ASSERT_FALSE(close);
    ASSERT_TRUE(far_off);
}

TEST_F(IcpTests, TestComputeAndFillA)
{
    glm::vec3 vertex(2.0f, 3.0f, 1.0f);
    glm::vec3 normal(1.0f, 4.0f, 2.0f);

    std::array<float, 6> true_mat_a = { -2, 3, -5, 1, 4, 2 };

    std::array<float, 6> mat_a;
    computeAndFillATestWrapper(&mat_a, vertex, normal);

    for (int i = 0; i < 6; i++)
    {
        ASSERT_FLOAT_EQ(true_mat_a[i], mat_a[i]);
    }
}

TEST_F(IcpTests, TestComputeAndFillB)
{
    glm::vec3 vertex(2.0f, 3.0f, 1.0f);
    glm::vec3 target_normal(1.0f, 4.0f, 2.0f);
    glm::vec3 target_vertex(1.0f, 2.0f, 0.0f);

    float true_b = -7.0;

    float b = computeAndFillBTestWrapper(vertex, target_vertex, target_normal);

    ASSERT_FLOAT_EQ(true_b, b);
}

TEST_F(IcpTests, TestSolveLinearSystem)
{
    std::array<std::array<float, 6>, 7> mat_a = { { { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
                                                    { 0.0, 2.0, 0.0, 0.0, 0.0, 0.0 },
                                                    { 0.0, 0.0, 3.0, 0.0, 0.0, 0.0 },
                                                    { 0.0, 0.0, 0.0, 4.0, 0.0, 0.0 },
                                                    { 0.0, 0.0, 0.0, 0.0, 5.0, 0.0 },
                                                    { 0.0, 0.0, 0.0, 0.0, 0.0, 6.0 },
                                                    { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 } } };
    std::array<std::array<float, 6>, 7> *mat_a_device;
    HANDLE_ERROR(cudaMalloc(&mat_a_device, sizeof(std::array<std::array<float, 6>, 7>)));
    cuda_pointers_to_free.push_back(mat_a_device);
    HANDLE_ERROR(cudaMemcpy(mat_a_device, &mat_a, sizeof(std::array<std::array<float, 6>, 7>), cudaMemcpyHostToDevice));

    std::array<float, 7> vec_b = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 6.0 };
    std::array<float, 7> *vec_b_device;
    HANDLE_ERROR(cudaMalloc(&vec_b_device, sizeof(std::array<float, 7>)));
    cuda_pointers_to_free.push_back(vec_b_device);
    HANDLE_ERROR(cudaMemcpy(vec_b_device, &vec_b, sizeof(std::array<float, 7>), cudaMemcpyHostToDevice));

    std::array<float, 6> *result_device;
    HANDLE_ERROR(cudaMalloc(&result_device, sizeof(std::array<float, 6>)));
    cuda_pointers_to_free.push_back(vec_b_device);

    solveLinearSystem((float *)mat_a_device, (float *)vec_b_device, 7, (float *)result_device);

    std::array<float, 6> result_host;
    HANDLE_ERROR(cudaMemcpy(&result_host, result_device, sizeof(std::array<float, 6>), cudaMemcpyDeviceToHost));

    for (int i = 0; i < 6; i ++)
    {
        ASSERT_NEAR(1.0, result_host[i], 0.0001);
    }
}

TEST_F(IcpTests, TestConstructIcpResiduals)
{
    std::array<std::array<float, 4>, 4> target_vertices = { { { 1.0,  1.0, 3.0, -1.0 },
                                                              { 1.0, -1.0, 3.0, -2.0 },
                                                              {-1.0,  1.0, 3.0, -2.0 },
                                                              {-1.0, -1.0, 3.0, -2.0 } } };
    CudaGridMap target_vertex_map(2, 2, format_description);
    int n_bytes = 16 * 2 * 2;
    HANDLE_ERROR(cudaMemcpyToArray(target_vertex_map.getCudaArray(), 0, 0, &target_vertices[0][0], n_bytes,
        cudaMemcpyHostToDevice));

    std::array<std::array<float, 4>, 4> target_normals = { { { 0.0,  0.0, -1.0, 0.0 },
                                                             { 0.0,  0.0, -1.0, 0.0 },
                                                             { 0.0,  0.0, -1.0, 0.0 },
                                                             { 0.0,  0.0, -1.0, 0.0 } } };
    CudaGridMap target_normal_map(2, 2, format_description);
    HANDLE_ERROR(cudaMemcpyToArray(target_normal_map.getCudaArray(), 0, 0, &target_normals[0][0], n_bytes,
        cudaMemcpyHostToDevice));

    std::array<std::array<float, 4>, 4> vertices = { { { 1.0,  1.0, 4.0, -1.0 },
                                                       { 1.0, -1.0, 4.0, -2.0 },
                                                       {-1.0,  1.0, 5.0, -2.0 },
                                                       {-1.0, -1.0, 4.0, -2.0 } } };
    CudaGridMap vertex_map(2, 2, format_description);
    HANDLE_ERROR(cudaMemcpyToArray(vertex_map.getCudaArray(), 0, 0, &vertices[0][0], n_bytes, cudaMemcpyHostToDevice));

    glm::mat3x3 no_rotation(
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f));
    glm::vec3 no_translation(0.0);

    glm::mat3x3 intrinsics(
        glm::vec3(2.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 2.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f));

    float distance_threshold = 1.5;
    float angle_threshold = pi;

    std::array<std::array<float, 6>, 4> *mat_a_device;
    std::array<float, 4> *vec_b_device;
    HANDLE_ERROR(cudaMalloc(&mat_a_device, sizeof(std::array<std::array<float, 6>, 4>)));
    HANDLE_ERROR(cudaMalloc(&vec_b_device, sizeof(std::array<float, 4>)));

    std::array<std::array<float, 6>, 4> true_mat_a = { { { 1.0, -1.0,  0.0, 0.0, 0.0, -1.0 },
                                                         { 0.0,  0.0,  0.0, 0.0, 0.0,  0.0 },
                                                         { 0.0,  0.0,  0.0, 0.0, 0.0,  0.0 },
                                                         { 0.0,  0.0,  0.0, 0.0, 0.0,  0.0 } } };
    std::array<float, 4> true_vec_b = { 1.0, 0.0, 0.0, 0.0 };

    std::cout.rdbuf(std::cerr.rdbuf());

    kernel::constructIcpResiduals(vertex_map, target_vertex_map, target_normal_map, no_rotation, no_translation,
        no_rotation, no_translation, intrinsics, distance_threshold, angle_threshold, &(*mat_a_device)[0],
        (float*)vec_b_device);

    HANDLE_ERROR(cudaDeviceSynchronize());

    std::array<std::array<float, 6>, 4> mat_a;
    std::array<float, 4> vec_b;
    HANDLE_ERROR(cudaMemcpy(&mat_a, mat_a_device, sizeof(std::array<std::array<float, 6>, 4>), cudaMemcpyDeviceToHost));
    HANDLE_ERROR(cudaMemcpy(&vec_b, vec_b_device, sizeof(std::array<float, 4>), cudaMemcpyDeviceToHost));
    
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 6; j++)
        {
            ASSERT_FLOAT_EQ(true_mat_a[i][j], mat_a[i][j]);
        }
        ASSERT_FLOAT_EQ(true_vec_b[i], vec_b[i]);
    }
}

TEST_F(IcpTests, TestMatrixMatrixMultiply)
{
    std::array<std::array<float, 6>, 2> mat_a = { { { 1.0, 2.0, 0.0, 1.0, 0.0, 2.0 },
                                                    { 0.0, 1.0, 2.0, 1.0, 0.0, 0.0 } } };
    
    std::array<std::array<float, 6>, 2> *mat_a_device;
    HANDLE_ERROR(cudaMalloc(&mat_a_device, sizeof(std::array<std::array<float, 6>, 2>)));
    cuda_pointers_to_free.push_back(mat_a_device);
    HANDLE_ERROR(cudaMemcpy(mat_a_device, &mat_a, sizeof(std::array<std::array<float, 6>, 2>), cudaMemcpyHostToDevice));

    std::array<std::array<float, 6>, 6> true_result = { { { 1.0, 2.0, 0.0, 1.0, 0.0, 2.0 },
                                                          { 2.0, 5.0, 2.0, 3.0, 0.0, 4.0 },
                                                          { 0.0, 2.0, 4.0, 2.0, 0.0, 0.0 },
                                                          { 1.0, 3.0, 2.0, 2.0, 0.0, 2.0 },
                                                          { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
                                                          { 2.0, 4.0, 0.0, 2.0, 0.0, 4.0 } } };

    std::array<std::array<float, 6>, 6> *result_device;
    HANDLE_ERROR(cudaMalloc(&result_device, sizeof(std::array<std::array<float, 6>, 6>)));
    cuda_pointers_to_free.push_back(result_device);

    cudaMatrixMatrixMultiplication((float *)mat_a_device, (float *)mat_a_device, (float *)result_device, 2, CUBLAS_OP_T);

    std::array<std::array<float, 6>, 6> result_host;
    HANDLE_ERROR(cudaMemcpy(&result_host, result_device, sizeof(std::array<std::array<float, 6>, 6>), 
        cudaMemcpyDeviceToHost));

    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < 6; j++)
        {
            ASSERT_FLOAT_EQ(true_result[i][j], result_host[i][j]);
        }
    }
}

TEST_F(IcpTests, TestMatrixVectorMultiply)
{
    std::array<std::array<float, 6>, 2> mat_a = { { { 1.0, 2.0, 0.0, 1.0, 0.0, 2.0 },
                                                    { 0.0, 1.0, 2.0, 1.0, 0.0, 0.0 } } };

    std::array<std::array<float, 6>, 2> *mat_a_device;
    HANDLE_ERROR(cudaMalloc(&mat_a_device, sizeof(std::array<std::array<float, 6>, 2>)));
    cuda_pointers_to_free.push_back(mat_a_device);
    HANDLE_ERROR(cudaMemcpy(mat_a_device, &mat_a, sizeof(std::array<std::array<float, 6>, 2>), cudaMemcpyHostToDevice));

    std::array<float, 2> vec_b = { 1, 1 };
    std::array<float, 2> *vec_b_device;
    HANDLE_ERROR(cudaMalloc(&vec_b_device, sizeof(std::array<float, 2>)));
    cuda_pointers_to_free.push_back(vec_b_device);
    HANDLE_ERROR(cudaMemcpy(vec_b_device, &vec_b, sizeof(std::array<float, 2>), cudaMemcpyHostToDevice));

    std::array<float, 6> true_result = { 1.0, 3.0, 2.0, 2.0, 0.0, 2.0 };
                                                          

    std::array<float, 6> *result_device;
    HANDLE_ERROR(cudaMalloc(&result_device, sizeof(std::array<float, 6>)));
    cuda_pointers_to_free.push_back(result_device);

    cudaMatrixVectorMultiplication((float *)mat_a_device, (float *)vec_b_device, (float *)result_device, 2);

    std::array<float, 6> result_host;
    HANDLE_ERROR(cudaMemcpy(&result_host, result_device, sizeof(std::array<float, 6>), cudaMemcpyDeviceToHost));

    for (int i = 0; i < 6; i++)
    {
        ASSERT_FLOAT_EQ(true_result[i], result_host[i]);
    }
}
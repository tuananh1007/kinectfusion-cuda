#include "glm_macro.h"
#include <glm/gtx/transform.hpp>
#include <glm/glm.hpp>

#include "kinect_fusion.h"

int main()
{
    static constexpr float pi = 3.14159265358979323846f;

    KinectFusionConfig config_dataset = {};
    config_dataset.use_kinect = false;
    config_dataset.verbose = true;
    config_dataset.compute_pose_error = true;
    config_dataset.use_static_view = false;
	config_dataset.use_shading = false;
    config_dataset.dataset_dir = "rgbd_dataset_freiburg1_xyz";
    config_dataset.depth_scale = 1.0f / 5000.0f;
    config_dataset.frame_width = 640;
    config_dataset.frame_height = 480;
    config_dataset.field_of_view = 525.0f;
    config_dataset.voxel_grid_size = 4.0f;
    config_dataset.voxel_grid_resolution = 512;
    config_dataset.static_viewpoint = glm::rotate(- pi / 36.0f, glm::vec3(1.0f, 0.0f, 0.0f));
    config_dataset.static_viewpoint[3] = glm::vec4(0.0f, -0.2f, -1.8f, 1.0f);
    config_dataset.initial_pose = glm::mat4x4(1.0f);
    config_dataset.initial_pose[3] = glm::vec4(0.0, 0.0, -1.0, 1.0);

    KinectFusionConfig config_kinect = {};
    config_kinect.use_kinect = true;
    config_kinect.verbose = false;
    config_kinect.use_static_view = false;
    config_kinect.load_mode = false;
    config_kinect.depth_scale = 1.0f / 1000.0f;
    config_kinect.frame_width = 640;
    config_kinect.frame_height = 480;
    config_kinect.field_of_view = 571.0f;
    config_kinect.voxel_grid_size = 5.5f;
    config_kinect.voxel_grid_resolution = 400;
    config_kinect.static_viewpoint = glm::rotate(-pi / 36.0f, glm::vec3(1.0f, 0.0f, 0.0f));
    config_kinect.static_viewpoint[3] = glm::vec4(0.0f, -0.2f, 0.0f, 1.0f);
    config_kinect.initial_pose = glm::mat4x4(1.0f);
    //config_kinect.initial_pose[3] = glm::vec4(0.0, 0.0, -0.5, 1.0);

    IcpConfig icp_config = {};
    icp_config.iters_per_layer = {10, 4, 2};
    icp_config.width = 640;
    icp_config.height = 480;
    icp_config.distance_thresh = 0.4f;
    icp_config.angle_thresh = 5.0f * pi / 18.0f;
    icp_config.iteration_stop_thresh_angle = 1.0f;
	icp_config.iteration_stop_thresh_distance = 0.01f;

    KinectFusion awesome(config_dataset, icp_config);
    awesome.startTracking(790);
    //awesome.loadTSDF("kitchen_detail_1.bin");
    //awesome.saveTSDF("kitchen.bin");
    awesome.walk();

	/*std::vector<unsigned int> iters_per_layer[4] = { { 1, 1, 1 }, { 10, 4, 2 }, { 10, 0, 0 }, { 0, 10, 0 } };
	float distance_thresh[3] = { 0.2f, 0.4f, 0.6f };
	float angle_thresh[3] = { 3.0f * (pi / 18.0f), 6.0f * (pi / 18.0f), 9.0f * (pi / 18.0f) };

	for (int layer_index = 0; layer_index < 4; layer_index++)
	{
		for (int dist_index = 0; dist_index < 3; dist_index++)
		{
			for (int angle_index = 0; angle_index < 3; angle_index++)
			{
				icp_config.iters_per_layer = iters_per_layer[layer_index];
				icp_config.distance_thresh = distance_thresh[dist_index];
				icp_config.angle_thresh = angle_thresh[angle_index];
				KinectFusion awesome(config_dataset, icp_config);
				awesome.startTracking(790);
			}
		}
	}*/
	
}
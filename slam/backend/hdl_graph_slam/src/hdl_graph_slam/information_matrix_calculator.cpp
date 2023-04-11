// SPDX-License-Identifier: BSD-2-Clause

#include <hdl_graph_slam/information_matrix_calculator.hpp>

#include <pcl/search/kdtree.h>
#include <pcl/common/transforms.h>

namespace hdl_graph_slam {

InformationMatrixCalculator::InformationMatrixCalculator() {
  use_const_inf_matrix = false;
  const_stddev_x = 0.5;
  const_stddev_q = 0.1;

  var_gain_a = 20.0;
  min_stddev_x = 0.1;
  max_stddev_x = 5.0;
  min_stddev_q = 0.05;
  max_stddev_q = 0.2;
  fitness_score_thresh = 0.5;
}

InformationMatrixCalculator::~InformationMatrixCalculator() {}

Eigen::MatrixXd InformationMatrixCalculator::calc_information_matrix(const pcl::PointCloud<PointT>::ConstPtr& cloud1, const pcl::PointCloud<PointT>::ConstPtr& cloud2, const Eigen::Isometry3d& relpose) const {
  if(use_const_inf_matrix) {
    Eigen::MatrixXd inf = Eigen::MatrixXd::Identity(6, 6);
    inf.topLeftCorner(3, 3).array() /= const_stddev_x;
    inf.bottomRightCorner(3, 3).array() /= const_stddev_q;
    return inf;
  }

  double fitness_score = calc_fitness_score(cloud1, cloud2, relpose);

  double min_var_x = std::pow(min_stddev_x, 2);
  double max_var_x = std::pow(max_stddev_x, 2);
  double min_var_q = std::pow(min_stddev_q, 2);
  double max_var_q = std::pow(max_stddev_q, 2);

  float w_x = weight(var_gain_a, fitness_score_thresh, min_var_x, max_var_x, fitness_score);
  float w_q = weight(var_gain_a, fitness_score_thresh, min_var_q, max_var_q, fitness_score);

  Eigen::MatrixXd inf = Eigen::MatrixXd::Identity(6, 6);
  inf.topLeftCorner(3, 3).array() /= w_x;
  inf.bottomRightCorner(3, 3).array() /= w_q;
  return inf;
}

Eigen::MatrixXd InformationMatrixCalculator::const_information_matrix() const {
  Eigen::MatrixXd inf = Eigen::MatrixXd::Identity(6, 6);
  inf.topLeftCorner(3, 3).array() /= const_stddev_x;
  inf.bottomRightCorner(3, 3).array() /= const_stddev_q;
  return inf;
}

double InformationMatrixCalculator::calc_fitness_score(const pcl::PointCloud<PointT>::ConstPtr& cloud1, const pcl::PointCloud<PointT>::ConstPtr& cloud2, const Eigen::Isometry3d& relpose, double max_range) {
  pcl::search::KdTree<PointT>::Ptr tree_(new pcl::search::KdTree<PointT>());
  tree_->setInputCloud(cloud1);

  double fitness_score = 0.0;

  // Transform the input dataset using the final transformation
  pcl::PointCloud<PointT> input_transformed;
  pcl::transformPointCloud(*cloud2, input_transformed, relpose.cast<float>());

  std::vector<int> nn_indices(1);
  std::vector<float> nn_dists(1);

  // For each point in the source dataset
  int nr = 0;
  for(size_t i = 0; i < input_transformed.points.size(); ++i) {
    // Find its nearest neighbor in the target
    tree_->nearestKSearch(input_transformed.points[i], 1, nn_indices, nn_dists);

    // Deal with occlusions (incomplete targets)
    if(nn_dists[0] <= max_range) {
      // Add to the fitness score
      fitness_score += nn_dists[0];
      nr++;
    }
  }

  if(nr > 0)
    return (fitness_score / nr);
  else
    return (std::numeric_limits<double>::max());
}


static pcl::search::KdTree<pcl::PointXYZI>::Ptr kd_tree_(new pcl::search::KdTree<pcl::PointXYZI>());
void InformationMatrixCalculator::rebuild_kd_tree(const pcl::PointCloud<PointT>::ConstPtr& cloud) {
  kd_tree_->setInputCloud(cloud);
}

double InformationMatrixCalculator::fitness_score(const pcl::PointCloud<PointT>::ConstPtr& cloud1, const pcl::PointCloud<PointT>::ConstPtr& cloud2, const Eigen::Isometry3d& relpose, const double& floor_height, int& nr, pcl::PointIndices::Ptr &inliers, double max_range) {
  double fitness_score = 0.0;

  std::vector<int> nn_indices(1);
  std::vector<float> nn_dists(1);

  Eigen::Affine3f relative = Eigen::Affine3f(relpose.matrix().cast<float>());
  inliers->indices.reserve(cloud2->points.size());
  double floor_height_max = floor_height + 2.0;

  // For each point in the source dataset
  nr = 0;
  for(size_t i = 0; i < cloud2->points.size(); ++i) {
    // Find its nearest neighbor in the target
    kd_tree_->nearestKSearch(cloud2->points[i], 1, nn_indices, nn_dists);

    // Deal with occlusions (incomplete targets)
    if(nn_dists[0] <= max_range) {
      // Add to the fitness score
      fitness_score += nn_dists[0];
      nr++;
    }

    PointT p1 = pcl::transformPoint(cloud1->points[nn_indices[0]], relative);
    PointT p2 = pcl::transformPoint(cloud2->points[i], relative);

    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    float horizon_dist = dx * dx + dy * dy;
    if (horizon_dist <= 10.0 && p1.z < floor_height_max && p2.z < floor_height_max && fabs(p1.z - p2.z) > 0.25) {
      inliers->indices.push_back(i);
    }
  }

  if(nr > 0)
    return (fitness_score / nr);
  else
    return (std::numeric_limits<double>::max());
}

}  // namespace hdl_graph_slam

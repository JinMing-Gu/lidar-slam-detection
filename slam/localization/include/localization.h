#ifndef __RELOCALIZATION_H
#define __RELOCALIZATION_H

#include <mutex>
#include <memory>
#include <thread>
#include <atomic>

#include "slam_base.h"
#include "localization_base.h"
#include "mapping_types.h"
#include "map_loader.h"
#include "global_localization.h"
#include "UTMProjector.h"

namespace Locate {

class Localization : public SlamBase {
 public:
  static void releaseStaticResources();
  void mergeMap(const std::string &directory, std::vector<std::shared_ptr<KeyFrame>> &frames);
  Localization();
  virtual ~Localization();
  bool init(InitParameter &param) override;
  bool isInited() override;
  bool originIsSet() override {
    return mOriginIsSet;
  }
  RTKType& getOrigin() override {
    return mOrigin;
  }
  void setOrigin(RTKType rtk) override {
  }
  std::vector<std::string> setSensors(std::vector<std::string> &sensors) override;
  void setInitPoseRange(PoseRange &r) override;
  void setInitPose(const Eigen::Matrix4d &t) override;
  int getEstimatePose(Eigen::Matrix4d &t) override;
  void feedInsData(std::shared_ptr<RTKType> ins) override;
  void feedImuData(ImuType &imu) override;
  void feedPointData(const uint64_t &timestamp, std::map<std::string, PointCloudAttrPtr> &points) override;
  void feedImageData(const uint64_t &timestamp,
                     std::map<std::string, ImageType> &images,
                     std::map<std::string, cv::Mat> &images_stream) override;
  Eigen::Matrix4d getPose(PointCloudAttrImagePose &frame) override;
  bool getTimedPose(uint64_t timestamp, Eigen::Matrix4d &pose) override;
  bool getTimedPose(RTKType &ins, Eigen::Matrix4d &pose) override;
  void getGraphMap(std::vector<std::shared_ptr<KeyFrame>> &frames) override;
  void getColorMap(PointCloudRGB::Ptr &points) override;

 protected:
  void initLocalizer(uint64_t stamp, const Eigen::Matrix4d& pose);

 private:
  void startMapUpdateThread();
  void runUpdateLocalMap();

 protected:
  InitParameter mConfig;
  RTKType mOrigin;
  bool mOriginIsSet;
  Eigen::Vector3d mZeroUtm;

  std::string mLidarName;
  std::string mImageName;
  PointCloudAttrPtr mFrameAttr;
  ImageType mImage;

  std::atomic<bool> mInitialized;
  int mFailureLocalizeCount;
  Eigen::Isometry3d mLastOdom;
  PointCloud::Ptr mLocalMap;
  std::vector<std::shared_ptr<KeyFrame>> mKeyFrames;
  pcl::KdTreeFLANN<pcl::PointXYZ>::Ptr mGraphKDTree;

  // thread related
  bool mThreadStart;
  std::mutex mMutex;
  std::mutex mLocalizerMutex;
  std::unique_ptr<std::thread> mLocalMapUpdateThread;
  RWQueue<Eigen::Isometry3d> mPoseQueue;

  // sub modules
  std::unique_ptr<UTMProjector> mProjector;
  std::unique_ptr<LocalizationBase> mLocalizer;
  std::unique_ptr<GlobalLocalization> mGlobalLocator;

  static std::unique_ptr<MapLoader> mMap;
};

}
#endif
#include "monocular-slam-node.hpp"
#include <opencv2/core/core.hpp>
#include <Eigen/Core>
#include <Eigen/Geometry>

using std::placeholders::_1;

MonocularSlamNode::MonocularSlamNode(ORB_SLAM3::System* pSLAM)
:   Node("ORB_SLAM3_ROS2")
{
    this->declare_parameter<std::string>("frame_id", "map");
    this->declare_parameter<std::string>("child_frame_id", "camera_link");
    this->declare_parameter<std::string>("camera_topic", "camera");
    this->get_parameter("frame_id", frame_id);
    this->get_parameter("child_frame_id", child_frame_id);
    this->get_parameter("camera_topic", camera_topic);
    
    m_SLAM = pSLAM;
    // std::cout << "slam changed" << std::endl;
    m_image_subscriber = this->create_subscription<ImageMsg>(
        this->camera_topic,
        10,
        std::bind(&MonocularSlamNode::GrabImage, this, _1));
    tf_broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);
    std::cout << "slam changed" << std::endl;
}

MonocularSlamNode::~MonocularSlamNode()
{
    // Stop all threads
    m_SLAM->Shutdown();
    // Save camera trajectory
    m_SLAM->SaveKeyFrameTrajectoryTUM("KeyFrameTrajectory.txt");
}

void MonocularSlamNode::GrabImage(const ImageMsg::SharedPtr msg)
{
    // Copy the ros image message to cv::Mat.
    try
    {
        m_cvImPtr = cv_bridge::toCvCopy(msg);
    }
    catch (cv_bridge::Exception& e)
    {
        RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
        return;
    }

    std::cout<<"one frame has been sent"<<std::endl;
    // m_SLAM->TrackMonocular(m_cvImPtr->image, Utility::StampToSec(msg->header.stamp));
    // 여기부터 순서 중요 !!
    // 1) SE3<float> 형태로 받아오기, map->.. 해야 하므로 inverse 때리기.
    Sophus::SE3f Twc_se3 = m_SLAM->TrackMonocular(
                 m_cvImPtr->image, Utility::StampToSec(msg->header.stamp)).inverse();
    // 2) SE3에서 translation, rotation 구하기
    Eigen::Vector3f t_slam = Twc_se3.translation();
    Eigen::Quaternionf q_slam = Twc_se3.so3().unit_quaternion();

    geometry_msgs::msg::TransformStamped tf_msg;
    tf_msg.header.stamp = msg->header.stamp;
    tf_msg.header.frame_id = frame_id;
    tf_msg.child_frame_id = child_frame_id;
    // 4) translation : cv -> tf 변환
    tf_msg.transform.translation.x = t_slam.z();
    tf_msg.transform.translation.y = -t_slam.x();
    tf_msg.transform.translation.z = -t_slam.y();
    // 5) rotation : cv -> tf 변환
    tf_msg.transform.rotation.x = q_slam.x();
    tf_msg.transform.rotation.y = q_slam.z();
    tf_msg.transform.rotation.z = q_slam.y();
    tf_msg.transform.rotation.w = q_slam.w();
    // 6) 씨 발행
    tf_broadcaster_->sendTransform(tf_msg);
}

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <chrono>


class CamPub : public rclcpp::Node {
public:
  CamPub() : Node("cam_pub") {
    this->declare_parameter<std::string>("device", "/dev/video0");
    std::string dev;
    this->get_parameter("device", dev);
    this->declare_parameter("camera_topic", "camera");
    std::string cam;
    this->get_parameter("camera_topic", cam);
    pub_ = create_publisher<sensor_msgs::msg::Image>(cam, 10);

    // 2) VideoCapture 열기
    //    dev 가 "/dev/videoX" 형태면 그대로, 숫자로만 들어오면 인덱스로 변환
    if (dev.size()>1 && dev[0]=='/') {
      // V4L2 백엔드를 명시하여 디바이스 파일로 열기
      cap_.open(dev, cv::CAP_V4L2);
    } else {
      // "0", "1" 처럼 숫자만 들어온 경우
      int idx = std::stoi(dev);
      cap_.open(idx, cv::CAP_V4L2);
    }

    if (!cap_.isOpened()) {
      RCLCPP_FATAL(this->get_logger(), "Failed to open camera device: %s", dev.c_str());
      rclcpp::shutdown();
      return;
    }
    // 3) 원하는 설정
    cap_.set(cv::CAP_PROP_FRAME_WIDTH,  640);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap_.set(cv::CAP_PROP_FPS,          30);

    timer_ = create_wall_timer(
      std::chrono::milliseconds(33),
      std::bind(&CamPub::onTimer, this));
  }

private:
  // void onTimer() {
  //   cv::Mat frame;
  //   cap_ >> frame;
  //   if(frame.empty()) return;
  //   // frame 은 CV_8UC3 BGR8 이라 가정
  //   auto msg = cv_bridge::CvImage(
  //     std_msgs::msg::Header(), "bgr8", frame).toImageMsg();
  //   pub_->publish(*msg);
  // }
  void onTimer() {
    cv::Mat frame, gray;
    cap_ >> frame;
    if(frame.empty()) return;

    // 컬러 → 그레이 변환
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    // mono8 로 퍼블리시
    auto msg = cv_bridge::CvImage(
      std_msgs::msg::Header(), "mono8", gray
    ).toImageMsg();
    pub_->publish(*msg);
  }
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  cv::VideoCapture cap_;
};

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<CamPub>());
  rclcpp::shutdown();
}

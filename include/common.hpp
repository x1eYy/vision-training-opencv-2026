#pragma once
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <string>

namespace vision {
void ensure_dir(const std::filesystem::path& path);
void save_image(const std::filesystem::path& path, const cv::Mat& image);
cv::VideoWriter open_writer(const std::filesystem::path& path, double fps, cv::Size size, bool color=true);
std::string arg_value(int argc, char** argv, const std::string& name, const std::string& fallback);
void label(cv::Mat& image, const std::string& text, cv::Point pos, cv::Scalar color=cv::Scalar(255,255,255));
}

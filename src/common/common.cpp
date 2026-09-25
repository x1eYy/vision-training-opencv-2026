#include "common.hpp"
#include <stdexcept>

namespace vision {
void ensure_dir(const std::filesystem::path& path) {
    if (!path.empty()) std::filesystem::create_directories(path);
}
void save_image(const std::filesystem::path& path, const cv::Mat& image) {
    ensure_dir(path.parent_path());
    if (image.empty() || !cv::imwrite(path.string(), image))
        throw std::runtime_error("Cannot write image: " + path.string());
}
cv::VideoWriter open_writer(const std::filesystem::path& path, double fps, cv::Size size, bool color) {
    ensure_dir(path.parent_path());
    cv::VideoWriter writer(path.string(), cv::VideoWriter::fourcc('m','p','4','v'), fps, size, color);
    if (!writer.isOpened()) throw std::runtime_error("Cannot write video: " + path.string());
    return writer;
}
std::string arg_value(int argc, char** argv, const std::string& name, const std::string& fallback) {
    for (int i=1; i+1<argc; ++i) if (argv[i] == name) return argv[i+1];
    return fallback;
}
void label(cv::Mat& image, const std::string& text, cv::Point pos, cv::Scalar color) {
    cv::putText(image, text, pos, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0,0,0), 4, cv::LINE_AA);
    cv::putText(image, text, pos, cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2, cv::LINE_AA);
}
}

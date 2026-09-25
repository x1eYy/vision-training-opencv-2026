#include "common.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) try {
    namespace fs = std::filesystem;
    const fs::path input = vision::arg_value(argc, argv, "--input", "resources/test_image.jpg");
    const fs::path out = vision::arg_value(argc, argv, "--output", "result/task1_images");
    const cv::Mat img = cv::imread(input.string());
    if (img.empty()) throw std::runtime_error("Cannot read " + input.string());
    auto save = [&](const std::string& name, const cv::Mat& m){vision::save_image(out/name, m);};
    cv::Mat gray, hsv;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> channels; cv::split(hsv, channels);
    save("gray.png", gray); save("hsv_h.png", channels[0]);
    save("hsv_s.png", channels[1]); save("hsv_v.png", channels[2]);
    cv::Mat mean, gaussian, median;
    cv::blur(img, mean, {5,5});
    cv::GaussianBlur(img, gaussian, {5,5}, 1.5);
    cv::medianBlur(img, median, 5);
    save("mean_filter.png", mean); save("gaussian_filter.png", gaussian);
    save("median_filter.png", median);
    cv::Mat low, high, mask;
    cv::inRange(hsv, cv::Scalar(0,100,80), cv::Scalar(10,255,255), low);
    cv::inRange(hsv, cv::Scalar(170,100,80), cv::Scalar(179,255,255), high);
    cv::bitwise_or(low, high, mask); save("red_mask.png", mask);
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, {5,5});
    cv::Mat eroded, dilated, opened, closed;
    cv::erode(mask, eroded, kernel); cv::dilate(mask, dilated, kernel);
    cv::morphologyEx(mask, opened, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(opened, closed, cv::MORPH_CLOSE, kernel);
    save("erode.png", eroded); save("dilate.png", dilated);
    save("open.png", opened); save("close.png", closed);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(closed, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    cv::Mat boxes=img.clone(); std::vector<double> areas;
    for (auto& c:contours) {
        double area=cv::contourArea(c);
        if (area<200.0) continue;
        areas.push_back(area);
        cv::drawContours(boxes, std::vector<std::vector<cv::Point>>{c}, -1, {0,255,0}, 2);
        auto box=cv::boundingRect(c); cv::rectangle(boxes, box, {0,0,255}, 2);
        vision::label(boxes, std::to_string(static_cast<int>(area)), box.tl()+cv::Point(0,-5), {0,255,255});
    }
    save("contours_boxes.png", boxes);
    cv::Mat drawing=img.clone();
    cv::circle(drawing, {img.cols/2,img.rows/2}, 60, {255,0,0}, 3);
    cv::rectangle(drawing, {50,50}, {img.cols/2,img.rows/2}, {0,255,0}, 3);
    vision::label(drawing, "OpenCV drawing", {60,90}, {0,0,255});
    save("drawing.png", drawing);
    cv::Mat rotated;
    auto matrix=cv::getRotationMatrix2D(cv::Point2f(img.cols/2.0f,img.rows/2.0f),35.0,1.0);
    cv::warpAffine(img, rotated, matrix, img.size()); save("rotated_35deg.png", rotated);
    save("crop_top_left.png", img(cv::Rect(0,0,img.cols/2,img.rows/2)));
    vision::ensure_dir(out);
    std::ofstream report(out/"contour_areas.txt");
    report << "Area threshold: 200 px^2\nSelected contours: " << areas.size() << "\n";
    for (size_t i=0;i<areas.size();++i) report << i+1 << ": " << std::fixed << std::setprecision(1) << areas[i] << " px^2\n";
    std::cout << "Wrote 16 images; selected " << areas.size() << " contours\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}

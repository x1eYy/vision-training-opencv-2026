#include "common.hpp"
#include "windmill.hpp"
#include <fstream>
#include <iostream>
int main() {
    windmill::Config cfg;
    cfg.load("config/windmill.yaml");
    windmill::Detector detector(cfg, cv::imread("config/r_template.png", 0));
    vision::ensure_dir("result/task3_windmill/validation");
    std::ofstream csv("result/task3_windmill/validation/predictions.csv");
    csv << "video,frame,rx,ry,x,y,width,height,angle,valid,layers,arrows\n";
    std::string last;
    cv::VideoCapture cap;
    for (int k = 0; k < 80; k++) {
        std::string name = k < 40 ? "task_3" : "task_4";
        int index = (k % 40) * (k < 40 ? 20 : 45);
        if (name != last) {
            cap.open("resources/" + name + ".mp4");
            last = name;
        }
        cap.set(cv::CAP_PROP_POS_FRAMES, index);
        cv::Mat f;
        cap.read(f);
        auto d = detector.detect(f);
        std::string prefix =
            name + "," + std::to_string(index) + "," +
            (d.center.reliable ? std::to_string(d.center.point.x) + "," + std::to_string(d.center.point.y)
                               : ",");
        csv << prefix << ",,,,,,0,0,0\n";
        for (auto &o : d.candidates)
            csv << prefix << ',' << o.center.x << ',' << o.center.y << ',' << o.outer.size.width << ','
                << o.outer.size.height << ',' << o.outer.angle << ',' << o.valid << ',' << o.layers << ','
                << o.arrows << '\n';
        cv::Mat debug;
        windmill::render(f, debug, d, {}, index);
        for (auto &o : d.candidates)
            cv::ellipse(f, o.outer, o.valid ? cv::Scalar(0, 255, 0) : cv::Scalar(255, 0, 0), 2);
        cv::imwrite("result/task3_windmill/validation/" + name + "_" + std::to_string(index) + ".jpg", f);
    }
}

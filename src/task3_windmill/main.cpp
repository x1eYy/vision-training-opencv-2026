#include "common.hpp"
#include "windmill.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
int main(int argc, char **argv) try {
    using namespace windmill;
    auto input = vision::arg_value(argc, argv, "--input", "resources/task_3.mp4");
    std::filesystem::path out = vision::arg_value(argc, argv, "--output", "result/task3_windmill/task_3");
    Config cfg;
    cfg.load(vision::arg_value(argc, argv, "--config", "config/windmill.yaml"));
    auto mode = vision::arg_value(argc, argv, "--mode", "small");
    if (mode != "small" && mode != "large")
        throw std::runtime_error("mode must be small or large");
    Detector detector(cfg,
                      cv::imread(vision::arg_value(argc, argv, "--template", "config/r_template.png"), 0));
    Tracker tracker(cfg);
    cv::VideoCapture cap(input);
    if (!cap.isOpened())
        throw std::runtime_error("Cannot open input");
    double fps = cap.get(cv::CAP_PROP_FPS);
    int expected = cvRound(cap.get(cv::CAP_PROP_FRAME_COUNT));
    cv::Size size(cvRound(cap.get(3)), cvRound(cap.get(4)));
    if (fps <= 0 || size.area() <= 0)
        throw std::runtime_error("Bad video metadata");
    vision::ensure_dir(out);
    auto video = vision::open_writer(out / "recognition_overlay.mp4", fps, size),
         binary = vision::open_writer(out / "binary_process.mp4", fps, size);
    std::ofstream csv(out / "frames.csv"), candidates(out / "candidates.csv"), tracks(out / "tracks.csv");
    tracks << "frame,id,selected,observed,missing,invalid,last_observed,relative_x,relative_y,radius,score,"
              "retired\n";
    csv << std::setprecision(9);
    candidates << std::setprecision(9);
    csv << "frame,time,center_valid,rx,ry,center_score,selected_id,status,target_x,target_y,angle,reason\n";
    candidates << "frame,index,x,y,width,height,angle,layers,arrow_peaks,chain_coverage,valid,solid_bar,"
                  "score,ring_contrast,ring_support,gap_cv,solid_fraction,clear_absence,peak_balance\n";
    int index = 0, detected = 0, centers = 0;
    cv::Mat f;
    while (cap.read(f)) {
        auto d = detector.detect(f);
        auto t = tracker.update(d, index);
        cv::Mat overlay = f.clone(), debug;
        render(overlay, debug, d, t, index);
        video.write(overlay);
        binary.write(debug);
        csv << index << ',' << index / fps << ',' << d.center.reliable << ',';
        if (d.center.reliable) {
            csv << d.center.point.x << ',' << d.center.point.y;
            ++centers;
        } else
            csv << ',';
        csv << ',' << d.center.score << ',' << t.selected_id << ',' << (t.selected ? "detected" : "lost")
            << ',';
        if (t.selected) {
            auto p = t.selected->center;
            csv << p.x << ',' << p.y << ',' << atan2(d.center.point.y - p.y, p.x - d.center.point.x);
            ++detected;
        } else
            csv << ",,";
        csv << ',' << t.reason << '\n';
        for (size_t j = 0; j < d.candidates.size(); j++) {
            auto &o = d.candidates[j];
            candidates << index << ',' << j << ',' << o.center.x << ',' << o.center.y << ','
                       << o.outer.size.width << ',' << o.outer.size.height << ',' << o.outer.angle << ','
                       << o.layers << ',' << o.arrows << ',' << o.coverage << ',' << o.valid << ','
                       << o.solid_bar << ',' << o.score << ',' << o.ring_contrast << ',' << o.ring_support
                       << ',' << o.gap_cv << ',' << o.solid_fraction << ',' << o.clear_absence << ','
                       << o.peak_balance << '\n';
        }
        for (const auto &track : tracker.states()) {
            if (!track.retired || index - track.last_frame <= cfg.lost_tolerance + 1)
                tracks << index << ',' << track.id << ',' << (track.id == t.selected_id) << ','
                       << track.observed << ',' << track.missing << ',' << track.invalid << ','
                       << track.last_frame << ',' << track.relative.x << ',' << track.relative.y << ','
                       << track.target.radius() << ',' << track.target.score << ',' << track.retired << '\n';
        }
        ++index;
        if (index % 300 == 0)
            std::cout << input << ": " << index << " frames" << std::endl;
    }
    video.release();
    binary.release();
    std::ofstream stats(out / "tracking_stats.txt");
    stats << "frames=" << index << "\nfps=" << fps << "\nwidth=" << size.width << "\nheight=" << size.height
          << "\ncenter_detected_frames=" << centers << "\ntarget_detected_frames=" << detected
          << "\nlost_tolerance=" << cfg.lost_tolerance << "\ninvalid_confirm=" << cfg.invalid_confirm << '\n';
    std::cout << "Completed " << index << " frames\n";
    return index == expected ? 0 : 2;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}

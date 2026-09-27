#pragma once
#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <vector>
namespace windmill {
struct Config {
    double center_score = .68, ellipse_error = .15, concentric_offset = .18, minimum_coverage = .50;
    double arrow_prominence = .04, max_gap_cv = .45, inner_contrast = .22, inner_support = .80,
           solid_run = .30;
    double min_peak_balance = .25;
    int arrow_peaks = 5, lost_tolerance = 12, invalid_confirm = 3;
    void load(const std::string &path);
};
struct CenterObservation {
    cv::Point2f point;
    double score = 0;
    bool reliable = false;
};
struct TargetObservation {
    cv::RotatedRect outer;
    cv::Point2f center;
    double score = 0, coverage = 0, ring_contrast = 0, ring_support = 0, gap_cv = 0, solid_fraction = 0,
           peak_balance = 0;
    int layers = 0, arrows = 0;
    bool valid = false, solid_bar = false, clear_absence = false;
    double radius() const { return (outer.size.width + outer.size.height) / 4.; }
};
struct Detection {
    CenterObservation center;
    std::vector<TargetObservation> candidates;
    cv::Mat binary;
};
class Detector {
    Config cfg;
    cv::Mat reference;

  public:
    Detector(Config c, const cv::Mat &templ);
    Detection detect(const cv::Mat &frame) const;
};
struct TrackState {
    int id = 0, last_frame = 0, missing = 0, invalid = 0;
    bool retired = false, observed = false;
    cv::Point2f relative, velocity;
    double angular_step = 0; // Radians per frame, only for association.
    TargetObservation target;
};
struct TrackingResult {
    int selected_id = 0;
    std::optional<TargetObservation> selected;
    std::string reason;
};
class Tracker {
    Config cfg;
    int next_id = 1, selected_id = 0;
    std::vector<TrackState> tracks;
    std::string pending_reason;

  public:
    explicit Tracker(Config c) : cfg(c){};
    TrackingResult update(const Detection &d, int frame);
    const std::vector<TrackState> &states() const { return tracks; }
};
void render(cv::Mat &overlay, cv::Mat &debug, const Detection &d, const TrackingResult &t, int frame);
} // namespace windmill

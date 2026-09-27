#include "common.hpp"
#include "windmill.hpp"
namespace windmill {
void render(cv::Mat &image, cv::Mat &debug, const Detection &d, const TrackingResult &t, int frame) {
    cv::cvtColor(d.binary, debug, cv::COLOR_GRAY2BGR);
    for (auto &o : d.candidates) {
        cv::ellipse(debug, o.outer, o.valid ? cv::Scalar(0, 255, 0) : cv::Scalar(255, 100, 0), 2);
        vision::label(debug, cv::format("L%d A%d", o.layers, o.arrows), o.center, {255, 200, 0});
    }
    if (d.center.reliable) {
        for (auto *m : {&image, &debug}) {
            cv::drawMarker(*m, d.center.point, {0, 255, 0}, cv::MARKER_CROSS, 14, 2);
            vision::label(*m, "R", d.center.point + cv::Point2f(9, -9), {0, 255, 0});
        }
    }
    if (t.selected && d.center.reliable) {
        auto &o = *t.selected;
        for (auto *m : {&image, &debug}) {
            cv::ellipse(*m, o.outer, {0, 255, 255}, 2, cv::LINE_AA);
            cv::circle(*m, o.center, 3, {0, 0, 255}, -1);
            cv::line(*m, d.center.point, o.center, {255, 255, 0}, 2);
        }
        double angle = std::atan2(d.center.point.y - o.center.y, o.center.x - d.center.point.x);
        vision::label(image, cv::format("ID %d detected  theta %.3f", t.selected_id, angle), {25, 40},
                      {0, 255, 0});
    } else
        vision::label(image, cv::format("ID %d lost", t.selected_id), {25, 40}, {0, 0, 255});
    vision::label(image, cv::format("frame %d", frame), {25, 75});
    vision::label(debug, "BLUE geometry / GREEN valid A / YELLOW locked", {25, 35});
    if (!t.reason.empty())
        vision::label(image, t.reason, {25, 110});
}
} // namespace windmill

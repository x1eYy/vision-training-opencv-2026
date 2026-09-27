#include "windmill.hpp"
#include <algorithm>
#include <cmath>
namespace windmill {
namespace {
double wrap(double a) { return std::atan2(std::sin(a), std::cos(a)); }
} // namespace

TrackingResult Tracker::update(const Detection &d, int frame) {
    TrackingResult result;
    std::string release;
    struct Edge {
        double cost;
        int track, candidate;
    };
    std::vector<Edge> edges;
    std::vector<bool> used(d.candidates.size(), false);
    for (size_t i = 0; i < tracks.size(); i++) {
        auto &t = tracks[i];
        t.observed = false;
        if (t.retired)
            continue;
        if (d.center.reliable)
            for (size_t j = 0; j < d.candidates.size(); j++) {
                auto &o = d.candidates[j];
                if (!o.valid)
                    continue;
                auto relative = o.center - d.center.point;
                auto predicted = t.relative + t.velocity * float(std::min(4, frame - t.last_frame));
                double rad = t.target.radius(), distance = cv::norm(relative - predicted) / rad,
                       size = std::abs(std::log(o.radius() / rad));
                double angle_error = std::abs(wrap(std::atan2(relative.y, relative.x) -
                                                   std::atan2(t.relative.y, t.relative.x) -
                                                   t.angular_step * std::min(4, frame - t.last_frame)));
                if (distance < 1.3 + std::min(t.missing, 6) * .12 && size < .35)
                    edges.push_back(
                        {distance + size + angle_error * .3 + std::abs(o.score - t.target.score) * .03,
                         int(i), int(j)});
            }
    }
    // Globally sorted gated edges ensure each observation and each trajectory is consumed once.
    std::sort(edges.begin(), edges.end(), [](auto a, auto b) { return a.cost < b.cost; });
    for (auto e : edges) {
        auto &t = tracks[e.track];
        if (t.observed || used[e.candidate])
            continue;
        auto o = d.candidates[e.candidate];
        auto rel = o.center - d.center.point;
        int dt = std::max(1, frame - t.last_frame);
        auto step = (rel - t.relative) / float(dt);
        if (cv::norm(step) > o.radius() * .6)
            step *= float(o.radius() * .6 / cv::norm(step));
        t.velocity = t.velocity * .4f + step * .6f;
        t.angular_step = .4 * t.angular_step +
                         .6 * wrap(std::atan2(rel.y, rel.x) - std::atan2(t.relative.y, t.relative.x)) / dt;
        t.relative = rel;
        t.target = o;
        t.last_frame = frame;
        t.missing = 0;
        t.invalid = 0;
        t.observed = true;
        used[e.candidate] = true;
    }
    for (auto &t : tracks) {
        if (t.retired || t.observed)
            continue;
        t.missing++;
        bool confirmed = false;
        std::string evidence;
        if (d.center.reliable) {
            auto pred = t.relative + t.velocity * float(std::min(4, frame - t.last_frame));
            for (auto &o : d.candidates) {
                if ((o.solid_bar || o.clear_absence) &&
                    cv::norm((o.center - d.center.point) - pred) < t.target.radius() * .8) {
                    confirmed = true;
                    evidence = o.solid_bar ? "confirmed_B" : "clear_structure_absence";
                }
            }
        }
        // Black/occluded ROIs alone never prove a hit. A visible B is explicit negative evidence.
        t.invalid = confirmed ? t.invalid + 1 : 0;
        if (t.invalid >= cfg.invalid_confirm || t.missing > cfg.lost_tolerance) {
            t.retired = true;
            if (t.id == selected_id) {
                release = t.invalid >= cfg.invalid_confirm ? evidence : "lost_timeout";
                selected_id = 0;
                pending_reason = release;
            }
        }
    }
    if (d.center.reliable)
        for (size_t j = 0; j < d.candidates.size(); j++)
            if (d.candidates[j].valid && !used[j]) {
                TrackState t;
                t.id = next_id++;
                t.last_frame = frame;
                t.relative = d.candidates[j].center - d.center.point;
                t.target = d.candidates[j];
                t.observed = true;
                tracks.push_back(t);
            }
    if (selected_id == 0) {
        for (auto &t : tracks)
            if (!t.retired && t.observed) {
                selected_id = t.id;
                result.reason =
                    pending_reason.empty() ? "initial_acquisition" : pending_reason + "_reacquire";
                pending_reason.clear();
                break;
            }
        if (selected_id == 0)
            result.reason = release;
    }
    result.selected_id = selected_id;
    for (auto &t : tracks)
        if (t.id == selected_id && t.observed)
            result.selected = t.target;
    return result;
}
} // namespace windmill

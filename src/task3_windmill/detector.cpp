#include "windmill.hpp"
#include <algorithm>
#include <cmath>
namespace windmill {
void Config::load(const std::string &p) {
    cv::FileStorage f(p, cv::FileStorage::READ);
    if (!f.isOpened())
        throw std::runtime_error("Cannot read config: " + p);
#define READ(x)                                                                                              \
    if (!f[#x].empty())                                                                                      \
    f[#x] >> x
    READ(center_score);
    READ(ellipse_error);
    READ(concentric_offset);
    READ(minimum_coverage);
    READ(arrow_peaks);
    READ(min_peak_balance);
    READ(arrow_prominence);
    READ(max_gap_cv);
    READ(inner_contrast);
    READ(inner_support);
    READ(solid_run);
    READ(lost_tolerance);
    READ(invalid_confirm);
#undef READ
    if (center_score <= 0 || center_score > 1 || ellipse_error <= 0 || concentric_offset <= 0 ||
        minimum_coverage <= 0 || minimum_coverage > 1 || arrow_peaks < 1 || lost_tolerance < 0 ||
        invalid_confirm < 1)
        throw std::runtime_error("Invalid windmill configuration");
}
Detector::Detector(Config c, const cv::Mat &t) : cfg(c) {
    if (t.empty())
        throw std::runtime_error("Missing R template");
    cv::resize(t, reference, {32, 32}, 0, 0, cv::INTER_NEAREST);
    cv::threshold(reference, reference, 127, 255, cv::THRESH_BINARY);
}
namespace {
struct Fit {
    cv::RotatedRect e;
    double error;
};
double radius(const cv::RotatedRect &e) { return (e.size.width + e.size.height) / 4.; }
double residual(const cv::RotatedRect &e, const std::vector<cv::Point> &points) {
    double a = e.angle * CV_PI / 180, co = cos(a), si = sin(a), sum = 0;
    for (auto p : points) {
        double x = p.x - e.center.x, y = p.y - e.center.y;
        sum += std::abs(
            std::hypot((x * co + y * si) / (e.size.width / 2), (-x * si + y * co) / (e.size.height / 2)) - 1);
    }
    return sum / points.size();
}
struct Chain {
    int peaks;
    double mean, solid, spacing_cv, peak_balance;
};
Chain arrowProfile(const cv::Mat &m, cv::Point2f r, const cv::RotatedRect &e, double prominence) {
    auto delta = e.center - r;
    double dist = cv::norm(delta), rad = radius(e);
    if (dist < 1)
        return {0, 0, 0, 99, 0};
    auto u = delta / float(dist);
    cv::Point2f n(-u.y, u.x);
    std::vector<float> values;
    for (double t = std::max(15., rad * .65); t < dist - rad * 1.25; t++) {
        int on = 0, total = 0;
        for (double w = -std::max(4., rad * .28); w <= std::max(4., rad * .28); w++) {
            auto p = r + u * float(t) + n * float(w);
            int x = cvRound(p.x), y = cvRound(p.y);
            if (x >= 0 && y >= 0 && x < m.cols && y < m.rows) {
                ++total;
                on += m.at<uchar>(y, x) > 0;
            }
        }
        values.push_back(total ? float(on) / total : 0);
    }
    if (values.size() < 30)
        return {0, 0, 0, 99, 0};
    cv::Mat prof(values, true);
    cv::GaussianBlur(prof, prof, {1, 5}, .8);
    int peaks = 0, last = -10;
    std::vector<double> gaps, prominences;
    double mean = cv::mean(prof)[0];
    for (int i = 3; i < prof.rows - 3; i++) {
        float v = prof.at<float>(i);
        if (v < .12 || v < prof.at<float>(i - 1) || v < prof.at<float>(i + 1))
            continue;
        float l = 1, r = 1;
        for (int j = std::max(0, i - 10); j < i; j++)
            l = std::min(l, prof.at<float>(j));
        for (int j = i + 1; j < std::min(prof.rows, i + 11); j++)
            r = std::min(r, prof.at<float>(j));
        if (v - std::max(l, r) > prominence && i - last > 4) {
            if (peaks)
                gaps.push_back(i - last);
            prominences.push_back(v - std::max(l, r));
            peaks++;
            last = i;
        }
    }
    int run = 0, longest = 0;
    for (int i = 0; i < prof.rows; i++) {
        run = prof.at<float>(i) > .45 ? run + 1 : 0;
        longest = std::max(longest, run);
    }
    double meanGap = 0, var = 0;
    for (auto g : gaps)
        meanGap += g;
    meanGap /= std::max(size_t(1), gaps.size());
    for (auto g : gaps)
        var += (g - meanGap) * (g - meanGap);
    double cv = meanGap > 0 ? sqrt(var / std::max(size_t(1), gaps.size())) / meanGap : 99;
    std::sort(prominences.begin(), prominences.end());
    double balance = prominences.empty() ? 0 : prominences[prominences.size() / 2] / prominences.back();
    return {peaks, mean, double(longest) / prof.rows, cv, balance};
}
// Verify a dim concentric inner ring by angular coverage and radial contrast.
std::pair<double, double> innerRing(const cv::Mat &mask, const cv::RotatedRect &e) {
    double best = -1, support = 0, a = e.angle * CV_PI / 180;
    auto sample = [&](double r, int k) {
        double t = k * 2 * CV_PI / 72, u = e.size.width * .5 * r * cos(t),
               v = e.size.height * .5 * r * sin(t);
        int x = cvRound(e.center.x + u * cos(a) - v * sin(a)),
            y = cvRound(e.center.y + u * sin(a) + v * cos(a));
        return x >= 0 && y >= 0 && x < mask.cols && y < mask.rows && mask.at<uchar>(y, x) > 0;
    };
    for (double r = .32; r <= .62; r += .02) {
        double on = 0, bg = 0;
        for (int k = 0; k < 72; k++) {
            on += sample(r, k);
            bg += (sample(r - .15, k) + sample(r + .15, k)) * .5;
        }
        double contrast = (on - bg) / 72;
        if (contrast > best) {
            best = contrast;
            support = on / 72;
        }
    }
    return {best, support};
}
// Recover a broken outer ring from observed radial edge pixels, not a drawn nominal circle.
std::optional<cv::RotatedRect> recover(const cv::Mat &mask, const cv::RotatedRect &inner, double &coverage) {
    double expected = radius(inner) * 2.12;
    std::vector<cv::Point2f> pts;
    for (int k = 0; k < 120; k++) {
        double a = k * 2 * CV_PI / 120, co = cos(a), si = sin(a), best = 1e9, rr = 0;
        for (double r = expected * .84; r < expected * 1.16; r += .5) {
            int x = cvRound(inner.center.x + r * co), y = cvRound(inner.center.y + r * si);
            if (x < 1 || y < 1 || x >= mask.cols - 1 || y >= mask.rows - 1)
                continue;
            if (mask.at<uchar>(y, x) && std::abs(r - expected) < best) {
                best = std::abs(r - expected);
                rr = r;
            }
        }
        if (rr > 0)
            pts.emplace_back(inner.center.x + rr * co, inner.center.y + rr * si);
    }
    coverage = pts.size() / 120.;
    if (pts.size() < 60)
        return {};
    auto e = cv::fitEllipse(pts);
    if (cv::norm(e.center - inner.center) > expected * .12 || radius(e) < expected * .85 ||
        radius(e) > expected * 1.15)
        return {};
    return e;
}
} // namespace
Detection Detector::detect(const cv::Mat &original) const {
    double scale = original.cols / 1440.;
    cv::Mat f;
    if (original.cols != 1440)
        cv::resize(original, f, {1440, cvRound(original.rows / scale)});
    else
        f = original;
    cv::Mat hsv, bright, broad, chainMask, dim;
    cv::cvtColor(f, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, cv::Scalar(0, 130, 120), cv::Scalar(32, 255, 255), bright);
    cv::inRange(hsv, cv::Scalar(0, 80, 28), cv::Scalar(32, 255, 255), broad);
    cv::inRange(hsv, cv::Scalar(0, 80, 100), cv::Scalar(45, 255, 255), chainMask);
    cv::inRange(hsv, cv::Scalar(0, 80, 15), cv::Scalar(32, 255, 255), dim);
    Detection result;
    result.binary = broad;
    std::vector<Fit> fits;
    for (int level : {20, 35, 55, 85}) {
        cv::Mat m;
        cv::inRange(hsv, cv::Scalar(0, 80, level), cv::Scalar(32, 255, 255), m);
        cv::morphologyEx(m, m, cv::MORPH_CLOSE, cv::Mat::ones(3, 3, CV_8U));
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(m, contours, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);
        for (auto &c : contours) {
            if (c.size() < 16)
                continue;
            auto e = cv::fitEllipse(c);
            double mi = std::min(e.size.width, e.size.height), ma = std::max(e.size.width, e.size.height);
            if (mi <= 7 || ma >= 120 || mi / ma < .65)
                continue;
            double err = residual(e, c);
            if (err < cfg.ellipse_error)
                fits.push_back({e, err});
        }
    }
    cv::Mat labels, stats, centroids;
    int count = cv::connectedComponentsWithStats(bright, labels, stats, centroids);
    double best = 0;
    for (int i = 1; i < count; i++) {
        int x = stats.at<int>(i, 0), y = stats.at<int>(i, 1), w = stats.at<int>(i, 2),
            h = stats.at<int>(i, 3), area = stats.at<int>(i, 4);
        if (w < 10 || w > 40 || h < 10 || h > 40 || area < 80 || area > 650)
            continue;
        cv::Mat patch, resized, inter, uni;
        patch = labels(cv::Rect(x, y, w, h)) == i;
        cv::resize(patch, resized, {32, 32}, 0, 0, cv::INTER_NEAREST);
        cv::bitwise_and(resized, reference, inter);
        cv::bitwise_or(resized, reference, uni);
        double score = double(cv::countNonZero(inter)) / std::max(1, cv::countNonZero(uni));
        if (score < cfg.center_score)
            continue;
        cv::Point2f p(centroids.at<double>(i, 0), centroids.at<double>(i, 1));
        int nearby = 0;
        for (auto &fit : fits) {
            double d = cv::norm(fit.e.center - p);
            if (radius(fit.e) > 14 && d > 95 && d < 330)
                nearby++;
        }
        double ranked = score + std::min(nearby, 3) * .01;
        if (ranked > best) {
            best = ranked;
            result.center = {p, score, true};
        }
    }
    if (result.center.reliable) {
        auto pivot = result.center.point;
        std::vector<Fit> outer;
        for (auto &fit : fits) {
            double rad = radius(fit.e), d = cv::norm(fit.e.center - pivot);
            if (d < 95 || d > 330)
                continue;
            if (rad > 17 && rad < 53)
                outer.push_back(fit);
            if (rad > 13 && rad < 21) {
                bool small = false;
                for (auto &z : fits)
                    if (cv::norm(z.e.center - fit.e.center) < 4 && radius(z.e) > .28 * rad &&
                        radius(z.e) < .65 * rad)
                        small = true;
                if (small) {
                    double cov = 0;
                    auto e = recover(broad, fit.e, cov);
                    if (e && cov >= cfg.minimum_coverage)
                        outer.push_back({*e, .045});
                }
            }
        }
        for (auto &fit : outer) {
            TargetObservation o;
            o.outer = fit.e;
            double rad = radius(fit.e);
            std::vector<double> layers;
            cv::Point2f common = fit.e.center;
            int n = 1;
            for (auto &z : fits) {
                double ratio = radius(z.e) / rad;
                if (ratio < .14 || ratio > .64 ||
                    cv::norm(z.e.center - fit.e.center) > cfg.concentric_offset * rad)
                    continue;
                bool duplicate = false;
                for (auto r : layers)
                    if (std::abs(r - ratio) < .09)
                        duplicate = true;
                if (!duplicate) {
                    layers.push_back(ratio);
                    common += z.e.center;
                    n++;
                }
            }
            o.layers = layers.size();
            o.center = common / float(n);
            o.outer.center = o.center;
            auto ap = arrowProfile(chainMask, pivot, o.outer, cfg.arrow_prominence);
            auto ring = innerRing(dim, o.outer);
            o.arrows = ap.peaks;
            o.coverage = ap.mean;
            o.ring_contrast = ring.first;
            o.ring_support = ring.second;
            o.gap_cv = ap.spacing_cv;
            o.solid_fraction = ap.solid;
            o.peak_balance = ap.peak_balance;
            bool concentric = ring.first > cfg.inner_contrast && ring.second > cfg.inner_support;
            if (concentric)
                o.layers = std::max(1, o.layers);
            bool periodic = o.arrows >= cfg.arrow_peaks && ap.spacing_cv < cfg.max_gap_cv &&
                            ap.peak_balance > cfg.min_peak_balance;
            o.valid = rad > 27 && concentric && periodic;
            o.solid_bar = ap.solid > cfg.solid_run && (!periodic || (ring.second < .3 && ring.first < .1));
            // An intact outer contour around a dark, unoccluded interior is observable negative evidence.
            // Colored or bright foreground in the interior is treated as an occlusion, regardless of its hue.
            int interior = 0, occluded = 0;
            for (int y = std::max(0, int(o.center.y - rad * .6));
                 y < std::min(hsv.rows, int(o.center.y + rad * .6)); ++y)
                for (int x = std::max(0, int(o.center.x - rad * .6));
                     x < std::min(hsv.cols, int(o.center.x + rad * .6)); ++x)
                    if (cv::norm(cv::Point2f(x, y) - o.center) < rad * .6) {
                        ++interior;
                        auto pixel = hsv.at<cv::Vec3b>(y, x);
                        if (pixel[2] > 55 && (pixel[0] > 45 || pixel[1] < 65))
                            ++occluded;
                    }
            o.clear_absence = !concentric && ring.second < .20 && periodic && fit.error < .06 &&
                              interior > 0 && double(occluded) / interior < .10;
            o.score = (o.valid ? 10 : 0) + std::min(o.layers, 2) * .3 + std::min(o.arrows, 8) * .03 +
                      rad * .05 - fit.error * .5;
            bool duplicate = false;
            for (auto &old : result.candidates)
                if (cv::norm(old.center - o.center) < rad * .65) {
                    duplicate = true;
                    if (o.score > old.score)
                        old = o;
                    break;
                }
            if (!duplicate)
                result.candidates.push_back(o);
        }
    }
    if (scale != 1) {
        result.center.point *= scale;
        for (auto &o : result.candidates) {
            o.center *= scale;
            o.outer.center *= scale;
            o.outer.size.width *= scale;
            o.outer.size.height *= scale;
        }
        cv::resize(result.binary, result.binary, original.size(), 0, 0, cv::INTER_NEAREST);
    }
    return result;
}
} // namespace windmill

#include "windmill.hpp"
#include <iostream>
#include <stdexcept>
using namespace windmill;
void require(bool b, const char *message) {
    if (!b)
        throw std::runtime_error(message);
}
Detection scene(float rx, std::vector<float> targets, std::vector<float> bars = {}) {
    Detection d;
    d.center = {{rx, 300}, .9, true};
    for (float x : targets) {
        TargetObservation o;
        o.center = {rx + x, 300};
        o.outer = {o.center, {76, 80}, 0};
        o.valid = true;
        o.score = 11;
        d.candidates.push_back(o);
    }
    for (float x : bars) {
        TargetObservation o;
        o.center = {rx + x, 300};
        o.outer = {o.center, {76, 80}, 0};
        o.solid_bar = true;
        d.candidates.push_back(o);
    }
    return d;
}
int main() {
    Config c;
    Tracker t(c);
    auto a = t.update(scene(400, {180}), 0);
    int id = a.selected_id;
    require(a.selected.has_value(), "initial lock");
    for (int f = 1; f <= 20; f++) {
        auto d = scene(400 + f * 8, {180, -180});
        if (f % 2)
            std::reverse(d.candidates.begin(), d.candidates.end());
        auto r = t.update(d, f);
        require(r.selected_id == id && r.selected && r.selected->center.x > d.center.point.x,
                "camera movement or candidate reordering changed lock");
    }
    for (int f = 21; f <= 32; f++) {
        auto r = t.update(scene(560, {-180}), f);
        require(r.selected_id == id && !r.selected, "12 frame identity tolerance violated");
    }
    auto b = t.update(scene(560, {-180, 180}), 33);
    require(b.selected_id == id && b.selected, "recovery at tolerance boundary failed");
    for (int f = 34; f <= 35; f++) {
        auto r = t.update(scene(560, {-180}, {180}), f);
        require(r.selected_id == id && !r.selected, "invalidated before three frames");
    }
    auto r = t.update(scene(560, {-180}, {180}), 36);
    require(r.selected_id != id && r.selected && r.reason == "confirmed_B_reacquire",
            "B did not release on third frame");
    int second = r.selected_id;
    for (int f = 37; f <= 49; f++) {
        Detection d;
        r = t.update(d, f);
    }
    require(r.selected_id == 0 && !r.selected && r.reason == "lost_timeout",
            "center loss must expire at frame13");
    r = t.update(scene(900, {180}), 50);
    require(r.selected_id > second, "released ID reused");
    Tracker one(c);
    one.update(scene(400, {180, -180}), 0);
    one.update(scene(400, {180}), 1);
    int observations = 0;
    for (auto &s : one.states())
        observations += s.observed;
    require(observations == 1, "multiple tracks consumed one observation");
    Tracker absent(c);
    absent.update(scene(400, {180}), 0);
    for (int f = 1; f <= 3; ++f) {
        auto d = scene(400, {-180}, {180});
        d.candidates.back().solid_bar = false;
        d.candidates.back().clear_absence = true;
        auto result = absent.update(d, f);
        require(f == 3 ? result.reason == "clear_structure_absence_reacquire" : result.selected_id == 1,
                "clear absence must wait for three consecutive observations");
    }
    std::cout << "Tracking state-machine checks passed\n";
}

#include "common.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>

namespace {
constexpr int LOST_TOLERANCE=12;
struct Circle {cv::Point2d center; double radius, score;};
struct Center {cv::Point2d point;double score;};
cv::Mat orange_mask(const cv::Mat& frame,int min_s,int min_v) {
    cv::Mat hsv,mask;cv::cvtColor(frame,hsv,cv::COLOR_BGR2HSV);
    cv::inRange(hsv,cv::Scalar(0,min_s,min_v),cv::Scalar(30,255,255),mask);
    return mask;
}
std::optional<Center> find_center(const cv::Mat& mask,const cv::Mat& templ,std::optional<cv::Point2d> previous) {
    cv::Mat labels,stats,centroids;
    int count=cv::connectedComponentsWithStats(mask,labels,stats,centroids,8);
    cv::Mat ref;cv::resize(templ,ref,{32,32},0,0,cv::INTER_NEAREST);cv::threshold(ref,ref,127,255,cv::THRESH_BINARY);
    Center best{{0,0},0};
    for(int i=1;i<count;++i){
        int x=stats.at<int>(i,cv::CC_STAT_LEFT),y=stats.at<int>(i,cv::CC_STAT_TOP);
        int w=stats.at<int>(i,cv::CC_STAT_WIDTH),h=stats.at<int>(i,cv::CC_STAT_HEIGHT),area=stats.at<int>(i,cv::CC_STAT_AREA);
        if(w<12||w>36||h<12||h>36||area<100||area>550)continue;
        cv::Rect rect(x,y,w,h);cv::Mat component=(labels(rect)==i),resized;
        cv::resize(component,resized,{32,32},0,0,cv::INTER_NEAREST);
        cv::Mat intersection,uni;cv::bitwise_and(resized,ref,intersection);cv::bitwise_or(resized,ref,uni);
        double score=double(cv::countNonZero(intersection))/std::max(1,cv::countNonZero(uni));
        cv::Point2d pos(centroids.at<double>(i,0),centroids.at<double>(i,1));
        if(previous && cv::norm(pos-*previous)>75)continue;
        if(score>best.score)best={pos,score};
    }
    if(best.score<0.62)return std::nullopt;
    return best;
}
double disk_fraction(const cv::Mat& mask,cv::Point2d center,double radius){
    int x0=std::max(0,int(center.x-radius)),x1=std::min(mask.cols-1,int(center.x+radius));
    int y0=std::max(0,int(center.y-radius)),y1=std::min(mask.rows-1,int(center.y+radius));
    int all=0,on=0;
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
        if(std::hypot(x-center.x,y-center.y)<radius){++all;if(mask.at<uchar>(y,x))++on;}
    }
    return all?double(on)/all:1;
}
std::vector<Circle> find_targets(const cv::Mat& mask,const cv::Mat& bright,const cv::Mat& green,cv::Point2d pivot,bool large){
    cv::Mat labels,stats,centroids;
    int count=cv::connectedComponentsWithStats(mask,labels,stats,centroids,8);
    std::vector<Circle> result;
    for(int i=1;i<count;++i){
        int x=stats.at<int>(i,cv::CC_STAT_LEFT),y=stats.at<int>(i,cv::CC_STAT_TOP);
        int w=stats.at<int>(i,cv::CC_STAT_WIDTH),h=stats.at<int>(i,cv::CC_STAT_HEIGHT),area=stats.at<int>(i,cv::CC_STAT_AREA);
        if(large){
            if(w<36||w>68||h<36||h>68||area<240||area>1400)continue;
        }else if(w<60||w>110||h<60||h>110||area<350||area>2400)continue;
        double ratio=double(w)/h;if(ratio<0.75||ratio>1.30)continue;
        cv::Point2d center(x+w/2.0,y+h/2.0);double radius=(w+h)/4.0;
        double distance=cv::norm(center-pivot);
        if(distance<(large?95:105)||distance>320)continue;
        double inner=disk_fraction(mask,center,radius*0.5);
        if(inner>0.10)continue; // Glowing blade sectors and concentric sight markers are not empty target rings.
        if(disk_fraction(green,center,radius*1.5)>0.12)continue; // Green impact cloud invalidates an otherwise visible ring.
        cv::Rect rect(x,y,w,h);
        cv::Mat member=(labels(rect)==i),bright_member;
        cv::bitwise_and(member,bright(rect),bright_member);
        double bright_ratio=double(cv::countNonZero(bright_member))/area;
        if(bright_ratio<(large?0.18:0.25))continue; // A dim ring remains visible after an invalidated target.
        double score=2*bright_ratio+1.0-inner-std::abs(w-h)/double(std::max(w,h));
        result.push_back({center,radius,score});
    }
    std::sort(result.begin(),result.end(),[](const Circle& a,const Circle& b){return a.score>b.score;});
    return result;
}
struct Tracker {
    int id=0,next_id=1,lost=0,detected=0,reselected=0,missing=0;
    std::optional<Circle> selected;
    cv::Point2d relative{0,0};
    std::optional<cv::Point2d> pending_relative;
    std::optional<cv::Point2d> blocked_relative;
    int blocked_age=0;
    int pending_count=0;
    std::vector<int> switch_frames;
    std::vector<int> hit_frames;
    std::optional<Circle> update(const std::vector<Circle>& candidates,const cv::Mat& green,cv::Point2d pivot,int frame) {
        if(id && selected && disk_fraction(green,pivot+relative,selected->radius*1.5)>0.12){
            blocked_relative=relative;blocked_age=0;hit_frames.push_back(frame);
            id=0;selected.reset();lost=0;pending_relative.reset();pending_count=0;
        }
        std::vector<Circle> available;
        int blocked_match=-1;double blocked_distance=std::numeric_limits<double>::infinity();
        if(blocked_relative){
            ++blocked_age;
            for(size_t i=0;i<candidates.size();++i){
                double distance=cv::norm((candidates[i].center-pivot)-*blocked_relative);
                if(distance<blocked_distance){blocked_distance=distance;blocked_match=int(i);}
            }
            if(blocked_match>=0 && blocked_distance<50)
                blocked_relative=candidates[blocked_match].center-pivot;
            else blocked_match=-1;
            if(blocked_age>150){blocked_relative.reset();blocked_match=-1;}
        }
        for(size_t i=0;i<candidates.size();++i)
            if(int(i)!=blocked_match)available.push_back(candidates[i]);
        int match=-1;double best=std::numeric_limits<double>::infinity();
        if(id){
            for(size_t i=0;i<available.size();++i){
                double distance=cv::norm((available[i].center-pivot)-relative);
                if(distance<best && distance<35+lost*8){best=distance;match=int(i);}
            }
        }
        if(match>=0){selected=available[match];relative=selected->center-pivot;lost=0;pending_relative.reset();pending_count=0;++detected;return selected;}
        if(id){
            ++lost;
            if(!available.empty()){
                cv::Point2d alternative=available.front().center-pivot;
                if(pending_relative && cv::norm(alternative-*pending_relative)<40)++pending_count;
                else {pending_relative=alternative;pending_count=1;}
                if(pending_count>=3){
                    id=next_id++;selected=available.front();relative=alternative;lost=0;
                    pending_relative.reset();pending_count=0;++detected;++reselected;switch_frames.push_back(frame);
                    return selected;
                }
            }else {pending_relative.reset();pending_count=0;}
            if(lost<=LOST_TOLERANCE){++missing;return std::nullopt;}
            id=0;selected.reset();
        }
        if(!available.empty()){
            id=next_id++;selected=available.front();relative=selected->center-pivot;lost=0;
            ++detected;if(id>1){++reselected;switch_frames.push_back(frame);}return selected;
        }
        ++missing;return std::nullopt;
    }
};
}
int main(int argc,char** argv) try {
    namespace fs=std::filesystem;
    fs::path input=vision::arg_value(argc,argv,"--input","resources/task_3.mp4");
    fs::path out=vision::arg_value(argc,argv,"--output","result/task3_windmill/task_3");
    fs::path template_path=vision::arg_value(argc,argv,"--template","config/r_template.png");
    bool large=vision::arg_value(argc,argv,"--mode",input.filename()=="task_4.mp4"?"large":"small")=="large";
    cv::Mat templ=cv::imread(template_path.string(),cv::IMREAD_GRAYSCALE);
    if(templ.empty())throw std::runtime_error("Cannot read R template");
    cv::VideoCapture cap(input.string());if(!cap.isOpened())throw std::runtime_error("Cannot read "+input.string());
    double fps=cap.get(cv::CAP_PROP_FPS);cv::Size size(int(cap.get(cv::CAP_PROP_FRAME_WIDTH)),int(cap.get(cv::CAP_PROP_FRAME_HEIGHT)));
    int expected=int(cap.get(cv::CAP_PROP_FRAME_COUNT));if(fps<=0||size.width<=0)throw std::runtime_error("Invalid video metadata");
    auto video=vision::open_writer(out/"recognition_overlay.mp4",fps,size);
    auto binary=vision::open_writer(out/"binary_process.mp4",fps,size);
    Tracker tracker;std::optional<cv::Point2d> previous_center;int center_seen=0,frame_index=0;cv::Mat frame;
    while(cap.read(frame)){
        cv::Mat bright=orange_mask(frame,130,120),broad=orange_mask(frame,80,55);
        cv::Mat hsv,green;cv::cvtColor(frame,hsv,cv::COLOR_BGR2HSV);
        cv::inRange(hsv,cv::Scalar(30,30,20),cv::Scalar(100,255,255),green);
        auto center=find_center(bright,templ,previous_center);
        if(!center)center=find_center(bright,templ,std::nullopt);
        std::vector<Circle> candidates;
        if(center){
            previous_center=center->point;++center_seen;
            candidates=find_targets(broad,bright,green,center->point,large);
        }
        cv::Mat overlay=frame.clone(),binary_bgr;cv::cvtColor(broad,binary_bgr,cv::COLOR_GRAY2BGR);
        std::optional<Circle> chosen;
        if(center){
            cv::circle(overlay,center->point,5,{0,255,0},-1);
            vision::label(overlay,"R center",center->point+cv::Point2d(12,-12),{0,255,0});
            cv::circle(binary_bgr,center->point,7,{0,255,0},2);
            chosen=tracker.update(candidates,green,center->point,frame_index);
        }else{
            ++tracker.missing;
            if(tracker.id && ++tracker.lost>LOST_TOLERANCE){tracker.id=0;tracker.selected.reset();}
            tracker.pending_relative.reset();tracker.pending_count=0;
            previous_center.reset();
        }
        for(const auto& c:candidates)cv::circle(binary_bgr,c.center,int(c.radius),{0,255,255},2);
        if(chosen&&center){
            cv::circle(overlay,chosen->center,int(chosen->radius),{0,255,255},3);
            cv::circle(overlay,chosen->center,5,{0,0,255},-1);
            cv::line(overlay,center->point,chosen->center,{255,255,0},2);
            double theta=std::atan2(center->point.y-chosen->center.y,chosen->center.x-center->point.x);
            vision::label(overlay,cv::format("ID %d  detected  theta %.2f rad",tracker.id,theta),{30,45},{0,255,0});
        }else vision::label(overlay,cv::format("ID %d  lost",tracker.id),{30,45},{0,0,255});
        vision::label(overlay,cv::format("frame %d / %d",frame_index,expected),{30,80});
        video.write(overlay);binary.write(binary_bgr);++frame_index;
    }
    vision::ensure_dir(out);std::ofstream stats(out/"tracking_stats.txt");
    stats<<"input="<<input<<"\nframes="<<frame_index<<"\nfps="<<fps<<"\nwidth="<<size.width<<"\nheight="<<size.height
         <<"\ncenter_detected_frames="<<center_seen<<"\ntarget_detected_frames="<<tracker.detected
         <<"\nlost_frames="<<tracker.missing<<"\nnew_ids="<<tracker.next_id-1<<"\nreselect_count="<<tracker.reselected
         <<"\nlost_tolerance="<<LOST_TOLERANCE<<"\nreselection_frames=";
    for(auto f:tracker.switch_frames)stats<<f<<",";stats<<"\n";
    stats<<"impact_frames=";for(auto f:tracker.hit_frames)stats<<f<<",";stats<<"\n";
    std::cout<<input<<": "<<frame_index<<" frames, center="<<center_seen<<", target="<<tracker.detected<<", IDs="<<tracker.next_id-1<<"\n";
    return frame_index==expected?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

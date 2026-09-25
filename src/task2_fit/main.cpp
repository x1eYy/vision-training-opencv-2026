#include "common.hpp"
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>

namespace {
constexpr double PI=3.14159265358979323846;
struct Sample {int frame; double t, theta; cv::Point2d point;};
struct Fit {double omega=0, c=0,b=0,u=0,v=0,sse=std::numeric_limits<double>::infinity();};
double angle(const cv::Point2d& p) {return std::atan2(360.0-p.y,p.x-480.0);}
bool detect(const cv::Mat& frame, cv::Point2d& point) {
    cv::Mat hsv,mask; cv::cvtColor(frame,hsv,cv::COLOR_BGR2HSV);
    cv::inRange(hsv,cv::Scalar(80,110,110),cv::Scalar(105,255,255),mask);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask,contours,cv::RETR_EXTERNAL,cv::CHAIN_APPROX_SIMPLE);
    double best=-1;
    for (const auto& c:contours) {
        double area=cv::contourArea(c); if(area<10 || area>1000) continue;
        auto m=cv::moments(c); if(m.m00<=0) continue;
        cv::Point2d p(m.m10/m.m00,m.m01/m.m00);
        double radius=cv::norm(p-cv::Point2d(480,360));
        if(std::abs(radius-220)>35) continue;
        if(area>best) {best=area;point=p;}
    }
    return best>0;
}
Fit fit_at(const std::vector<Sample>& data, double omega) {
    Eigen::Matrix4d normal=Eigen::Matrix4d::Zero(); Eigen::Vector4d rhs=Eigen::Vector4d::Zero();
    for(const auto& s:data) {
        Eigen::Vector4d x(1,s.t,std::cos(omega*s.t),std::sin(omega*s.t));
        normal.noalias()+=x*x.transpose(); rhs.noalias()+=x*s.theta;
    }
    Eigen::Vector4d q=normal.ldlt().solve(rhs);
    Fit f; f.omega=omega;f.c=q[0];f.b=q[1];f.u=q[2];f.v=q[3];
    double A=omega*std::hypot(f.u,f.v);
    if(!q.allFinite() || f.b<=A || A<=0) return f;
    f.sse=0;
    for(const auto& s:data) {
        double e=s.theta-(f.c+f.b*s.t+f.u*std::cos(omega*s.t)+f.v*std::sin(omega*s.t));
        f.sse+=e*e;
    }
    return f;
}
Fit fit(const std::vector<Sample>& data) {
    Fit best;
    for(double w=0.05;w<=12.0;w+=0.01) {auto f=fit_at(data,w);if(f.sse<best.sse) best=f;}
    if(!std::isfinite(best.sse)) throw std::runtime_error("No feasible fit");
    double lo=std::max(0.001,best.omega-0.02),hi=best.omega+0.02;
    for(int j=0;j<80;++j) {
        double l=lo+(hi-lo)*0.382,h=lo+(hi-lo)*0.618;
        if(fit_at(data,l).sse<fit_at(data,h).sse) hi=h; else lo=l;
    }
    auto refined=fit_at(data,(lo+hi)/2);
    return refined.sse<best.sse?refined:best;
}
double predict(const Fit& f,double t){return f.c+f.b*t+f.u*std::cos(f.omega*t)+f.v*std::sin(f.omega*t);}
double velocity(const Fit& f,double t){return f.b+f.omega*(-f.u*std::sin(f.omega*t)+f.v*std::cos(f.omega*t));}
void chart(const std::filesystem::path& file, const std::string& title,
           const std::vector<double>& xs, const std::vector<double>& ys,
           const std::vector<double>& fitted, const std::string& ylabel) {
    const int W=1200,H=650,L=95,R=45,T=65,B=85;
    cv::Mat pic(H,W,CV_8UC3,cv::Scalar(255,255,255));
    double xmin=*std::min_element(xs.begin(),xs.end()),xmax=*std::max_element(xs.begin(),xs.end());
    double ymin=*std::min_element(ys.begin(),ys.end()),ymax=*std::max_element(ys.begin(),ys.end());
    if(!fitted.empty()){ymin=std::min(ymin,*std::min_element(fitted.begin(),fitted.end())); ymax=std::max(ymax,*std::max_element(fitted.begin(),fitted.end()));}
    double pad=std::max(0.01,(ymax-ymin)*0.08);ymin-=pad;ymax+=pad;
    auto pt=[&](double x,double y){return cv::Point(L+int((x-xmin)/(xmax-xmin)*(W-L-R)),H-B-int((y-ymin)/(ymax-ymin)*(H-T-B)));};
    for(int i=0;i<=5;++i){
        int y=T+i*(H-T-B)/5;cv::line(pic,{L,y},{W-R,y},{225,225,225},1);
        double val=ymax-i*(ymax-ymin)/5;
        cv::putText(pic,cv::format("%.3f",val),{8,y+5},cv::FONT_HERSHEY_SIMPLEX,.55,{60,60,60},1);
    }
    cv::line(pic,{L,T},{L,H-B},{50,50,50},2);cv::line(pic,{L,H-B},{W-R,H-B},{50,50,50},2);
    for(int i=0;i<=6;++i){double x=xmin+i*(xmax-xmin)/6;auto p=pt(x,ymin);cv::putText(pic,cv::format("%.1f",x),{p.x-15,H-B+27},cv::FONT_HERSHEY_SIMPLEX,.55,{60,60,60},1);}
    for(size_t i=0;i<xs.size();++i) cv::circle(pic,pt(xs[i],ys[i]),2,{210,90,35},-1);
    for(size_t i=1;i<fitted.size();++i) cv::line(pic,pt(xs[i-1],fitted[i-1]),pt(xs[i],fitted[i]),{35,45,205},2,cv::LINE_AA);
    cv::putText(pic,title,{L,T-20},cv::FONT_HERSHEY_SIMPLEX,.85,{20,20,20},2);
    cv::putText(pic,"Time (s)",{W/2,H-20},cv::FONT_HERSHEY_SIMPLEX,.7,{30,30,30},2);
    cv::putText(pic,ylabel,{L+10,T+25},cv::FONT_HERSHEY_SIMPLEX,.55,{50,50,50},1);
    if(!fitted.empty()){cv::putText(pic,"blue: observation",{W-340,T+20},cv::FONT_HERSHEY_SIMPLEX,.55,{210,90,35},1);
        cv::putText(pic,"red: model",{W-340,T+45},cv::FONT_HERSHEY_SIMPLEX,.55,{35,45,205},1);}
    vision::save_image(file,pic);
}
}
int main(int argc,char** argv) try {
    namespace fs=std::filesystem;
    fs::path input=vision::arg_value(argc,argv,"--input","resources/task_2.mp4");
    fs::path out=vision::arg_value(argc,argv,"--output","result/task2_fit");
    fs::path report=vision::arg_value(argc,argv,"--report","result/task2_fit_result.md");
    cv::VideoCapture cap(input.string());if(!cap.isOpened())throw std::runtime_error("Cannot open "+input.string());
    double fps=cap.get(cv::CAP_PROP_FPS);int total=int(cap.get(cv::CAP_PROP_FRAME_COUNT));
    if(fps<=0)throw std::runtime_error("Invalid FPS");
    std::vector<Sample> data; cv::Mat frame; int idx=0;double prev=0;
    while(cap.read(frame)){
        cv::Point2d point;
        if(detect(frame,point)) {
            double a=angle(point);
            if(!data.empty()) {while(a-prev>PI)a-=2*PI;while(a-prev<-PI)a+=2*PI;}
            prev=a;data.push_back({idx,idx/fps,a,point});
        }
        ++idx;
    }
    if(data.size()<100)throw std::runtime_error("Too few cyan target observations");
    Fit f=fit(data);double A=f.omega*std::hypot(f.u,f.v);
    double phase=std::atan2(f.v,-f.u);if(phase>=PI)phase-=2*PI;
    double theta0=f.c+f.u;
    std::vector<double> xs,ys,model,residual,vel;
    for(const auto& s:data){xs.push_back(s.t);ys.push_back(s.theta);model.push_back(predict(f,s.t));residual.push_back(s.theta-model.back());vel.push_back(velocity(f,s.t));}
    vision::ensure_dir(out);
    chart(out/"fit_comparison.png","Unwrapped angle: observation vs fit",xs,ys,model,"Angle (rad)");
    chart(out/"residuals.png","Angle residual (observation - fit)",xs,residual,{},"Residual (rad)");
    chart(out/"angular_velocity.png","Estimated angular velocity",xs,vel,{},"Angular velocity (rad/s)");
    cap.release();cap.open(input.string());
    auto writer=vision::open_writer(out/"tracking_overlay.mp4",fps,{int(cap.get(cv::CAP_PROP_FRAME_WIDTH)),int(cap.get(cv::CAP_PROP_FRAME_HEIGHT))});
    idx=0;size_t j=0;
    while(cap.read(frame)){
        cv::Mat overlay=frame.clone();cv::circle(overlay,{480,360},5,{255,255,255},2);
        if(j<data.size() && data[j].frame==idx){
            cv::circle(overlay,data[j].point,13,{0,255,0},2);cv::line(overlay,{480,360},data[j].point,{0,255,0},2);
            vision::label(overlay,"detected",{20,40},{0,255,0});++j;
        }else vision::label(overlay,"lost",{20,40},{0,0,255});
        vision::label(overlay,cv::format("frame %d / %d",idx,total),{20,75});writer.write(overlay);++idx;
    }
    double rmse=std::sqrt(f.sse/data.size());
    vision::ensure_dir(report.parent_path());std::ofstream md(report);
    md<<std::fixed<<std::setprecision(8);
    md<<"# 任务 2：合成视频角度拟合结果\n\n";
    md<<"输入：`"<<input.string()<<"`；"<<idx<<" 帧，"<<fps<<" FPS。第 0 帧为 t=0。\n\n";
    md<<"模型：ω(t)=b+A sin(Ωt+φ)；θ(t)=θ₀+bt+(A/Ω)[cosφ−cos(Ωt+φ)]。图像角度为 atan2(360−y,x−480)，逐帧展开。\n\n";
    md<<"| 参数 | 估计值 | 单位 |\n|---|---:|---|\n";
    md<<"| A | "<<A<<" | rad/s |\n| b | "<<f.b<<" | rad/s |\n| Ω | "<<f.omega<<" | rad/s |\n| φ | "<<phase<<" | rad |\n| θ₀ | "<<theta0<<" | rad |\n";
    md<<"\n方法：扫描 Ω ∈ [0.05,12] rad/s（步长 0.01），各点用线性最小二乘求 c、b、u、v；舍弃不满足 A>0、b>A 的解，在最优点附近做一维细化。最终 Ω="<<f.omega<<"；最终平方误差="<<f.sse<<"，求解状态：可行并收敛。\n\n";
    md<<"角度 RMSE：**"<<rmse<<" rad**；有效样本 "<<data.size()<<"，帧范围 "<<data.front().frame<<"–"<<data.back().frame<<"。φ 归一到 [−π,π)。\n\n";
    md<<"[标注视频](task2_fit/tracking_overlay.mp4) · [观测与拟合](task2_fit/fit_comparison.png) · [角速度](task2_fit/angular_velocity.png) · [残差](task2_fit/residuals.png)\n";
    std::cout<<"Task 2: "<<data.size()<<" observations, RMSE="<<rmse<<" rad, A="<<A<<", b="<<f.b<<", Omega="<<f.omega<<", phi="<<phase<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

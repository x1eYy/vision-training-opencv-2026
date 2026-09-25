# 第二次培训：OpenCV C++ 图像处理与目标跟踪

本工程对应讲义中的三个任务。所有结果均由本仓库的 C++ 程序从 `resources/` 素材生成；程序不依赖图形桌面，适合命令行运行。任务 3 仅处理视觉检测与稳定锁定，不把播放时间当成真实采集时间进行速度拟合。

## 环境与构建

在 Ubuntu 22.04 上验证：GCC 11、CMake 3.22、OpenCV 4.5.4、Eigen 3.4。安装依赖：

```bash
sudo apt update
sudo apt install build-essential cmake libopencv-dev libeigen3-dev
cmake -S . -B build
cmake --build build -j4
```

三个程序分别为 `build/task1_image`、`build/task2_fit`、`build/task3_windmill`。运行前请切换到工程根目录；所有参数路径均相对于运行时目录。`build/` 为本机产物，无需提交。

## 输入与运行

`resources/test_image.jpg` 为讲义指定郁金香图；`resources/task_2.mp4`、`task_3.mp4`、`task_4.mp4` 为随作业发放的视频，按原文件名放在 `resources/`。本仓库包含本次实际使用的素材。

```bash
./build/task1_image --input resources/test_image.jpg --output result/task1_images
./build/task2_fit --input resources/task_2.mp4 --output result/task2_fit --report result/task2_fit_result.md
./build/task3_windmill --input resources/task_3.mp4 --output result/task3_windmill/task_3 --mode small
./build/task3_windmill --input resources/task_4.mp4 --output result/task3_windmill/task_4 --mode large
```

`task3_windmill` 的 `--template` 默认为 `config/r_template.png`。`--mode small` 针对较大的扇叶端部圆环，`--mode large` 针对大能量场景扇叶端部的小圆靶心；两种模式共用中心检测、跨帧关联、丢失和重选逻辑。

## 任务 1：图像处理与结果

输入图像为 1280×853。程序检查读图结果，输出讲义所列的 16 张图：

| 操作 | 文件 | 参数与说明 |
|---|---|---|
| 灰度与 HSV 通道 | `gray.png`、`hsv_h.png`、`hsv_s.png`、`hsv_v.png` | BGR 转灰度/HSV，三个 HSV 通道单独存为灰度图 |
| 均值、高斯、中值滤波 | `mean_filter.png`、`gaussian_filter.png`、`median_filter.png` | 核均为 5×5；高斯 σ=1.5 |
| 红色提取 | `red_mask.png` | HSV 范围 H=0–10 或 170–179，S≥100，V≥80 |
| 形态学 | `erode.png`、`dilate.png`、`open.png`、`close.png` | 5×5 椭圆核；闭运算接在开运算结果后 |
| 轮廓与外接框 | `contours_boxes.png` | 仅保留外轮廓，面积阈值 200 px²；绿色轮廓、红色矩形、黄色面积数字 |
| 绘制与变换 | `drawing.png`、`rotated_35deg.png`、`crop_top_left.png` | 绘制圆/矩形/文字；绕中心旋转 35°；左上角取原宽高各一半 |

本次筛选出的外轮廓面积为 **4114.0、907.5、48449.5、236366.5 px²**，程序还将原始列表写入 `result/task1_images/contour_areas.txt`。阈值主要保留红色花瓣，部分黄色边缘因色相和饱和度变化未进入掩膜，阴影中的低亮度红色仍可能保留。相邻花瓣连接后会成为同一连通区域，所以大矩形表示红色连通区域，而非一朵完整的花。均值滤波使边缘较模糊；高斯滤波较平滑且保留更多边界结构；中值滤波能抑制孤立噪点，但细小纹理会变弱。

## 任务 2：合成旋转视频

输入为 960×720、60 FPS、1440 帧。青色目标通过 HSV 阈值和半径约 220 px 的几何限制逐帧提取；第 0 帧为 `t=0`，角度按 `atan2(360-y, x-480)` 计算并逐帧展开。使用积分形式拟合角度，随后由模型计算角速度。参数、误差、帧范围和方法见 [任务 2 独立说明](result/task2_fit_result.md)。

- [完整跟踪标注视频](result/task2_fit/tracking_overlay.mp4)
- [角度观测与拟合曲线](result/task2_fit/fit_comparison.png)
- [估计角速度曲线](result/task2_fit/angular_velocity.png)
- [角度残差图](result/task2_fit/residuals.png)

## 任务 3：真实视频视觉跟踪

`task_3.mp4` 为小能量机关场景，1440×1080、30 FPS、796 帧；`task_4.mp4` 为大能量机关场景，1440×1080、30 FPS、1800 帧。逐帧从橙红色 R 标形状找移动中心，不使用固定中心；相对位置、稳定 ID、`detected/lost` 状态均绘制在原图。正常漏检容忍 12 帧；旧目标被绿色击中特效覆盖时立即失效，排除该目标并选择新的有效圆。详情、检测统计、重选帧和已知失败情况见 [任务 3 独立说明](result/task3_tracking_result.md)。

| 输入 | 完整叠加视频 | 二值化过程视频 |
|---|---|---|
| `task_3.mp4` | [recognition_overlay.mp4](result/task3_windmill/task_3/recognition_overlay.mp4) | [binary_process.mp4](result/task3_windmill/task_3/binary_process.mp4) |
| `task_4.mp4` | [recognition_overlay.mp4](result/task3_windmill/task_4/recognition_overlay.mp4) | [binary_process.mp4](result/task3_windmill/task_4/binary_process.mp4) |

输出视频保留输入的顺序、帧数、分辨率和固定帧率。两个子目录的 `tracking_stats.txt` 由程序自动生成，可核对检测数量。

## 工程结构与复现检查

`include/common.hpp` 和 `src/common/common.cpp` 提供保存图片、打开视频写入器与命令行参数处理。三个任务的入口分别在 `src/task1_image/`、`src/task2_fit/`、`src/task3_windmill/`。`config/r_template.png` 是从本次素材截取的 R 字母二值模板，用于形状比对；更换不同字体或拍摄尺度的素材时应重新校准模板与阈值。

复现后应看到 16 张任务 1 PNG、任务 2 的 3 张 PNG 与 1 个 MP4、任务 3 的 4 个 MP4。可用 OpenCV 或播放器检查文件可读，并核对标注视频的帧数与 FPS。结果可能因 OpenCV 编码器版本不同而有轻微体积差异。

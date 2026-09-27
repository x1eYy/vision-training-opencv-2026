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

`task3_windmill` 的 `--template` 默认为 `config/r_template.png`，`--config` 默认为 `config/windmill.yaml`。两个 mode 使用统一的 A/B 语义和检测逻辑。

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

## 任务 3：真实视频视觉跟踪（重构版）

**A：箭头链连接的同心圆，为有效目标。B：实心灯条连接的单层空心圆，排除。** 两个视频使用相同定义和检测器。绿色物体不会直接触发击中判定。

场景对应关系：`task_3.mp4` 是小能量机关场景，最多同时亮起一个目标；`task_4.mp4` 是大能量机关场景，最多同时亮起两个目标。`task_3.mp4` 为 1440×1080、30 FPS、796 帧；`task_4.mp4` 为 1440×1080、30 FPS、1800 帧。画面显示逐帧检测的 R 中心、拟合的外层椭圆、目标中心、连线、ID 和 `detected/lost`。中心不可靠或当前目标未匹配时，不绘制假定的有效目标，也不输出角度。

### 检测与身份保持

1. 在等效宽度 1440 的尺度处理，输出映射回原始尺寸。橙红色掩膜采用多个亮度阈值和 3×3 闭运算；提取所有层级轮廓并拟合椭圆。
2. 内外轮廓中心接近、径向亮环有足够覆盖与对比度，才具有同心证据。外环断裂时，从内环引导搜索实际外环像素，再拟合椭圆。内外边缘的重复候选合并。
3. 在 R 到候选的走廊内检查亮峰数量和峰间距规律。实心灯条和没有内圈的 B 不构成有效 A。
4. R 通过二值模板形状及周围目标布局评分，不把历史坐标当作当前观测。
5. 为每个有效候选维护轨迹，按相对 R 的位置、大小、外观和帧间旋转变化进行带门限的一对一关联。被选目标仍被观测到时不改变选中 ID。
6. 漏检保留 ID **12 帧**，显示 `lost`；第 13 个连续漏检帧允许释放。连续 **3 帧**有清楚的 B 或结构消失证据时提前释放；全黑、遮挡和仅出现另一个目标不属于失效证据。退休 ID 不复用。空闲时选择仍在观测中的最早轨迹。

`--config config/windmill.yaml` 可调整模板分数、轮廓误差、同心偏移、断环覆盖、内环对比度、箭头周期性、丢失容忍和失效确认帧数。`--mode small/large` 保留旧命令兼容，目前两者使用同一套已验证参数。

### 输出与复核

| 输入 | 原图叠加视频 | 二值化过程视频 | 逐帧记录 |
|---|---|---|---|
| `task_3.mp4` | [recognition_overlay.mp4](result/task3_windmill/task_3/recognition_overlay.mp4) | [binary_process.mp4](result/task3_windmill/task_3/binary_process.mp4) | [frames.csv](result/task3_windmill/task_3/frames.csv) |
| `task_4.mp4` | [recognition_overlay.mp4](result/task3_windmill/task_4/recognition_overlay.mp4) | [binary_process.mp4](result/task3_windmill/task_4/binary_process.mp4) | [frames.csv](result/task3_windmill/task_4/frames.csv) |

每个目录另有 `candidates.csv`（所有几何候选与各项分数）、`tracks.csv`（各轨迹的观测、丢失、失效、退休状态）和 `tracking_stats.txt`。二值化视频中蓝色为几何候选、绿色为有效 A、黄色为最终锁定；这些标记叠加在二值掩膜上。CSV 的帧号从 **0** 开始，时间为 `frame / FPS`。ID=0 表示没有保留中的目标身份。

详见 [任务 3 独立说明](result/task3_tracking_result.md)、[参考集指标](result/task3_windmill/validation/metrics.json)、[视频完整性](result/task3_windmill/validation/video_integrity.json) 和 [全部事件索引](result/task3_windmill/validation/events.json)。检测数量不等于识别正确数量。

### 可复现验证

评估脚本需要 `python3-opencv`、`python3-numpy` 和 `ffmpeg`。每个视频固定 40 个代表帧；参考椭圆为候选辅助、目视审核与部分手动修正的开发标注，**不是独立人工真值或留出测试集**。用户可检查 `config/windmill_reference.json` 中的标注及 `validation/` 中对应原图候选叠加图；任务 3 第 460 帧是结构转换期，明确从清晰帧指标中排除。

```bash
ctest --test-dir build --output-on-failure
./build/windmill_reference_check
python3 src/task3_windmill/evaluate.py
python3 src/task3_windmill/verify_outputs.py
python3 src/task3_windmill/review_events.py
```

## 工程结构与复现检查

根目录保持讲义目录。`include/common.hpp` 和 `src/common/common.cpp` 提供通用读写工具；`include/windmill.hpp` 声明检测、轨迹和配置数据结构。任务 3 的 `main.cpp` 只组织读帧、检测、跟踪、绘制、写出；`detector.cpp` 实现几何与箭头链检测，`tracker.cpp` 维护身份，`render.cpp` 绘制结果。`tracker_test.cpp` 检查候选重排、中心移动、双目标、12 帧恢复、3 帧失效及 ID 不复用。

完整结果包括任务 1 的 16 张 PNG、任务 2 的 3 张 PNG 和 1 个 MP4、任务 3 的 4 个 MP4。视频逐帧顺序写出，不插帧或跳帧；验证程序完整解码、检查 CSV 帧序和视频时间戳，并比较原图与叠加图的对应内容。MP4 使用 OpenCV 的 `mp4v` 编码，不包含原音轨。

学习用的详细项目文档位于仓库目录之外，不随作业提交。

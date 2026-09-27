# 任务 3：有效目标圆识别与身份跟踪结果

## 1. 本次修正与目标定义

**有效 A 是箭头链连接的同心圆；实心灯条连接的空心圆 B 排除。** 本次重构替换了原先判反的“空心程度高即有效”规则，删除绿色击中判断、固定 150 次更新排除名单和“出现另一个候选就提前切换”的逻辑。

两个输入使用同一个 C++ 检测器和跟踪器；`--mode small/large` 仅保留接口，目前参数相同。任务 1、任务 2 的算法与结果未修改。

## 2. 检测、绘制与关联方法

- 多亮度橙红色掩膜（V=20/35/55/85）配合 3×3 闭运算，提取所有层级轮廓并拟合椭圆。长度在等效宽度 1440 下定义，输入/输出之间按图像尺度转换。
- 将中心接近、半径递增的轮廓组成同心证据，合并圆环内外边缘重复候选。暗内圈另用 72 个角度的径向覆盖/背景对比验证，覆盖 >0.80、对比 >0.22。
- 外环断裂时，以内圈为引导，从实际外环像素恢复椭圆。黄色轮廓取外层椭圆，目标中心取共同中心；没有用外接矩形代替圆心或固定半径。
- 在 R 与圆心之间检查重复箭头：至少 5 个亮峰，峰间距变异系数 <0.45，峰突出程度中位数/最大值 >0.25。后者排除实心灯条强峰周围的弱噪声峰。空心 B 缺少有效内环；灯条的连续亮段还提供失效证据。
- R 从当前帧二值形状模板候选检测，分数阈值 0.68，周围圆形布局帮助候选排序。中心不可靠时不输出有效角度。
- 对所有有效候选维护轨迹，使用相对 R 的位置、圆环尺寸、外观分数和帧间相对旋转变化关联，带门限的代价排序实现一对一匹配。轨迹 ID 单调增加、不复用；候选数组顺序不作为 ID。
- 原目标仍有效时保持锁定。普通漏检容忍 **12 帧**，期间保留 ID、显示 `lost`；第 13 个连续漏检帧允许释放。对应位置有清楚的 B 或完整外环内 A 结构消失证据时，连续 **3 帧**提前释放。全黑、外来遮挡和只出现另一个候选不能作为确认失效的理由。

清楚的结构消失要求外环贴合、内部低占用、连接结构可见，并排除内部明显外来彩色/明亮遮挡。这里没有特定绿色击中条件。运动量仅用于身份关联，不拟合真实角速度。

## 3. 固定参考帧验证

每个视频固定 40 个系统采样帧，合计 80 帧。参考记录包含 R、A 外椭圆和 B；任务 3 第 460 帧预先标为结构过渡，不参与清晰帧指标，其余帧均保留。参考集在调参前冻结，SHA-256：

`98e51d07cbfaf68b6e27cadc53bf7913fb5b5b2201713a3a29b269f3b216615c`

**标注是实验候选辅助、目视复核和部分手动外轮廓修正的开发参考集，不是独立人工真值或留出测试集。** 这些指标说明在当前参考集上通过门限，不能推出全视频每帧都正确，更不能保证其他素材或最终评分。R 的数值差异接近零也不能解释为独立亚像素精度，因为参考中心同样由模板候选辅助确定。

| 视频 | 有效 A 真阳性/漏检/误检 | 精确率 | 召回率 | 中心误差均值/最大值（半径比例） | 轮廓平均偏差均值/最大值（半径比例） |
|---|---:|---:|---:|---:|---:|
| task_3 | 29/0/0 | 100.00% | 100.00% | 0.82% / 3.13% | 3.44% / 5.83% |
| task_4 | 48/0/0 | 100.00% | 100.00% | 0.92% / 3.51% | 3.92% / 9.37% |

每个已匹配参考 A 的中心误差和平均轮廓偏差均不超过半径的 10%。轮廓距离为两条椭圆各采样 360 点后的双向平均最近距离。半径采用参考椭圆两半轴的平均值。

数据与复现：[参考标注](../config/windmill_reference.json)、[预测明细](task3_windmill/validation/predictions.csv)、[指标 JSON](task3_windmill/validation/metrics.json)。80 张参考帧候选叠加图位于同一 validation 目录，以 `task_3_帧号.jpg` / `task_4_帧号.jpg` 命名。

## 4. 双目标身份稳定性

- `task_4` 第 0–30 帧：选中 ID 1，ID 变化 0 次；观测相对角度的最大相邻变化为 0.1198 rad/帧。
- `task_4` 第 945–1080 帧：选中 ID 11，ID 变化 0 次；观测相对角度的最大相邻变化为 0.0789 rad/帧。

这些片段由原图与事件图目视检查，CSV 再核对身份和相对角度连续性；属于开发期检查。任务 4 的 ID 11 从第 915 帧获取后持续保留到第 1435 帧，期间另一有效候选没有抢走选中身份；短暂未观测时显示 `lost`。程序级状态机测试还覆盖候选重排、中心平移、12 帧恢复、13 帧超时、3 帧 B/结构消失确认以及一对一匹配。见 [连续性记录](task3_windmill/validation/continuity.json)。

## 5. 全部释放与重选事件

所有下表事件已对照前后原图叠加帧目视复核，没有发现选中 A 持续有效可见时的无理由换 ID。这里的检查不等于每一帧都已有独立人工真值。帧号从 0 开始，时间按 30 FPS 计算。ID 0 表示已释放、尚未获取新目标；跳过的 ID 曾属于未选中的其他轨迹。

| 视频 | 帧 | 时间（秒） | 事件后 ID | 原因 | 前后对照图 |
|---|---:|---:|---:|---|---|
| task_3 | 199 | 6.633 | 1 | 首次获取 | [查看](task3_windmill/validation/task_3_events_0.jpg) |
| task_3 | 461 | 15.367 | 0 | 连续 3 帧结构消失，释放 | [查看](task3_windmill/validation/task_3_events_0.jpg) |
| task_3 | 462 | 15.400 | 2 | 结构消失后重选 | [查看](task3_windmill/validation/task_3_events_0.jpg) |
| task_3 | 539 | 17.967 | 3 | B 确认后重选 | [查看](task3_windmill/validation/task_3_events_1.jpg) |
| task_3 | 625 | 20.833 | 0 | 连续 3 帧结构消失，释放 | [查看](task3_windmill/validation/task_3_events_1.jpg) |
| task_3 | 627 | 20.900 | 4 | 结构消失后重选 | [查看](task3_windmill/validation/task_3_events_1.jpg) |
| task_3 | 706 | 23.533 | 0 | 连续 3 帧 B，释放 | [查看](task3_windmill/validation/task_3_events_2.jpg) |
| task_3 | 707 | 23.567 | 5 | B 确认后重选 | [查看](task3_windmill/validation/task_3_events_2.jpg) |
| task_4 | 0 | 0.000 | 1 | 首次获取 | [查看](task3_windmill/validation/task_4_events_0.jpg) |
| task_4 | 113 | 3.767 | 3 | 漏检超时后重选 | [查看](task3_windmill/validation/task_4_events_0.jpg) |
| task_4 | 174 | 5.800 | 4 | 漏检超时后重选 | [查看](task3_windmill/validation/task_4_events_0.jpg) |
| task_4 | 242 | 8.067 | 5 | 漏检超时后重选 | [查看](task3_windmill/validation/task_4_events_1.jpg) |
| task_4 | 331 | 11.033 | 7 | 漏检超时后重选 | [查看](task3_windmill/validation/task_4_events_1.jpg) |
| task_4 | 437 | 14.567 | 0 | 第 13 帧漏检，释放 | [查看](task3_windmill/validation/task_4_events_1.jpg) |
| task_4 | 454 | 15.133 | 9 | 漏检超时后重选 | [查看](task3_windmill/validation/task_4_events_2.jpg) |
| task_4 | 540 | 18.000 | 10 | 漏检超时后重选 | [查看](task3_windmill/validation/task_4_events_2.jpg) |
| task_4 | 599 | 19.967 | 0 | 第 13 帧漏检，释放 | [查看](task3_windmill/validation/task_4_events_2.jpg) |
| task_4 | 915 | 30.500 | 11 | 漏检超时后重选 | [查看](task3_windmill/validation/task_4_events_3.jpg) |
| task_4 | 1436 | 47.867 | 13 | 漏检超时后重选 | [查看](task3_windmill/validation/task_4_events_3.jpg) |
| task_4 | 1554 | 51.800 | 14 | 漏检超时后重选 | [查看](task3_windmill/validation/task_4_events_3.jpg) |
| task_4 | 1669 | 55.633 | 0 | 第 13 帧漏检，释放 | [查看](task3_windmill/validation/task_4_events_4.jpg) |
| task_4 | 1683 | 56.100 | 15 | 漏检超时后重选 | [查看](task3_windmill/validation/task_4_events_4.jpg) |

典型情况：

- task_3：459–461 帧原圆的内圈消失，连续三帧清楚的空圆证据触发释放；462 帧新 A 可观测后获取 ID 2。
- task_3：537–539 帧实心灯条出现，即使旧圆内短暂残留同心纹理，也通过连接结构失效判断释放，539 帧选中 ID 3。
- task_3：623–625 帧原 A 结构消失，627 帧获取 ID 4；704、705 帧标记丢失，706 帧完成 B 确认，707 帧选中新的 ID 5。短暂恢复困难时仍保留 ID 5。
- task_4：113、174、242 等帧属于旧目标已消失后的超时重选，另一个 A 的出现本身没有使程序提前切换。437、599、1669 帧释放时没有有效候选，等待后续恢复。
- task_4：第 630 帧的五个实心灯条/空心圆均为 B，状态为 `lost`，没有把满盘 B 当成有效 A。

## 6. 完整视频与逐帧证据

| 输入 | 原图叠加视频 | 二值化调试视频 | 完整解码帧数 | 尺寸/FPS | 输出 detected 帧数（不是准确率） |
|---|---|---|---:|---|---:|
| task_3 | [播放](task3_windmill/task_3/recognition_overlay.mp4) | [播放](task3_windmill/task_3/binary_process.mp4) | 796 | 1440×1080 / 30 | 565 |
| task_4 | [播放](task3_windmill/task_4/recognition_overlay.mp4) | [播放](task3_windmill/task_4/binary_process.mp4) | 1800 | 1440×1080 / 30 | 1317 |

四个输出均完整解码通过，帧率、尺寸、帧数与输入一致；PTS 按帧顺序递增。CSV 检查覆盖每一帧的序号、时间、状态和角度有效性；原图与叠加图逐帧比较了排除少量标注像素后的内容对应性。详见 [完整性记录](task3_windmill/validation/video_integrity.json)。输出没有音轨。

每个输入结果目录包含 `frames.csv`、`candidates.csv`、`tracks.csv` 和 `tracking_stats.txt`。蓝色几何候选、绿色有效 A、黄色锁定目标在二值化视频中区分显示。原图上的 R 为绿色十字，目标中心为红点，中心连线为青色，外椭圆为黄色。

## 7. 用户手动复核入口与限制

未将过渡帧宣称为精确的真实击中时刻。以下位置建议你按原视频逐帧确认：

| 视频/时间 | 需要确认 | 原图与候选 |
|---|---|---|
| task_3，15.300–15.400 秒 | 原 A 内圈消失与新 A 出现的过渡，是否认可三帧确认后的重选 | [459 原图](task3_windmill/validation/task_3_459_original.jpg) / [候选](task3_windmill/validation/task_3_459_candidates.jpg) |
| task_3，17.900–17.967 秒 | 旧圆仍有残留纹理但已连接实心灯条，是否应视为失效 | [537 原图](task3_windmill/validation/task_3_537_original.jpg) / [候选](task3_windmill/validation/task_3_537_candidates.jpg) |
| task_3，23.433–23.667 秒 | 703→704 帧从箭头链变为灯条，706 释放、707 新目标锁定；短漏检时身份保留 | [704 原图](task3_windmill/validation/task_3_704_original.jpg) / [候选](task3_windmill/validation/task_3_704_candidates.jpg) |
| task_4，3.367–3.767 秒、5.400–5.800 秒 | 原目标消失后的 12 帧等待和第 13 帧重选是否符合你对遮挡/熄灭的理解 | [101 原图](task3_windmill/validation/task_4_101_original.jpg) / [候选](task3_windmill/validation/task_4_101_candidates.jpg) |
| task_4，21.000 秒 | 五个空心 B 均不应被选中 | [630 原图](task3_windmill/validation/task_4_630_original.jpg) / [候选](task3_windmill/validation/task_4_630_candidates.jpg) |
| task_4，40.500 秒 | 画面闪灭时应显示 lost，并等待原 ID 恢复 | [1215 原图](task3_windmill/validation/task_4_1215_original.jpg) / [候选](task3_windmill/validation/task_4_1215_candidates.jpg) |

更多原图与候选对照见 [手动检查索引](task3_windmill/validation/manual_review.json)。本算法仍会在严重断环、光晕、快速移动及结构转换期短暂漏检；此时诚实显示 `lost`。参考集通过不能代替用户对过渡语义和全视频的最终确认。

## 8. 复现

```bash
cmake -S . -B build
cmake --build build -j4
./build/task3_windmill --input resources/task_3.mp4 --output result/task3_windmill/task_3 --mode small --config config/windmill.yaml
./build/task3_windmill --input resources/task_4.mp4 --output result/task3_windmill/task_4 --mode large --config config/windmill.yaml
ctest --test-dir build --output-on-failure
./build/windmill_reference_check
python3 src/task3_windmill/evaluate.py
python3 src/task3_windmill/verify_outputs.py
python3 src/task3_windmill/review_events.py
```

验证工具依赖 Python OpenCV、NumPy 和 ffprobe（ffmpeg 包）。`FFPROBE` 环境变量可指定 ffprobe 路径。本次构建和状态机检查通过；最终参数见 `config/windmill.yaml`。详细初学者源码说明保存在仓库目录之外，不上传。

# 上游来源与同步说明

本仓库是 Monado Mercury 手部追踪器的独立提取版，不是 Monado 完整运行时的镜像。

## 源码基线

- 上游：[Monado](https://gitlab.freedesktop.org/monado/monado)
- 提取提交：`2f194e366fbcc54272a755dc0e6ada7995b11719`
- 上游主目录：`src/xrt/tracking/hand/mercury/`
- 独立项目目录：`src/mercury/`

除 Mercury 主体外，还提取了它直接依赖的相机模型、数学、历史缓冲、ONNX Runtime 包装器、手部关节定义以及 Tiny Ceres 子集：

| Monado 中的来源 | 本仓库位置 | 用途 |
| --- | --- | --- |
| `src/xrt/tracking/hand/mercury/` | `src/mercury/` | 检测、关键点、状态追踪和运动学优化 |
| `src/xrt/auxiliary/onnx/` | `src/compat/`、`src/compat/include/onnx/` | ONNX Runtime C API 包装 |
| `src/xrt/auxiliary/tracking/` | `src/compat/`、`src/compat/include/tracking/` | 相机模型和追踪接口 |
| `src/xrt/auxiliary/math/`、`util/` 与 XRT 公共定义 | `src/compat/`、`src/compat/include/` | Mercury 所需的最小兼容层 |
| Monado vendored Tiny Ceres | `third_party/tinyceres/` | Levenberg–Marquardt 运动学拟合 |

原文件的版权行和 SPDX 标识均予以保留。许可汇总见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。

## 独立化改动

提取版在算法主体外增加了以下边界：

- 不暴露 Monado/XRT 类型的 C++20 公共 API：`include/mercury/hand_tracker.hpp`；
- 独立 CMake 构建、安装目标、示例程序和测试；
- 仅实现 Mercury 所需的最小 XRT/工具兼容层，移除 Monado 运行时、IPC、驱动和调试 UI 依赖；
- 对生命周期、类型和独立调用路径做了必要的安全性及正确性修补。

相对提取基线，Mercury 算法文件中的有意修补为：

- 将检测置信度从误声明的 `bool` 恢复为 `float`；
- 将 curl 输出的方差写入从误用的 curl 值改为模型实际计算的 variance；
- 为 ONNX session 的部分初始化失败与释放路径补齐空指针防护；
- 初始化所有生命周期相关裸指针，并处理 worker/optimizer 部分创建失败，确保模型加载中途失败时可安全析构；
- 让优化器销毁函数真正清空调用方句柄，并释放上游遗漏的 worker pool 引用；
- 实现上游已声明但缺失的 tuneable-values accessor，供独立公共 API 配置；
- 删除图像畸变器中两个未使用、会额外引入 Monado 依赖的 include。

独立兼容层使用可复用的固定线程池承载 Mercury 的并行任务；公共 API 还会拒绝重复或倒退的时间戳，避免上游预测计算中的除零和无符号时间差下溢。

这意味着不能直接用整目录覆盖的方式升级 Mercury；上游变更必须结合兼容层逐项审阅。

## 模型基线

- 上游：[Monado hand-tracking-models](https://gitlab.freedesktop.org/monado/utilities/hand-tracking-models)
- 固定提交：`37a8a81bc8f433ac6cbdf2471909d2bac74beca1`
- 获取方法：`scripts/fetch_models.sh`

模型提交与源码提交分别固定。模型不随仓库分发，许可风险见 [`models/README.md`](models/README.md)。

## 后续同步建议

1. 比较当前 Monado 基线与目标上游提交中 Mercury 及上述依赖目录的差异。
2. 按功能选择性移植，保留本仓库的公共 API 和最小兼容层边界。
3. 重新构建并运行单元测试、空白双目流水线冒烟测试和真实标定数据回归。
4. 检查新增文件的版权与许可证，并更新本文的源码提交及 `THIRD_PARTY_NOTICES.md`。
5. 若模型发生变化，独立审查模型来源、许可、文件名和 SHA-256，不要静默移动固定版本。

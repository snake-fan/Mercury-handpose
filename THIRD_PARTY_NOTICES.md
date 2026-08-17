# Third-party notices

本项目从 Monado 中提取 Mercury，并保留了上游文件中的版权与 SPDX 标识。下表概括随源码仓库分发的第三方代码；完整许可文本位于 [`LICENSES/`](LICENSES/)。

| 组件 | 来源与归属 | 本仓库位置 | 许可 |
| --- | --- | --- | --- |
| Monado Mercury 与必要的 XRT/辅助代码 | Monado contributors；包括 Collabora, Ltd.、NVIDIA CORPORATION、Nova King、Beyley Cardellio 等文件内列明的权利人 | `src/mercury/`、`src/compat/` 及相应头文件 | [Boost Software License 1.0](LICENSES/BSL-1.0.txt) |
| Tiny Ceres 子集 | Copyright Google Inc.，Ceres Solver contributors | `third_party/tinyceres/` | [BSD 3-Clause](LICENSES/tinyceres-BSD-3-Clause.txt) |
| Ceres rotation helpers 的改编代码 | Copyright Google, Inc. 与 Collabora, Ltd. | `src/mercury/kine_lm/lm_rotations_ceres.inl` | [BSD 3-Clause](LICENSES/BSD-3-Clause.txt) |
| ONNX Runtime C API 示例的相关实现思路 | Copyright Microsoft Corporation；`hg_model.cpp` 保留了上游来源链接和 MIT 许可说明 | `src/mercury/hg_model.cpp` | [MIT](LICENSES/MIT.txt) |

提取基线及目录映射见 [`UPSTREAM.md`](UPSTREAM.md)。根目录 [`LICENSE`](LICENSE) 适用于本项目中以 BSL-1.0 发布的代码，但不会改变任何第三方组件自己的许可条件。

## 外部构建/运行依赖

Eigen、OpenCV 和 ONNX Runtime 由使用者在系统中另行安装，不作为本仓库源码的一部分分发。它们各自适用的许可及附加组件条款，以实际安装版本为准。

## 模型权重不在本仓库内

两个 ONNX 权重来自独立的上游模型仓库，且固定提交没有显式许可声明。它们不随本项目分发，也不应由上面的源码许可推定为已获授权。详见 [`models/README.md`](models/README.md)。

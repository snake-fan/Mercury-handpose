# Mercury 模型文件

本目录默认不含模型权重，Git 也会忽略所有 `*.onnx` 文件。Mercury 运行时需要下面两个固定文件名：

| 文件 | 字节数 | SHA-256 |
| --- | ---: | --- |
| `grayscale_detection_160x160.onnx` | 7,299,956 | `1f1a039a266e13dc186bb884430ebd9c8216bdda680ab08a533d4c671f27ed36` |
| `grayscale_keypoint_jan18.onnx` | 3,966,382 | `40c0daa598cedb993b54fff17685231b7465d6db342656c401b01c2029efd1d5` |

它们来自 Monado 的 [hand-tracking-models](https://gitlab.freedesktop.org/monado/utilities/hand-tracking-models) 仓库，固定到提交：

```text
37a8a81bc8f433ac6cbdf2471909d2bac74beca1
```

下载并校验：

```bash
./scripts/fetch_models.sh
```

也可指定仓库外的目标目录：

```bash
./scripts/fetch_models.sh /absolute/path/to/mercury-models
```

## 许可提醒

截至上述固定提交，上游模型仓库没有提供 `LICENSE`、模型卡或其他显式许可声明。因此：

- 本项目不重新分发这些权重，项目根目录的 Boost Software License 也不覆盖它们；
- 下载脚本只是可复现地获取和校验上游文件，不代表授予任何模型使用、复制或分发许可；
- 在研究、产品或再分发场景中使用前，请自行确认权利来源，并取得所需授权。

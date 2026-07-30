# CanMV K230 钢珠视觉检测

本目录是 01Studio CanMV K230 的 MicroPython 应用，按 `CanMV IDE` 工作流编写。
首版目标是在白纸背景下找到单个银色抛光钢珠，并在 IDE 画面中标出圆心和半径。
当前不使用外接屏幕、不使用 UART，也不判断钢珠的真实物理直径是否为 `10 mm`。

## 文件

- `main.py`：CanMV IDE 运行入口和 IDE 虚拟画面显示；
- `config.py`：相机、分辨率、圆检测和连续帧确认参数；
- `ball_detector.py`：圆候选筛选和连续帧确认逻辑。

## CanMV IDE 运行

1. 使用 Type-C 连接 K230，启动 `CanMV IDE` 并连接开发板。
2. 在 IDE 中打开本目录的 `main.py`。
3. 点击运行。固件有 `cv_lite` 时使用官方 RGB888 示例的 `Display.ST7701(..., to_ide=True)` 路径；没有 `cv_lite` 时使用 `Display.VIRT`。两种路径都把画面发送到 IDE，不需要外接屏幕。
4. 在 IDE 的串口终端查看 `FOUND`、`SEARCHING`、`LOST`、圆心、半径和 FPS。

`main.py` 内置了默认配置和检测器实现，因此只上传/运行 `main.py` 也可以启动，不会因为缺少
`config.py` 而报错。如果同时把 `config.py` 和 `ball_detector.py` 放在 IDE 当前工作目录或开发板文件系统中，
`main.py` 会优先使用这两个拆分模块。

## 离线运行

调试完成后，至少将 `main.py` 复制到开发板的 `CanMV\sdcard` 目录，按 01Studio 的离线运行方式重启开发板。
如果要保留外部参数配置，再同时复制 `config.py` 和 `ball_detector.py`；通常只需要使用 `main.py` 作为上电入口。

## 调参顺序

1. 先确认启动后终端打印的 `detector` 和 `frame`。默认 `DETECTOR_BACKEND="auto"`：
   固件有 `cv_lite` 时使用 `RGB888/320x240` 高速圆检测和官方 IDE 显示路径；没有时自动使用
   `RGB565/320x240` 的普通圆检测。01Studio 板载摄像头通常连接 CSI2；若你的摄像头接在
   其他 CSI 接口，只修改 `CAMERA_ID`。
2. 保持摄像头固定并尽量垂直白纸，使用漫射光，避免点光源在钢珠上形成大面积高光。
3. 如果钢珠没有被找到，先扩大 `MIN_RADIUS`/`MAX_RADIUS` 范围，再降低
   `HOUGH_THRESHOLD`。
4. 如果白纸纹理或反光造成误检，收窄半径范围、提高 `HOUGH_THRESHOLD`，并增加
   `CONFIRM_HITS`。
5. 如果确认固件包含 `cv_lite`，可以将 `DETECTOR_BACKEND` 固定为 `cv_lite`；该模式使用
   `RGB888/320x240` 和 K230 的 `cv_lite.rgb888_find_circles`。如果固定为 `image`，
   程序会使用 `RGB565/320x240`，不建议再直接提高到 `960x540`。

## 现场限制

- 两种后端默认都使用 `320x240`；该尺寸不代表钢珠的物理尺寸标定结果；
- `MIN_RADIUS` 和 `MAX_RADIUS` 必须根据镜头视场、安装高度和钢珠大小实测调整；
- 纯白纸、强反光、阴影和纸面纹理都可能产生圆形候选；
- 当前结果是“检测到单个钢珠”，不是对 `10 mm` 直径的计量。

参考资料：

- [01Studio CanMV K230 IDE](https://wiki.01studio.cc/docs/canmv_k230/getting_start/canmv_ide)
- [01Studio K230 摄像头](https://wiki.01studio.cc/docs/canmv_k230/machine_vision/camera)
- [01Studio K230 图像显示](https://wiki.01studio.cc/docs/canmv_k230/machine_vision/display)
- [01Studio K230 圆形检测](https://wiki.01studio.cc/docs/canmv_k230/machine_vision/image_detection/find_circles)

# ESP32 Remote Controller

`remote_controller` 是独立 ESP-IDF 项目，用 ESP32 模组作为小车遥控端。它连接 `car_controller` 创建的 Wi-Fi AP，通过两个摇杆控制小车和摄像头方向，并预留 4 寸 TFT 视频显示、麦克风和扬声器对讲模块。

当前版本已经实现：

- Wi-Fi STA 连接小车 AP
- 遥控端 `client_id`，格式为 `remote-xxxxxx`
- `/heartbeat?source=remote`
- `/control?source=remote`
- `/gimbal?source=remote` 占位请求
- 两个摇杆 ADC 采样
- 急停/摇杆按钮停车
- TFT 显示任务占位
- 音频客户端占位

TFT MJPEG 解码和 I2S/WebSocket 音频还没有接入具体驱动。

## 硬件目标

| 模块 | 说明 |
| --- | --- |
| 主控 | ESP32 模组，建议带 PSRAM |
| 车体摇杆 | 控制前进、后退、左转、右转、停止 |
| 摄像头摇杆 | 控制摄像头/云台上、下、左、右、居中 |
| TFT | 4 寸屏幕，后续接入 `esp_lcd` 驱动 |
| 麦克风 | I2S 麦克风，后续用于遥控端讲话 |
| 扬声器 | I2S 功放，后续播放小车端声音 |
| 电源 | 电池供电，建议后续增加电量检测 |

## 构建

Windows PowerShell：

```powershell
. C:\esp\v6.0.2\esp-idf\export.ps1
cd remote_controller
idf.py set-target esp32
idf.py menuconfig
idf.py build flash monitor
```

## 默认配置

遥控端默认连接小车 AP：

```text
SSID: ESP32CAM-Car
Password: 12345678
Base URL: http://192.168.4.1
```

可在 `idf.py menuconfig` 中修改：

```text
Remote Controller Configuration
```

## 默认 GPIO

| 功能 | 默认 GPIO |
| --- | --- |
| 车体摇杆 X | GPIO34 |
| 车体摇杆 Y | GPIO35 |
| 车体摇杆按键 | GPIO32 |
| 摄像头摇杆 X | GPIO36 |
| 摄像头摇杆 Y | GPIO39 |
| 摄像头摇杆按键 | GPIO33 |
| 急停按键 | GPIO25 |
| TFT 背光 | -1，默认不控制 |

按键默认使用内部上拉，按下为低电平。

## 控制逻辑

- 车体摇杆 Y 轴主导前进/后退。
- 车体摇杆 X 轴主导左转/右转。
- 摇杆回到死区发送 `stop`。
- 车体摇杆按键或急停按键会强制发送 `stop`。
- 速度由摇杆偏移量映射为百分比，并受 `Minimum non-zero drive speed percent` 限制。
- 摄像头摇杆会发送 `/gimbal` 请求；小车端当前返回 `gimbal_not_configured`，后续接云台舵机后再启用。

## 接口

心跳：

```text
GET http://192.168.4.1/heartbeat?seq=1&source=remote&client=remote-xxxxxx
```

车体控制：

```text
GET http://192.168.4.1/control?cmd=forward&speed=60&seq=2&source=remote&client=remote-xxxxxx
```

摄像头/云台控制占位：

```text
GET http://192.168.4.1/gimbal?pan=20&tilt=-10&center=false&seq=3&source=remote&client=remote-xxxxxx
```

## 后续实现

- 接入具体 4 寸 TFT 驱动。
- 连接 `/stream`，解析 MJPEG 并解码 JPEG 帧。
- 将视频绘制到 TFT，控制链路优先，视频允许丢帧。
- 接入 I2S 麦克风和 I2S 扬声器。
- 接入 `/audio` WebSocket，实现双向 PCM16 16 kHz 单声道对讲。
- 在屏幕显示 Wi-Fi、FPS、控制命令、小车状态和音频状态。

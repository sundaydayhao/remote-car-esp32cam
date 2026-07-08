# Remote Car ESP32CAM

本仓库包含两个相互独立的 ESP-IDF 项目：

- `car_controller/`：小车端。ESP32-CAM 创建 Wi-Fi AP，提供视频流、网页控制、HTTP 控制接口、状态接口和可选音频对讲。
- `remote_controller/`：遥控端。ESP32 作为 Wi-Fi STA 连接小车 AP，读取两个摇杆，发送控制/心跳/摄像头方向请求，并预留 4 寸 TFT 视频显示和音频对讲模块。

两个目录都应从各自目录单独构建，不要在一个项目里引用另一个项目的源码。

## 构建

Windows PowerShell 下先导出 ESP-IDF 环境：

```powershell
. C:\esp\v6.0.2\esp-idf\export.ps1
```

构建小车端：

```powershell
cd car_controller
idf.py build
```

构建遥控端：

```powershell
cd remote_controller
idf.py build
```

## 当前通信方式

当前只使用 Wi-Fi，不实现 4G。

小车端默认创建 AP：

```text
SSID: ESP32CAM-Car
Password: 12345678
Base URL: http://192.168.4.1
```

遥控端默认连接该 AP，并使用：

- `GET /heartbeat?source=remote`
- `GET /control?source=remote`
- `GET /gimbal?source=remote`

视频和音频后续继续复用小车端：

- `GET /stream`
- `WebSocket /audio`

## 当前实现状态

- 小车端可独立运行，并支持手机网页控制。
- 遥控端已具备独立 IDF 工程骨架。
- 遥控端已实现 Wi-Fi STA、摇杆 ADC 输入、心跳、车体控制请求和摄像头/云台占位请求。
- 遥控端 TFT 和音频模块已拆分为独立占位模块，等待接入具体屏幕驱动、MJPEG 解码和 I2S/WebSocket 音频。

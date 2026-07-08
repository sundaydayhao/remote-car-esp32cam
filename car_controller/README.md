# ESP32CAM Wi-Fi 智能摄像头小车

这是一个基于 ESP-IDF 的 ESP32-CAM/ESP32 摄像头小车工程。当前版本只实现 Wi-Fi 控制：小车启动后创建热点，手机或电脑连接后打开 `http://192.168.4.1`，即可查看摄像头画面、控制小车运动、查看状态，并可选启用手机/小车双向音频对讲。

4G/蜂窝网络当前不实现，只在协议和状态结构中保留后期扩展位置。

## 当前功能

- ESP32 SoftAP 热点：默认 `ESP32CAM-Car` / `12345678`
- OV2640 摄像头 MJPEG 实时视频流：`/stream`
- 网页遥控：支持触摸、鼠标、键盘方向键和 WASD
- ESP32 遥控端控制：支持 `source=remote` 的控制和心跳
- 统一控制协议：控制命令带 `client_id`、`seq`、`source`
- 心跳接口：手机/控制端定时上报在线状态
- 摄像头/云台控制占位接口：`/gimbal`，当前返回未启用
- 状态回传：运动、速度、左右轮输出、避障、Wi-Fi、音频、控制端、运行时间
- 双直流电机 PWM 调速，默认可适配 TB6612FNG 或 L298N
- 电机失联保护：超过配置时间未收到控制命令会自动停车
- 可选 HC-SR04 超声波避障，前方过近时自动停止前进
- 可选手机/小车双向对讲：I2S 麦克风 + I2S 功放
- 手机网页语音控制：浏览器识别语音后发送动作命令
- 预留后期 4G/云端链路，不影响当前 Wi-Fi 版本

## 当前状态

- 构建目标：`esp32`
- 已验证构建：`idf.py build` 成功
- 生成固件：`build/esp32cam-smart-car.bin`
- 固件大小：约 `0xfbb60` bytes，3 MB app 分区剩余约 67%
- 当前网络：仅 Wi-Fi AP，本地控制
- 当前 4G：未启用、未引入 PPP/ML307 逻辑

## 快速构建和烧录

Windows PowerShell 下建议先导出 ESP-IDF 环境：

```powershell
. C:\esp\v6.0.2\esp-idf\export.ps1
idf.py build
```

首次配置目标或清理后可执行：

```powershell
idf.py set-target esp32
idf.py menuconfig
idf.py build flash monitor
```

启动后连接小车热点：

```text
SSID: ESP32CAM-Car
Password: 12345678
URL: http://192.168.4.1
```

如果 CMake 报工具链版本不匹配，先确认已执行 `export.ps1`。ESP-IDF 6.2 需要 `xtensa-esp-elf` 的 `esp-16.1.0_20260609` 工具链。

## 网页控制

网页首页集成视频、方向控制、速度调节、语音按钮、对讲按钮和状态面板。

支持的方向命令：

```text
forward
backward
left
right
spin_left
spin_right
stop
```

控制端会自动生成稳定的 `client_id`，并对每条命令递增 `seq`。小车端会记录最近控制端、控制来源和心跳时间，方便后期接入手机 App、物理遥控器或云端链路。

## HTTP/WebSocket 接口

### 视频

```text
GET /stream
```

返回 MJPEG 视频流。

```text
GET /jpg
```

返回单张 JPEG 图片。

### 控制

```text
GET /control?cmd=forward&speed=60&seq=1&source=web&client=web-abc
```

字段说明：

| 字段 | 说明 |
| --- | --- |
| `cmd` | 动作命令 |
| `speed` | 速度百分比，0-100 |
| `seq` | 控制端递增序号 |
| `source` | 控制来源：`web`、`voice`、`remote`、`api` |
| `client` | 控制端 ID |

返回示例：

```json
{"ok":true,"cmd":"forward","speed":60,"left":60,"right":60,"blocked":false,"distance_cm":-1,"seq":1,"source":"web","client":"web-abc"}
```

### 心跳

```text
GET /heartbeat?seq=2&source=web&client=web-abc
```

用于让小车知道控制端仍在线。当前心跳只记录状态，不直接驱动电机。

### 摄像头/云台控制占位

```text
GET /gimbal?pan=20&tilt=-10&center=false&seq=4&source=remote&client=remote-abc
```

当前小车端还没有实现云台舵机驱动，因此该接口返回：

```json
{"ok":false,"enabled":false,"reason":"gimbal_not_configured"}
```

这个接口用于先让 `remote_controller` 完整联调摄像头摇杆。后续接入云台硬件后，可以在小车端把 `pan`、`tilt`、`center` 映射到舵机或其他摄像头方向机构。

### 状态

```text
GET /status
```

返回运动、避障、音视频、Wi-Fi 和扩展状态。关键字段：

| 字段 | 说明 |
| --- | --- |
| `uptime_s` | 运行时间 |
| `cmd` / `speed` | 当前动作和速度 |
| `left` / `right` | 左右轮输出百分比 |
| `blocked` / `distance_cm` | 避障状态和距离 |
| `camera` | 摄像头是否就绪 |
| `audio_ready` / `audio_client` | 对讲模块状态 |
| `control_client` / `control_source` / `control_seq` | 最近控制端信息 |
| `heartbeat_age_ms` | 最近心跳距现在的时间 |
| `gimbal.enabled` | 摄像头/云台控制是否启用，当前为 `false` |
| `network.transport` | 当前为 `wifi` |
| `network.station_count` | 当前连接热点的设备数量 |
| `expansion.cellular_reserved` | 是否保留蜂窝网络扩展位 |
| `expansion.cellular_active` | 当前是否启用蜂窝网络，现阶段为 `false` |

### 语音控制

```text
GET /voice?text=%E5%B7%A6%E8%BD%AC&speed=60&seq=3&client=web-abc
```

网页语音依赖手机浏览器的 Web Speech API。浏览器负责识别“前进、后退、左转、右转、停止、原地左转、原地右转”等语音，小车只接收识别后的文本。

### 双向对讲

需要在 `idf.py menuconfig` 中启用：

```text
Smart Camera Car Configuration -> Audio Intercom -> Enable phone/car audio intercom
```

接口：

```text
WebSocket ws://192.168.4.1/audio
Binary frame: PCM16 little-endian, 16000 Hz, mono
car -> phone: 小车麦克风音频，经过噪声门
phone -> car: 手机麦克风音频，小车扬声器播放
```

注意：手机浏览器的麦克风权限通常要求安全上下文。有些浏览器会拒绝 `http://192.168.4.1` 使用麦克风；这种情况下可以用原生 App 连接同一个 WebSocket，或改成 HTTPS/受信任环境。

## 硬件清单

| 模块 | 建议型号 | 说明 |
| --- | --- | --- |
| 主控 | ESP32-CAM / ESP32 + OV2640 | 当前构建目标为 `esp32` |
| 摄像头 | OV2640 | ESP32-CAM 常见板载摄像头 |
| 电机驱动 | TB6612FNG / L298N | TB6612FNG 更适合低压小车 |
| 电机 | TT 减速电机 x2 | 左右轮差速 |
| 电源 | 2S 锂电或 6-12V 电池 + 5V 降压 | 电机和 ESP32 不要共用薄弱 USB 供电 |
| 避障 | HC-SR04，可选 | Echo 必须降压到 3.3V |
| 麦克风 | INMP441 I2S，可选 | 启用对讲时使用 |
| 扬声器功放 | MAX98357A I2S，可选 | 启用对讲时使用 |

## 默认 GPIO

默认摄像头引脚按常见 ESP32-CAM/AI Thinker 风格配置：

| OV2640 信号 | ESP32 GPIO |
| --- | --- |
| PWDN | GPIO32 |
| RESET | -1 |
| XCLK | GPIO0 |
| SIOD / SDA | GPIO26 |
| SIOC / SCL | GPIO27 |
| Y9 / D7 | GPIO35 |
| Y8 / D6 | GPIO34 |
| Y7 / D5 | GPIO39 |
| Y6 / D4 | GPIO36 |
| Y5 / D3 | GPIO21 |
| Y4 / D2 | GPIO19 |
| Y3 / D1 | GPIO18 |
| Y2 / D0 | GPIO5 |
| VSYNC | GPIO25 |
| HREF | GPIO23 |
| PCLK | GPIO22 |

默认电机 GPIO：

| 功能 | ESP32 GPIO |
| --- | --- |
| Left PWM | GPIO12 |
| Left IN1 | GPIO13 |
| Left IN2 | GPIO15 |
| Right PWM | GPIO14 |
| Right IN1 | GPIO2 |
| Right IN2 | GPIO4 |
| Standby | -1 |

ESP32-CAM 可用 GPIO 很少，且部分 GPIO 与启动绑定位、闪光灯或 SD 卡复用。实际接线前请进入 `idf.py menuconfig` 的 `Smart Camera Car Configuration` 修改电机、避障和音频引脚，避免和摄像头冲突。

## 电源和接线注意

- 电机不要从 ESP32 的 3.3V 引脚取电。
- 电机电源、降压模块和 ESP32 必须共地。
- 建议在电机驱动电源端并联 470uF 以上电解电容，减少电机启动导致的复位。
- HC-SR04 的 Echo 是 5V 电平，必须分压到 3.3V。
- 扬声器会被小车麦克风拾音，麦克风和扬声器尽量拉开距离。
- 后期如果接 4G 模块，必须单独设计供电，不从 ESP32 3.3V 取电。

## 后期 4G 扩展

当前版本不包含 ML307、PPP、APN、云端推流等 4G 实现。已保留的部分：

- 控制协议带 `source` 和 `client`
- `/status` 中有 `expansion.cellular_reserved`
- 控制、状态、音频、视频接口按通道拆分
- 后续可把 4G/云端链路接入同一套控制消息

## 工程结构

```text
remote-car-esp32cam/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── README.md
├── TODO.md
└── main/
    ├── CMakeLists.txt
    ├── Kconfig.projbuild
    ├── idf_component.yml
    ├── main.c
    ├── control_protocol.c/h
    ├── wifi_ap.c/h
    ├── web_server.c/h
    ├── camera_app.c/h
    ├── motor_control.c/h
    ├── ultrasonic.c/h
    ├── audio_intercom.c/h
    └── web/index.html
```

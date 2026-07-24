# Remote Car Wiring Guide

本文档记录 `car_controller` 小车控制端的推荐接线。目标功能包括：

- 两个直流减速电机控制
- 摄像头云台左右 / 上下控制
- 车端麦克风采集
- 车端扬声器播放
- IO 不够时使用 PCF8574 扩展低速数字输出

## 重要结论

如果使用常见 AI-Thinker ESP32-CAM，同时启用摄像头、两路电机、两个舵机、I2S 麦克风和 I2S 扬声器，原生 IO 基本不够。PCF8574 只能扩展低速数字输入 / 输出，适合接 L298N 的 IN1-IN4、LED、蜂鸣器使能等，不适合直接接舵机 PWM，也不适合接 I2S 麦克风或 I2S 功放。

因此推荐两种方案：

| 方案 | 适用情况 | 说明 |
| --- | --- | --- |
| A. 直连简化方案 | 只做摄像头 + 两电机 + 双舵机 | 不接车端麦克风 / 扬声器，L298N 方向脚和舵机都接 ESP32-CAM |
| B. PCF8574 扩展方案 | 摄像头 + 两电机 + 双舵机 + 音频 | L298N 方向脚放到 PCF8574，ESP32 原生 GPIO 留给舵机和 I2S 音频 |

如果要求所有功能稳定同时运行，更建议使用 ESP32-S3-CAM 或其他 IO 更多的 ESP32 开发板。

## 1. 电源总接线

建议使用电池给 L298N 和 LM2596 供电，LM2596 输出稳定 5V 给 ESP32-CAM、舵机和音频模块。接 ESP32-CAM 前，先用万用表把 LM2596 输出调到 5.0V 左右。

```text
电池正极 + -> 电源开关 -> 分两路：
                       1. L298N +12V / VMS
                       2. LM2596 IN+

电池负极 - -> L298N GND
电池负极 - -> LM2596 IN-

LM2596 OUT+ 5V -> ESP32-CAM 5V
LM2596 OUT+ 5V -> SG90 Pan 红线
LM2596 OUT+ 5V -> SG90 Tilt 红线
LM2596 OUT+ 5V -> I2S 麦克风 VCC
LM2596 OUT+ 5V -> I2S 功放 VIN / 5V

LM2596 OUT- GND -> ESP32-CAM GND
LM2596 OUT- GND -> L298N GND
LM2596 OUT- GND -> SG90 Pan 棕色 / 黑色线
LM2596 OUT- GND -> SG90 Tilt 棕色 / 黑色线
LM2596 OUT- GND -> I2S 麦克风 GND
LM2596 OUT- GND -> I2S 功放 GND
```

所有模块必须共地：ESP32-CAM GND、L298N GND、LM2596 OUT-、两个 SG90 GND、麦克风 GND、功放 GND 都要连在一起。

两个 SG90 和扬声器动作时电流较大，LM2596 建议至少 5V 3A。不要从 ESP32-CAM 的 5V 引脚反向带舵机和功放，应该从 LM2596 5V 输出端直接分线。

## 2. ESP32-CAM 摄像头固定占用

AI-Thinker ESP32-CAM 摄像头通常已经占用以下 GPIO，不要再接电机、舵机或音频：

| 功能 | GPIO |
| --- | --- |
| PWDN | GPIO32 |
| XCLK | GPIO0 |
| SIOD / SDA | GPIO26 |
| SIOC / SCL | GPIO27 |
| D7 / Y9 | GPIO35 |
| D6 / Y8 | GPIO34 |
| D5 / Y7 | GPIO39 |
| D4 / Y6 | GPIO36 |
| D3 / Y5 | GPIO21 |
| D2 / Y4 | GPIO19 |
| D1 / Y3 | GPIO18 |
| D0 / Y2 | GPIO5 |
| VSYNC | GPIO25 |
| HREF | GPIO23 |
| PCLK | GPIO22 |

可优先考虑的外设 GPIO 通常只剩 GPIO12、GPIO13、GPIO14、GPIO15、GPIO2、GPIO4、GPIO16。GPIO12、GPIO2、GPIO4、GPIO15 都和启动状态有关，接线错误可能导致无法启动。

## 3. 方案 A：直连简化方案

此方案适合先实现摄像头、两个电机和双舵机。暂不接车端麦克风和扬声器。

### 3.1 L298N 电机驱动

当前固件默认给左右电机各一路 PWM，因此 L298N 的 ENA / ENB 跳帽需要拔掉，由 ESP32-CAM 输出 PWM 调速。

| L298N | ESP32-CAM | 固件配置项 |
| --- | --- | --- |
| ENA | GPIO12 | `CONFIG_CAR_MOTOR_LEFT_PWM_GPIO` |
| IN1 | GPIO13 | `CONFIG_CAR_MOTOR_LEFT_IN1_GPIO` |
| IN2 | GPIO15 | `CONFIG_CAR_MOTOR_LEFT_IN2_GPIO` |
| ENB | GPIO14 | `CONFIG_CAR_MOTOR_RIGHT_PWM_GPIO` |
| IN3 | GPIO2 | `CONFIG_CAR_MOTOR_RIGHT_IN1_GPIO` |
| IN4 | GPIO4 | `CONFIG_CAR_MOTOR_RIGHT_IN2_GPIO` |
| GND | GND | 公共地 |

```text
左电机 -> L298N OUT1 / OUT2
右电机 -> L298N OUT3 / OUT4
```

如果 App 点前进时某个轮子反转，把该电机在 L298N 上的 OUT 两根线对调。若左右整体相反，可以交换左右电机输出，或修改固件里的左右电机反向配置。

### 3.2 SG90 摄像头云台

SG90 常见线色：

| SG90 线色 | 功能 | 接到 |
| --- | --- | --- |
| 棕色 / 黑色 | GND | LM2596 OUT- / 公共 GND |
| 红色 | 5V | LM2596 OUT+ 5V |
| 黄色 / 橙色 | PWM 信号 | ESP32-CAM 原生 GPIO |

直连简化方案中，电机已经占用了主要可用 GPIO，所以云台需要调整固件引脚，或者改用方案 B 释放 GPIO。推荐不要把舵机信号接 PCF8574。

## 4. 方案 B：PCF8574 扩展方案

此方案把 L298N 的 4 根方向线移到 PCF8574，ESP32-CAM 只负责两路电机 PWM、两个舵机 PWM 和 I2S 音频。PCF8574 只能控制方向，不能输出稳定 PWM。

### 4.1 PCF8574 与 ESP32-CAM

PCF8574 是 I2C 扩展芯片。由于 ESP32-CAM 摄像头 SCCB 已占用 GPIO26 / GPIO27，不建议共用摄像头线。实际接线建议使用仍可用的 GPIO16 作为 SDA，并选择一个未被当前硬件占用的 GPIO 作为 SCL；如果没有空闲 GPIO，AI-Thinker ESP32-CAM 不适合同时实现所有功能。

| PCF8574 | ESP32-CAM | 说明 |
| --- | --- | --- |
| VCC | 3V3 | 推荐 3.3V 供电，避免 I2C 上拉到 5V |
| GND | GND | 公共地 |
| SDA | GPIO16 | I2C 数据 |
| SCL | 需要空闲 GPIO | I2C 时钟 |
| A0 | GND | 地址选择 |
| A1 | GND | 地址选择 |
| A2 | GND | 地址选择 |

当 A0/A1/A2 都接 GND 时，PCF8574 常见 7-bit I2C 地址为 `0x20`。部分模块是 PCF8574A，地址范围不同，常见为 `0x38`。

### 4.2 PCF8574 到 L298N

L298N 的 ENA / ENB 仍由 ESP32 原生 PWM 控制，IN1-IN4 由 PCF8574 控制方向。

| L298N | 连接到 | 说明 |
| --- | --- | --- |
| ENA | ESP32-CAM GPIO12 | 左电机 PWM |
| ENB | ESP32-CAM GPIO14 | 右电机 PWM |
| IN1 | PCF8574 P0 | 左电机方向 1 |
| IN2 | PCF8574 P1 | 左电机方向 2 |
| IN3 | PCF8574 P2 | 右电机方向 1 |
| IN4 | PCF8574 P3 | 右电机方向 2 |
| GND | 公共 GND | 必须共地 |

PCF8574 输出驱动能力弱，只适合接 L298N 逻辑输入，不要直接带电机、舵机、继电器线圈或扬声器。

注意：当前固件的 `motor_control.c` 仍按 ESP32 GPIO 直接控制 L298N IN1-IN4。若采用 PCF8574，需要修改固件，增加 I2C 初始化和 PCF8574 输出逻辑。

## 5. 云台左右 / 上下控制

推荐云台舵机信号线接 ESP32 原生 GPIO，因为舵机需要稳定 50Hz PWM。

| 云台方向 | SG90 信号线 | 推荐 GPIO | 说明 |
| --- | --- | --- | --- |
| Pan 左右 | 黄色 / 橙色 | GPIO13 | 需要固件增加舵机 PWM |
| Tilt 上下 | 黄色 / 橙色 | GPIO15 | 需要固件增加舵机 PWM |

如果 GPIO13 / GPIO15 已用于直连电机方向，请改用方案 B，把 L298N 方向脚移到 PCF8574。

当前 `car_controller` 的 `/gimbal` 接口仍是占位实现，会返回 `gimbal_not_configured`。接好舵机后，还需要在固件中增加两路 SG90 PWM 控制逻辑。

## 6. 麦克风接线

推荐使用 I2S 数字麦克风模块，例如 INMP441。I2S 必须接 ESP32 原生 GPIO，不能接 PCF8574。

| INMP441 | ESP32 原生 GPIO | 说明 |
| --- | --- | --- |
| VDD | 3V3 | 按模块要求供电；多数 INMP441 用 3.3V |
| GND | GND | 公共地 |
| SCK / BCLK | 空闲输出 GPIO | I2S bit clock |
| WS / LRCL | 空闲输出 GPIO | I2S word select |
| SD / DOUT | 空闲输入 GPIO | 麦克风数据输入 |
| L/R | GND | 左声道；若接 3V3，固件需启用 right slot |

当前 Kconfig 默认音频 GPIO 是 `35/36/37/38/45/46`，这些默认值不适合普通 ESP32-CAM：GPIO35/36 是摄像头输入脚，GPIO37/38/45/46 在 ESP32-CAM 上通常不可用。启用音频前必须在 `idf.py menuconfig` 中改成实际可用引脚。

## 7. 扬声器接线

推荐使用 I2S 数字功放模块，例如 MAX98357A。不要把喇叭直接接 ESP32 GPIO。

| MAX98357A | ESP32 原生 GPIO / 电源 | 说明 |
| --- | --- | --- |
| VIN | LM2596 OUT+ 5V | 按模块支持范围供电，常见可用 3.3V 或 5V |
| GND | GND | 公共地 |
| BCLK | 空闲输出 GPIO | I2S bit clock |
| LRC / WS | 空闲输出 GPIO | I2S word select |
| DIN | 空闲输出 GPIO | 音频数据输出 |
| SD / EN | 3V3 或按模块说明接使能 | 不用时可固定使能 |
| + / - | 喇叭两端 | 4Ω / 8Ω 小喇叭，按功放模块规格选择 |

扬声器功放电流脉冲明显，功放 VIN/GND 建议直接从 LM2596 输出端取电，不要从 ESP32-CAM 板子的 5V/GND 小焊盘串过去。

## 8. 烧录注意

烧录程序时：

```text
GPIO0 -> GND
```

烧录完成并正常运行时：

```text
GPIO0 必须断开 GND
```

如果烧录或串口日志异常，先拔掉 L298N 控制线、舵机信号线、PCF8574 I2C 线和音频 I2S 线，只保留 ESP32-CAM 的 5V / GND / 串口烧录线。确认程序能正常启动后，再逐步接回外设。

## 9. 总接线速查

### 9.1 电源

| 模块端子 | 连接到 |
| --- | --- |
| 电池 + | 电源开关输入 |
| 电源开关输出 | L298N +12V / VMS、LM2596 IN+ |
| 电池 - | L298N GND、LM2596 IN- |
| LM2596 OUT+ | ESP32-CAM 5V、SG90 红线、功放 VIN |
| LM2596 OUT- | ESP32-CAM GND、L298N GND、SG90 GND、音频模块 GND |

### 9.2 直连简化方案

| 外设 | 连接到 |
| --- | --- |
| L298N ENA | ESP32-CAM GPIO12 |
| L298N IN1 | ESP32-CAM GPIO13 |
| L298N IN2 | ESP32-CAM GPIO15 |
| L298N ENB | ESP32-CAM GPIO14 |
| L298N IN3 | ESP32-CAM GPIO2 |
| L298N IN4 | ESP32-CAM GPIO4 |
| 左电机 | L298N OUT1 / OUT2 |
| 右电机 | L298N OUT3 / OUT4 |

### 9.3 PCF8574 扩展方案

| 外设 | 连接到 |
| --- | --- |
| PCF8574 VCC | ESP32-CAM 3V3 |
| PCF8574 GND | 公共 GND |
| PCF8574 SDA | ESP32-CAM GPIO16 |
| PCF8574 SCL | ESP32-CAM 空闲 GPIO |
| L298N ENA | ESP32-CAM GPIO12 |
| L298N ENB | ESP32-CAM GPIO14 |
| L298N IN1 | PCF8574 P0 |
| L298N IN2 | PCF8574 P1 |
| L298N IN3 | PCF8574 P2 |
| L298N IN4 | PCF8574 P3 |
| SG90 Pan 信号 | ESP32-CAM GPIO13 |
| SG90 Tilt 信号 | ESP32-CAM GPIO15 |
| I2S 麦克风 | ESP32 原生 GPIO，不接 PCF8574 |
| I2S 功放 | ESP32 原生 GPIO，不接 PCF8574 |


# Remote Car Wiring Guide

本文档记录 `car_controller` 小车控制端的接线方案。当前硬件目标改为使用 GOOOUUU / 同类 ESP32-S3-CAM 开发板，功能包括：

- OV2640 摄像头
- Micro SD 卡
- 两个直流减速电机
- 摄像头云台 Pan / Tilt 两个舵机
- I2S 数字麦克风
- I2S 数字功放 + 扬声器

## 重要结论

这块 ESP32-S3-CAM 的 GPIO 比 AI-Thinker ESP32-CAM 宽裕很多。按照图片标注，摄像头、SD 卡和 PSRAM 占用固定引脚后，仍然可以分配出足够 GPIO 做电机、云台和音频。

不建议继续使用 L298N 作为最终电机驱动。L298N 压降大、发热明显、效率低。推荐使用 TB6612FNG 或 DRV8833。两者都可以驱动两个小直流电机，接线逻辑类似。

## 1. 板载资源占用

根据你提供的 ESP32-S3-CAM 引脚图，以下 GPIO 已经被板载功能占用，不建议再接外设。

| 功能 | GPIO |
| --- | --- |
| 摄像头 SIOD | GPIO4 |
| 摄像头 SIOC | GPIO5 |
| 摄像头 VSYNC | GPIO6 |
| 摄像头 HREF | GPIO7 |
| 摄像头 Y4 | GPIO8 |
| 摄像头 Y3 | GPIO9 |
| 摄像头 Y5 | GPIO10 |
| 摄像头 Y2 | GPIO11 |
| 摄像头 Y6 | GPIO12 |
| 摄像头 PCLK | GPIO13 |
| 摄像头 XCLK | GPIO15 |
| 摄像头 Y9 | GPIO16 |
| 摄像头 Y8 | GPIO17 |
| 摄像头 Y7 | GPIO18 |
| USB D- | GPIO20 |
| USB D+ | GPIO19 |
| PSRAM | GPIO35、GPIO36、GPIO37 |
| SD DATA | GPIO40 |
| SD CLK | GPIO39 |
| SD CMD | GPIO38 |
| BOOT | GPIO0 |
| UART0 TX/RX | GPIO43、GPIO44 |
| 板载 LED / WS2812 | GPIO2、GPIO48 |

可优先用于外设的 GPIO：

```text
GPIO1, GPIO3, GPIO14, GPIO21, GPIO41, GPIO42, GPIO45, GPIO47
```

如果不需要板载 WS2812，可以使用 GPIO48。GPIO43 / GPIO44 是串口调试口，正常运行时也可以复用，但调试会不方便。

## 2. 推荐 GPIO 分配

本方案保留 USB、SD 卡、摄像头和 PSRAM，不占用 GPIO19 / GPIO20 / GPIO35-GPIO40。串口 GPIO43 / GPIO44 尽量保留给日志。

| 外设功能 | 推荐 GPIO | 说明 |
| --- | --- | --- |
| 左电机 PWM / PWMA | GPIO1 | LEDC PWM |
| 左电机 AIN1 | GPIO3 | 普通数字输出 |
| 左电机 AIN2 | GPIO14 | 普通数字输出 |
| 右电机 PWM / PWMB | GPIO21 | LEDC PWM |
| 右电机 BIN1 | GPIO41 | 普通数字输出 |
| 右电机 BIN2 | GPIO42 | 普通数字输出 |
| 云台 Pan 舵机 | GPIO45 | 50Hz PWM |
| 云台 Tilt 舵机 | GPIO47 | 50Hz PWM |
| I2S BCLK | GPIO43 | 麦克风和功放共用 |
| I2S WS / LRCLK | GPIO44 | 麦克风和功放共用 |
| I2S 麦克风 DOUT | GPIO48 | 麦克风数据输入 |
| I2S 功放 DIN | GPIO2 | 音频数据输出 |

注意：GPIO2 和 GPIO48 在图中标了板载 LED / WS2812。若板载 LED 会影响音频，改用 GPIO43 / GPIO44 做音频数据线，并用 USB CDC 日志替代 UART0 日志。

## 3. 电源总接线

建议使用电池给电机驱动和 LM2596 降压模块供电，LM2596 输出稳定 5V 给 ESP32-S3-CAM、舵机和 I2S 功放。接板子前，先用万用表把 LM2596 输出调到 5.0V 左右。

```text
电池正极 + -> 电源开关 -> 分两路：
                       1. 电机驱动 VM / VMOT
                       2. LM2596 IN+

电池负极 - -> 电机驱动 GND
电池负极 - -> LM2596 IN-

LM2596 OUT+ 5V -> ESP32-S3-CAM 5V
LM2596 OUT+ 5V -> SG90 Pan 红线
LM2596 OUT+ 5V -> SG90 Tilt 红线
LM2596 OUT+ 5V -> MAX98357A VIN

LM2596 OUT- GND -> ESP32-S3-CAM GND
LM2596 OUT- GND -> 电机驱动 GND
LM2596 OUT- GND -> SG90 Pan 棕色 / 黑色线
LM2596 OUT- GND -> SG90 Tilt 棕色 / 黑色线
LM2596 OUT- GND -> INMP441 GND
LM2596 OUT- GND -> MAX98357A GND
```

所有模块必须共地。两个舵机和扬声器动作时电流较大，LM2596 建议至少 5V 3A。不要从 ESP32-S3-CAM 的 3V3 引脚给舵机或功放供电。

## 4. TB6612FNG 电机驱动接线

推荐使用 TB6612FNG。它比 L298N 效率高、压降低、发热小，更适合电池小车。

| TB6612FNG | 连接到 | 说明 |
| --- | --- | --- |
| VM | 电池正极，经过电源开关 | 电机电源 |
| VCC | ESP32-S3-CAM 3V3 | 逻辑电源 |
| GND | 公共 GND | 必须和 ESP32 共地 |
| STBY | 3V3 | 固定使能；也可接一个 GPIO 控制 |
| PWMA | GPIO1 | 左电机 PWM |
| AIN1 | GPIO3 | 左电机方向 1 |
| AIN2 | GPIO14 | 左电机方向 2 |
| PWMB | GPIO21 | 右电机 PWM |
| BIN1 | GPIO41 | 右电机方向 1 |
| BIN2 | GPIO42 | 右电机方向 2 |
| AO1 / AO2 | 左电机两根线 | 方向反了就对调 |
| BO1 / BO2 | 右电机两根线 | 方向反了就对调 |

如果 App 点前进时某个轮子反转，把该电机在 TB6612FNG 上的输出两根线对调。若左右整体相反，可以交换左右电机输出，或修改固件里的左右电机反向配置。

## 5. DRV8833 电机驱动接线

如果使用 DRV8833，可以按下面接。DRV8833 通常不需要单独的方向脚和 PWM 脚，直接给 `IN1/IN2` 输入 PWM / 电平组合即可。为了兼容当前固件“PWM + IN1 + IN2”的模型，推荐优先用 TB6612FNG。

| DRV8833 | 连接到 | 说明 |
| --- | --- | --- |
| VM | 电池正极，经过电源开关 | 电机电源 |
| GND | 公共 GND | 必须和 ESP32 共地 |
| AIN1 | GPIO3 | 左电机输入 1 |
| AIN2 | GPIO14 | 左电机输入 2 |
| BIN1 | GPIO41 | 右电机输入 1 |
| BIN2 | GPIO42 | 右电机输入 2 |
| AOUT1 / AOUT2 | 左电机两根线 | 方向反了就对调 |
| BOUT1 / BOUT2 | 右电机两根线 | 方向反了就对调 |

若用 DRV8833，固件里的电机 PWM 逻辑最好调整为在 IN1/IN2 上直接输出 PWM，而不是单独使用 ENA/ENB。

## 6. 摄像头云台接线

SG90 常见线色：

| SG90 线色 | 功能 | 接到 |
| --- | --- | --- |
| 棕色 / 黑色 | GND | LM2596 OUT- / 公共 GND |
| 红色 | 5V | LM2596 OUT+ 5V |
| 黄色 / 橙色 | PWM 信号 | ESP32-S3-CAM 原生 GPIO |

推荐接线：

| 云台方向 | SG90 信号线 | ESP32-S3-CAM |
| --- | --- | --- |
| Pan 左右 | 黄色 / 橙色 | GPIO45 |
| Tilt 上下 | 黄色 / 橙色 | GPIO47 |

当前 `car_controller` 的 `/gimbal` 接口仍是占位实现，会返回 `gimbal_not_configured`。接好舵机后，还需要在固件中增加两路 SG90 PWM 控制逻辑。

## 7. I2S 麦克风接线

推荐使用 INMP441 或同类 I2S 数字麦克风。I2S 必须接 ESP32 原生 GPIO，不能接 PCF8574。

| INMP441 | ESP32-S3-CAM | 说明 |
| --- | --- | --- |
| VDD | 3V3 | 多数 INMP441 使用 3.3V |
| GND | 公共 GND | 必须共地 |
| SCK / BCLK | GPIO43 | I2S bit clock |
| WS / LRCL | GPIO44 | I2S word select |
| SD / DOUT | GPIO48 | 麦克风数据输入 |
| L/R | GND | 左声道；若接 3V3，固件需启用 right slot |

## 8. I2S 功放和扬声器接线

推荐使用 MAX98357A I2S 数字功放。不要把喇叭直接接 ESP32 GPIO。

| MAX98357A | ESP32-S3-CAM / 电源 | 说明 |
| --- | --- | --- |
| VIN | LM2596 OUT+ 5V | 按模块支持范围供电 |
| GND | 公共 GND | 必须共地 |
| BCLK | GPIO43 | 与麦克风共用 BCLK |
| LRC / WS | GPIO44 | 与麦克风共用 WS |
| DIN | GPIO2 | ESP32 输出到功放 |
| SD / EN | 3V3 | 固定使能，或按模块说明接 |
| + / - | 喇叭两端 | 4Ω / 8Ω 小喇叭 |

扬声器功放电流脉冲明显，功放 VIN/GND 建议直接从 LM2596 输出端取电，不要从 ESP32-S3-CAM 板子的 5V/GND 小焊盘串过去。

## 9. 固件配置提醒

当前固件默认还是 AI-Thinker ESP32-CAM 的 GPIO 配置。换成 ESP32-S3-CAM 后，需要同步修改：

- `CONFIG_IDF_TARGET` 改为 `esp32s3`
- 摄像头 GPIO 改成 ESP32-S3-CAM 引脚图里的 GPIO4-GPIO18
- 电机 GPIO 改成本文档第 2 节推荐分配
- 音频 GPIO 改成 GPIO43 / GPIO44 / GPIO48 / GPIO2
- 增加云台两路 SG90 PWM 控制逻辑
- 如果使用 DRV8833，需要调整电机 PWM 输出方式

当前 `motor_control.c` 适合 TB6612FNG 这种 `PWM + IN1 + IN2` 的驱动方式。若选择 TB6612FNG，固件改动最小。

## 10. 烧录和调试注意

这块板子有 USB 口，优先用 USB 烧录和串口日志。烧录失败时检查 BOOT 键和 RST 键。

如果启动或日志异常，先拔掉电机驱动、舵机信号线和音频 I2S 线，只保留 ESP32-S3-CAM 的 USB 连接。确认固件能正常启动后，再逐步接回外设。

## 11. 总接线速查

| 外设 | 连接到 |
| --- | --- |
| 电池 + | 电源开关输入 |
| 电源开关输出 | TB6612FNG VM、LM2596 IN+ |
| 电池 - | TB6612FNG GND、LM2596 IN- |
| LM2596 OUT+ | ESP32-S3-CAM 5V、两个 SG90 红线、MAX98357A VIN |
| LM2596 OUT- | ESP32-S3-CAM GND、TB6612FNG GND、两个 SG90 GND、INMP441 GND、MAX98357A GND |
| TB6612FNG VCC | ESP32-S3-CAM 3V3 |
| TB6612FNG STBY | ESP32-S3-CAM 3V3 |
| TB6612FNG PWMA | GPIO1 |
| TB6612FNG AIN1 | GPIO3 |
| TB6612FNG AIN2 | GPIO14 |
| TB6612FNG PWMB | GPIO21 |
| TB6612FNG BIN1 | GPIO41 |
| TB6612FNG BIN2 | GPIO42 |
| 左电机 | TB6612FNG AO1 / AO2 |
| 右电机 | TB6612FNG BO1 / BO2 |
| SG90 Pan 信号 | GPIO45 |
| SG90 Tilt 信号 | GPIO47 |
| INMP441 SCK / BCLK | GPIO43 |
| INMP441 WS / LRCL | GPIO44 |
| INMP441 SD / DOUT | GPIO48 |
| MAX98357A BCLK | GPIO43 |
| MAX98357A LRC / WS | GPIO44 |
| MAX98357A DIN | GPIO2 |
| MAX98357A SD / EN | 3V3 |
| 喇叭 | MAX98357A + / - |


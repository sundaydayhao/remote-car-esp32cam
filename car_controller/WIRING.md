# Remote Car Wiring Guide

本文档记录当前小车控制端模块的接线方案。适用模块：

- ESP32-CAM
- L298N 电机驱动板
- LM2596 降压模块
- 船型电源开关
- 2 个直流减速电机
- 2 个 SG90 舵机，用于摄像头云台左右 / 上下转向

## 1. 电源总接线

建议使用电池给 L298N 和 LM2596 供电，LM2596 输出稳定 5V 给 ESP32-CAM 和两个 SG90。

接 ESP32-CAM 前，先用万用表把 LM2596 输出调到 5.0V 左右。

```text
电池正极 +  -> 电源开关 -> 分两路：
                 1. L298N +12V / VMS
                 2. LM2596 IN+

电池负极 -  -> L298N GND
电池负极 -  -> LM2596 IN-

LM2596 OUT+ 5V -> ESP32-CAM 5V
LM2596 OUT+ 5V -> SG90 Pan 红线
LM2596 OUT+ 5V -> SG90 Tilt 红线

LM2596 OUT- GND -> ESP32-CAM GND
LM2596 OUT- GND -> SG90 Pan 棕色 / 黑色线
LM2596 OUT- GND -> SG90 Tilt 棕色 / 黑色线
LM2596 OUT- GND -> L298N GND
```

所有模块必须共地：ESP32-CAM GND、L298N GND、LM2596 OUT-、两个 SG90 GND 要连在一起。

两个 SG90 动作时电流较大，LM2596 建议至少能提供 5V 2A。不要从 ESP32-CAM 的 5V 脚再反向带两个舵机。

## 2. L298N 电机驱动接线

当前建议释放 GPIO12 和 GPIO14 给两个 SG90 舵机使用，因此 L298N 的 ENA / ENB 跳帽插回去，让电机满速运行，ESP32-CAM 只控制方向。

```text
L298N ENA 跳帽插上，不接 ESP32-CAM
L298N ENB 跳帽插上，不接 ESP32-CAM
```

### 方向控制线

| L298N | ESP32-CAM |
| --- | --- |
| IN1 | GPIO13 |
| IN2 | GPIO15 |
| IN3 | GPIO2 |
| IN4 | GPIO4 |
| GND | GND |

### 电机输出

```text
左电机 -> L298N OUT1 / OUT2
右电机 -> L298N OUT3 / OUT4
```

如果 App 点前进时某个轮子反转，把那个电机在 L298N 上的 OUT 两根线对调。

如果左右方向整体反了，可以把左右电机整组互换，或后续修改固件里的电机方向配置。

## 3. SG90 摄像头云台接线

SG90 常见线色：

| SG90 线色 | 功能 | 接到 |
| --- | --- | --- |
| 棕色 / 黑色 | GND | LM2596 OUT- / 公共 GND |
| 红色 | 5V | LM2596 OUT+ 5V |
| 黄色 / 橙色 | PWM 信号 | ESP32-CAM GPIO |

推荐信号线：

| 云台方向 | SG90 信号线 | ESP32-CAM |
| --- | --- | --- |
| Pan 左右 | 黄色 / 橙色 | GPIO14 |
| Tilt 上下 | 黄色 / 橙色 | GPIO12 |

GPIO12 是 ESP32 的启动绑定位之一。如果接上 Tilt 舵机后 ESP32-CAM 上电启动失败，可以临时拔掉 GPIO12 的舵机信号线再启动，或后续把 Tilt 改到其他可用 GPIO。

## 4. ESP32-CAM 烧录注意

烧录程序时：

```text
GPIO0 -> GND
```

烧录完成正常运行时：

```text
GPIO0 必须断开 GND
```

如果烧录或串口日志异常，先拔掉 L298N 控制线和 SG90 信号线，只保留 ESP32-CAM 的 5V / GND / 串口烧录线，确认程序能正常启动后再逐步接回外设。

## 5. 当前固件状态

当前 `car_controller` 里的 `/gimbal` 接口还是占位实现，返回 `gimbal_not_configured`。也就是说：

- 按本文档接好 SG90 后，硬件供电和信号线位置是预留好了。
- 还需要在 `car_controller` 固件中加入两个 SG90 的 PWM 控制逻辑，App 或网页上的上下左右云台控制才会真正驱动舵机。

## 6. 总接线速查

| 模块端子 | 连接到 |
| --- | --- |
| 电池 + | 电源开关输入 |
| 电源开关输出 | L298N +12V / VMS、LM2596 IN+ |
| 电池 - | L298N GND、LM2596 IN- |
| LM2596 OUT+ | ESP32-CAM 5V、两个 SG90 红线 |
| LM2596 OUT- | ESP32-CAM GND、L298N GND、两个 SG90 GND |
| L298N IN1 | ESP32-CAM GPIO13 |
| L298N IN2 | ESP32-CAM GPIO15 |
| L298N IN3 | ESP32-CAM GPIO2 |
| L298N IN4 | ESP32-CAM GPIO4 |
| SG90 Pan 信号 | ESP32-CAM GPIO14 |
| SG90 Tilt 信号 | ESP32-CAM GPIO12 |
| 左电机 | L298N OUT1 / OUT2 |
| 右电机 | L298N OUT3 / OUT4 |


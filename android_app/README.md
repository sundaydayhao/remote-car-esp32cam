# Android Car Controller

`android_app/` 是一个独立的 Android 原生控制端工程，默认对接小车端的 HTTP 接口：

- `GET /status`
- `GET /control`
- `GET /gimbal`
- `GET /heartbeat`
- `GET /jpg`

当前实现提供：

- 连接地址配置
- 方向控制和急停
- 速度滑条
- 云台占位控制
- 状态轮询
- 单帧预览

## 默认地址

```text
http://192.168.4.1
```

## 打开方式

用 Android Studio 直接打开 `android_app/` 目录。

## 说明

- 该工程默认允许明文 HTTP，因为小车热点当前使用 `http://192.168.4.1`。
- 目前先用 `/jpg` 做预览，后续可以再换成真正的 `/stream` 解码。
- `/audio` 还没有接入。

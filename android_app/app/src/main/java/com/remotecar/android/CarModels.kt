package com.remotecar.android

data class CarStatus(
    val uptimeSeconds: Long = 0L,
    val motion: String = "stop",
    val speed: Int = 0,
    val leftPercent: Int = 0,
    val rightPercent: Int = 0,
    val blocked: Boolean = false,
    val distanceCm: Int = -1,
    val cameraReady: Boolean = false,
    val audioReady: Boolean = false,
    val audioClient: Boolean = false,
    val controlAgeMs: Long = 0L,
    val heartbeatAgeMs: Long = 0L,
    val controlSource: String = "unknown",
    val controlClient: String = "",
    val networkMode: String = "",
    val ssid: String = "",
    val stationCount: Int = 0,
    val maxStationCount: Int = 0,
)

data class ControlResponse(
    val ok: Boolean,
    val cmd: String,
    val speed: Int,
    val left: Int,
    val right: Int,
    val blocked: Boolean,
    val distanceCm: Int,
    val seq: Long,
    val source: String,
    val client: String,
)

data class GimbalResponse(
    val ok: Boolean,
    val enabled: Boolean,
    val reason: String? = null,
)

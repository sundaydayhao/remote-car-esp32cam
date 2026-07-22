package com.remotecar.android

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import org.json.JSONObject
import java.io.BufferedInputStream
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

class CarApi {
    fun getStatus(baseUrl: String): CarStatus {
        val json = requestText(baseUrl, "/status")
        val root = JSONObject(json)
        val network = root.optJSONObject("network")

        return CarStatus(
            uptimeSeconds = root.optLong("uptime_s", 0L),
            motion = root.optString("cmd", "stop"),
            speed = root.optInt("speed", 0),
            leftPercent = root.optInt("left", 0),
            rightPercent = root.optInt("right", 0),
            blocked = root.optBoolean("blocked", false),
            distanceCm = root.optInt("distance_cm", -1),
            cameraReady = root.optBoolean("camera", false),
            audioReady = root.optBoolean("audio_ready", false),
            audioClient = root.optBoolean("audio_client", false),
            controlAgeMs = root.optLong("control_age_ms", 0L),
            heartbeatAgeMs = root.optLong("heartbeat_age_ms", 0L),
            controlSource = root.optString("control_source", "unknown"),
            controlClient = root.optString("control_client", ""),
            networkMode = network?.optString("mode", "") ?: "",
            ssid = network?.optString("ssid", "") ?: "",
            stationCount = network?.optInt("station_count", 0) ?: 0,
            maxStationCount = network?.optInt("max_station_count", 0) ?: 0,
        )
    }

    fun sendControl(baseUrl: String, command: String, speed: Int, seq: Long, clientId: String): ControlResponse {
        val path = "/control?cmd=$command&speed=$speed&seq=$seq&source=android&client=${encode(clientId)}"
        val root = JSONObject(requestText(baseUrl, path))
        return ControlResponse(
            ok = root.optBoolean("ok", false),
            cmd = root.optString("cmd", command),
            speed = root.optInt("speed", speed),
            left = root.optInt("left", 0),
            right = root.optInt("right", 0),
            blocked = root.optBoolean("blocked", false),
            distanceCm = root.optInt("distance_cm", -1),
            seq = root.optLong("seq", seq),
            source = root.optString("source", "android"),
            client = root.optString("client", clientId),
        )
    }

    fun sendGimbal(baseUrl: String, pan: Int, tilt: Int, center: Boolean, seq: Long, clientId: String): GimbalResponse {
        val path = "/gimbal?pan=$pan&tilt=$tilt&center=$center&seq=$seq&source=android&client=${encode(clientId)}"
        val root = JSONObject(requestText(baseUrl, path))
        return GimbalResponse(
            ok = root.optBoolean("ok", false),
            enabled = root.optBoolean("enabled", false),
            reason = root.optString("reason").takeIf { it.isNotBlank() },
        )
    }

    fun sendHeartbeat(baseUrl: String, seq: Long, clientId: String) {
        requestText(baseUrl, "/heartbeat?seq=$seq&source=android&client=${encode(clientId)}")
    }

    fun fetchSnapshot(baseUrl: String): Bitmap? {
        val url = URL(normalizeBaseUrl(baseUrl) + "/jpg?ts=" + System.currentTimeMillis())
        val connection = (url.openConnection() as HttpURLConnection).apply {
            connectTimeout = 4000
            readTimeout = 4000
            useCaches = false
            requestMethod = "GET"
        }

        return try {
            connection.inputStream.use { input ->
                BitmapFactory.decodeStream(BufferedInputStream(input))
            }
        } finally {
            connection.disconnect()
        }
    }

    private fun requestText(baseUrl: String, path: String): String {
        val connection = openConnection(baseUrl, path)
        return try {
            val stream = if (connection.responseCode >= 400) connection.errorStream else connection.inputStream
            stream?.bufferedReader()?.use { it.readText() }
                ?: throw IOException("empty response from ${connection.url}")
        } finally {
            connection.disconnect()
        }
    }

    private fun openConnection(baseUrl: String, path: String): HttpURLConnection {
        val connection = URL(normalizeBaseUrl(baseUrl) + path).openConnection() as HttpURLConnection
        connection.connectTimeout = 4000
        connection.readTimeout = 4000
        connection.useCaches = false
        connection.requestMethod = "GET"
        return connection
    }

    private fun normalizeBaseUrl(baseUrl: String): String = baseUrl.trim().trimEnd('/')

    private fun encode(value: String): String = java.net.URLEncoder.encode(value, Charsets.UTF_8.name())
}

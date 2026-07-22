package com.remotecar.android

import android.graphics.Bitmap
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.concurrent.atomic.AtomicLong

data class CarUiState(
    val baseUrl: String = "http://192.168.4.1",
    val speed: Int = 55,
    val status: CarStatus? = null,
    val preview: Bitmap? = null,
    val busy: Boolean = false,
    val lastMessage: String = "Not connected",
    val error: String? = null,
)

class CarViewModel : ViewModel() {
    private val api = CarApi()
    private val seq = AtomicLong(1)
    private var monitorJob: Job? = null

    var uiState by mutableStateOf(CarUiState())
        private set

    init {
        startMonitoring()
    }

    fun updateBaseUrl(value: String) {
        uiState = uiState.copy(baseUrl = value.trim())
        startMonitoring()
    }

    fun updateSpeed(value: Int) {
        uiState = uiState.copy(speed = value.coerceIn(0, 100))
    }

    fun refreshNow() {
        viewModelScope.launch {
            refreshStatus()
            refreshSnapshot()
        }
    }

    fun control(command: String) {
        viewModelScope.launch {
            val baseUrl = uiState.baseUrl
            val speed = uiState.speed
            runCatching {
                withContext(Dispatchers.IO) {
                    api.sendControl(
                        baseUrl = baseUrl,
                        command = command,
                        speed = speed,
                        seq = nextSeq(),
                        clientId = clientId(),
                    )
                }
            }.onSuccess { response ->
                uiState = uiState.copy(
                    busy = false,
                    lastMessage = "Control: ${response.cmd} @${response.speed}",
                    error = null,
                )
                refreshStatus()
            }.onFailure { error ->
                uiState = uiState.copy(busy = false, error = error.message)
            }
        }
    }

    fun gimbal(center: Boolean, pan: Int = 0, tilt: Int = 0) {
        viewModelScope.launch {
            val baseUrl = uiState.baseUrl
            runCatching {
                withContext(Dispatchers.IO) {
                    api.sendGimbal(
                        baseUrl = baseUrl,
                        pan = pan,
                        tilt = tilt,
                        center = center,
                        seq = nextSeq(),
                        clientId = clientId(),
                    )
                }
            }.onSuccess { response ->
                uiState = uiState.copy(
                    lastMessage = response.reason ?: "Gimbal request sent",
                    error = if (response.ok) null else response.reason,
                )
                refreshStatus()
            }.onFailure { error ->
                uiState = uiState.copy(error = error.message)
            }
        }
    }

    private fun startMonitoring() {
        monitorJob?.cancel()
        monitorJob = viewModelScope.launch {
            while (isActive) {
                refreshStatus()
                refreshSnapshot()
                runCatching {
                    withContext(Dispatchers.IO) {
                        api.sendHeartbeat(uiState.baseUrl, nextSeq(), clientId())
                    }
                }
                delay(1000)
            }
        }
    }

    private suspend fun refreshStatus() {
        runCatching {
            withContext(Dispatchers.IO) {
                api.getStatus(uiState.baseUrl)
            }
        }.onSuccess { status ->
            uiState = uiState.copy(
                status = status,
                lastMessage = "Status refreshed",
                error = null,
            )
        }.onFailure { error ->
            uiState = uiState.copy(error = error.message)
        }
    }

    private suspend fun refreshSnapshot() {
        runCatching {
            withContext(Dispatchers.IO) {
                api.fetchSnapshot(uiState.baseUrl)
            }
        }.onSuccess { bitmap ->
            if (bitmap != null) {
                uiState = uiState.copy(preview = bitmap)
            }
        }.onFailure { error ->
            uiState = uiState.copy(error = error.message)
        }
    }

    private fun nextSeq(): Long = seq.getAndIncrement()

    private fun clientId(): String = "android-${android.os.Build.MODEL.replace(' ', '_')}"
}

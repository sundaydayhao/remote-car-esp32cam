package com.remotecar.android

import android.graphics.Bitmap
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.ArrowDownward
import androidx.compose.material.icons.filled.ArrowForward
import androidx.compose.material.icons.filled.ArrowUpward
import androidx.compose.material.icons.filled.CenterFocusStrong
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    val vm: CarViewModel = viewModel()
                    CarScreen(vm)
                }
            }
        }
    }
}

@Composable
private fun CarScreen(vm: CarViewModel) {
    val state = vm.uiState
    var baseUrlDraft by remember(state.baseUrl) { mutableStateOf(state.baseUrl) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Text(
            text = "Remote Car",
            style = MaterialTheme.typography.headlineSmall,
            fontWeight = FontWeight.SemiBold,
        )

        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(
                        value = baseUrlDraft,
                        onValueChange = { baseUrlDraft = it },
                        modifier = Modifier.weight(1f),
                        singleLine = true,
                        label = { Text("Car URL") },
                    )
                    IconButton(onClick = { vm.updateBaseUrl(baseUrlDraft) }) {
                        Icon(Icons.Default.Refresh, contentDescription = "Refresh")
                    }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TextButton(onClick = {
                        baseUrlDraft = "http://192.168.4.1"
                        vm.updateBaseUrl(baseUrlDraft)
                    }) {
                        Text("Default")
                    }
                    TextButton(onClick = vm::refreshNow) {
                        Text("Refresh")
                    }
                }
                Text(state.lastMessage, style = MaterialTheme.typography.bodyMedium)
                state.error?.let {
                    Text(it, color = MaterialTheme.colorScheme.error)
                }
            }
        }

        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Preview", style = MaterialTheme.typography.titleMedium)
                SnapshotView(bitmap = state.preview)
                Text(
                    text = state.status?.let {
                        "camera=${it.cameraReady}  audio=${it.audioReady}  network=${it.networkMode}"
                    } ?: "Waiting for status",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }

        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Drive", style = MaterialTheme.typography.titleMedium)
                DPad(
                    onForward = { vm.control("forward") },
                    onBack = { vm.control("backward") },
                    onLeft = { vm.control("left") },
                    onRight = { vm.control("right") },
                    onStop = { vm.control("stop") },
                )
                Text("Speed ${state.speed}", style = MaterialTheme.typography.bodyMedium)
                Slider(
                    value = state.speed.toFloat(),
                    onValueChange = { vm.updateSpeed(it.toInt()) },
                    valueRange = 0f..100f,
                )
            }
        }

        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Gimbal", style = MaterialTheme.typography.titleMedium)
                DPad(
                    onForward = { vm.gimbal(center = false, pan = 0, tilt = 15) },
                    onBack = { vm.gimbal(center = false, pan = 0, tilt = -15) },
                    onLeft = { vm.gimbal(center = false, pan = -15, tilt = 0) },
                    onRight = { vm.gimbal(center = false, pan = 15, tilt = 0) },
                    onStop = { vm.gimbal(center = true) },
                )
            }
        }

        Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Status", style = MaterialTheme.typography.titleMedium)
                StatusLine("Motion", state.status?.motion ?: "-")
                StatusLine("Speed", state.status?.speed?.toString() ?: "-")
                StatusLine("Distance", state.status?.distanceCm?.let { "$it cm" } ?: "-")
                StatusLine("Control", state.status?.let { "${it.controlSource} / ${it.controlClient}" } ?: "-")
                StatusLine("Wi-Fi", state.status?.let { "${it.networkMode} / ${it.ssid}" } ?: "-")
                StatusLine("Clients", state.status?.stationCount?.toString() ?: "-")
                StatusLine("Audio", if (state.status?.audioReady == true) "ready" else "off")
            }
        }
    }
}

@Composable
private fun SnapshotView(bitmap: Bitmap?) {
    if (bitmap == null) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .aspectRatio(4f / 3f),
            contentAlignment = Alignment.Center,
        ) {
            Text("No preview")
        }
        return
    }

    Image(
        bitmap = bitmap.asImageBitmap(),
        contentDescription = "snapshot",
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(4f / 3f),
        contentScale = ContentScale.Crop,
    )
}

@Composable
private fun StatusLine(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, style = MaterialTheme.typography.bodyMedium)
        Text(value, style = MaterialTheme.typography.bodyMedium)
    }
}

@Composable
private fun DPad(
    onForward: () -> Unit,
    onBack: () -> Unit,
    onLeft: () -> Unit,
    onRight: () -> Unit,
    onStop: () -> Unit,
) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        IconButton(onClick = onForward, modifier = Modifier.size(64.dp)) {
            Icon(Icons.Default.ArrowUpward, contentDescription = "Forward")
        }
        Row(horizontalArrangement = Arrangement.spacedBy(16.dp), verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = onLeft, modifier = Modifier.size(64.dp)) {
                Icon(Icons.Default.ArrowBack, contentDescription = "Left")
            }
            Button(onClick = onStop, contentPadding = PaddingValues(12.dp)) {
                Icon(Icons.Default.Pause, contentDescription = "Stop")
                Spacer(modifier = Modifier.size(6.dp))
                Text("Stop")
            }
            IconButton(onClick = onRight, modifier = Modifier.size(64.dp)) {
                Icon(Icons.Default.ArrowForward, contentDescription = "Right")
            }
        }
        IconButton(onClick = onBack, modifier = Modifier.size(64.dp)) {
            Icon(Icons.Default.ArrowDownward, contentDescription = "Back")
        }
        IconButton(onClick = onStop, modifier = Modifier.size(64.dp)) {
            Icon(Icons.Default.CenterFocusStrong, contentDescription = "Center")
        }
    }
}

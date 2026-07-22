const els = {
  baseUrl: document.querySelector("#baseUrl"),
  applyUrl: document.querySelector("#applyUrl"),
  linkDot: document.querySelector("#linkDot"),
  linkState: document.querySelector("#linkState"),
  motionState: document.querySelector("#motionState"),
  distanceState: document.querySelector("#distanceState"),
  speed: document.querySelector("#speed"),
  speedValue: document.querySelector("#speedValue"),
  refreshNow: document.querySelector("#refreshNow"),
  liveNow: document.querySelector("#liveNow"),
  snapshotNow: document.querySelector("#snapshotNow"),
  stream: document.querySelector("#stream"),
  previewFallback: document.querySelector("#previewFallback"),
  centerGimbal: document.querySelector("#centerGimbal"),
  stopDrive: document.querySelector("#stopDrive"),
  statusList: document.querySelector("#statusList"),
};

const state = {
  baseUrl: normalizeBaseUrl(els.baseUrl.value || defaultBaseUrl()),
  speed: Number(els.speed.value),
  seq: 1,
  clientId: makeClientId(),
  polling: null,
  heartbeat: null,
  streamOk: true,
};

els.baseUrl.value = state.baseUrl;
els.speedValue.textContent = String(state.speed);

els.applyUrl.addEventListener("click", () => {
  state.baseUrl = normalizeBaseUrl(els.baseUrl.value);
  els.baseUrl.value = state.baseUrl;
  applyStreamSource();
  refreshAll();
});

els.baseUrl.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    els.applyUrl.click();
  }
});

els.speed.addEventListener("input", () => {
  state.speed = Number(els.speed.value);
  els.speedValue.textContent = String(state.speed);
});

els.refreshNow.addEventListener("click", refreshAll);
els.liveNow.addEventListener("click", refreshLive);
els.snapshotNow.addEventListener("click", refreshSnapshot);
els.centerGimbal.addEventListener("click", () => sendGimbal({ center: true }));
els.stopDrive.addEventListener("click", () => sendDrive("stop"));

document.querySelectorAll("[data-drive]").forEach((button) => {
  holdButton(button, () => sendDrive(button.dataset.drive), () => sendDrive("stop"));
});

document.querySelectorAll("[data-gimbal]").forEach((button) => {
  holdButton(button, () => sendGimbal(gimbalFromAction(button.dataset.gimbal)), () => sendGimbal({ center: true }));
});

applyStreamSource();
startTimers();
refreshAll();

function startTimers() {
  clearInterval(state.polling);
  clearInterval(state.heartbeat);

  state.polling = setInterval(refreshStatus, 1000);
  state.heartbeat = setInterval(() => sendHeartbeat().catch(() => {}), 1000);
}

function applyStreamSource() {
  const streamUrl = joinUrl(state.baseUrl, "/stream");
  els.stream.src = streamUrl;
  els.stream.onerror = () => {
    state.streamOk = false;
    els.previewFallback.classList.remove("hidden");
    els.linkDot.classList.add("warn");
  };
  els.stream.onload = () => {
    state.streamOk = true;
    els.previewFallback.classList.add("hidden");
    els.linkDot.classList.remove("warn");
  };
}

async function refreshAll() {
  await Promise.allSettled([refreshStatus(), refreshLive()]);
}

async function refreshStatus() {
  try {
    const status = await requestJson("/status");
    els.linkState.textContent = "Connected";
    els.linkDot.classList.remove("warn");
    els.motionState.textContent = status.cmd || "stop";
    els.distanceState.textContent = `${status.distance_cm ?? "--"} cm`;
    renderStatus(status);
  } catch (error) {
    els.linkState.textContent = "Offline";
    els.linkDot.classList.add("warn");
    renderStatus(null, error.message);
  }
}

async function refreshSnapshot() {
  const url = `${joinUrl(state.baseUrl, "/jpg")}?ts=${Date.now()}`;
  els.stream.src = url;
  els.previewFallback.classList.add("hidden");
  state.streamOk = true;
}

async function refreshLive() {
  const url = `${joinUrl(state.baseUrl, "/stream")}?ts=${Date.now()}`;
  els.stream.src = url;
  els.previewFallback.classList.add("hidden");
  state.streamOk = true;
}

async function sendDrive(command) {
  await requestJson(`/control?cmd=${encodeURIComponent(command)}&speed=${state.speed}&seq=${nextSeq()}&source=web&client=${encodeURIComponent(state.clientId)}`);
  if (command === "stop") {
    els.motionState.textContent = "stop";
  }
}

async function sendGimbal({ pan = 0, tilt = 0, center = false }) {
  await requestJson(`/gimbal?pan=${pan}&tilt=${tilt}&center=${center}&seq=${nextSeq()}&source=web&client=${encodeURIComponent(state.clientId)}`);
}

async function sendHeartbeat() {
  await requestJson(`/heartbeat?seq=${nextSeq()}&source=web&client=${encodeURIComponent(state.clientId)}`);
}

async function requestJson(path) {
  const response = await fetch(joinUrl(state.baseUrl, path), { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`${response.status} ${response.statusText}`);
  }
  return response.json();
}

function renderStatus(status, errorText) {
  const rows = [];
  if (status) {
    rows.push(["Motion", status.cmd ?? "-"]);
    rows.push(["Speed", String(status.speed ?? "-")]);
    rows.push(["Left / Right", `${status.left ?? "-"} / ${status.right ?? "-"}`]);
    rows.push(["Blocked", String(Boolean(status.blocked))]);
    rows.push(["Distance", `${status.distance_cm ?? "-"} cm`]);
    rows.push(["Camera", String(Boolean(status.camera))]);
    rows.push(["Audio", String(Boolean(status.audio_ready))]);
    rows.push(["Control", `${status.control_source ?? "-"} / ${status.control_client ?? "-"}`]);
    rows.push(["Wi-Fi", `${status.network?.mode ?? "-"} / ${status.network?.ssid ?? "-"}`]);
    rows.push(["Clients", `${status.network?.station_count ?? "-"} / ${status.network?.max_station_count ?? "-"}`]);
  } else {
    rows.push(["Status", errorText || "Unavailable"]);
  }
  els.statusList.innerHTML = rows.map(([label, value]) => `
    <div class="status-item"><span>${escapeHtml(label)}</span><span>${escapeHtml(value)}</span></div>
  `).join("");
}

function holdButton(button, onPress, onRelease) {
  let active = false;

  const start = async (event) => {
    event.preventDefault();
    if (active) return;
    active = true;
    button.classList.add("active");
    try {
      await onPress();
    } finally {
      // no-op
    }
  };

  const end = async (event) => {
    if (!active) return;
    active = false;
    button.classList.remove("active");
    event.preventDefault();
    await onRelease();
  };

  button.addEventListener("pointerdown", start);
  button.addEventListener("pointerup", end);
  button.addEventListener("pointercancel", end);
  button.addEventListener("pointerleave", () => {
    if (!active) return;
    active = false;
    button.classList.remove("active");
    onRelease().catch(() => {});
  });
}

function gimbalFromAction(action) {
  switch (action) {
    case "tilt-up":
      return { pan: 0, tilt: 15, center: false };
    case "tilt-down":
      return { pan: 0, tilt: -15, center: false };
    case "pan-left":
      return { pan: -15, tilt: 0, center: false };
    case "pan-right":
      return { pan: 15, tilt: 0, center: false };
    default:
      return { center: true };
  }
}

function nextSeq() {
  state.seq += 1;
  return state.seq;
}

function joinUrl(baseUrl, path) {
  return `${normalizeBaseUrl(baseUrl)}${path}`;
}

function normalizeBaseUrl(value) {
  return String(value || "").trim().replace(/\/+$/, "") || "http://192.168.4.1";
}

function makeClientId() {
  const key = "remote_car_web_client_id";
  let value = localStorage.getItem(key);
  if (!value) {
    value = `web-${Math.random().toString(36).slice(2, 10)}`;
    localStorage.setItem(key, value);
  }
  return value;
}

function defaultBaseUrl() {
  return location.origin && location.origin !== "null" ? location.origin : "http://192.168.4.1";
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

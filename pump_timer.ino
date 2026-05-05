//written with chatgpt

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

ESP8266WebServer server(80);

// ---------- WiFi Access Point ----------
const char* apName = "Pump_Timer";
const char* apPassword = "pump12345";

// ---------- Relay pins ----------
const int RELAY_1_PIN = D1;   // GPIO5
const int RELAY_2_PIN = D2;   // GPIO4

// Change this if your relay board is active HIGH
const bool RELAY_ACTIVE_LOW = false;

// ---------- Timer state ----------
unsigned long onTimeMs = 10000;
unsigned long offTimeMs = 10000;

bool timerRunning = false;
bool pumpOn = false;

unsigned long phaseStartTime = 0;

// ---------- Relay control ----------
void setRelays(bool on) {
  pumpOn = on;

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_1_PIN, on ? LOW : HIGH);
    digitalWrite(RELAY_2_PIN, on ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_1_PIN, on ? HIGH : LOW);
    digitalWrite(RELAY_2_PIN, on ? HIGH : LOW);
  }
}

// ---------- Web page ----------
String webPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP8266 Pump Timer</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      max-width: 500px;
      margin: 30px auto;
      padding: 20px;
      background: #f4f4f4;
    }
    .card {
      background: white;
      padding: 20px;
      border-radius: 12px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.15);
    }
    input, button {
      width: 100%;
      padding: 12px;
      margin-top: 10px;
      font-size: 1rem;
    }
    button {
      border: none;
      border-radius: 8px;
      color: white;
      cursor: pointer;
    }
    .start { background: #1b8f3a; }
    .stop { background: #c62828; }
    .status {
      margin-top: 20px;
      padding: 15px;
      background: #eef;
      border-radius: 8px;
      font-size: 1.1rem;
    }
  </style>
</head>
<body>
  <div class="card">
    <h2>ESP8266 Pump Timer</h2>

    <label>ON time, seconds</label>
    <input id="onTime" type="number" min="1" value="10">

    <label>OFF time, seconds</label>
    <input id="offTime" type="number" min="1" value="10">

    <button class="start" onclick="startTimer()">Start</button>
    <button class="stop" onclick="stopTimer()">Stop</button>

    <div class="status">
      <strong>Status:</strong>
      <div id="status">Loading...</div>
    </div>
  </div>

<script>
function startTimer() {
  let onTime = document.getElementById("onTime").value;
  let offTime = document.getElementById("offTime").value;

  fetch(`/start?on=${onTime}&off=${offTime}`)
    .then(() => updateStatus());
}

function stopTimer() {
  fetch('/stop')
    .then(() => updateStatus());
}

function updateStatus() {
  fetch('/status')
    .then(response => response.json())
    .then(data => {
      document.getElementById("status").innerHTML =
        "Timer running: " + data.running + "<br>" +
        "Pump: " + data.pump + "<br>" +
        "Current phase: " + data.phase + "<br>" +
        "Seconds remaining: " + data.remaining;
    });
}

setInterval(updateStatus, 1000);
updateStatus();
</script>
</body>
</html>
)rawliteral";
}

// ---------- Routes ----------
void handleRoot() {
  server.send(200, "text/html", webPage());
}

void handleStart() {
  if (server.hasArg("on") && server.hasArg("off")) {
    int onSeconds = server.arg("on").toInt();
    int offSeconds = server.arg("off").toInt();

    if (onSeconds < 1) onSeconds = 1;
    if (offSeconds < 1) offSeconds = 1;

    onTimeMs = (unsigned long)onSeconds * 1000UL;
    offTimeMs = (unsigned long)offSeconds * 1000UL;

    timerRunning = true;
    phaseStartTime = millis();
    setRelays(true);

    server.send(200, "text/plain", "Timer started");
  } else {
    server.send(400, "text/plain", "Missing ON or OFF time");
  }
}

void handleStop() {
  timerRunning = false;
  setRelays(false);
  server.send(200, "text/plain", "Timer stopped");
}

void handleStatus() {
  unsigned long now = millis();
  unsigned long elapsed = now - phaseStartTime;
  unsigned long phaseLength = pumpOn ? onTimeMs : offTimeMs;
  unsigned long remaining = 0;

  if (timerRunning && elapsed < phaseLength) {
    remaining = (phaseLength - elapsed) / 1000UL;
  }

  String json = "{";
  json += "\"running\":\"" + String(timerRunning ? "YES" : "NO") + "\",";
  json += "\"pump\":\"" + String(pumpOn ? "ON" : "OFF") + "\",";
  json += "\"phase\":\"" + String(pumpOn ? "ON time" : "OFF time") + "\",";
  json += "\"remaining\":" + String(remaining);
  json += "}";

  server.send(200, "application/json", json);
}

// ---------- Setup ----------
void setup() {
  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);

  setRelays(false);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName, apPassword);

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/status", handleStatus);

  server.begin();
}

// ---------- Main loop ----------
void loop() {
  server.handleClient();

  if (!timerRunning) {
    return;
  }

  unsigned long now = millis();
  unsigned long elapsed = now - phaseStartTime;

  if (pumpOn) {
    if (elapsed >= onTimeMs) {
      setRelays(false);       // turn pump OFF
      phaseStartTime = now;   // restart timer for OFF phase
    }
  } else {
    if (elapsed >= offTimeMs) {
      setRelays(true);        // turn pump ON
      phaseStartTime = now;   // restart timer for ON phase
    }
  }
}
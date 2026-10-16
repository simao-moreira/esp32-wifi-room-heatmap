/*
  Wi-Fi Room Mapper

  Arduino IDE setup for a later manual upload:
    Board: ESP32 Dev Module (or the matching ESP32 board for your clone)
    Upload speed: leave at the board default until the board is identified
    Serial monitor: 115200 baud

  This sketch intentionally stores no survey data on the ESP32. The web page,
  measurements, and heat map live only in the browser's localStorage.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <vector>

// Change these before flashing. WPA2 passwords must be at least 8 characters.
static const char *AP_SSID = "WiFi-Room-Mapper";
static const char *AP_PASSWORD = "change-this-password";

WebServer server(80);

enum class ScanState { Idle, Scanning };

struct NetworkInfo {
  String bssid;
  String ssid;
  int32_t rssi;
  int32_t channel;
  String encryption;
};

ScanState scanState = ScanState::Idle;
std::vector<NetworkInfo> latestNetworks;
unsigned long completedAtMs = 0;
String lastScanError;

String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    switch (c) {
      case '\\': escaped += "\\\\"; break;
      case '"': escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          escaped += " ";
        } else {
          escaped += c;
        }
    }
  }
  return escaped;
}

const char *stateName() {
  return scanState == ScanState::Scanning ? "scanning" : "idle";
}

void sendStatus() {
  String body = "{\"state\":\"" + String(stateName()) + "\",\"chip\":\"";
  body += jsonEscape(ESP.getChipModel());
  body += "\",\"ip\":\"" + WiFi.softAPIP().toString() + "\"}";
  server.send(200, "application/json; charset=utf-8", body);
}

void startScan() {
  latestNetworks.clear();
  lastScanError = "";
  const int result = WiFi.scanNetworks(true, true);  // async, include hidden SSIDs
  if (result == WIFI_SCAN_FAILED) {
    lastScanError = "Unable to start Wi-Fi scan.";
    scanState = ScanState::Idle;
    return;
  }
  scanState = ScanState::Scanning;
}

void sendLatestScan() {
  String body = "{\"state\":\"" + String(stateName()) + "\",\"completedAtMs\":";
  body += String(completedAtMs);
  body += ",\"error\":\"" + jsonEscape(lastScanError) + "\",\"networks\":[";

  for (size_t i = 0; i < latestNetworks.size(); ++i) {
    const NetworkInfo &network = latestNetworks[i];
    if (i > 0) body += ',';
    body += "{\"bssid\":\"" + jsonEscape(network.bssid);
    body += "\",\"ssid\":\"" + jsonEscape(network.ssid);
    body += "\",\"rssi\":" + String(network.rssi);
    body += ",\"channel\":" + String(network.channel);
    body += ",\"encryption\":\"" + jsonEscape(network.encryption) + "\"}";
  }
  body += "]}";
  server.send(200, "application/json; charset=utf-8", body);
}

void updateScanState() {
  if (scanState != ScanState::Scanning) return;

  const int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) return;

  if (count == WIFI_SCAN_FAILED) {
    lastScanError = "Wi-Fi scan failed.";
    scanState = ScanState::Idle;
    return;
  }

  if (count >= 0) {
    latestNetworks.reserve(count);
    for (int i = 0; i < count; ++i) {
      NetworkInfo network;
      network.bssid = WiFi.BSSIDstr(i);
      network.ssid = WiFi.SSID(i);
      network.rssi = WiFi.RSSI(i);
      network.channel = WiFi.channel(i);
      network.encryption = "auth-" + String((int)WiFi.encryptionType(i));
      latestNetworks.push_back(network);
    }
    completedAtMs = millis();
  }

  WiFi.scanDelete();
  scanState = ScanState::Idle;
}

const char INDEX_HTML[] PROGMEM = R"WIFI_MAPPER_HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <title>Wi-Fi Room Mapper</title>
  <style>
    :root { color-scheme: dark; --bg:#10151d; --panel:#18212d; --line:#2c3a4b; --ink:#eaf2fa; --muted:#99aabe; --accent:#51b7ff; --danger:#ff7171; }
    * { box-sizing:border-box; } body { margin:0; font:16px system-ui,-apple-system,Segoe UI,sans-serif; background:var(--bg); color:var(--ink); }
    header { padding:18px max(16px,env(safe-area-inset-left)); background:#131c27; border-bottom:1px solid var(--line); }
    h1 { margin:0; font-size:1.25rem; } h2 { margin:0 0 12px; font-size:1rem; } p { color:var(--muted); margin:6px 0; }
    main { max-width:1040px; margin:auto; padding:16px; display:grid; gap:16px; grid-template-columns:minmax(0,1fr) minmax(250px,340px); }
    .panel { background:var(--panel); border:1px solid var(--line); border-radius:12px; padding:14px; }
    .map-panel { grid-row:span 2; } .controls { display:flex; gap:8px; flex-wrap:wrap; align-items:end; }
    label { display:grid; gap:5px; color:var(--muted); font-size:.82rem; } input,select,button { font:inherit; border-radius:8px; border:1px solid var(--line); padding:9px; }
    input,select { color:var(--ink); background:#111923; min-width:82px; } button { cursor:pointer; color:#06131d; font-weight:700; background:var(--accent); border-color:var(--accent); }
    button.secondary { color:var(--ink); background:#223043; border-color:#3a4b61; } button.danger { color:#2b0808; background:var(--danger); border-color:var(--danger); } button:disabled { opacity:.45; cursor:not-allowed; }
    #status { min-height:24px; color:var(--muted); } #status.error { color:#ff9a9a; } #room { width:100%; aspect-ratio:1.35; display:block; border-radius:9px; background:#0b1017; touch-action:manipulation; }
    #measurements { max-height:230px; overflow:auto; display:grid; gap:7px; } .measurement { display:flex; justify-content:space-between; gap:8px; align-items:center; padding:8px; border:1px solid var(--line); border-radius:8px; }
    .measurement small { color:var(--muted); display:block; } .measurement button { padding:5px 8px; font-size:.8rem; } .hint { font-size:.85rem; color:var(--muted); }
    .file { max-width:145px; } @media (max-width:760px) { main { grid-template-columns:1fr; } .map-panel { grid-row:auto; } }
  </style>
</head>
<body>
  <header><h1>Wi-Fi Room Mapper</h1><p>Tap the room, then capture a Wi-Fi scan at that position.</p></header>
  <main>
    <section class="panel map-panel">
      <div class="controls">
        <label>Room width <input id="width" type="number" min="0.1" step="0.1"></label>
        <label>Room height <input id="height" type="number" min="0.1" step="0.1"></label>
        <label>Heat map network <select id="network"><option value="">No heat map</option></select></label>
      </div>
      <p class="hint">Room dimensions label the survey only. Point locations are stored proportionally, so changing dimensions keeps them in place.</p>
      <canvas id="room" aria-label="Room plan"></canvas>
      <p id="point" class="hint">Tap/click in the room to choose a new measurement point.</p>
    </section>
    <section class="panel">
      <h2>Capture</h2><p id="status">Connecting to ESP32…</p>
      <button id="scan" disabled>Scan at selected point</button>
      <button id="cancel" class="secondary">Clear selected point</button>
    </section>
    <section class="panel">
      <h2>Measurements</h2><div id="measurements"></div>
    </section>
    <section class="panel">
      <h2>Data</h2>
      <div class="controls"><button id="export" class="secondary">Export JSON</button><label>Import <input id="import" class="file" type="file" accept="application/json"></label><button id="clear" class="danger">Clear project</button></div>
    </section>
  </main>
  <script>
  (() => {
    'use strict';
    const KEY='wifi-room-mapper.project.v1';
    const $=id=>document.getElementById(id);
    const room=$('room'), ctx=room.getContext('2d'), width=$('width'), height=$('height'), network=$('network');
    const scanButton=$('scan'), status=$('status'), pointLabel=$('point'), measurementsEl=$('measurements');
    let pending=null, pollTimer=null;
    const blank=()=>({schema:1,room:{width:5,height:4},measurements:[]});
    function validProject(value) {
      if (!value || value.schema!==1 || !value.room || !Array.isArray(value.measurements)) return false;
      if (!(Number(value.room.width)>0) || !(Number(value.room.height)>0)) return false;
      return value.measurements.every(m=>m && Number.isFinite(m.x) && Number.isFinite(m.y) && m.x>=0 && m.x<=1 && m.y>=0 && m.y<=1 && Array.isArray(m.networks));
    }
    function load() { try { const stored=JSON.parse(localStorage.getItem(KEY)); return validProject(stored)?stored:blank(); } catch (_) { return blank(); } }
    let project=load();
    function save() { try { localStorage.setItem(KEY,JSON.stringify(project)); } catch (_) { setStatus('Browser storage failed. Export your project.',true); } }
    function setStatus(message,isError=false) { status.textContent=message; status.className=isError?'error':''; }
    function escapeText(value) { return String(value || '(hidden)'); }
    function resizeCanvas() { const rect=room.getBoundingClientRect(), dpr=window.devicePixelRatio||1; room.width=Math.max(1,Math.round(rect.width*dpr)); room.height=Math.max(1,Math.round(rect.height*dpr)); ctx.setTransform(dpr,0,0,dpr,0,0); draw(); }
    function canvasSize() { const r=room.getBoundingClientRect(); return {w:r.width,h:r.height}; }
    function selectedSamples() { const key=network.value; if (!key) return []; const samples=[]; project.measurements.forEach(m=>{ const n=m.networks.find(item=>item.bssid===key); if (n && Number.isFinite(Number(n.rssi))) samples.push({x:m.x,y:m.y,rssi:Number(n.rssi)}); }); return samples; }
    function heatColor(rssi,alpha) { const t=Math.max(0,Math.min(1,(rssi+90)/55)); return `hsla(${Math.round(t*120)},85%,48%,${alpha})`; }
    function drawHeat(w,h) {
      const samples=selectedSamples(); if (samples.length<4) return;
      const cells=32, cw=w/cells, ch=h/cells;
      for(let gy=0;gy<cells;gy++) for(let gx=0;gx<cells;gx++) {
        const x=(gx+.5)/cells,y=(gy+.5)/cells; let sum=0,weights=0,minDistance=Infinity;
        samples.forEach(s=>{ const d=Math.hypot(x-s.x,y-s.y); const weight=1/(d*d+.0008); sum+=s.rssi*weight; weights+=weight; minDistance=Math.min(minDistance,d); });
        const alpha=Math.max(.08,Math.min(.72,1-minDistance/.62)); ctx.fillStyle=heatColor(sum/weights,alpha); ctx.fillRect(gx*cw,gy*ch,cw+1,ch+1);
      }
    }
    function draw() {
      const {w,h}=canvasSize(); ctx.clearRect(0,0,w,h); ctx.fillStyle='#111923'; ctx.fillRect(0,0,w,h); drawHeat(w,h);
      ctx.strokeStyle='#5d7189'; ctx.lineWidth=2; ctx.strokeRect(1,1,w-2,h-2);
      project.measurements.forEach((m,index)=>{ ctx.fillStyle='#f3f7fb'; ctx.beginPath(); ctx.arc(m.x*w,m.y*h,5,0,Math.PI*2); ctx.fill(); ctx.fillStyle='#0a1017'; ctx.font='10px system-ui'; ctx.textAlign='center'; ctx.textBaseline='middle'; ctx.fillText(index+1,m.x*w,m.y*h+.5); });
      if (pending) { ctx.strokeStyle='#51b7ff'; ctx.lineWidth=2; ctx.beginPath(); ctx.arc(pending.x*w,pending.y*h,9,0,Math.PI*2); ctx.stroke(); }
      const samples=selectedSamples(); if(network.value && samples.length<4) { ctx.fillStyle='#c8d5e3'; ctx.font='13px system-ui'; ctx.fillText(`Need 4 samples for ${network.value}`,w/2,h-16); }
    }
    function updateInputs() { width.value=project.room.width; height.value=project.room.height; }
    function updateNetworkOptions() {
      const current=network.value, found=new Map(); project.measurements.forEach(m=>m.networks.forEach(n=>{ if(n.bssid && !found.has(n.bssid)) found.set(n.bssid,n.ssid||'(hidden)'); }));
      network.replaceChildren(new Option('No heat map','')); [...found.entries()].sort((a,b)=>a[0].localeCompare(b[0])).forEach(([bssid,ssid])=>network.add(new Option(`${escapeText(ssid)} · ${bssid}`,bssid)));
      network.value=found.has(current)?current:'';
    }
    function renderMeasurements() {
      measurementsEl.replaceChildren(); if(!project.measurements.length) { const p=document.createElement('p'); p.className='hint'; p.textContent='No saved measurements.'; measurementsEl.append(p); return; }
      project.measurements.forEach((m,index)=>{ const row=document.createElement('div'), text=document.createElement('div'), remove=document.createElement('button'); row.className='measurement'; text.textContent=`#${index+1} · ${(m.x*project.room.width).toFixed(1)} × ${(m.y*project.room.height).toFixed(1)} m`; const small=document.createElement('small'); small.textContent=`${m.networks.length} networks · ${new Date(m.capturedAt).toLocaleString()}`; text.append(small); remove.textContent='Delete'; remove.className='secondary'; remove.onclick=()=>{project.measurements.splice(index,1);save();render();}; row.append(text,remove); measurementsEl.append(row); });
    }
    function render() { updateInputs(); updateNetworkOptions(); renderMeasurements(); pointLabel.textContent=pending?`Selected: ${(pending.x*project.room.width).toFixed(1)} × ${(pending.y*project.room.height).toFixed(1)} m. Capture when ready.`:'Tap/click in the room to choose a new measurement point.'; scanButton.disabled=!pending || Boolean(pollTimer); draw(); }
    function updateDimensions() { const w=Number(width.value),h=Number(height.value); if(w>0&&h>0){project.room.width=w;project.room.height=h;save();draw();renderMeasurements();} }
    room.addEventListener('pointerdown',event=>{const r=room.getBoundingClientRect(); pending={x:Math.max(0,Math.min(1,(event.clientX-r.left)/r.width)),y:Math.max(0,Math.min(1,(event.clientY-r.top)/r.height))};render();});
    width.addEventListener('change',updateDimensions); height.addEventListener('change',updateDimensions); network.addEventListener('change',draw);
    $('cancel').onclick=()=>{pending=null;render();};
    async function latest() { const response=await fetch('/api/scans/latest',{cache:'no-store'}); if(!response.ok) throw new Error(`ESP32 returned ${response.status}`); return response.json(); }
    function stopPolling() { if(pollTimer){clearTimeout(pollTimer);pollTimer=null;} }
    async function poll() { try { const result=await latest(); if(result.state==='scanning'){setStatus('Scanning nearby Wi-Fi networks…');pollTimer=setTimeout(poll,750);return;} stopPolling(); if(result.error) throw new Error(result.error); if(!pending) throw new Error('The selected point was cleared before scan completion.'); project.measurements.push({id:crypto.randomUUID?crypto.randomUUID():`${Date.now()}-${Math.random()}`,x:pending.x,y:pending.y,capturedAt:new Date().toISOString(),networks:Array.isArray(result.networks)?result.networks:[]}); pending=null;save();setStatus(`Saved ${result.networks.length} networks at this point.`);render(); } catch(error) { stopPolling();setStatus(`Scan not saved: ${error.message}`,true);render(); } }
    scanButton.onclick=async()=>{ if(!pending||pollTimer)return; try { const response=await fetch('/api/scans',{method:'POST'}); if(response.status!==202&&response.status!==409) throw new Error(`ESP32 returned ${response.status}`); setStatus('Starting Wi-Fi scan…'); scanButton.disabled=true; pollTimer=setTimeout(poll,250); } catch(error) { setStatus(`Could not start scan: ${error.message}`,true);render(); } };
    $('export').onclick=()=>{const blob=new Blob([JSON.stringify(project,null,2)],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download='wifi-room-survey.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
    $('import').onchange=event=>{const file=event.target.files[0];if(!file)return;const reader=new FileReader();reader.onload=()=>{try{const value=JSON.parse(reader.result);if(!validProject(value))throw new Error('unsupported project format');project=value;pending=null;save();setStatus('Project imported.');render();}catch(error){setStatus(`Import failed: ${error.message}`,true);}event.target.value='';};reader.readAsText(file);};
    $('clear').onclick=()=>{if(confirm('Delete this browser’s saved room and measurements?')){project=blank();pending=null;localStorage.removeItem(KEY);setStatus('Project cleared.');render();}};
    window.addEventListener('resize',resizeCanvas); updateInputs(); render(); resizeCanvas();
    fetch('/api/status',{cache:'no-store'}).then(r=>r.json()).then(s=>setStatus(`Connected to ${s.chip} at ${s.ip}.`)).catch(()=>setStatus('ESP32 API is unavailable. Rejoin its Wi-Fi hotspot and reload.',true));
  })();
  </script>
</body>
</html>
)WIFI_MAPPER_HTML";

void startAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Failed to start Wi-Fi hotspot. Check AP password length.");
  }
}

void setup() {
  Serial.begin(115200);
  startAccessPoint();

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/scans", HTTP_POST, []() {
    if (scanState == ScanState::Scanning) {
      server.send(409, "application/json; charset=utf-8", "{\"state\":\"scanning\"}");
      return;
    }
    startScan();
    if (scanState != ScanState::Scanning) {
      server.send(503, "application/json; charset=utf-8", "{\"state\":\"idle\",\"error\":\"Unable to start scan.\"}");
      return;
    }
    server.send(202, "application/json; charset=utf-8", "{\"state\":\"scanning\"}");
  });
  server.on("/api/scans/latest", HTTP_GET, sendLatestScan);
  server.onNotFound([]() {
    server.send(404, "application/json; charset=utf-8", "{\"error\":\"Not found\"}");
  });
  server.begin();

  Serial.print("Hotspot ready at http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  updateScanState();
}

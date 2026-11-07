#pragma once
#include <WebServer.h>
#include "config.h"
#include "adc.h"
#include "flash.h"
#include "fft.h"

WebServer server(80);
bool flashReady = false;
int fftChannel = 0;

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Analyzer</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,Arial,sans-serif;background:#0b0b14;color:#ddd;padding:8px;font-size:13px}
h1{color:#00d4ff;font-size:1.15rem;margin-bottom:2px}
.sub{color:#666;font-size:11px;margin-bottom:8px}
.layout{display:flex;gap:10px;align-items:flex-start;max-width:1200px;margin:0 auto}
.left{flex:1.8;min-width:0}
.mid{flex:0 0 120px}
.right{flex:0 0 320px;max-width:340px}
.card{background:#16162a;border-radius:10px;padding:10px;margin-bottom:8px}
.row{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:6px}
button{background:#00d4ff;color:#000;border:none;padding:8px 12px;border-radius:6px;font-size:12px;font-weight:600;cursor:pointer}
button:hover{background:#00b8e0}
button.sec{background:#2a2a40;color:#ccc}
.ch-grid{display:flex;flex-direction:column;gap:4px;font-size:11px}
.ch-grid label{background:#222;padding:5px 8px;border-radius:5px;cursor:pointer;display:block}
.ch-grid label.dig{background:#1a2a1a}
.controls{display:flex;flex-wrap:wrap;gap:6px;align-items:center;font-size:11px;margin:6px 0}
.controls select,.controls input{background:#222;color:#eee;border:1px solid #444;border-radius:4px;padding:3px 5px}
canvas.main{width:100%;background:#080812;border-radius:8px;display:block}
.legend{display:flex;flex-wrap:wrap;gap:5px;margin-top:4px;font-size:10px}
.legend span{display:inline-flex;align-items:center;gap:2px}
.dot{width:7px;height:7px;border-radius:50%}
.info{font-size:11px;color:#888;margin-top:4px}
#status{background:#0d0d18;border-radius:5px;padding:5px;font-size:11px;color:#00ff9d;margin-top:5px;min-height:22px}
.memwarn{background:#3a2000;color:#ffb84d;padding:6px;border-radius:6px;font-size:11px;margin-bottom:6px;display:none}
.filelist{max-height:75vh;overflow-y:auto}
.file{border:1px solid #2a2a40;border-radius:6px;margin-bottom:8px;background:#12122a}
.file-head{display:flex;align-items:center;gap:6px;padding:8px;cursor:pointer}
.file-head:hover{background:#1a1a32}
.file-name{flex:1;font-weight:600;font-size:12px;color:#00d4ff}
.file-meta{font-size:10px;color:#888}
.file-del{background:transparent;color:#f66;border:none;cursor:pointer;font-size:14px;padding:0 4px}
.file-body{display:none;padding:0 6px 8px}
.file.open .file-body{display:block}
.file canvas{width:100%;background:#080812;border-radius:4px;margin-top:4px}
.tabs button{padding:5px 10px;font-size:11px}
.tabs button.on{background:#00d4ff;color:#000}
.mid-title{font-size:11px;color:#888;margin-bottom:6px;text-align:center}
@media(max-width:900px){
  .layout{flex-direction:column}
  .mid{flex:auto;width:100%}
  .ch-grid{display:grid;grid-template-columns:1fr 1fr;flex-direction:row}
  .right{flex:auto;max-width:none}
}
</style>
</head>
<body>
<div class="layout">
  <div class="left">
    <h1>ESP32 Analyzer</h1>
    <div class="sub">6A + 8D • сохранение файлов</div>
    <div class="card">
      <div class="row">
        <button onclick="capture()">Захватить</button>
        <button onclick="saveFile()">Сохранить</button>
        
      </div>
      <div class="controls">
        <label>Триггер
          <select id="trigCh" onchange="updTrig()">
            <option value="0">A0</option><option value="1">A1</option><option value="2">A2</option>
            <option value="3">A3</option><option value="4">A4</option><option value="5">A5</option>
            <option value="6">D0</option><option value="7">D1</option><option value="8">D2</option>
            <option value="9">D3</option><option value="10">D4</option><option value="11">D5</option>
            <option value="12">D6</option><option value="13">D7</option>
          </select>
        </label>
        <label>Ур.
          <input type="range" id="trigLvl" min="0" max="4095" value="2048" style="width:70px"
            oninput="trigV.textContent=(this.value/4095*3.3).toFixed(2)+'V'" onchange="updTrig()">
          <span id="trigV">1.65V</span>
        </label>
        <label><select id="trigEdge" onchange="updTrig()"><option value="1">↑</option><option value="0">↓</option></select></label>
        <label>Окно
          <select id="winMs" onchange="fetch('/window?ms='+this.value)">
            <option value="0">макс.быстро</option>
            <option value="10">10 мс</option>
            <option value="20">20 мс</option>
            <option value="50" selected>50 мс</option>
            <option value="100">100 мс</option>
            <option value="200">200 мс</option>
            <option value="500">500 мс</option>
          </select>
        </label>
        <label>FFT <select id="fftCh"><option>0</option><option>1</option><option>2</option><option>3</option><option>4</option><option>5</option></select></label>
        <label><input type="checkbox" id="liveCb" onchange="toggleLive(this.checked)"> Live</label>
      </div>
      <div class="controls" style="margin-top:2px">
        <span style="color:#888">Калибровка A0:</span>
        <label>offset <input id="calOff" type="number" step="1" value="0" style="width:60px"></label>
        <label>scale <input id="calSc" type="number" step="0.0001" value="0.0008" style="width:70px" title="В/отсчёт, 3.3/4095≈0.000805"></label>
        <button class="sec" type="button" onclick="applyCal()">Применить к A0–A5</button>
      </div>
      <div class="tabs row">
        <button class="on" id="tabWave" onclick="showTab('wave')">Осциллограф</button>
        <button id="tabSpec" onclick="showTab('spec')">Спектр</button>
      </div>
      <canvas class="main" id="wave" width="720" height="380"></canvas>
      <canvas class="main" id="spec" width="720" height="220" style="display:none"></canvas>
      <div class="legend" id="legend"></div>
      <div class="info" id="info">—</div>
      <div id="status"></div>
    </div>
  </div>

  <div class="mid">
    <div class="card">
      <div class="mid-title">Каналы</div>
      <div class="ch-grid" id="chgrid"></div>
    </div>
  </div>

  <div class="right">
    <div class="card">
      <div style="font-weight:600;margin-bottom:6px">Сохранённые</div>
      <div class="memwarn" id="memwarn">Мало места: удалите файлы</div>
      <div class="filelist" id="filelist"><div style="color:#666;font-size:11px">Нет файлов</div></div>
    </div>
  </div>
</div>
<script>
const ACOLORS=['#00d4ff','#ff6b6b','#51cf66','#fcc419','#cc5de8','#20c997'];
const DCOLORS=['#a5d8ff','#ffc9c9','#b2f2bb','#ffe066','#eebefa','#99e9f2','#ffec99','#ffc078'];
const ANAMES=['A0','A1','A2','A3','A4','A5'];
const DNAMES=['D0','D1','D2','D3','D4','D5','D6','D7'];
let aEn=[1,1,1,1,1,1], dEn=[1,1,1,1,1,1,1,1];
let liveMode=false, liveTimer=null, lastData=null;
let openFiles={};

const grid=document.getElementById('chgrid'), leg=document.getElementById('legend');
ANAMES.forEach((n,i)=>{
  grid.innerHTML+=`<label><input type="checkbox" checked onchange="aEn[${i}]=+this.checked;setCh()"> ${n}</label>`;
  leg.innerHTML+=`<span><i class="dot" style="background:${ACOLORS[i]}"></i>${n}</span>`;
});
DNAMES.forEach((n,i)=>{
  grid.innerHTML+=`<label class="dig"><input type="checkbox" checked onchange="dEn[${i}]=+this.checked;setCh()"> ${n}</label>`;
  leg.innerHTML+=`<span><i class="dot" style="background:${DCOLORS[i]}"></i>${n}</span>`;
});

function setCh(){ fetch('/channels?a='+aEn.join(',')+'&d='+dEn.join(',')); }
function updTrig(){ fetch(`/trigger?ch=${trigCh.value}&lvl=${trigLvl.value}&edge=${trigEdge.value}`); }
function showTab(t){
  const w=t==='wave';
  wave.style.display=w?'block':'none';
  spec.style.display=w?'none':'block';
  tabWave.classList.toggle('on',w); tabSpec.classList.toggle('on',!w);
  if(!w&&lastData) drawSpec(lastData,spec);
}

function refreshFiles(){
  fetch('/files').then(r=>r.json()).then(d=>{
    const list=document.getElementById('filelist');
    const warn=document.getElementById('memwarn');
    if(d.used>=d.max-2) warn.style.display='block'; else warn.style.display='none';
    if(!d.files.length){ list.innerHTML='<div style="color:#666;font-size:11px">Нет файлов</div>'; return; }
    list.innerHTML='';
    d.files.forEach(f=>{
      const open=!!openFiles[f.id];
      const div=document.createElement('div');
      div.className='file'+(open?' open':'');
      div.id='file-'+f.id;
      div.innerHTML=`<div class="file-head" onclick="toggleFile(${f.id})">
        <span class="file-name">${f.name}</span>
        <span class="file-meta">${(f.time_us/1000).toFixed(0)}ms</span>
        <button class="file-del" style="color:#8cf" onclick="event.stopPropagation();dlCsv(${f.id},'${f.name}')" title="CSV">↓</button>
        <button class="file-del" onclick="event.stopPropagation();delFile(${f.id})" title="Удалить">✕</button>
      </div>
      <div class="file-body"><canvas id="fc-${f.id}" width="300" height="200"></canvas></div>`;
      list.appendChild(div);
      if(open) drawWave(openFiles[f.id], document.getElementById('fc-'+f.id), true);
    });
  });
}

function toggleFile(id){
  const el=document.getElementById('file-'+id);
  if(openFiles[id]){
    delete openFiles[id];
    el.classList.remove('open');
    return;
  }
  fetch('/file?id='+id).then(r=>r.json()).then(d=>{
    if(!d.points){ status.textContent='Ошибка чтения'; return; }
    openFiles[id]=d;
    el.classList.add('open');
    drawWave(d, document.getElementById('fc-'+id), true);
  });
}

function delFile(id){
  if(!confirm('Удалить файл?')) return;
  fetch('/delete?id='+id).then(r=>r.text()).then(()=>{
    delete openFiles[id];
    refreshFiles();
    status.textContent='Удалено';
  });
}

function dlCsv(id, name){
  status.textContent='CSV…';
  window.location.href='/csv?id='+id;
  setTimeout(()=>status.textContent='Скачивание '+name, 300);
}

function saveFile(){
  status.textContent='Сохраняю…';
  fetch('/save').then(r=>r.text()).then(t=>{
    status.textContent = t.startsWith('S') ? 'Сохранено: '+t : (t==='FULL'?'Нет места':'Ошибка');
    refreshFiles();
  });
}

fetch('/status').then(r=>r.json()).then(d=>{
  status.textContent=d.flash?'W25Q32: OK':'W25Q32: err';
  refreshFiles();
});

function capture(){
  status.textContent='Захват…';
  fetch('/capture?fftch='+fftCh.value).then(r=>r.json()).then(d=>{
    lastData=d;
    info.textContent=`${d.points} pts • ${(d.time_us/1000).toFixed(1)}ms • ~${d.rate} S/s`+(d.peak_freq?` • ${d.peak_freq}Hz`:'');
    status.textContent='OK';
    drawWave(d, wave, false);
    if(spec.style.display!=='none') drawSpec(d,spec);
  }).catch(()=>status.textContent='Ошибка');
}

function toggleLive(on){
  liveMode=!!on;
  if(liveTimer){clearInterval(liveTimer);liveTimer=null;}
  if(liveMode){
    capture();
    liveTimer=setInterval(capture, 1500); // пауза между захватами — меньше нагрев
  }
}
function applyCal(){
  const off=calOff.value, sc=calSc.value;
  fetch('/cal?off='+off+'&scale='+sc).then(r=>r.text()).then(t=>status.textContent=t);
}

function drawWave(d, canvas, mini){
  if(!canvas||!d) return;
  const ctx=canvas.getContext('2d'), W=canvas.width, H=canvas.height;
  ctx.fillStyle='#080812'; ctx.fillRect(0,0,W,H);
  const aH=H*(mini?0.5:0.55);
  ctx.strokeStyle='#1a1a2a'; ctx.fillStyle='#555'; ctx.font=mini?'9px sans-serif':'10px sans-serif'; ctx.lineWidth=1;
  const tMs = (d.time_us||0)/1000;
  if(!mini){
    for(let i=0;i<=4;i++){const y=i*aH/4;ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(W,y);ctx.stroke();ctx.fillText((3.3*(1-i/4)).toFixed(1)+'V',2,y+10);}
    for(let i=0;i<=4;i++){
      const x=i*W/4;
      ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,H);ctx.stroke();
      const t=tMs*i/4;
      ctx.fillText(t>=100?t.toFixed(0)+'ms':t.toFixed(1)+'ms', x+2, H-4);
    }
  } else {
    for(let i=0;i<=2;i++){const y=i*aH/2;ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(W,y);ctx.stroke();}
    for(let x=0;x<=W;x+=W/4){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,H);ctx.stroke();}
  }
  ctx.strokeStyle='#333';ctx.beginPath();ctx.moveTo(0,aH);ctx.lineTo(W,aH);ctx.stroke();
  const pts=d.points||0;
  if(d.channels){
    for(let ch=0;ch<d.channels.length;ch++){
      if(!mini && !aEn[ch]) continue;
      const data=d.channels[ch];
      ctx.strokeStyle=ACOLORS[ch%ACOLORS.length]; ctx.lineWidth=mini?1.2:1.3; ctx.beginPath();
      for(let i=0;i<pts;i++){const x=(i/(pts-1))*W,y=aH-(data[i]/4095)*aH;i?ctx.lineTo(x,y):ctx.moveTo(x,y);}
      ctx.stroke();
    }
  }
  if(d.digital){
    const dH=H-aH, laneH=dH/Math.max(1,d.digital.length);
    for(let ch=0;ch<d.digital.length;ch++){
      if(!mini && !dEn[ch]) continue;
      const data=d.digital[ch];
      const base=aH+ch*laneH+laneH*0.2, top=aH+ch*laneH+laneH*0.8;
      ctx.strokeStyle=DCOLORS[ch%DCOLORS.length]; ctx.lineWidth=1.1; ctx.beginPath();
      let prev=data[0]; ctx.moveTo(0,prev?top:base);
      for(let i=0;i<pts;i++){
        const x=(i/(pts-1))*W, y=data[i]?top:base;
        if(data[i]!==prev){ctx.lineTo(x,prev?top:base);ctx.lineTo(x,y);prev=data[i];}
        else ctx.lineTo(x,y);
      }
      ctx.stroke();
    }
  }
}

function drawSpec(d, canvas){
  const ctx=canvas.getContext('2d'), W=canvas.width, H=canvas.height;
  ctx.fillStyle='#080812'; ctx.fillRect(0,0,W,H);
  if(!d||!d.spectrum) return;
  const mag=d.spectrum,n=mag.length; let maxM=1;
  for(let i=1;i<n;i++) if(mag[i]>maxM) maxM=mag[i];
  ctx.fillStyle='#00d4ff'; const barW=Math.max(1,W/n);
  for(let i=0;i<n;i++){const h=(mag[i]/maxM)*(H-12);ctx.fillRect((i/n)*W,H-h-8,barW-0.5,h);}
  if(d.peak_freq){ctx.fillStyle='#ff6b6b';ctx.font='12px sans-serif';ctx.fillText(d.peak_freq+' Hz',W-90,14);}
}
</script>
</body>
</html>
)rawliteral";

inline String slotJson(int id, bool withSpec) {
  if (id < 0 || id >= NUM_SLOTS || !slots[id].valid) return "{}";
  CaptureSlot& sl = slots[id];
  float rate = sl.timeUs ? (BUFFER_SIZE * 1e6f) / sl.timeUs : 0;
  int peakFreq = 0;
  if (withSpec) {
    sampleBuffer = slots[id].analog;
    runFFT(fftChannel, rate);
    int peakBin = 1; float peakMag = 0;
    for (int i = 1; i < FFT_SIZE / 2; i++)
      if (fftMag[i] > peakMag) { peakMag = fftMag[i]; peakBin = i; }
    peakFreq = (int)binFreq(peakBin);
  }
  const int sendPoints = 256;
  const int step = max(1, (int)BUFFER_SIZE / sendPoints);
  String json = "{";
  json += "\"points\":" + String(sendPoints) + ",";
  json += "\"time_us\":" + String(sl.timeUs) + ",";
  json += "\"rate\":" + String((int)rate) + ",";
  json += "\"peak_freq\":" + String(peakFreq) + ",";
  if (withSpec) {
    json += "\"spectrum\":[";
    const int specBins = 64, specStep = (FFT_SIZE / 2) / specBins;
    for (int i = 0; i < specBins; i++) {
      if (i) json += ",";
      float m = 0;
      for (int j = 0; j < specStep; j++) {
        int idx = i * specStep + j;
        if (idx < FFT_SIZE/2 && fftMag[idx] > m) m = fftMag[idx];
      }
      json += String(m, 1);
    }
    json += "],";
  }
  json += "\"channels\":[";
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    if (ch) json += ",";
    json += "[";
    for (int i = 0; i < sendPoints; i++) {
      if (i) json += ",";
      json += String(sl.analog[ch][min(i * step, BUFFER_SIZE - 1)]);
    }
    json += "]";
  }
  json += "],\"digital\":[";
  for (int d = 0; d < NUM_DIGITAL; d++) {
    if (d) json += ",";
    json += "[";
    for (int i = 0; i < sendPoints; i++) {
      if (i) json += ",";
      json += String(sl.digital[d][min(i * step, BUFFER_SIZE - 1)]);
    }
    json += "]";
  }
  json += "]}";
  return json;
}

// JSON из данных Flash без полной загрузки в слот (чтение с downsample)
inline String fileToJson(uint8_t id) {
  FileEntry table[MAX_FILES];
  readIndex(table);
  if (id >= MAX_FILES || !(table[id].used && table[id].magic0 == 0xCA)) return "{}";

  uint32_t dataAddr = fileDataAddr(id);
  uint8_t hdr[16];
  flashRead(dataAddr, hdr, 16);
  if (hdr[0] != 'C') return "{}";

  uint32_t timeUs = hdr[4] | (hdr[5]<<8) | (hdr[6]<<16) | (hdr[7]<<24);
  uint16_t points = hdr[8] | (hdr[9]<<8);
  uint8_t nA = hdr[10], nD = hdr[11];
  if (points > BUFFER_SIZE) points = BUFFER_SIZE;
  if (nA > NUM_CHANNELS) nA = NUM_CHANNELS;
  if (nD > NUM_DIGITAL) nD = NUM_DIGITAL;

  const int sendPoints = 128;
  const int step = max(1, (int)points / sendPoints);
  float rate = timeUs ? (points * 1e6f) / timeUs : 0;

  String json = "{";
  json += "\"name\":\"" + String(table[id].name) + "\",";
  json += "\"points\":" + String(sendPoints) + ",";
  json += "\"time_us\":" + String(timeUs) + ",";
  json += "\"rate\":" + String((int)rate) + ",";
  json += "\"channels\":[";

  uint32_t addr = dataAddr + 16;
  // analog block: for each ch, points * 2 bytes
  for (int ch = 0; ch < nA; ch++) {
    if (ch) json += ",";
    json += "[";
    uint32_t chBase = addr + (uint32_t)ch * points * 2;
    for (int i = 0; i < sendPoints; i++) {
      if (i) json += ",";
      uint16_t idx = min(i * step, points - 1);
      uint8_t b[2];
      flashRead(chBase + idx * 2, b, 2);
      json += String((uint16_t)(b[0] | (b[1] << 8)));
    }
    json += "]";
  }
  json += "],\"digital\":[";
  uint32_t digBase = addr + (uint32_t)nA * points * 2;
  for (int d = 0; d < nD; d++) {
    if (d) json += ",";
    json += "[";
    uint32_t dBase = digBase + (uint32_t)d * points;
    for (int i = 0; i < sendPoints; i++) {
      if (i) json += ",";
      uint16_t idx = min(i * step, points - 1);
      uint8_t b;
      flashRead(dBase + idx, &b, 1);
      json += String(b ? 1 : 0);
    }
    json += "]";
  }
  json += "]}";
  return json;
}

inline void handleRoot() { server.send_P(200, "text/html", PAGE); }

inline void handleStatus() {
  String j = "{\"flash\":";
  j += flashReady ? "true" : "false";
  j += "}";
  server.send(200, "application/json", j);
}

inline void handleFiles() {
  FileEntry table[MAX_FILES];
  readIndex(table);
  int used = countUsedFiles(table);
  String j = "{\"used\":" + String(used) + ",\"max\":" + String(MAX_FILES) + ",\"files\":[";
  bool first = true;
  int order[MAX_FILES];
  int n = 0;
  for (int i = 0; i < MAX_FILES; i++)
    if (table[i].used && table[i].magic0 == 0xCA) order[n++] = i;
  for (int a = 0; a < n; a++)
    for (int b = a + 1; b < n; b++)
      if (table[order[b]].seq > table[order[a]].seq) {
        int t = order[a]; order[a] = order[b]; order[b] = t;
      }
  for (int k = 0; k < n; k++) {
    int i = order[k];
    if (!first) j += ",";
    first = false;
    j += "{\"id\":" + String(i);
    j += ",\"name\":\"" + String(table[i].name) + "\"";
    j += ",\"seq\":" + String(table[i].seq);
    j += ",\"time_us\":" + String(table[i].timeUs);
    j += "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

inline void handleSave() {
  String name = saveToFile();
  server.send(200, "text/plain", name.length() ? name : "ERR");
}

inline void handleDelete() {
  int id = server.hasArg("id") ? server.arg("id").toInt() : -1;
  bool ok = deleteFile(id);
  server.send(200, "text/plain", ok ? "OK" : "ERR");
}

inline void handleFile() {
  int id = server.hasArg("id") ? server.arg("id").toInt() : -1;
  server.send(200, "application/json", fileToJson(id));
}

inline void handleChannels() {
  if (server.hasArg("a")) {
    String e = server.arg("a"); int idx = 0;
    for (int i = 0; i < e.length() && idx < NUM_CHANNELS; i++) {
      if (e[i]=='0') channelEnabled[idx++]=false;
      else if (e[i]=='1') channelEnabled[idx++]=true;
    }
  }
  if (server.hasArg("d")) {
    String e = server.arg("d"); int idx = 0;
    for (int i = 0; i < e.length() && idx < NUM_DIGITAL; i++) {
      if (e[i]=='0') digitalEnabled[idx++]=false;
      else if (e[i]=='1') digitalEnabled[idx++]=true;
    }
  }
  server.send(200, "text/plain", "OK");
}

inline void handleTrigger() {
  if (server.hasArg("ch"))  triggerChannel = constrain(server.arg("ch").toInt(), 0, 13);
  if (server.hasArg("lvl")) triggerLevel   = constrain(server.arg("lvl").toInt(), 0, 4095);
  if (server.hasArg("edge")) triggerRising = server.arg("edge").toInt() != 0;
  server.send(200, "text/plain", "OK");
}

inline void handleWindow() {
  if (server.hasArg("ms")) windowMs = constrain(server.arg("ms").toInt(), 0, 2000);
  server.send(200, "text/plain", "OK");
}

inline void handleCal() {
  float off = server.hasArg("off") ? server.arg("off").toFloat() : 0;
  float sc  = server.hasArg("scale") ? server.arg("scale").toFloat() : (3.3f/4095);
  if (sc < 1e-7f) sc = 3.3f/4095;
  for (int i = 0; i < NUM_CHANNELS; i++) {
    calOffset[i] = off;
    calScale[i] = sc;
  }
  server.send(200, "text/plain", "Калибровка применена");
}

inline void handleCapture() {
  if (server.hasArg("fftch")) fftChannel = constrain(server.arg("fftch").toInt(), 0, 5);
  doCaptureWithTrigger();
  server.send(200, "application/json", slotJson(viewA, true));
}


inline void handleCsv() {
  int id = server.hasArg("id") ? server.arg("id").toInt() : -1;
  if (id < 0 || id >= MAX_FILES) {
    server.send(404, "text/plain", "not found");
    return;
  }
  FileEntry table[MAX_FILES];
  readIndex(table);
  if (!(table[id].used && table[id].magic0 == 0xCA)) {
    server.send(404, "text/plain", "not found");
    return;
  }

  uint32_t dataAddr = fileDataAddr(id);
  uint8_t hdr[16];
  flashRead(dataAddr, hdr, 16);
  if (hdr[0] != 'C') {
    server.send(500, "text/plain", "bad file");
    return;
  }
  uint16_t points = hdr[8] | (hdr[9] << 8);
  uint8_t nA = hdr[10], nD = hdr[11];
  if (points > BUFFER_SIZE) points = BUFFER_SIZE;
  if (nA > NUM_CHANNELS) nA = NUM_CHANNELS;
  if (nD > NUM_DIGITAL) nD = NUM_DIGITAL;
  uint32_t timeUs = hdr[4] | (hdr[5]<<8) | (hdr[6]<<16) | (hdr[7]<<24);

  // Разделитель ; — Excel (RU) не склеивает в один столбец
  // Также колонки V0..V5 в вольтах по калибровке
  String csv;
  csv.reserve((size_t)points * 60 + 300);
  csv += "t_ms";
  for (int ch = 0; ch < nA; ch++) { csv += ";A"; csv += ch; csv += "_raw"; }
  for (int ch = 0; ch < nA; ch++) { csv += ";A"; csv += ch; csv += "_V"; }
  for (int d = 0; d < nD; d++) { csv += ";D"; csv += d; }
  csv += "\r\n";

  uint32_t base = dataAddr + 16;
  float dt_ms = points > 1 ? (timeUs / 1000.0f) / (points - 1) : 0;

  for (uint16_t i = 0; i < points; i++) {
    csv += String(dt_ms * i, 3);
    // raw
    for (int ch = 0; ch < nA; ch++) {
      uint8_t b[2];
      flashRead(base + (uint32_t)ch * points * 2 + (uint32_t)i * 2, b, 2);
      uint16_t raw = b[0] | (b[1] << 8);
      csv += ";";
      csv += String(raw);
    }
    // volts
    for (int ch = 0; ch < nA; ch++) {
      uint8_t b[2];
      flashRead(base + (uint32_t)ch * points * 2 + (uint32_t)i * 2, b, 2);
      uint16_t raw = b[0] | (b[1] << 8);
      float v = rawToVolt(ch, raw);
      csv += ";";
      csv += String(v, 4);
    }
    uint32_t digBase = base + (uint32_t)nA * points * 2;
    for (int d = 0; d < nD; d++) {
      uint8_t b;
      flashRead(digBase + (uint32_t)d * points + i, &b, 1);
      csv += ";";
      csv += (b ? "1" : "0");
    }
    csv += "\r\n";
    if ((i & 31) == 0) delay(0);
  }

  String fname = String(table[id].name) + ".csv";
  server.sendHeader("Content-Disposition", "attachment; filename=" + fname);
  server.sendHeader("Content-Type", "text/csv; charset=utf-8");
  server.send(200, "text/csv", csv);
}

inline void setupWeb() {

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/capture", handleCapture);
  server.on("/channels", handleChannels);
  server.on("/trigger", handleTrigger);
  server.on("/window", handleWindow);
  server.on("/cal", handleCal);
  server.on("/files", handleFiles);
  server.on("/save", handleSave);
  server.on("/delete", handleDelete);
  server.on("/file", handleFile);
  server.on("/csv", handleCsv);
  server.begin();
}

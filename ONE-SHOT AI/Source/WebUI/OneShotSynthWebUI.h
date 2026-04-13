#pragma once

#include <string>

// HTML/CSS/JS for the Synth Editor tab.
// 88-parameter knob interface with collapsible sections, waveform display,
// and render/init/randomize controls. Professional dark UI matching plugin theme.
// Split into multiple string functions for MSVC 16380-char literal limit.

namespace OneShotSynthWebUI
{

static const char* getCSS()
{
    return R"synth_css(
<style>
/* === Synth Editor Tab === */
.synth-container { display: none; overflow-y: auto; max-height: calc(100vh - 100px); }
.synth-container.active { display: block; }
.synth-container::-webkit-scrollbar { width: 5px; }
.synth-container::-webkit-scrollbar-thumb { background: #333355; border-radius: 3px; }

.se-waveform-wrap {
    background: #1c1c30; border: 1px solid #333355; border-radius: 8px;
    padding: 8px; margin-bottom: 10px; position: relative;
}
.se-waveform-canvas { width: 100%; height: 64px; display: block; border-radius: 4px; }
.se-waveform-label {
    position: absolute; top: 10px; left: 14px;
    font-size: 9px; color: #8888aa; text-transform: uppercase; letter-spacing: 1.5px;
}

.se-toolbar {
    display: flex; gap: 6px; margin-bottom: 10px; flex-wrap: wrap;
}
.se-btn {
    flex: 1; min-width: 80px; padding: 10px 0; border-radius: 8px;
    font-size: 12px; font-weight: 700; letter-spacing: 1.5px;
    text-transform: uppercase; cursor: pointer; transition: all 0.15s;
    border: none; text-align: center;
}
.se-btn-render {
    background: #e94560; color: #fff; flex: 2;
    font-size: 14px; padding: 12px 0;
}
.se-btn-render:hover { background: #ff6b81; transform: translateY(-1px); }
.se-btn-render:active { transform: translateY(0); }
.se-btn-render.loading { position: relative; color: transparent; }
.se-btn-render.loading::after {
    content: ''; position: absolute; width: 18px; height: 18px;
    border: 2px solid transparent; border-top-color: #fff;
    border-radius: 50%; animation: spin 0.6s linear infinite;
    top: 50%; left: 50%; margin: -9px 0 0 -9px;
}
.se-btn-util {
    background: #252540; color: #8888aa; border: 1px solid #333355;
}
.se-btn-util:hover { background: #333355; color: #e8e8e8; }

.se-load-row {
    display: flex; gap: 6px; margin-bottom: 10px;
}
.se-btn-load {
    flex: 1; padding: 8px 0; border-radius: 6px;
    font-size: 10px; font-weight: 600; letter-spacing: 1px;
    text-transform: uppercase; cursor: pointer; transition: all 0.15s;
    background: #1c1c30; color: #4ecdc4; border: 1px solid rgba(78,205,196,0.3);
}
.se-btn-load:hover { background: rgba(78,205,196,0.1); border-color: #4ecdc4; }
.se-btn-load:disabled { opacity: 0.3; cursor: not-allowed; }

.se-section {
    background: #1c1c30; border: 1px solid #333355; border-radius: 8px;
    margin-bottom: 8px; overflow: hidden;
}
.se-section-header {
    display: flex; align-items: center; padding: 10px 12px;
    cursor: pointer; user-select: none; transition: background 0.15s;
}
.se-section-header:hover { background: #252540; }
.se-section-dot {
    width: 8px; height: 8px; border-radius: 50%; margin-right: 10px; flex-shrink: 0;
}
.se-section-name {
    font-size: 11px; font-weight: 700; letter-spacing: 1.5px;
    text-transform: uppercase; color: #e8e8e8; flex: 1;
}
.se-section-arrow {
    font-size: 10px; color: #8888aa; transition: transform 0.2s;
    flex-shrink: 0;
}
.se-section.collapsed .se-section-arrow { transform: rotate(-90deg); }
.se-section.collapsed .se-section-body { display: none; }
.se-section-body {
    padding: 6px 8px 10px; display: flex; flex-wrap: wrap;
    gap: 2px 0; justify-content: flex-start;
}

/* Knob */
.se-knob-wrap {
    width: 64px; display: flex; flex-direction: column;
    align-items: center; padding: 4px 2px;
}
.se-knob-canvas {
    width: 44px; height: 44px; cursor: grab; touch-action: none;
}
.se-knob-canvas:active { cursor: grabbing; }
.se-knob-val {
    font-size: 10px; color: #e8e8e8; margin-top: 2px;
    font-variant-numeric: tabular-nums; text-align: center;
    white-space: nowrap; max-width: 60px; overflow: hidden;
}
.se-knob-name {
    font-size: 8px; color: #8888aa; text-transform: uppercase;
    letter-spacing: 0.5px; text-align: center; line-height: 1.1;
    max-width: 60px; word-break: break-all;
}

/* Select param (oscType etc) */
.se-select-wrap {
    width: 128px; display: flex; flex-direction: column;
    padding: 4px 6px; gap: 3px;
}
.se-select-label {
    font-size: 8px; color: #8888aa; text-transform: uppercase;
    letter-spacing: 0.5px;
}
.se-select {
    background: #252540; color: #e8e8e8; border: 1px solid #333355;
    border-radius: 6px; padding: 6px 8px; font-size: 11px;
    cursor: pointer; outline: none; appearance: none; -webkit-appearance: none;
    background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='10' viewBox='0 0 10 10'%3E%3Cpath fill='%238888aa' d='M2 3l3 4 3-4'/%3E%3C/svg%3E");
    background-repeat: no-repeat; background-position: right 6px center;
}
.se-select:focus { border-color: #e94560; }

/* Player (reuses main styles but scoped) */
.se-player { display: none; align-items: center; gap: 10px; margin-bottom: 10px; }
.se-player.visible { display: flex; }

.se-status {
    text-align: center; font-size: 11px; color: #8888aa;
    margin-top: 4px; min-height: 16px;
}
.se-status.success { color: #4ecdc4; }
.se-status.error { color: #e94560; }


/* === Virtual MIDI Keyboard === */
.se-keyboard-wrap {
    background: #1c1c30; border: 1px solid #333355; border-radius: 8px;
    padding: 8px 8px 10px; margin-bottom: 10px;
}
.se-keyboard-header {
    display: flex; align-items: center; justify-content: space-between;
    margin-bottom: 6px;
}
.se-keyboard-title {
    font-size: 9px; color: #8888aa; text-transform: uppercase;
    letter-spacing: 1.5px;
}
.se-octave-controls {
    display: flex; align-items: center; gap: 6px;
}
.se-octave-btn {
    width: 24px; height: 24px; border-radius: 4px;
    background: #252540; color: #8888aa; border: 1px solid #333355;
    font-size: 14px; cursor: pointer; display: flex;
    align-items: center; justify-content: center; transition: all 0.15s;
    padding: 0; line-height: 1;
}
.se-octave-btn:hover { background: #333355; color: #e8e8e8; }
.se-octave-label {
    font-size: 11px; color: #e8e8e8; font-weight: 600;
    font-variant-numeric: tabular-nums; min-width: 24px; text-align: center;
}
.se-keyboard {
    position: relative; height: 72px; user-select: none;
    -webkit-user-select: none;
}
.se-key-white {
    position: absolute; bottom: 0; height: 72px;
    background: #2a2a44; border: 1px solid #333355;
    border-radius: 0 0 4px 4px; cursor: pointer;
    transition: background 0.08s;
}
.se-key-white:hover { background: #363658; }
.se-key-white.active { background: #e94560; border-color: #e94560; }
.se-key-white .se-key-label {
    position: absolute; bottom: 3px; left: 50%;
    transform: translateX(-50%); font-size: 7px;
    color: #555577; pointer-events: none;
}
.se-key-white.active .se-key-label { color: #fff; }
.se-key-black {
    position: absolute; top: 0; height: 44px; z-index: 2;
    background: #141420; border: 1px solid #1a1a2e;
    border-radius: 0 0 3px 3px; cursor: pointer;
    transition: background 0.08s;
}
.se-key-black:hover { background: #252540; }
.se-key-black.active { background: #c73050; border-color: #c73050; }
.se-keyboard-loading {
    position: absolute; top: 50%; left: 50%; transform: translate(-50%,-50%);
    font-size: 10px; color: #8888aa; pointer-events: none; display: none;
}
.se-keyboard-loading.visible { display: block; }
</style>
)synth_css";
}

static const char* getHTML()
{
    return R"synth_html(
<!-- Synth Editor container -->
<div class="synth-container" id="synthContainer">

<div class="se-waveform-wrap">
    <span class="se-waveform-label">Waveform</span>
    <canvas class="se-waveform-canvas" id="seWaveformCanvas"></canvas>
</div>

<div class="se-keyboard-wrap">
    <div class="se-keyboard-header">
        <span class="se-keyboard-title">Keyboard</span>
        <div class="se-octave-controls">
            <button class="se-octave-btn" id="seOctDown">&#8722;</button>
            <span class="se-octave-label" id="seOctLabel">C3</span>
            <button class="se-octave-btn" id="seOctUp">+</button>
        </div>
    </div>
    <div class="se-keyboard" id="seKeyboard">
        <span class="se-keyboard-loading" id="seKbLoading">rendering...</span>
    </div>
</div>

<div class="se-toolbar">
    <button class="se-btn se-btn-render" id="seRenderBtn">Render</button>
    <button class="se-btn se-btn-util" id="seInitBtn">Init</button>
    <button class="se-btn se-btn-util" id="seRandomBtn">Rnd</button>
</div>

<div class="se-load-row">
    <button class="se-btn-load" id="seLoadGenBtn" disabled>&#8592; From Generator</button>
    <button class="se-btn-load" id="seLoadMatchBtn" disabled>&#8592; From Match</button>
</div>

<div class="section player se-player" id="sePlayerSection">
    <button class="btn-play" id="sePlayBtn">&#9654;</button>
    <div class="player-info">
        <span class="player-label" id="sePlayerLabel">Synth Editor</span>
        <span class="player-duration" id="sePlayerDur">0.00s</span>
    </div>
    <button class="btn-download" id="seDownloadBtn">Download WAV</button>
</div>

<div id="seSections"></div>

<div class="se-status" id="seStatus"></div>

</div><!-- /synth-container -->
)synth_html";
}

static const char* getJS()
{
    return R"synth_js(
<script>
(function(){
'use strict';

// === Param definitions: [idx, name, min, max, default, decimals, unit, groupIdx] ===
var defined = [
// GLOBAL (0)
[0,'Osc Type',0,13,0,0,'',0,'s','Sine,Triangle,Saw,Square,Pulse 25%,Pulse 12%,HalfRect,AbsSine,Parabolic,Staircase,DblSine,ClipSine,Wavetable,Noise'],
[1,'Pitch',20,12000,50,1,'Hz',0],
[2,'Duration',0.01,5,0.5,3,'s',0],
[3,'Master Gain',0,1,0.9,2,'',0],
[4,'Stereo Width',0,1,0,2,'',0],
[5,'Pan',0,1,0.5,2,'',0],
// PITCH (1)
[6,'Env Depth',0,96,0,1,'st',1],
[7,'Env Fast',0.0003,0.05,0.008,4,'s',1],
[8,'Env Slow',0.003,0.5,0.06,3,'s',1],
[9,'Env Balance',0,1,0.65,2,'',1],
[10,'Hold Time',0,0.03,0,4,'s',1],
[11,'Bounce',0,1,0,2,'',1],
[12,'Wobble',0,1,0,2,'',1],
[13,'Wobble Rate',1,40,8,1,'Hz',1],
// TONAL (2)
[14,'Level',0,1,0.8,2,'',2],
[15,'Harmonics',0,1,0,2,'',2],
[16,'FM Depth',0,1,0,2,'',2],
[17,'FM Ratio',0.5,12,2,2,'',2],
[18,'FM Decay',0.003,0.3,0.05,3,'s',2],
[19,'Additive',0,1,0,2,'',2],
[20,'Harm 2',0,1,0,2,'',2],
[21,'Harm 3',0,1,0,2,'',2],
[22,'Harm 4',0,1,0,2,'',2],
[23,'Harm 5',0,1,0,2,'',2],
[24,'Inharm',0,0.1,0,3,'',2],
[25,'Unison',0,1,0,2,'',2],
[26,'Uni Detune',0.1,80,10,1,'ct',2],
[27,'Uni Drift',0,1,0,2,'',2],
[28,'Phase Dist',0,1,0,2,'',2],
[29,'PD Decay',0.003,0.2,0.05,3,'s',2],
[30,'Sub Level',0,1,0,2,'',2],
[31,'Sub Detune',-12,12,0,1,'st',2],
// NOISE (3)
[32,'Level',0,1,0,2,'',3],
[33,'Color',0,1,0.5,2,'',3],
[34,'Filter Freq',100,16000,4000,0,'Hz',3],
[35,'Filter Q',0,1,0.3,2,'',3],
[36,'Decay',0.005,2,0.2,3,'s',3],
[37,'Burst Count',1,8,1,0,'',3],
[38,'Burst Space',0.003,0.03,0.01,3,'s',3],
[39,'Attack',0,0.02,0,4,'s',3],
[40,'Residual',0,1,0,2,'',3],
[41,'Res Level',0,1,0.5,2,'',3],
[42,'HP Freq',20,2000,20,0,'Hz',3],
[43,'Stereo',0,1,0,2,'',3],
[44,'Evolution',0,1,0,2,'',3],
[45,'Granular',0,1,0,2,'',3],
// MODAL (4)
[46,'Level',0,1,0,2,'',4],
[47,'Mode',0,1,0,2,'',4],
[48,'Num Modes',1,12,6,0,'',4],
[49,'Decay',0.01,2,0.3,2,'s',4],
[50,'Spread',0,1,0,2,'',4],
[51,'Ratio Base',1,4,1,2,'',4],
[52,'Damping',0,1,0.3,2,'',4],
[53,'KS Feedbk',0.9,0.999,0.995,3,'',4],
[54,'KS Damp',0,1,0.5,2,'',4],
[55,'KS Bright',0,1,0.5,2,'',4],
[56,'KS Pick',0,1,0.5,2,'',4],
[57,'KS Body',0,1,0,2,'',4],
// TRANSIENT (5)
[58,'Level',0,1,0,2,'',5],
[59,'Click Type',0,3,0,0,'',5,'s','Noise,Impulse,FM Burst,Chirp'],
[60,'Click Freq',500,14000,4000,0,'Hz',5],
[61,'Click Decay',0.0001,0.015,0.002,4,'s',5],
[62,'Click Width',0,1,0.3,2,'',5],
[63,'Snap',0,1,0,2,'',5],
[64,'Transient',0,1,0,2,'',5],
[65,'Top Noise',0,1,0,2,'',5],
// ENVELOPE (6)
[66,'Attack',0.0001,0.05,0.001,4,'s',6],
[67,'Punch Decay',0.005,0.5,0.1,3,'s',6],
[68,'Body Decay',0.02,3,0.4,2,'s',6],
[69,'Punch Level',0,1,0.55,2,'',6],
[70,'Sustain Lvl',0,1,0,2,'',6],
[71,'Sustain Time',0,3,0,2,'s',6],
[72,'Release',0.005,5,0.1,3,'s',6],
[73,'Curve',0.1,4,1,2,'',6],
// FILTER (7)
[74,'Cutoff',100,20000,20000,0,'Hz',7],
[75,'Resonance',0,0.95,0,2,'',7],
[76,'Sweep Amt',0,1,0,2,'',7],
[77,'Sweep Start',200,18000,8000,0,'Hz',7],
[78,'Sweep End',50,5000,500,0,'Hz',7],
[79,'Formant',0,1,0,2,'',7],
[80,'Form Freq1',200,3500,700,0,'Hz',7],
[81,'Form Freq2',400,5000,1500,0,'Hz',7],
// FX (8)
[82,'Reverb',0,1,0,2,'',8],
[83,'Rev Decay',0.03,2,0.3,2,'s',8],
[84,'Chorus',0,1,0,2,'',8],
[85,'Saturation',0,1,0,2,'',8],
[86,'Sat Type',0,2,0,0,'',8,'s','Soft Clip,Tape,Tube'],
[87,'Compress',0,1,0,2,'',8]
];

var GROUPS = [
    { name:'GLOBAL',     color:'#e94560' },
    { name:'PITCH',      color:'#ff8c42' },
    { name:'TONAL',      color:'#4ecdc4' },
    { name:'NOISE',      color:'#9b59b6' },
    { name:'MODAL',      color:'#3498db' },
    { name:'TRANSIENT',  color:'#f39c12' },
    { name:'ENVELOPE',   color:'#2ecc71' },
    { name:'FILTER',     color:'#e91e8a' },
    { name:'FX',         color:'#95a5a6' }
];

// Current values (88 floats)
var vals = new Float32Array(88);
var defaults = new Float32Array(88);
var knobCanvases = [];
var knobData = []; // per-param metadata

// Init defaults
for (var i = 0; i < defined.length; i++) {
    var d = defined[i];
    defaults[d[0]] = d[4];
    vals[d[0]] = d[4];
}

// Build the UI
var sectionsDiv = document.getElementById('seSections');

GROUPS.forEach(function(g, gi) {
    var sec = document.createElement('div');
    sec.className = 'se-section';
    sec.setAttribute('data-group', gi);

    // Header
    var hdr = document.createElement('div');
    hdr.className = 'se-section-header';
    hdr.innerHTML = '<div class="se-section-dot" style="background:'+g.color+'"></div>'
        + '<span class="se-section-name">'+g.name+'</span>'
        + '<span class="se-section-arrow">&#9660;</span>';
    hdr.addEventListener('click', function(){ sec.classList.toggle('collapsed'); });
    sec.appendChild(hdr);

    // Body
    var body = document.createElement('div');
    body.className = 'se-section-body';

    // Add params for this group
    for (var i = 0; i < defined.length; i++) {
        var p = defined[i];
        if (p[7] !== gi) continue;

        var isSelect = p.length > 8 && p[8] === 's';

        if (isSelect) {
            var wrap = document.createElement('div');
            wrap.className = 'se-select-wrap';
            var lbl = document.createElement('div');
            lbl.className = 'se-select-label';
            lbl.textContent = p[1];
            var sel = document.createElement('select');
            sel.className = 'se-select';
            sel.setAttribute('data-idx', p[0]);
            var opts = p[9].split(',');
            for (var o = 0; o < opts.length; o++) {
                var opt = document.createElement('option');
                opt.value = o;
                opt.textContent = o + ': ' + opts[o];
                if (o === Math.round(p[4])) opt.selected = true;
                sel.appendChild(opt);
            }
            (function(idx) {
                sel.addEventListener('change', function(){
                    vals[idx] = parseInt(this.value);
                });
            })(p[0]);
            wrap.appendChild(lbl);
            wrap.appendChild(sel);
            body.appendChild(wrap);
            knobData[p[0]] = { el: sel, type: 'select', def: p };
        } else {
            var wrap = document.createElement('div');
            wrap.className = 'se-knob-wrap';
            var cvs = document.createElement('canvas');
            cvs.className = 'se-knob-canvas';
            cvs.width = 88; cvs.height = 88;
            cvs.setAttribute('data-idx', p[0]);
            var valSpan = document.createElement('div');
            valSpan.className = 'se-knob-val';
            valSpan.textContent = formatVal(p[0]);
            var nameSpan = document.createElement('div');
            nameSpan.className = 'se-knob-name';
            nameSpan.textContent = p[1];
            wrap.appendChild(cvs);
            wrap.appendChild(valSpan);
            wrap.appendChild(nameSpan);
            body.appendChild(wrap);
            knobCanvases.push(cvs);
            knobData[p[0]] = { canvas: cvs, valEl: valSpan, type: 'knob', def: p, color: g.color };
            drawKnob(cvs, p[0], g.color);
        }
    }

    sec.appendChild(body);
    // All sections collapsed by default
    sec.classList.add('collapsed');
    sectionsDiv.appendChild(sec);
});

function formatVal(idx) {
    var p = findDef(idx);
    if (!p) return vals[idx].toString();
    var v = vals[idx];
    var dec = p[5];
    var u = p[6];
    if (dec === 0) return Math.round(v) + (u ? ' '+u : '');
    return v.toFixed(dec) + (u ? ' '+u : '');
}

function findDef(idx) {
    for (var i = 0; i < defined.length; i++) {
        if (defined[i][0] === idx) return defined[i];
    }
    return null;
}

function drawKnob(canvas, idx, color) {
    var ctx = canvas.getContext('2d');
    var w = canvas.width, h = canvas.height;
    var cx = w/2, cy = h/2, r = 34;
    var p = findDef(idx);
    if (!p) return;

    var norm = (vals[idx] - p[2]) / (p[3] - p[2]);
    norm = Math.max(0, Math.min(1, norm));

    var startAngle = 0.75 * Math.PI;  // 225 deg (7 o'clock)
    var endAngle = 2.25 * Math.PI;    // 405 deg (5 o'clock)
    var sweep = endAngle - startAngle; // 270 deg
    var valAngle = startAngle + norm * sweep;

    ctx.clearRect(0, 0, w, h);

    // Track ring
    ctx.beginPath();
    ctx.arc(cx, cy, r, startAngle, endAngle);
    ctx.strokeStyle = 'rgba(51,51,85,0.6)';
    ctx.lineWidth = 3;
    ctx.lineCap = 'round';
    ctx.stroke();

    // Value arc
    if (norm > 0.003) {
        ctx.beginPath();
        ctx.arc(cx, cy, r, startAngle, valAngle);
        ctx.strokeStyle = color;
        ctx.lineWidth = 3;
        ctx.lineCap = 'round';
        ctx.stroke();
    }

    // Indicator dot
    var dotX = cx + r * Math.cos(valAngle);
    var dotY = cy + r * Math.sin(valAngle);
    ctx.beginPath();
    ctx.arc(dotX, dotY, 4, 0, Math.PI*2);
    ctx.fillStyle = '#fff';
    ctx.fill();
    ctx.beginPath();
    ctx.arc(dotX, dotY, 2.5, 0, Math.PI*2);
    ctx.fillStyle = color;
    ctx.fill();

    // Center dot
    ctx.beginPath();
    ctx.arc(cx, cy, 6, 0, Math.PI*2);
    ctx.fillStyle = '#252540';
    ctx.fill();
    ctx.beginPath();
    ctx.arc(cx, cy, 5, 0, Math.PI*2);
    ctx.fillStyle = '#1a1a2e';
    ctx.fill();
}

function updateKnob(idx) {
    var kd = knobData[idx];
    if (!kd) return;
    if (kd.type === 'knob') {
        drawKnob(kd.canvas, idx, kd.color);
        kd.valEl.textContent = formatVal(idx);
    } else if (kd.type === 'select') {
        kd.el.value = Math.round(vals[idx]);
    }
}

function updateAllKnobs() {
    for (var i = 0; i < 88; i++) updateKnob(i);
}

// Expose globally for cross-tab communication
window._synthVals = vals;
window._synthDefined = defined;
window._synthDefaults = defaults;
window._synthUpdateAll = updateAllKnobs;
window._synthSetParams = function(arr) {
    for (var i = 0; i < 88 && i < arr.length; i++) vals[i] = arr[i];
    updateAllKnobs();
};

// === Knob drag interaction ===
var dragging = null; // { idx, startY, startVal }

document.addEventListener('mousedown', function(e) {
    var cvs = e.target.closest('.se-knob-canvas');
    if (!cvs) return;
    var idx = parseInt(cvs.getAttribute('data-idx'));
    dragging = { idx: idx, startY: e.clientY, startVal: vals[idx] };
    e.preventDefault();
});

document.addEventListener('mousemove', function(e) {
    if (!dragging) return;
    var p = findDef(dragging.idx);
    if (!p) return;
    var dy = dragging.startY - e.clientY;
    var range = p[3] - p[2];
    var sensitivity = range / 150;
    if (e.shiftKey) sensitivity *= 0.1; // Fine mode
    var newVal = dragging.startVal + dy * sensitivity;
    // Snap integers
    if (p[5] === 0) newVal = Math.round(newVal);
    vals[dragging.idx] = Math.max(p[2], Math.min(p[3], newVal));
    updateKnob(dragging.idx);
});

document.addEventListener('mouseup', function() { dragging = null; });

// Double-click to reset
document.addEventListener('dblclick', function(e) {
    var cvs = e.target.closest('.se-knob-canvas');
    if (!cvs) return;
    var idx = parseInt(cvs.getAttribute('data-idx'));
    vals[idx] = defaults[idx];
    updateKnob(idx);
});

// Scroll wheel on knobs
document.addEventListener('wheel', function(e) {
    var cvs = e.target.closest('.se-knob-canvas');
    if (!cvs) return;
    e.preventDefault();
    var idx = parseInt(cvs.getAttribute('data-idx'));
    var p = findDef(idx);
    if (!p) return;
    var range = p[3] - p[2];
    var step = range / 100;
    if (e.shiftKey) step *= 0.1;
    var dir = e.deltaY < 0 ? 1 : -1;
    vals[idx] = Math.max(p[2], Math.min(p[3], vals[idx] + dir * step));
    if (p[5] === 0) vals[idx] = Math.round(vals[idx]);
    updateKnob(idx);
}, { passive: false });

})();
</script>
)synth_js";
}

static const char* getJS2()
{
    return R"synth_js2(
<script>
(function(){
'use strict';

var seAudioCtx = null, seDecoded = null, seSrc = null, seWavData = null;
var sePlaying = false, seRendering = false;
var $ = function(id){ return document.getElementById(id); };

var renderBtn = $('seRenderBtn'), initBtn = $('seInitBtn'), randomBtn = $('seRandomBtn');
var loadGenBtn = $('seLoadGenBtn'), loadMatchBtn = $('seLoadMatchBtn');
var playBtn = $('sePlayBtn'), dlBtn = $('seDownloadBtn');
var playerSec = $('sePlayerSection'), playerDur = $('sePlayerDur');
var statusEl = $('seStatus');
var wfCanvas = $('seWaveformCanvas');

function seStatus(msg, type) {
    statusEl.textContent = msg;
    statusEl.className = 'se-status' + (type ? ' '+type : '');
    if (type === 'success') setTimeout(function(){
        if (statusEl.textContent === msg) statusEl.textContent = '';
    }, 4000);
}

function seStop() {
    if (seSrc) { try{seSrc.stop();}catch(e){} seSrc.disconnect(); seSrc=null; }
    sePlaying = false; playBtn.innerHTML = '&#9654;';
}

function sePlay() {
    if (!seDecoded || !seAudioCtx) return;
    seStop();
    seSrc = seAudioCtx.createBufferSource();
    seSrc.buffer = seDecoded;
    seSrc.connect(seAudioCtx.destination);
    seSrc.onended = function(){ sePlaying=false; playBtn.innerHTML='&#9654;'; seSrc=null; };
    seSrc.start(); sePlaying = true; playBtn.innerHTML = '&#9632;';
}

function drawWaveform(buffer) {
    var canvas = wfCanvas;
    var dpr = window.devicePixelRatio || 1;
    var rect = canvas.getBoundingClientRect();
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;
    var ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);
    var w = rect.width, h = rect.height;

    ctx.clearRect(0, 0, w, h);

    if (!buffer) {
        ctx.strokeStyle = 'rgba(51,51,85,0.4)';
        ctx.beginPath(); ctx.moveTo(0, h/2); ctx.lineTo(w, h/2); ctx.stroke();
        return;
    }

    var data = buffer.getChannelData(0);
    var step = Math.ceil(data.length / w);
    var mid = h / 2;

    // Draw filled waveform
    ctx.beginPath();
    ctx.moveTo(0, mid);
    for (var x = 0; x < w; x++) {
        var idx = Math.floor(x * data.length / w);
        var end = Math.min(idx + step, data.length);
        var max = 0;
        for (var j = idx; j < end; j++) {
            var abs = Math.abs(data[j]);
            if (abs > max) max = abs;
        }
        ctx.lineTo(x, mid - max * mid * 0.9);
    }
    for (var x = w - 1; x >= 0; x--) {
        var idx = Math.floor(x * data.length / w);
        var end = Math.min(idx + step, data.length);
        var max = 0;
        for (var j = idx; j < end; j++) {
            var abs = Math.abs(data[j]);
            if (abs > max) max = abs;
        }
        ctx.lineTo(x, mid + max * mid * 0.9);
    }
    ctx.closePath();
    ctx.fillStyle = 'rgba(233,69,96,0.25)';
    ctx.fill();

    // Waveform line
    ctx.beginPath();
    for (var x = 0; x < w; x++) {
        var idx = Math.floor(x * data.length / w);
        var end = Math.min(idx + step, data.length);
        var max = 0, min = 0;
        for (var j = idx; j < end; j++) {
            if (data[j] > max) max = data[j];
            if (data[j] < min) min = data[j];
        }
        if (x === 0) ctx.moveTo(x, mid - max * mid * 0.9);
        else ctx.lineTo(x, mid - max * mid * 0.9);
    }
    ctx.strokeStyle = '#e94560';
    ctx.lineWidth = 1;
    ctx.stroke();

    // Center line
    ctx.strokeStyle = 'rgba(51,51,85,0.3)';
    ctx.setLineDash([4,4]);
    ctx.beginPath(); ctx.moveTo(0, mid); ctx.lineTo(w, mid); ctx.stroke();
    ctx.setLineDash([]);
}

// Init waveform
drawWaveform(null);

// === RENDER ===
async function seRender() {
    if (seRendering) return;
    seRendering = true;
    renderBtn.classList.add('loading');
    seStatus('Rendering...');
    seStop();
    try {
        if (!seAudioCtx) seAudioCtx = new (window.AudioContext||window.webkitAudioContext)();
        var qs = '';
        for (var i = 0; i < 88; i++) {
            qs += (i > 0 ? '&' : '') + 'p'+i+'='+window._synthVals[i];
        }
        qs += '&seed=0';
        qs += '&t=' + Date.now();
        var r = await fetch('/api/synth/render?' + qs);
        if (!r.ok) throw new Error('Failed: '+r.status);
        var ab = await r.arrayBuffer();
        seWavData = ab;
        seDecoded = await seAudioCtx.decodeAudioData(ab.slice(0));
        playerDur.textContent = seDecoded.duration.toFixed(2)+'s';
        playerSec.classList.add('visible');
        drawWaveform(seDecoded);
        seStatus('Rendered ' + seDecoded.duration.toFixed(2) + 's', 'success');
        sePlay();
    } catch(e) { seStatus('Error: '+e.message, 'error'); }
    finally {
        seRendering = false;
        renderBtn.classList.remove('loading');
    }
}

renderBtn.addEventListener('click', seRender);

// === INIT ===
initBtn.addEventListener('click', function() {
    for (var i = 0; i < 88; i++) window._synthVals[i] = window._synthDefaults[i];
    window._synthUpdateAll();
    seStatus('Initialized', 'success');
});

// === RANDOMIZE ===
randomBtn.addEventListener('click', function() {
    for (var i = 0; i < 88; i++) {
        var p = null;
        for (var j = 0; j < window._synthDefined.length; j++) {
            if (window._synthDefined[j][0] === i) { p = window._synthDefined[j]; break; }
        }
        if (p) {
            var v = p[2] + Math.random() * (p[3] - p[2]);
            if (p[5] === 0) v = Math.round(v);
            window._synthVals[i] = v;
        }
    }
    // Keep some sensible ranges
    window._synthVals[3] = 0.7 + Math.random()*0.3; // gain 0.7-1
    window._synthVals[5] = 0.3 + Math.random()*0.4; // pan near center
    window._synthVals[73] = 0.5 + Math.random()*2;   // curve
    window._synthUpdateAll();
    seStatus('Randomized', 'success');
});

// === LOAD FROM GENERATOR ===
loadGenBtn.addEventListener('click', async function() {
    try {
        var r = await fetch('/api/synth/last-params?t='+Date.now());
        if (!r.ok) throw new Error('No params');
        var arr = await r.json();
        if (arr && arr.length === 88) {
            window._synthSetParams(arr);
            seStatus('Loaded from Generator', 'success');
        }
    } catch(e) { seStatus('No generated params yet', 'error'); }
});

// === LOAD FROM MATCH ===
loadMatchBtn.addEventListener('click', async function() {
    try {
        var r = await fetch('/api/synth/last-params?source=match&t='+Date.now());
        if (!r.ok) throw new Error('No params');
        var arr = await r.json();
        if (arr && arr.length === 88) {
            window._synthSetParams(arr);
            seStatus('Loaded from Match', 'success');
        }
    } catch(e) { seStatus('No matched params yet', 'error'); }
});

// === PLAYER ===
playBtn.addEventListener('click', function(){ if(sePlaying) seStop(); else sePlay(); });
dlBtn.addEventListener('click', function() {
    if (!seWavData) return;
    var n = 'SynthEditor.wav';
    var b = new Blob([seWavData],{type:'audio/wav'});
    var u = URL.createObjectURL(b);
    var a = document.createElement('a');
    a.href=u; a.download=n; document.body.appendChild(a); a.click();
    document.body.removeChild(a); URL.revokeObjectURL(u);
    seStatus('Downloaded: '+n, 'success');
});

// Expose defined for randomize
window._synthDefined = window._synthDefined || [];
// Populate from the IIFE in getJS — we need a bridge
// Actually defined is in the other IIFE scope. Let's re-expose it.

})();
</script>
)synth_js2";
}

static const char* getJS3()
{
    return R"synth_js3(
<script>
(function(){
    // Enable load buttons
    var genBtn = document.getElementById('seLoadGenBtn');
    var matchBtn = document.getElementById('seLoadMatchBtn');
    if (genBtn) genBtn.disabled = false;
    if (matchBtn) matchBtn.disabled = false;

    // === Virtual MIDI Keyboard ===
    var kbDiv = document.getElementById('seKeyboard');
    var octLabel = document.getElementById('seOctLabel');
    var kbLoading = document.getElementById('seKbLoading');
    var baseOctave = 3; // C3
    var kbAudioCtx = null;
    var kbRendering = false;

    // Note layout: 2 octaves of white + black keys
    // White key indices in chromatic: 0,2,4,5,7,9,11 (C,D,E,F,G,A,B)
    var WHITE = [0,2,4,5,7,9,11];
    var BLACK = [1,3,null,6,8,10,null]; // null = no black key after E,B
    var NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
    var NUM_OCTAVES = 2;

    function buildKeyboard() {
        // Clear existing keys (keep loading span)
        var existing = kbDiv.querySelectorAll('.se-key-white,.se-key-black');
        existing.forEach(function(k){ k.remove(); });

        var totalWhites = NUM_OCTAVES * 7 + 1; // +1 for final C
        var kbWidth = kbDiv.offsetWidth || 440;
        var whiteW = Math.floor(kbWidth / totalWhites);
        var blackW = Math.floor(whiteW * 0.58);

        var whiteIdx = 0;
        for (var oct = 0; oct < NUM_OCTAVES; oct++) {
            for (var w = 0; w < 7; w++) {
                var note = WHITE[w] + oct * 12;
                var midi = (baseOctave + oct) * 12 + WHITE[w] + 12; // MIDI note
                var freq = 440.0 * Math.pow(2, (midi - 69) / 12.0);

                // White key
                var wk = document.createElement('div');
                wk.className = 'se-key-white';
                wk.style.left = (whiteIdx * whiteW) + 'px';
                wk.style.width = (whiteW - 1) + 'px';
                wk.setAttribute('data-freq', freq.toFixed(2));
                wk.setAttribute('data-note', NOTE_NAMES[WHITE[w]] + (baseOctave + oct));

                var lbl = document.createElement('span');
                lbl.className = 'se-key-label';
                if (WHITE[w] === 0) lbl.textContent = 'C' + (baseOctave + oct);
                wk.appendChild(lbl);
                kbDiv.appendChild(wk);

                // Black key (if applicable)
                if (BLACK[w] !== null) {
                    var bk = document.createElement('div');
                    bk.className = 'se-key-black';
                    bk.style.left = (whiteIdx * whiteW + whiteW - blackW/2) + 'px';
                    bk.style.width = blackW + 'px';
                    var bmidi = (baseOctave + oct) * 12 + BLACK[w] + 12;
                    var bfreq = 440.0 * Math.pow(2, (bmidi - 69) / 12.0);
                    bk.setAttribute('data-freq', bfreq.toFixed(2));
                    bk.setAttribute('data-note', NOTE_NAMES[BLACK[w]] + (baseOctave + oct));
                    kbDiv.appendChild(bk);
                }

                whiteIdx++;
            }
        }
        // Final C (top octave)
        var finalMidi = (baseOctave + NUM_OCTAVES) * 12 + 12;
        var finalFreq = 440.0 * Math.pow(2, (finalMidi - 69) / 12.0);
        var fk = document.createElement('div');
        fk.className = 'se-key-white';
        fk.style.left = (whiteIdx * whiteW) + 'px';
        fk.style.width = (whiteW - 1) + 'px';
        fk.setAttribute('data-freq', finalFreq.toFixed(2));
        var flbl = document.createElement('span');
        flbl.className = 'se-key-label';
        flbl.textContent = 'C' + (baseOctave + NUM_OCTAVES);
        fk.appendChild(flbl);
        kbDiv.appendChild(fk);

        octLabel.textContent = 'C' + baseOctave;
    }

    // Octave controls
    document.getElementById('seOctDown').addEventListener('click', function(){
        if (baseOctave > 0) { baseOctave--; buildKeyboard(); }
    });
    document.getElementById('seOctUp').addEventListener('click', function(){
        if (baseOctave < 7) { baseOctave++; buildKeyboard(); }
    });

    // Key press handler
    async function playNote(freq, keyEl) {
        if (kbRendering) return;
        kbRendering = true;
        kbLoading.classList.add('visible');

        // Highlight key
        kbDiv.querySelectorAll('.active').forEach(function(k){ k.classList.remove('active'); });
        keyEl.classList.add('active');

        try {
            if (!kbAudioCtx) kbAudioCtx = new (window.AudioContext||window.webkitAudioContext)();

            // Build query with current params but override basePitch
            var qs = '';
            for (var i = 0; i < 88; i++) {
                var v = window._synthVals[i];
                if (i === 1) v = freq; // override basePitch
                qs += (i > 0 ? '&' : '') + 'p'+i+'='+v;
            }
            qs += '&seed=0';
            qs += '&t=' + Date.now();

            var r = await fetch('/api/synth/render?' + qs);
            if (!r.ok) throw new Error('Render failed');
            var ab = await r.arrayBuffer();
            var decoded = await kbAudioCtx.decodeAudioData(ab.slice(0));

            var src = kbAudioCtx.createBufferSource();
            src.buffer = decoded;
            src.connect(kbAudioCtx.destination);
            src.onended = function(){ keyEl.classList.remove('active'); };
            src.start();
        } catch(e) {
            keyEl.classList.remove('active');
        }
        finally {
            kbRendering = false;
            kbLoading.classList.remove('visible');
        }
    }

    // Event delegation for key clicks
    kbDiv.addEventListener('mousedown', function(e) {
        var key = e.target.closest('.se-key-white,.se-key-black');
        if (!key) return;
        e.preventDefault();
        var freq = parseFloat(key.getAttribute('data-freq'));
        if (freq > 0) playNote(freq, key);
    });

    // Build on load
    buildKeyboard();

    // Rebuild on resize
    var resizeTimer;
    window.addEventListener('resize', function(){
        clearTimeout(resizeTimer);
        resizeTimer = setTimeout(buildKeyboard, 200);
    });
})();
</script>
)synth_js3";
}

} // namespace OneShotSynthWebUI

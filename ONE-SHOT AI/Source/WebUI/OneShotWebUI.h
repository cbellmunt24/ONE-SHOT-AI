#pragma once

#include <string>
#include "OneShotMatchWebUI.h"

// HTML/CSS/JS completo para la WebUI del ONE-SHOT AI.
// Servido inline desde getResource() — no requiere archivos externos ni binary resources.
// Dividido en partes para cumplir el limite de 16380 chars por string literal de MSVC.
// Incluye dos modos: Generator (existente) y Kick Match (nuevo).

namespace OneShotWebUI
{

static const char* cssPart1()
{
    return R"css1(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ONE-SHOT AI</title>
<style>
:root {
    --bg: #141420;
    --surface: #1c1c30;
    --surface2: #252540;
    --accent: #e94560;
    --accent-hover: #ff6b81;
    --text: #e8e8e8;
    --text2: #8888aa;
    --border: #333355;
    --success: #4ecdc4;
    --radius: 8px;
}
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, sans-serif;
    background: var(--bg); color: var(--text);
    padding: 16px; min-height: 100vh;
    user-select: none; -webkit-user-select: none;
}
h1 {
    font-size: 18px; font-weight: 700; letter-spacing: 2px;
    text-transform: uppercase; color: var(--accent);
    margin-bottom: 16px; text-align: center;
}
.section {
    background: var(--surface); border: 1px solid var(--border);
    border-radius: var(--radius); padding: 14px; margin-bottom: 12px;
}
.section-title {
    font-size: 11px; font-weight: 600; text-transform: uppercase;
    letter-spacing: 1.5px; color: var(--text2); margin-bottom: 10px;
}
.row { display: flex; gap: 12px; margin-bottom: 10px; }
.row:last-child { margin-bottom: 0; }
.select-group { flex: 1; display: flex; flex-direction: column; gap: 4px; }
.select-group label {
    font-size: 11px; color: var(--text2);
    text-transform: uppercase; letter-spacing: 1px;
}
select {
    background: var(--surface2); color: var(--text);
    border: 1px solid var(--border); border-radius: 6px;
    padding: 8px 10px; font-size: 13px; cursor: pointer;
    outline: none; appearance: none; -webkit-appearance: none;
    background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%238888aa' d='M2 4l4 4 4-4'/%3E%3C/svg%3E");
    background-repeat: no-repeat; background-position: right 8px center;
}
select:focus { border-color: var(--accent); }
.btn-group { flex: 1; display: flex; flex-direction: column; gap: 4px; }
.btn-group label {
    font-size: 11px; color: var(--text2);
    text-transform: uppercase; letter-spacing: 1px;
}
.btn-group .buttons { display: flex; gap: 0; }
.btn-group .buttons button {
    flex: 1; background: var(--surface2); color: var(--text2);
    border: 1px solid var(--border); padding: 6px 0;
    font-size: 12px; cursor: pointer; transition: all 0.15s;
}
.btn-group .buttons button:first-child { border-radius: 6px 0 0 6px; }
.btn-group .buttons button:last-child { border-radius: 0 6px 6px 0; }
.btn-group .buttons button:not(:first-child) { border-left: none; }
.btn-group .buttons button.active {
    background: var(--accent); color: #fff; border-color: var(--accent);
}
.btn-group .buttons button.active + button { border-left-color: var(--accent); }
.btn-group .buttons button:hover:not(.active) {
    background: var(--border); color: var(--text);
}
)css1";
}

static const char* cssPart2()
{
    return R"css2(
.slider-row { display: flex; align-items: center; gap: 8px; margin-bottom: 8px; }
.slider-row:last-child { margin-bottom: 0; }
.slider-row .lock {
    width: 16px; height: 16px; accent-color: var(--accent);
    cursor: pointer; flex-shrink: 0;
}
.slider-row .param-name {
    font-size: 12px; color: var(--text2); width: 80px; flex-shrink: 0;
}
.slider-row input[type="range"] {
    flex: 1; height: 4px; -webkit-appearance: none; appearance: none;
    background: var(--border); border-radius: 2px; outline: none; cursor: pointer;
}
.slider-row input[type="range"]::-webkit-slider-thumb {
    -webkit-appearance: none; appearance: none;
    width: 16px; height: 16px; border-radius: 50%;
    background: var(--accent); cursor: pointer;
    border: 2px solid var(--bg); transition: transform 0.1s;
}
.slider-row input[type="range"]::-webkit-slider-thumb:hover { transform: scale(1.2); }
.slider-row .value {
    font-size: 12px; color: var(--text); width: 32px;
    text-align: right; font-variant-numeric: tabular-nums; flex-shrink: 0;
}
.seed-row { display: flex; align-items: center; gap: 8px; }
.seed-row label { font-size: 12px; color: var(--text2); width: 80px; flex-shrink: 0; }
.seed-row input[type="number"] {
    flex: 1; background: var(--surface2); color: var(--text);
    border: 1px solid var(--border); border-radius: 6px;
    padding: 6px 10px; font-size: 13px; outline: none;
    font-variant-numeric: tabular-nums;
}
.seed-row input[type="number"]:focus { border-color: var(--accent); }
.seed-row .btn-small {
    background: var(--surface2); color: var(--text2);
    border: 1px solid var(--border); border-radius: 6px;
    padding: 6px 10px; font-size: 12px; cursor: pointer; white-space: nowrap;
}
.seed-row .btn-small:hover { background: var(--border); color: var(--text); }
.seed-row .lock { width: 16px; height: 16px; accent-color: var(--accent); cursor: pointer; }
.actions { display: flex; gap: 10px; margin-bottom: 12px; }
.btn-primary {
    flex: 2; background: var(--accent); color: #fff; border: none;
    border-radius: var(--radius); padding: 14px 0; font-size: 15px;
    font-weight: 700; letter-spacing: 2px; text-transform: uppercase;
    cursor: pointer; transition: all 0.15s;
}
.btn-primary:hover { background: var(--accent-hover); transform: translateY(-1px); }
.btn-primary:active { transform: translateY(0); }
.btn-primary:disabled { opacity: 0.5; cursor: not-allowed; transform: none; }
.btn-secondary {
    flex: 1; background: var(--surface2); color: var(--text);
    border: 1px solid var(--border); border-radius: var(--radius);
    padding: 14px 0; font-size: 13px; font-weight: 600;
    letter-spacing: 1px; text-transform: uppercase;
    cursor: pointer; transition: all 0.15s;
}
.btn-secondary:hover { background: var(--border); }
.btn-secondary:disabled { opacity: 0.5; cursor: not-allowed; }
.player { display: none; align-items: center; gap: 10px; }
.player.visible { display: flex; }
.btn-play {
    width: 40px; height: 40px; background: var(--accent); color: #fff;
    border: none; border-radius: 50%; font-size: 16px; cursor: pointer;
    display: flex; align-items: center; justify-content: center;
    flex-shrink: 0; transition: background 0.15s;
}
.btn-play:hover { background: var(--accent-hover); }
.player-info { flex: 1; display: flex; flex-direction: column; gap: 4px; }
.player-label { font-size: 12px; color: var(--text2); }
.player-duration { font-size: 14px; font-weight: 600; font-variant-numeric: tabular-nums; }
.btn-download {
    background: var(--surface2); color: var(--success);
    border: 1px solid var(--success); border-radius: 6px;
    padding: 8px 14px; font-size: 12px; font-weight: 600;
    cursor: pointer; white-space: nowrap; transition: all 0.15s;
}
.btn-download:hover { background: var(--success); color: var(--bg); }
.status {
    text-align: center; font-size: 12px; color: var(--text2);
    margin-top: 8px; min-height: 18px; transition: color 0.3s;
}
.status.success { color: var(--success); }
.status.error { color: var(--accent); }
.btn-primary.loading { position: relative; color: transparent; }
.btn-primary.loading::after {
    content: ''; position: absolute; width: 20px; height: 20px;
    border: 2px solid transparent; border-top-color: #fff;
    border-radius: 50%; animation: spin 0.6s linear infinite;
    top: 50%; left: 50%; margin: -10px 0 0 -10px;
}
@keyframes spin { to { transform: rotate(360deg); } }
::-webkit-scrollbar { width: 6px; }
::-webkit-scrollbar-track { background: var(--bg); }
::-webkit-scrollbar-thumb { background: var(--border); border-radius: 3px; }

/* === Tab navigation === */
.tab-bar {
    display: flex; gap: 0; margin-bottom: 16px;
    background: var(--surface); border: 1px solid var(--border);
    border-radius: var(--radius); overflow: hidden;
}
.tab-btn {
    flex: 1; padding: 12px 0; text-align: center;
    font-size: 13px; font-weight: 700; letter-spacing: 1.5px;
    text-transform: uppercase; cursor: pointer;
    background: transparent; color: var(--text2);
    border: none; transition: all 0.2s;
}
.tab-btn:hover { color: var(--text); background: var(--surface2); }
.tab-btn.active { color: var(--accent); background: var(--surface2); }
.tab-btn:not(:first-child) { border-left: 1px solid var(--border); }
.gen-container { display: block; }
.gen-container.hidden { display: none; }
</style>
)css2";
}

static const char* htmlBody()
{
    return R"body1(
<body>
<h1>ONE-SHOT AI</h1>

<!-- Tab navigation -->
<div class="tab-bar">
    <button class="tab-btn active" data-tab="generator" id="tabGenerator">Generator</button>
    <button class="tab-btn" data-tab="oneshotmatch" id="tabOneShotMatch">One-Shot Match</button>
</div>

<!-- Generator container (existing UI) -->
<div class="gen-container" id="genContainer">
<div class="section">
    <div class="row">
        <div class="select-group">
            <label>Instrument</label>
            <select id="instrument">
                <option value="0">Kick</option>
                <option value="1">Snare</option>
                <option value="2">HiHat</option>
                <option value="3">Clap</option>
                <option value="4">Perc</option>
                <option value="5">Bass 808</option>
                <option value="6">Lead</option>
                <option value="7">Pluck</option>
                <option value="8">Pad</option>
                <option value="9">Texture</option>
            </select>
        </div>
        <div class="select-group">
            <label>Genre</label>
            <select id="genre">
                <option value="0">Trap</option>
                <option value="1">Hip Hop</option>
                <option value="2">Techno</option>
                <option value="3">House</option>
                <option value="4">Reggaeton</option>
                <option value="5">Afrobeat</option>
                <option value="6">R&B</option>
                <option value="7">EDM</option>
                <option value="8">Ambient</option>
            </select>
        </div>
    </div>
</div>
<div class="section">
    <div class="row">
        <div class="btn-group">
            <label>Attack</label>
            <div class="buttons" data-param="attack">
                <button class="active" data-value="0">Fast</button>
                <button data-value="1">Medium</button>
                <button data-value="2">Slow</button>
            </div>
        </div>
        <div class="btn-group">
            <label>Energy</label>
            <div class="buttons" data-param="energy">
                <button data-value="0">Low</button>
                <button data-value="1">Medium</button>
                <button class="active" data-value="2">High</button>
            </div>
        </div>
    </div>
</div>
<div class="section">
    <div class="section-title">Character</div>
    <div class="slider-row">
        <input type="checkbox" class="lock" data-param="brillo" title="Lock">
        <span class="param-name">Brillo</span>
        <input type="range" id="brillo" min="0" max="100" value="50">
        <span class="value" data-for="brillo">50</span>
    </div>
    <div class="slider-row">
        <input type="checkbox" class="lock" data-param="cuerpo" title="Lock">
        <span class="param-name">Cuerpo</span>
        <input type="range" id="cuerpo" min="0" max="100" value="50">
        <span class="value" data-for="cuerpo">50</span>
    </div>
    <div class="slider-row">
        <input type="checkbox" class="lock" data-param="textura" title="Lock">
        <span class="param-name">Textura</span>
        <input type="range" id="textura" min="0" max="100" value="50">
        <span class="value" data-for="textura">50</span>
    </div>
    <div class="slider-row">
        <input type="checkbox" class="lock" data-param="movimiento" title="Lock">
        <span class="param-name">Movimiento</span>
        <input type="range" id="movimiento" min="0" max="100" value="0">
        <span class="value" data-for="movimiento">0</span>
    </div>
    <div class="slider-row">
        <input type="checkbox" class="lock" data-param="impacto" title="Lock">
        <span class="param-name">Impacto</span>
        <input type="range" id="impacto" min="0" max="100" value="80">
        <span class="value" data-for="impacto">80</span>
    </div>
</div>
<div class="section">
    <div class="seed-row">
        <label>Seed</label>
        <input type="number" id="seed" value="12345" min="0" max="99999">
        <button class="btn-small" id="randomSeedBtn">Random</button>
        <input type="checkbox" class="lock" data-param="seed" title="Lock seed">
    </div>
</div>
<div class="actions">
    <button class="btn-primary" id="generateBtn">Generate</button>
    <button class="btn-secondary" id="mutateBtn">Mutate</button>
</div>
<div class="section player" id="playerSection">
    <button class="btn-play" id="playBtn">&#9654;</button>
    <div class="player-info">
        <span class="player-label" id="playerLabel">--</span>
        <span class="player-duration" id="playerDuration">0.00s</span>
    </div>
    <button class="btn-download" id="downloadBtn">Download WAV</button>
</div>
<div class="status" id="status"></div>
</div><!-- /gen-container -->
)body1";
}

static const char* jsCode()
{
    return R"js1(
<script>
// === Tab switching ===
(function() {
    var tabs = document.querySelectorAll('.tab-btn');
    var genC = document.getElementById('genContainer');
    var kmC = document.getElementById('kmContainer');
    tabs.forEach(function(tab) {
        tab.addEventListener('click', function() {
            tabs.forEach(function(t){ t.classList.remove('active'); });
            tab.classList.add('active');
            var which = tab.getAttribute('data-tab');
            if (which === 'generator') {
                genC.classList.remove('hidden');
                kmC.classList.remove('active');
            } else {
                genC.classList.add('hidden');
                kmC.classList.add('active');
            }
        });
    });
})();
</script>
<script>
(function() {
    'use strict';
    var audioCtx = null, decoded = null, src = null, wavData = null;
    var playing = false, generating = false;
    var $ = function(id) { return document.getElementById(id); };
    var instrumentSel = $('instrument'), genreSel = $('genre'), seedIn = $('seed');
    var genBtn = $('generateBtn'), mutBtn = $('mutateBtn');
    var playBtn = $('playBtn'), dlBtn = $('downloadBtn');
    var rndBtn = $('randomSeedBtn');
    var pSec = $('playerSection'), pLbl = $('playerLabel'), pDur = $('playerDuration');
    var stEl = $('status');
    var sIds = ['brillo','cuerpo','textura','movimiento','impacto'];
    var iNames = ['Kick','Snare','HiHat','Clap','Perc','Bass808','Lead','Pluck','Pad','Texture'];
    var gNames = ['Trap','HipHop','Techno','House','Reggaeton','Afrobeat','RnB','EDM','Ambient'];

    document.querySelectorAll('.btn-group .buttons').forEach(function(g) {
        g.querySelectorAll('button').forEach(function(b) {
            b.addEventListener('click', function() {
                g.querySelectorAll('button').forEach(function(x){x.classList.remove('active');});
                b.classList.add('active');
            });
        });
    });

    sIds.forEach(function(id) {
        var s = $(id), d = document.querySelector('.value[data-for="'+id+'"]');
        s.addEventListener('input', function() { d.textContent = s.value; });
    });

    rndBtn.addEventListener('click', function() {
        seedIn.value = Math.floor(Math.random()*99999);
    });

    function gv(p) {
        var g = document.querySelector('.buttons[data-param="'+p+'"]');
        var a = g.querySelector('button.active');
        return a ? a.getAttribute('data-value') : '0';
    }
    function locked(p) {
        var l = document.querySelector('.lock[data-param="'+p+'"]');
        return l ? l.checked : false;
    }
    function status(m, t) {
        stEl.textContent = m;
        stEl.className = 'status' + (t ? ' '+t : '');
        if (t === 'success') setTimeout(function(){
            if (stEl.textContent === m) stEl.textContent = '';
        }, 4000);
    }
    function stop() {
        if (src) { try{src.stop();}catch(e){} src.disconnect(); src=null; }
        playing = false; playBtn.innerHTML = '&#9654;';
    }
    function play() {
        if (!decoded || !audioCtx) return;
        stop();
        src = audioCtx.createBufferSource();
        src.buffer = decoded;
        src.connect(audioCtx.destination);
        src.onended = function(){ playing=false; playBtn.innerHTML='&#9654;'; src=null; };
        src.start(); playing = true; playBtn.innerHTML = '&#9632;';
    }
    function bp() {
        var p = new URLSearchParams();
        p.set('instrument', instrumentSel.value);
        p.set('genre', genreSel.value);
        p.set('attack', gv('attack'));
        p.set('energy', gv('energy'));
        sIds.forEach(function(id){ p.set(id, $(id).value); });
        p.set('seed', seedIn.value);
        p.set('t', Date.now().toString());
        return p;
    }
    async function generate() {
        if (generating) return;
        generating = true;
        genBtn.disabled = mutBtn.disabled = true;
        genBtn.classList.add('loading');
        status('Generating...');
        stop();
        try {
            if (!audioCtx) audioCtx = new (window.AudioContext||window.webkitAudioContext)();
            var r = await fetch('/api/generate?'+bp().toString());
            if (!r.ok) throw new Error('Failed: '+r.status);
            var ab = await r.arrayBuffer();
            wavData = ab;
            decoded = await audioCtx.decodeAudioData(ab.slice(0));
            var iN = iNames[parseInt(instrumentSel.value)];
            var gN = gNames[parseInt(genreSel.value)];
            pLbl.textContent = iN+' / '+gN;
            pDur.textContent = decoded.duration.toFixed(2)+'s';
            pSec.classList.add('visible');
            status(iN+' '+gN+' (seed '+seedIn.value+')', 'success');
            play();
        } catch(e) { status('Error: '+e.message, 'error'); }
        finally {
            generating = false;
            genBtn.disabled = mutBtn.disabled = false;
            genBtn.classList.remove('loading');
        }
    }
    function mutate() {
        if (!locked('seed')) seedIn.value = Math.floor(Math.random()*99999);
        sIds.forEach(function(id) {
            if (!locked(id)) {
                var s = $(id), v = parseInt(s.value);
                var nv = Math.max(0, Math.min(100, v + Math.floor((Math.random()-0.5)*30)));
                s.value = nv;
                document.querySelector('.value[data-for="'+id+'"]').textContent = nv;
            }
        });
        generate();
    }
    playBtn.addEventListener('click', function(){ if(playing) stop(); else play(); });
    dlBtn.addEventListener('click', function() {
        if (!wavData) return;
        var n = iNames[parseInt(instrumentSel.value)]+'_'+gNames[parseInt(genreSel.value)]+'_'+seedIn.value+'.wav';
        var b = new Blob([wavData],{type:'audio/wav'});
        var u = URL.createObjectURL(b);
        var a = document.createElement('a');
        a.href=u; a.download=n; document.body.appendChild(a); a.click();
        document.body.removeChild(a); URL.revokeObjectURL(u);
        status('Downloaded: '+n, 'success');
    });
    genBtn.addEventListener('click', generate);
    mutBtn.addEventListener('click', mutate);
    document.addEventListener('keydown', function(e) {
        if (e.target.tagName==='INPUT'||e.target.tagName==='SELECT') return;
        if (e.code==='Space'){e.preventDefault();generate();}
        else if (e.code==='Enter'){e.preventDefault();if(decoded){if(playing)stop();else play();}}
        else if (e.code==='KeyM'){e.preventDefault();mutate();}
    });
})();
</script>
)js1";
}

static const char* htmlClosing()
{
    return "</body>\n</html>\n";
}

static const char* getHTML()
{
    static std::string html = std::string (cssPart1()) + cssPart2()
                            + OneShotMatchWebUI::getCSS()
                            + htmlBody()
                            + OneShotMatchWebUI::getHTML()
                            + jsCode()
                            + OneShotMatchWebUI::getJS()
                            + OneShotMatchWebUI::getJS2()
                            + htmlClosing();
    return html.c_str();
}

static size_t getHTMLSize()
{
    getHTML();
    static size_t len = std::strlen (getHTML());
    return len;
}

} // namespace OneShotWebUI

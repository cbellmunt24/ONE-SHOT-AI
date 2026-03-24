#pragma once

#include <string>

// HTML/CSS/JS for the One-Shot Match tab UI.
// Served inline alongside the Generator UI — tab system switches between them.

namespace OneShotMatchWebUI
{

static const char* getCSS()
{
    return R"km_css(
<style>
.km-container { display: none; }
.km-container.active { display: block; }

.km-drop-zone {
    border: 2px dashed var(--border); border-radius: var(--radius);
    padding: 32px 16px; text-align: center; cursor: pointer;
    transition: all 0.2s; background: var(--surface);
    margin-bottom: 12px;
}
.km-drop-zone:hover, .km-drop-zone.dragover {
    border-color: var(--accent); background: rgba(233,69,96,0.08);
}
.km-drop-zone.loaded {
    border-color: var(--success); background: rgba(78,205,196,0.08);
}
.km-drop-icon { font-size: 28px; margin-bottom: 8px; color: var(--text2); }
.km-drop-text { font-size: 13px; color: var(--text2); }
.km-drop-file { font-size: 12px; color: var(--success); margin-top: 6px; font-weight: 600; }

.km-desc-group-title {
    font-size: 10px; color: var(--accent); text-transform: uppercase;
    letter-spacing: 1px; margin: 8px 0 4px; grid-column: 1 / -1;
    font-weight: 700; border-bottom: 1px solid rgba(233,69,96,0.2); padding-bottom: 2px;
}
.km-descriptors {
    display: grid; grid-template-columns: 1fr 1fr;
    gap: 4px 14px; margin-bottom: 12px;
}
.km-desc-item {
    display: flex; justify-content: space-between; align-items: center;
    font-size: 11px; padding: 3px 0;
    border-bottom: 1px solid rgba(51,51,85,0.4);
}
.km-desc-label { color: var(--text2); text-transform: uppercase; letter-spacing: 0.5px; }
.km-desc-value { color: var(--text); font-variant-numeric: tabular-nums; font-weight: 600; }

.km-progress-bar {
    width: 100%; height: 6px; background: var(--surface2);
    border-radius: 3px; overflow: hidden; margin: 10px 0;
}
.km-progress-fill {
    height: 100%; background: var(--accent); border-radius: 3px;
    transition: width 0.3s ease; width: 0%;
}
.km-progress-fill.converged { background: var(--success); }

.km-stats {
    display: flex; justify-content: space-between; font-size: 12px;
    color: var(--text2); margin-bottom: 10px;
}
.km-stats .value { color: var(--text); font-weight: 600; font-variant-numeric: tabular-nums; }

.km-result-section { display: none; }
.km-result-section.visible { display: block; }

.km-params-grid {
    display: grid; grid-template-columns: 1fr 1fr;
    gap: 3px 14px; font-size: 11px;
}
.km-param-row {
    display: flex; justify-content: space-between; align-items: center; padding: 3px 0;
    border-bottom: 1px solid rgba(51,51,85,0.3);
}
.km-param-name { color: var(--text2); }
.km-param-val { color: var(--success); font-weight: 600; font-variant-numeric: tabular-nums; }
.km-param-sens {
    display: inline-block; width: 28px; height: 3px;
    background: var(--surface2); border-radius: 2px; margin-left: 6px;
    position: relative; vertical-align: middle;
}
.km-param-sens-fill {
    height: 100%; border-radius: 2px;
    background: var(--accent); position: absolute; left: 0; top: 0;
}

.km-export-row { display: flex; gap: 8px; margin-top: 12px; }
.km-export-row button {
    flex: 1; padding: 10px 0; border-radius: var(--radius);
    font-size: 12px; font-weight: 600; cursor: pointer;
    text-transform: uppercase; letter-spacing: 1px; transition: all 0.15s;
}
.km-btn-audio {
    background: var(--success); color: var(--bg); border: none;
}
.km-btn-audio:hover { filter: brightness(1.15); }
.km-btn-preset {
    background: var(--surface2); color: var(--text);
    border: 1px solid var(--border);
}
.km-btn-preset:hover { background: var(--border); }

.km-compare-row {
    display: flex; gap: 8px; margin-bottom: 10px;
}
.km-compare-row button {
    flex: 1; padding: 8px 0; border-radius: 6px;
    font-size: 12px; cursor: pointer; transition: all 0.15s;
    background: var(--surface2); color: var(--text);
    border: 1px solid var(--border);
}
.km-compare-row button:hover { background: var(--border); }
.km-compare-row button.playing { background: var(--accent); color: #fff; border-color: var(--accent); }

.km-score {
    text-align: center; padding: 12px; background: var(--surface);
    border-radius: var(--radius); border: 1px solid var(--border);
    margin-bottom: 12px;
}
.km-score-label { font-size: 11px; color: var(--text2); text-transform: uppercase; letter-spacing: 1px; }
.km-score-value { font-size: 28px; font-weight: 700; margin-top: 4px; }
.km-score-value.good { color: var(--success); }
.km-score-value.ok { color: #f0c040; }
.km-score-value.poor { color: var(--accent); }
.km-score-sub { font-size: 11px; color: var(--text2); margin-top: 2px; }

.km-desc-compare {
    display: grid; grid-template-columns: auto 1fr 1fr;
    gap: 2px 10px; font-size: 10px; margin-top: 8px;
}
.km-desc-compare-hdr { font-weight: 700; color: var(--text2); text-transform: uppercase; letter-spacing: 0.5px; padding-bottom: 2px; border-bottom: 1px solid var(--border); }
.km-desc-compare-label { color: var(--text2); }
.km-desc-compare-ref { color: var(--text); font-variant-numeric: tabular-nums; }
.km-desc-compare-match { color: var(--success); font-variant-numeric: tabular-nums; }

.km-extensions {
    margin: 8px 0; padding: 8px; background: var(--surface);
    border-radius: var(--radius); border: 1px solid var(--border);
}
.km-ext-title {
    font-size: 10px; color: var(--accent); text-transform: uppercase;
    letter-spacing: 1px; font-weight: 700; margin-bottom: 4px;
}
.km-ext-chips { display: flex; flex-wrap: wrap; gap: 4px; }
.km-ext-chip {
    font-size: 10px; padding: 2px 8px; border-radius: 10px;
    background: rgba(233,69,96,0.15); color: var(--accent); font-weight: 600;
}
.km-ext-chip.inactive {
    background: var(--surface2); color: var(--text2); font-weight: 400;
}
.km-ext-phase-info { font-size: 10px; color: var(--text2); margin-top: 4px; }
</style>
)km_css";
}

static const char* getHTML()
{
    return R"km_html(
<div class="km-container" id="kmContainer">
    <!-- INPUT -->
    <div class="section">
        <div class="section-title">Reference Sound</div>
        <div class="km-drop-zone" id="kmDropZone">
            <div class="km-drop-icon">&#127928;</div>
            <div class="km-drop-text">Drop a one-shot sample here<br>or click to browse</div>
            <div class="km-drop-file" id="kmFileName" style="display:none"></div>
        </div>
        <input type="file" id="kmFileInput" accept=".wav,.mp3,.aif,.aiff,.flac" style="display:none">
    </div>

    <!-- ANALYSIS -->
    <div class="section" id="kmAnalysisSection" style="display:none">
        <div class="section-title">Descriptors</div>
        <div class="km-descriptors" id="kmDescriptors"></div>
        <div class="actions">
            <button class="btn-primary" id="kmMatchBtn">Match via Synthesis</button>
        </div>
    </div>

    <!-- MATCHING PROGRESS -->
    <div class="section" id="kmProgressSection" style="display:none">
        <div class="section-title">Matching</div>
        <div class="km-progress-bar">
            <div class="km-progress-fill" id="kmProgressFill"></div>
        </div>
        <div class="km-stats">
            <span>Iteration: <span class="value" id="kmIter">0</span> / <span id="kmMaxIter">250</span></span>
            <span>Distance: <span class="value" id="kmDist">--</span></span>
        </div>
        <div class="actions">
            <button class="btn-secondary" id="kmCancelBtn">Cancel</button>
        </div>
    </div>

    <!-- RESULT -->
    <div class="km-result-section" id="kmResultSection">
        <!-- Score -->
        <div class="km-score">
            <div class="km-score-label">Match Score</div>
            <div class="km-score-value" id="kmScoreValue">--</div>
            <div class="km-score-sub" id="kmScoreSub"></div>
        </div>

        <!-- Compare playback -->
        <div class="section">
            <div class="section-title">Compare</div>
            <div class="km-compare-row">
                <button id="kmPlayRef">&#9654; Reference</button>
                <button id="kmPlayMatch">&#9654; Matched</button>
            </div>
        </div>

        <!-- Extensions activated -->
        <div class="section" id="kmExtSection" style="display:none">
            <div class="km-extensions">
                <div class="km-ext-title">Self-Improving Synth Extensions</div>
                <div class="km-ext-chips" id="kmExtChips"></div>
                <div class="km-ext-phase-info" id="kmExtPhaseInfo"></div>
            </div>
        </div>

        <!-- Descriptor comparison -->
        <div class="section" id="kmDescCompareSection" style="display:none">
            <div class="section-title">Descriptor Comparison</div>
            <div class="km-desc-compare" id="kmDescCompare"></div>
        </div>

        <!-- Optimized params -->
        <div class="section">
            <div class="section-title">Synth Parameters</div>
            <div class="km-params-grid" id="kmParamsGrid"></div>
        </div>

        <!-- Export -->
        <div class="section">
            <div class="km-export-row">
                <button class="km-btn-audio" id="kmExportAudio">Export Matched WAV</button>
                <button class="km-btn-preset" id="kmExportPreset">Export Preset</button>
                <button class="km-btn-preset" id="kmCopyReport">Copy Report</button>
            </div>
            <div id="kmDiag" style="font-size:10px;color:var(--text2);margin-top:6px;font-family:monospace"></div>
        </div>
    </div>
</div>
)km_html";
}

static const char* getJS()
{
    return R"km_js1(
<script>
(function() {
    'use strict';

    var kmDropZone = document.getElementById('kmDropZone');
    var kmFileInput = document.getElementById('kmFileInput');
    var kmFileName = document.getElementById('kmFileName');
    var kmAnalysis = document.getElementById('kmAnalysisSection');
    var kmDescriptors = document.getElementById('kmDescriptors');
    var kmResult = document.getElementById('kmResultSection');
    var kmProgress = document.getElementById('kmProgressSection');

    window._kmRefAudio = null;

    function kmGetAudioCtx() {
        if (!window._kmAudioCtx) window._kmAudioCtx = new (window.AudioContext || window.webkitAudioContext)();
        return window._kmAudioCtx;
    }

    // === Drop zone ===
    kmDropZone.addEventListener('click', function() { kmFileInput.click(); });

    kmDropZone.addEventListener('dragover', function(e) {
        e.preventDefault(); kmDropZone.classList.add('dragover');
    });
    kmDropZone.addEventListener('dragleave', function() {
        kmDropZone.classList.remove('dragover');
    });
    kmDropZone.addEventListener('drop', function(e) {
        e.preventDefault(); kmDropZone.classList.remove('dragover');
        if (e.dataTransfer.files.length > 0) handleFile(e.dataTransfer.files[0]);
    });

    kmFileInput.addEventListener('change', function() {
        if (kmFileInput.files.length > 0) handleFile(kmFileInput.files[0]);
    });

    async function handleFile(file) {
        kmResult.classList.remove('visible');
        kmProgress.style.display = 'none';

        // Read file as ArrayBuffer
        var arrayBuf = await file.arrayBuffer();

        // Decode locally for preview playback
        var ctx = kmGetAudioCtx();
        try { window._kmRefAudio = await ctx.decodeAudioData(arrayBuf.slice(0)); } catch(e) {}

        // Convert to base64 for backend transfer (WebView2 can't access file paths)
        var bytes = new Uint8Array(arrayBuf);
        var chunkSize = 8192;
        var binary = '';
        for (var i = 0; i < bytes.length; i += chunkSize) {
            var chunk = bytes.subarray(i, Math.min(i + chunkSize, bytes.length));
            binary += String.fromCharCode.apply(null, chunk);
        }
        var b64 = btoa(binary);

        // Load in backend via base64 data
        var loadResp = await fetch('/api/match/load?name=' + encodeURIComponent(file.name) + '&data=' + encodeURIComponent(b64));
        if (!loadResp.ok) { alert('Error loading file'); return; }
        var loadData = await loadResp.json();
        if (!loadData.ok) { alert('Load error: ' + (loadData.error || 'unknown')); return; }

        kmFileName.textContent = file.name;
        kmFileName.style.display = 'block';
        kmDropZone.classList.add('loaded');

        // Analyze
        var analysisResp = await fetch('/api/match/analyze');
        if (!analysisResp.ok) { alert('Analysis failed'); return; }

        var data = await analysisResp.json();
        showDescriptors(data);
    }

    function fmtMs(v) { return (v * 1000).toFixed(1) + ' ms'; }
    function fmtHz(v) { return v.toFixed(1) + ' Hz'; }
    function fmtPct(v) { return (v * 100).toFixed(1) + '%'; }
    function fmtDb(v) { return v.toFixed(1) + ' dB'; }
    function fmtNum(v, d) { return (typeof v === 'number') ? v.toFixed(d) : '--'; }

    function showDescriptors(d) {
        var html = '';

        // Temporal
        html += '<div class="km-desc-group-title">Temporal</div>';
        html += descRow('Duration', fmtMs(d.totalDuration));
        html += descRow('Attack', fmtMs(d.attackTime));
        html += descRow('Decay', fmtMs(d.decayTime));
        html += descRow('Decay -40dB', fmtMs(d.decayTime40));
        html += descRow('Transient', fmtNum(d.transientStrength, 1));
        html += descRow('Env Shape', fmtNum(d.envelopeShape, 2));

        // Pitch
        html += '<div class="km-desc-group-title">Pitch</div>';
        html += descRow('Fundamental', fmtHz(d.fundamentalFreq));
        html += descRow('Pitch Start', fmtHz(d.pitchStart));
        html += descRow('Pitch End', fmtHz(d.pitchEnd));
        html += descRow('Pitch Drop', fmtNum(d.pitchDropSemitones, 1) + ' st');
        html += descRow('Drop Time', fmtMs(d.pitchDropTime));

        // Spectral
        html += '<div class="km-desc-group-title">Spectral</div>';
        html += descRow('Centroid', fmtHz(d.spectralCentroid));
        html += descRow('Rolloff', fmtHz(d.spectralRolloff));
        html += descRow('Brightness', fmtPct(d.brightness));
        html += descRow('H/N Ratio', fmtNum(d.harmonicNoiseRatio, 2));
        html += descRow('Tilt', fmtNum(d.spectralTilt, 1) + ' dB/oct');

        // Energy bands
        html += '<div class="km-desc-group-title">Energy Bands</div>';
        html += descRow('Sub', fmtPct(d.subEnergy));
        html += descRow('Low-Mid', fmtPct(d.lowMidEnergy));
        html += descRow('Mid', fmtPct(d.midEnergy));
        html += descRow('High', fmtPct(d.highEnergy));

        // Perceptual
        html += '<div class="km-desc-group-title">Perceptual</div>';
        html += descRow('RMS', fmtNum(d.rmsLoudness, 3));
        html += descRow('LUFS', fmtDb(d.lufs));

        // Regions
        html += '<div class="km-desc-group-title">Transient Region</div>';
        html += descRow('Centroid', fmtHz(d.transientCentroid));
        html += descRow('Flatness', fmtNum(d.transientFlatness, 3));
        html += descRow('Peak', fmtNum(d.transientPeak, 3));
        html += '<div class="km-desc-group-title">Body / Tail</div>';
        html += descRow('Body Centr.', fmtHz(d.bodyCentroid));
        html += descRow('Body Flatn.', fmtNum(d.bodyFlatness, 3));
        html += descRow('Tail Centr.', fmtHz(d.tailCentroid));
        html += descRow('Tail RMS', fmtNum(d.tailRMS, 4));

        kmDescriptors.innerHTML = html;
        kmAnalysis.style.display = 'block';
    }

    function descRow(label, value) {
        return '<div class="km-desc-item"><span class="km-desc-label">' + label +
               '</span><span class="km-desc-value">' + value + '</span></div>';
    }
})();
</script>
)km_js1";
}

static const char* getJS2()
{
    static std::string js2 = std::string(R"km_js2a(
<script>
(function() {
    'use strict';

    var kmMatchBtn = document.getElementById('kmMatchBtn');
    var kmProgress = document.getElementById('kmProgressSection');
    var kmProgressFill = document.getElementById('kmProgressFill');
    var kmIter = document.getElementById('kmIter');
    var kmMaxIter = document.getElementById('kmMaxIter');
    var kmDist = document.getElementById('kmDist');
    var kmCancelBtn = document.getElementById('kmCancelBtn');
    var kmResult = document.getElementById('kmResultSection');
    var kmScoreValue = document.getElementById('kmScoreValue');
    var kmScoreSub = document.getElementById('kmScoreSub');
    var kmDescCompare = document.getElementById('kmDescCompare');
    var kmDescCompareSection = document.getElementById('kmDescCompareSection');
    var kmExtSection = document.getElementById('kmExtSection');
    var kmExtChips = document.getElementById('kmExtChips');
    var kmExtPhaseInfo = document.getElementById('kmExtPhaseInfo');
    var kmParamsGrid = document.getElementById('kmParamsGrid');
    var kmPlayRef = document.getElementById('kmPlayRef');
    var kmPlayMatch = document.getElementById('kmPlayMatch');
    var kmExportAudio = document.getElementById('kmExportAudio');
    var kmExportPreset = document.getElementById('kmExportPreset');

    var kmAudioCtx = null, kmCurrentSrc = null, kmPlaying = null;
    var kmRefAudio = window._kmRefAudio || null;
    var kmMatchAudio = null;
    var kmPollTimer = null;

    var paramInfo = [
        {name:'oscType',         unit:'',   fmt:0, ext:false},
        {name:'basePitch',       unit:'Hz', fmt:1, ext:false},
        {name:'bodyHarmonics',   unit:'',   fmt:3, ext:false},
        {name:'pitchEnvDepth',   unit:'st', fmt:1, ext:false},
        {name:'pitchEnvFast',    unit:'s',  fmt:4, ext:false},
        {name:'pitchEnvSlow',    unit:'s',  fmt:3, ext:false},
        {name:'pitchEnvBalance', unit:'',   fmt:2, ext:false},
        {name:'ampAttack',       unit:'s',  fmt:4, ext:false},
        {name:'ampPunchDecay',   unit:'s',  fmt:3, ext:false},
        {name:'ampBodyDecay',    unit:'s',  fmt:3, ext:false},
        {name:'ampPunchLevel',   unit:'',   fmt:2, ext:false},
        {name:'subLevel',        unit:'',   fmt:2, ext:false},
        {name:'subTailDecay',    unit:'s',  fmt:3, ext:false},
        {name:'subDetune',       unit:'Hz', fmt:2, ext:false},
        {name:'clickAmount',     unit:'',   fmt:3, ext:false},
        {name:'clickFreq',       unit:'Hz', fmt:0, ext:false},
        {name:'clickWidth',      unit:'',   fmt:2, ext:false},
        {name:'clickDecay',      unit:'s',  fmt:4, ext:false},
        {name:'distortion',      unit:'',   fmt:3, ext:false},
        {name:'noiseAmount',     unit:'',   fmt:3, ext:false},
        {name:'filterCutoff',    unit:'Hz', fmt:0, ext:false},
        {name:'harmonicEmphasis',unit:'',   fmt:3, ext:false},
        {name:'fmDepth',         unit:'',   fmt:3, ext:true},
        {name:'fmRatio',         unit:'',   fmt:2, ext:true},
        {name:'fmDecay',         unit:'s',  fmt:3, ext:true},
        {name:'bodyResonance',   unit:'',   fmt:3, ext:true},
        {name:'bodyResonFreq',   unit:'Hz', fmt:0, ext:true},
        {name:'pitchWobble',     unit:'',   fmt:3, ext:true},
        {name:'wobbleRate',      unit:'Hz', fmt:1, ext:true},
        {name:'wobbleDecay',     unit:'s',  fmt:3, ext:true},
        {name:'transientSnap',   unit:'',   fmt:3, ext:true},
        {name:'transientHold',   unit:'s',  fmt:4, ext:true},
        {name:'combAmount',      unit:'',   fmt:3, ext:true},
        {name:'combFreq',        unit:'Hz', fmt:0, ext:true},
        {name:'combFeedback',    unit:'',   fmt:2, ext:true},
        {name:'lowSaturation',   unit:'',   fmt:3, ext:true},
        {name:'highSaturation',  unit:'',   fmt:3, ext:true},
        {name:'phaseDistort',    unit:'',   fmt:3, ext:true},
        {name:'phaseDistDecay',  unit:'s',  fmt:3, ext:true},
        // v2 extensions
        {name:'additiveAmt',    unit:'',   fmt:3, ext:true},
        {name:'harmonic2',      unit:'',   fmt:3, ext:true},
        {name:'harmonic3',      unit:'',   fmt:3, ext:true},
        {name:'harmonic4',      unit:'',   fmt:3, ext:true},
        {name:'harmonicDecayRate',unit:'', fmt:2, ext:true},
        {name:'inharmonicity',  unit:'',   fmt:4, ext:true},
        {name:'reson2Amt',      unit:'',   fmt:3, ext:true},
        {name:'reson2Freq',     unit:'Hz', fmt:0, ext:true},
        {name:'reson3Amt',      unit:'',   fmt:3, ext:true},
        {name:'reson3Freq',     unit:'Hz', fmt:0, ext:true},
        {name:'noiseShapeAmt',  unit:'',   fmt:3, ext:true},
        {name:'noiseColor',     unit:'',   fmt:2, ext:true},
        {name:'noiseFilterFreq',unit:'Hz', fmt:0, ext:true},
        {name:'noiseFilterQ',   unit:'',   fmt:2, ext:true},
        {name:'eqAmount',       unit:'',   fmt:3, ext:true},
        {name:'eqLowGain',     unit:'dB', fmt:1, ext:true},
        {name:'eqMidGain',     unit:'dB', fmt:1, ext:true},
        {name:'eqMidFreq',     unit:'Hz', fmt:0, ext:true},
        {name:'eqHighGain',    unit:'dB', fmt:1, ext:true},
        {name:'envSustainLevel',unit:'',   fmt:3, ext:true},
        {name:'envSustainTime', unit:'s',  fmt:3, ext:true},
        {name:'envRelease',     unit:'s',  fmt:3, ext:true},
        {name:'envCurve',       unit:'',   fmt:2, ext:true},
        {name:'stereoWidth',    unit:'',   fmt:3, ext:true},
        {name:'stereoFreq',     unit:'Hz', fmt:0, ext:true},
        // v3 extensions
        {name:'unisonAmt',     unit:'',   fmt:3, ext:true},
        {name:'unisonDetune',  unit:'ct', fmt:1, ext:true},
        {name:'unisonSpread',  unit:'',   fmt:2, ext:true},
        {name:'formantAmt',    unit:'',   fmt:3, ext:true},
        {name:'formantFreq1',  unit:'Hz', fmt:0, ext:true},
        {name:'formantFreq2',  unit:'Hz', fmt:0, ext:true},
        {name:'transLayerAmt', unit:'',   fmt:3, ext:true},
        {name:'transLayerFreq',unit:'Hz', fmt:0, ext:true},
        {name:'transLayerDecay',unit:'s', fmt:4, ext:true},
        {name:'reverbAmt',     unit:'',   fmt:3, ext:true},
        {name:'reverbDecay',   unit:'s',  fmt:3, ext:true},
        {name:'reverbDamp',    unit:'',   fmt:2, ext:true}
    ];
    var oscNames = ['Sine','Triangle','Saw','Square','Pulse25','Pulse12','HalfRect','AbsSine','Parabolic','Staircase','DblSine','ClipSine'];

    function kmGetAudioCtx() {
        if (!kmAudioCtx) kmAudioCtx = new (window.AudioContext || window.webkitAudioContext)();
        return kmAudioCtx;
    }
    function kmStopAudio() {
        if (kmCurrentSrc) { try { kmCurrentSrc.stop(); } catch(e) {} kmCurrentSrc.disconnect(); kmCurrentSrc = null; }
        kmPlaying = null;
        kmPlayRef.classList.remove('playing'); kmPlayMatch.classList.remove('playing');
        kmPlayRef.innerHTML = '&#9654; Reference'; kmPlayMatch.innerHTML = '&#9654; Matched';
    }
    function kmPlayBuffer(which) {
        var ctx = kmGetAudioCtx(); kmStopAudio();
        var buf = (which === 'ref') ? (window._kmRefAudio || null) : kmMatchAudio;
        if (!buf) return;
        kmCurrentSrc = ctx.createBufferSource(); kmCurrentSrc.buffer = buf;
        kmCurrentSrc.connect(ctx.destination);
        kmCurrentSrc.onended = function() { kmStopAudio(); };
        kmCurrentSrc.start(); kmPlaying = which;
        var btn = (which === 'ref') ? kmPlayRef : kmPlayMatch;
        btn.classList.add('playing');
        btn.innerHTML = '&#9632; ' + (which === 'ref' ? 'Reference' : 'Matched');
    }

    function fmtMs(v) { return (v * 1000).toFixed(1) + ' ms'; }
    function fmtHz(v) { return v.toFixed(1) + ' Hz'; }
    function fmtPct(v) { return (v * 100).toFixed(1) + '%'; }
    function fmtNum(v, d) { return (typeof v === 'number') ? v.toFixed(d) : '--'; }

    // === Match ===
    kmMatchBtn.addEventListener('click', async function() {
        kmMatchBtn.disabled = true;
        kmProgress.style.display = 'block';
        kmResult.classList.remove('visible');
        kmProgressFill.style.width = '0%';
        kmProgressFill.classList.remove('converged');

        await fetch('/api/match/start');

        kmPollTimer = setInterval(async function() {
            var resp = await fetch('/api/match/status');
            if (!resp.ok) return;
            var s = await resp.json();

            kmIter.textContent = s.iteration;
            kmMaxIter.textContent = s.maxIterations;
            kmDist.textContent = s.distance.toFixed(3);
            var pct = (s.iteration / s.maxIterations * 100);
            kmProgressFill.style.width = pct + '%';

            if (s.state === 'done') {
                clearInterval(kmPollTimer);
                kmProgressFill.style.width = '100%';
                kmProgressFill.classList.add('converged');
                kmMatchBtn.disabled = false;
                showResult(s);
            } else if (s.state === 'idle' || s.state === 'error') {
                clearInterval(kmPollTimer);
                kmMatchBtn.disabled = false;
                if (s.state === 'error') alert('Match error: ' + (s.error || 'unknown'));
            }
        }, 300);
    });

    kmCancelBtn.addEventListener('click', function() {
        fetch('/api/match/cancel');
        clearInterval(kmPollTimer);
        kmMatchBtn.disabled = false;
    });
)km_js2a") + R"km_js2b(
    async function showResult(s) {
        // Fetch matched audio
        var resp = await fetch('/api/match/audio');
        if (resp.ok) {
            var ab = await resp.arrayBuffer();
            var ctx = kmGetAudioCtx();
            kmMatchAudio = await ctx.decodeAudioData(ab.slice(0));
        }

        // Score (distance → percentage) — steeper curve for honest perceptual grading
        // dist < 1.5 = good (>75%), dist 1.5-4 = ok (30-75%), dist > 4 = poor (<30%)
        var dist = s.distance;
        var score = Math.round(Math.max(0, 100 / (1 + dist * 0.8)));
        kmScoreValue.textContent = score + '%';
        kmScoreValue.className = 'km-score-value ' +
            (score >= 75 ? 'good' : score >= 35 ? 'ok' : 'poor');
        // Show buffer diagnostics
        var diag = document.getElementById('kmDiag');
        if (s.bufSamples) {
            diag.textContent = 'Buffer: ' + s.bufSamples + ' samples, ' +
                s.bufDurationMs + 'ms, SR=' + s.bufSR + ', peak=' + s.bufPeak + ', RMS=' + s.bufRMS;
        }

        // Store report for copy
        window._kmLastReport = s;

        kmScoreSub.textContent = 'Distance: ' + dist.toFixed(4) +
            (s.converged ? ' (converged)' : '') +
            ' | ' + s.iteration + ' iterations';

        // Extensions display
        if (s.gapAnalysis) {
            var ga = s.gapAnalysis;
            var extModules = [
                {key:'needsFM',            label:'FM Synthesis'},
                {key:'needsResonance',     label:'Resonance'},
                {key:'needsWobble',        label:'Pitch Wobble'},
                {key:'needsTransientSnap', label:'Transient Snap'},
                {key:'needsComb',          label:'Comb Filter'},
                {key:'needsMultibandSat',  label:'Multiband Sat'},
                {key:'needsPhaseDistort',  label:'Phase Distort'},
                {key:'needsAdditive',      label:'Additive Harm'},
                {key:'needsMultiReson',    label:'Multi Reson'},
                {key:'needsNoiseShape',    label:'Noise Shape'},
                {key:'needsEQ',            label:'Param EQ'},
                {key:'needsEnvComplex',    label:'Env Complex'},
                {key:'needsStereo',        label:'Stereo'},
                // v3 extensions
                {key:'needsUnison',     label:'Unison'},
                {key:'needsFormant',    label:'Formant'},
                {key:'needsTransLayer', label:'Trans Layer'},
                {key:'needsReverb',     label:'Reverb'}
            ];
            var chipsHtml = '';
            for (var e = 0; e < extModules.length; e++) {
                var active = ga[extModules[e].key];
                chipsHtml += '<span class="km-ext-chip' + (active ? '' : ' inactive') + '">' +
                    extModules[e].label + '</span>';
            }
            kmExtChips.innerHTML = chipsHtml;
            if (ga.extensionsActivated > 0) {
                kmExtPhaseInfo.textContent = 'Phase 1 (core): ' + ga.phase1Iterations + ' iter, dist=' +
                    ga.phase1Distance.toFixed(3) + ' → Phase 2 (extended): ' +
                    ga.extensionsActivated + ' modules activated';
            } else {
                kmExtPhaseInfo.textContent = 'Core synth was sufficient — no extensions needed';
            }
            kmExtSection.style.display = 'block';
        }

        // Parameters with sensitivity bars (skip inactive extensions)
        if (s.params) {
            var html = '';
            for (var i = 0; i < paramInfo.length; i++) {
                var pi = paramInfo[i];
                var val = s.params[pi.name];
                var sens = s.sensitivity ? (s.sensitivity[pi.name] || 0) : 0;
                // Skip extension params that are inactive (value ≈ 0 and sensitivity ≈ 0)
                if (pi.ext && Math.abs(val) < 0.001 && sens < 0.01) continue;
                var display;
                if (pi.name === 'oscType') {
                    display = oscNames[Math.round(val)] || val;
                } else {
                    display = (typeof val === 'number') ? val.toFixed(pi.fmt) : val;
                }
                var sensWidth = Math.round(sens * 100);
                var sensColor = sens > 0.5 ? 'var(--accent)' : sens > 0.2 ? '#f0c040' : 'var(--text2)';
                var nameLabel = pi.ext ? '* ' + pi.name : pi.name;
                html += '<div class="km-param-row"><span class="km-param-name">' + nameLabel +
                    '</span><span class="km-param-val">' + display + (pi.unit ? ' ' + pi.unit : '') +
                    '<span class="km-param-sens"><span class="km-param-sens-fill" style="width:' +
                    sensWidth + '%;background:' + sensColor + '"></span></span>' +
                    '</span></div>';
            }
            kmParamsGrid.innerHTML = html;
        }

        // Descriptor comparison (ref vs matched)
        if (s.refDescriptors && s.matchedDescriptors) {
            var ref = s.refDescriptors;
            var mat = s.matchedDescriptors;
            var cmpItems = [
                ['Fundamental', fmtHz(ref.fundamentalFreq), fmtHz(mat.fundamentalFreq)],
                ['Attack', fmtMs(ref.attackTime), fmtMs(mat.attackTime)],
                ['Decay', fmtMs(ref.decayTime), fmtMs(mat.decayTime)],
                ['Centroid', fmtHz(ref.spectralCentroid), fmtHz(mat.spectralCentroid)],
                ['Brightness', fmtPct(ref.brightness), fmtPct(mat.brightness)],
                ['Sub Energy', fmtPct(ref.subEnergy), fmtPct(mat.subEnergy)],
                ['Transient', fmtNum(ref.transientStrength,1), fmtNum(mat.transientStrength,1)],
                ['RMS', fmtNum(ref.rmsLoudness,3), fmtNum(mat.rmsLoudness,3)],
                ['H/N Ratio', fmtNum(ref.harmonicNoiseRatio,2), fmtNum(mat.harmonicNoiseRatio,2)],
                ['Tilt', fmtNum(ref.spectralTilt,1), fmtNum(mat.spectralTilt,1)]
            ];
            var cmpHtml = '<div class="km-desc-compare-hdr"></div>' +
                '<div class="km-desc-compare-hdr">Ref</div>' +
                '<div class="km-desc-compare-hdr">Match</div>';
            for (var c = 0; c < cmpItems.length; c++) {
                cmpHtml += '<div class="km-desc-compare-label">' + cmpItems[c][0] + '</div>';
                cmpHtml += '<div class="km-desc-compare-ref">' + cmpItems[c][1] + '</div>';
                cmpHtml += '<div class="km-desc-compare-match">' + cmpItems[c][2] + '</div>';
            }
            kmDescCompare.innerHTML = cmpHtml;
            kmDescCompareSection.style.display = 'block';
        }

        kmResult.classList.add('visible');
    }

    // === Playback ===
    kmPlayRef.addEventListener('click', function() {
        if (kmPlaying === 'ref') kmStopAudio();
        else kmPlayBuffer('ref');
    });
    kmPlayMatch.addEventListener('click', function() {
        if (kmPlaying === 'match') kmStopAudio();
        else kmPlayBuffer('match');
    });

    // === Export ===
    kmExportAudio.addEventListener('click', async function() {
        var resp = await fetch('/api/match/export-audio');
        if (resp.ok) {
            var data = await resp.json();
            if (data.ok) alert('Exported: ' + data.path);
            else alert('Export failed');
        }
    });

    kmExportPreset.addEventListener('click', async function() {
        var resp = await fetch('/api/match/export-preset');
        if (resp.ok) {
            var data = await resp.json();
            if (data.ok) alert('Preset saved: ' + data.path);
            else alert('Export failed');
        }
    });

    document.getElementById('kmCopyReport').addEventListener('click', function() {
        var s = window._kmLastReport;
        if (!s) { alert('No match result yet'); return; }
        var rd = s.refDescriptors || {};
        var md = s.matchedDescriptors || {};
        var txt = '=== ONE-SHOT MATCH REPORT ===\n';
        txt += 'Distance: ' + s.distance.toFixed(4) + '\n';
        txt += 'Score: ' + Math.round(100 * Math.exp(-s.distance * 0.35)) + '%\n';
        txt += 'Iterations: ' + s.iteration + '/' + s.maxIterations + '\n';
        txt += 'Converged: ' + (s.converged ? 'yes' : 'no') + '\n';
        if (s.bufSamples) txt += 'Buffer: ' + s.bufSamples + ' smp, ' + s.bufDurationMs + 'ms, SR=' + s.bufSR + ', peak=' + s.bufPeak + ', RMS=' + s.bufRMS + '\n';
        txt += '\n--- DESCRIPTORS (ref / matched) ---\n';
        var keys = ['fundamentalFreq','pitchStart','pitchDropSemitones','pitchDropTime',
            'attackTime','decayTime','decayTime40','transientStrength','envelopeShape',
            'spectralCentroid','spectralRolloff','brightness','harmonicNoiseRatio',
            'subEnergy','lowMidEnergy','midEnergy','highEnergy','rmsLoudness','totalDuration',
            'spectralTilt','spectralCrest','subHarmonicRatio','noiseSpectralCentroid'];
        for (var i = 0; i < keys.length; i++) {
            var k = keys[i];
            var rv = (rd[k] !== undefined) ? Number(rd[k]).toFixed(4) : '?';
            var mv = (md[k] !== undefined) ? Number(md[k]).toFixed(4) : '?';
            txt += k + ': ' + rv + ' / ' + mv + '\n';
        }
        if (s.params) {
            txt += '\n--- SYNTH PARAMS ---\n';
            var pkeys = Object.keys(s.params);
            for (var i = 0; i < pkeys.length; i++)
                txt += pkeys[i] + ': ' + Number(s.params[pkeys[i]]).toFixed(4) + '\n';
        }
        navigator.clipboard.writeText(txt).then(function() { alert('Report copied to clipboard!'); });
    });

})();
</script>
)km_js2b";
    return js2.c_str();
}

} // namespace OneShotMatchWebUI

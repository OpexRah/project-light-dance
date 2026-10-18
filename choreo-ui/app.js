// ==========================================================================
// LIGHT DANCE CHOREOGRAPHY STUDIO - CORE LOGIC
// ==========================================================================

// Section Configurations matching Master_1_slave.ino & Slave_2_strips.ino
const SECTIONS = {
  'head':         { name: 'Hood / Head',  strip: 0, startLed: 101, endLed: 152, defaultColor: '#00ffff', macro: 'CUE_HEAD' },
  'left-hand':    { name: 'Left Hand',    strip: 0, startLed: 32,  endLed: 100, defaultColor: '#00ff88', macro: 'CUE_LEFT_HAND' },
  'right-hand':   { name: 'Right Hand',   strip: 0, startLed: 153, endLed: 225, defaultColor: '#00ff88', macro: 'CUE_RIGHT_HAND' },
  'left-zipper':  { name: 'Left Zipper',  strip: 0, startLed: 0,   endLed: 31,  defaultColor: '#3b82f6', macro: 'CUE_LEFT_ZIPPER' },
  'right-zipper': { name: 'Right Zipper', strip: 0, startLed: 226, endLed: 299, defaultColor: '#3b82f6', macro: 'CUE_RIGHT_ZIPPER' },
  'belt':         { name: 'Belt Area',    strip: 1, startLed: 225, endLed: 299, defaultColor: '#f59e0b', macro: 'CUE_BELT' },
  'left-leg':     { name: 'Left Leg',     strip: 1, startLed: 0,   endLed: 110, defaultColor: '#ef4444', macro: 'CUE_LEFT_LEG' },
  'right-leg':    { name: 'Right Leg',    strip: 1, startLed: 111, endLed: 224, defaultColor: '#ef4444', macro: 'CUE_RIGHT_LEG' }
};

// Cached section keys & index lookups to avoid recomputing Object.keys each frame
const SECTION_KEYS = Object.keys(SECTIONS);
const SECTION_INDEX = {};
SECTION_KEYS.forEach((key, i) => { SECTION_INDEX[key] = i; });

// Cached LED path DOM element references: ledElements[dancer][sectionKey]
const ledElements = {};
function cacheLedElements() {
  for (let d = 1; d <= 5; d++) {
    ledElements[d] = {};
    for (let s = 0; s < SECTION_KEYS.length; s++) {
      const key = SECTION_KEYS[s];
      ledElements[d][key] = document.querySelector(`.led-path[data-dancer="${d}"][data-sec="${key}"]`);
    }
  }
}

// ================= STATE =================
const state = {
  audioContext: null,
  audioBuffer: null,
  audioElement: null,
  audioLoaded: false,
  audioFileName: 'No audio loaded',
  totalDurationMs: 40000, // Default 40s if no audio
  currentTimeMs: 0,
  isPlaying: false,
  
  // Timeline zoom: pixels per second
  pxPerSec: 80,
  minPxPerSec: 25,
  maxPxPerSec: 350,
  
  // Active Filter Dancer (0 = ALL)
  activeDancerFilter: 0,

  // Magnet snap mode: cue edges snap to other cues, grid, and playhead
  magnetEnabled: false,

  // Selected Cue for editing
  selectedCueId: null,

  // Choreography Cue List
  cues: []
};

// ================= INITIALIZATION =================
document.addEventListener('DOMContentLoaded', () => {
  initSuitVectors();
  initAudio();
  initTimeline();
  initInspector();
  initModals();
  initKeyboardShortcuts();

  // Load default demo routine
  loadDefaultRoutine();
});

// ================= SUIT SVG VECTOR RENDERING =================
// Generates accurate SVG vector paths modeling the hoodie & pants from Image 2
function createSuitSvg(dancerId) {
  return `
    <svg class="suit-svg" viewBox="0 0 160 260" data-dancer="${dancerId}">
      <defs>
        <filter id="glow-${dancerId}" x="-30%" y="-30%" width="160%" height="160%">
          <feGaussianBlur stdDeviation="3.5" result="blur" />
          <feMerge>
            <feMergeNode in="blur" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>

      <!-- BASE SUIT SILHOUETTE (Dark fabric backdrop) -->
      <g class="suit-silhouette" opacity="0.35">
        <!-- Hoodie Body & Arms -->
        <path d="M 50,45 L 30,55 L 12,125 L 28,130 L 42,85 L 42,135 L 118,135 L 118,85 L 132,130 L 148,125 L 130,55 L 110,45 Z" fill="#0d1424" stroke="#1e293b" stroke-width="1.5" />
        <!-- Hood Outline -->
        <path d="M 55,45 C 55,10 105,10 105,45 Z" fill="#0d1424" stroke="#1e293b" stroke-width="1.5" />
        <!-- Pants / Legs -->
        <path d="M 44,142 L 116,142 L 112,245 L 88,245 L 80,170 L 72,245 L 48,245 Z" fill="#0d1424" stroke="#1e293b" stroke-width="1.5" />
      </g>

      <!-- ACTIVE LED TRACKS (Configurable Sections) -->
      <g class="led-tracks" filter="url(#glow-${dancerId})">
        
        <!-- 1. HOOD / HEAD (101-152) -->
        <path class="led-path" data-dancer="${dancerId}" data-sec="head"
              d="M 57,44 C 57,15 103,15 103,44" />

        <!-- 2. LEFT HAND / ARM (32-100) -->
        <path class="led-path" data-dancer="${dancerId}" data-sec="left-hand"
              d="M 58,48 L 32,58 L 16,124" />

        <!-- 3. RIGHT HAND / ARM (153-225) -->
        <path class="led-path" data-dancer="${dancerId}" data-sec="right-hand"
              d="M 102,48 L 128,58 L 144,124" />

        <!-- 4. LEFT ZIPPER (0-31) -->
        <path class="led-path" data-dancer="${dancerId}" data-sec="left-zipper"
              d="M 74,48 L 74,132" />

        <!-- 5. RIGHT ZIPPER (226-299) -->
        <path class="led-path" data-dancer="${dancerId}" data-sec="right-zipper"
              d="M 86,48 L 86,132" />

        <!-- 6. BELT AREA (225-299) -->
        <path class="led-path" data-dancer="${dancerId}" data-sec="belt"
              d="M 46,142 C 60,146 100,146 114,142" />

        <!-- 7. LEFT LEG (0-110) -->
        <path class="led-path" data-dancer="${dancerId}" data-sec="left-leg"
              d="M 47,146 L 47,243 L 72,243 L 78,172" />

        <!-- 8. RIGHT LEG (111-224) -->
        <path class="led-path" data-dancer="${dancerId}" data-sec="right-leg"
              d="M 113,146 L 113,243 L 88,243 L 82,172" />

      </g>
    </svg>
  `;
}

function initSuitVectors() {
  for (let i = 1; i <= 5; i++) {
    const wrap = document.getElementById(`suitWrap${i}`);
    if (wrap) {
      wrap.innerHTML = createSuitSvg(i);
    }
  }

  cacheLedElements();

  // Interactive section clicking directly on suit visualizer
  document.querySelectorAll('.led-path').forEach(el => {
    el.addEventListener('click', (e) => {
      e.stopPropagation();
      const secKey = el.dataset.sec;
      const dancer = parseInt(el.dataset.dancer);

      // Toggle this section in inspector
      const btn = document.querySelector(`.section-chip[data-sec="${secKey}"]`);
      if (btn) {
        btn.classList.toggle('active');
      }

      // Target this dancer in inspector
      setInspectorTargetDancers([dancer]);
    });
  });

  // Clicking dancer node selects dancer
  document.querySelectorAll('.dancer-node').forEach(node => {
    node.addEventListener('click', () => {
      const d = parseInt(node.dataset.dancer);
      setInspectorTargetDancers([d]);
    });
  });
}

// ================= AUDIO ENGINE & WAVEFORM =================
function initAudio() {
  state.audioElement = document.getElementById('audioElement');

  // File Upload
  const fileInput = document.getElementById('audioFileInput');
  fileInput.addEventListener('change', handleAudioUpload);

  // Drag & drop audio onto stage
  const stage = document.getElementById('stageSection');
  stage.addEventListener('dragover', (e) => { e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; });
  stage.addEventListener('drop', (e) => {
    e.preventDefault();
    if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      loadAudioFile(e.dataTransfer.files[0]);
    }
  });

  // Built-in Demo Track Generator
  document.getElementById('btnLoadDemoTrack').addEventListener('click', loadSynthesizedDemoBeat);

  // Playback is driven by a single requestAnimationFrame loop (see playbackAnimationLoop)
  // for smooth, synchronized stage rendering regardless of the audio element's sparse timeupdate events.
  state.audioElement.addEventListener('ended', () => {
    pausePlayback();
    seekToTime(0);
  });
}

function handleAudioUpload(e) {
  if (e.target.files && e.target.files.length > 0) {
    loadAudioFile(e.target.files[0]);
  }
}

function loadAudioFile(file) {
  state.audioFileName = file.name;
  document.getElementById('trackNameDisplay').textContent = file.name;
  document.getElementById('audioStatusPill').querySelector('.status-dot').classList.add('active');

  const fileUrl = URL.createObjectURL(file);
  state.audioElement.src = fileUrl;

  const reader = new FileReader();
  reader.onload = async (ev) => {
    const arrayBuffer = ev.target.result;
    if (!state.audioContext) {
      state.audioContext = new (window.AudioContext || window.webkitAudioContext)();
    }
    try {
      state.audioBuffer = await state.audioContext.decodeAudioData(arrayBuffer);
      state.totalDurationMs = Math.floor(state.audioBuffer.duration * 1000);
      state.audioLoaded = true;
      document.getElementById('trackDurationDisplay').textContent = formatTime(state.totalDurationMs);
      renderWaveform();
      renderRuler();
      renderCueMarkers();
      console.log(`Audio loaded: ${file.name}, Duration: ${state.totalDurationMs}ms`);
    } catch (err) {
      console.error('Audio decode error:', err);
      alert('Could not decode audio file. Please try another MP3 or WAV.');
    }
  };
  reader.readAsArrayBuffer(file);
}

// Built-in Synthesizer Beat (instant zero-file demo)
function loadSynthesizedDemoBeat() {
  const durationSec = 42;
  const sampleRate = 44100;
  if (!state.audioContext) {
    state.audioContext = new (window.AudioContext || window.webkitAudioContext)();
  }

  const numSamples = durationSec * sampleRate;
  const audioBuffer = state.audioContext.createBuffer(2, numSamples, sampleRate);
  const left = audioBuffer.getChannelData(0);
  const right = audioBuffer.getChannelData(1);

  // Generate punchy 128 BPM electronic dance beat
  const bpm = 128;
  const beatInterval = 60 / bpm; // ~0.46875s per beat

  for (let i = 0; i < numSamples; i++) {
    const t = i / sampleRate;
    const beatPhase = (t % beatInterval) / beatInterval;
    const beatIndex = Math.floor(t / beatInterval);

    let sample = 0;

    // 1. Four-on-the-floor Kick Drum
    if (beatPhase < 0.25) {
      const kickFreq = 150 * Math.exp(-beatPhase * 25);
      const kickEnv = Math.exp(-beatPhase * 16);
      sample += Math.sin(2 * Math.PI * kickFreq * t) * kickEnv * 0.7;
    }

    // 2. Off-beat Hi-Hat
    const hatPhase = ((t + beatInterval * 0.5) % beatInterval) / beatInterval;
    if (hatPhase < 0.1) {
      const hatEnv = Math.exp(-hatPhase * 40);
      sample += (Math.random() * 2 - 1) * hatEnv * 0.25;
    }

    // 3. Synth Bassline & Lead
    const bar = Math.floor(beatIndex / 4);
    const chordFreqs = [110, 130.81, 146.83, 164.81]; // A2, C3, D3, E3
    const freq = chordFreqs[bar % chordFreqs.length] * (1 + (beatIndex % 2) * 0.5);
    const synthEnv = Math.exp(-(beatPhase % 0.5) * 8);
    sample += ((t * freq) % 1 > 0.5 ? 0.2 : -0.2) * synthEnv;

    left[i] = sample;
    right[i] = sample;
  }

  state.audioBuffer = audioBuffer;
  state.totalDurationMs = durationSec * 1000;
  state.audioLoaded = true;
  state.audioFileName = 'Electric Anthem (Demo Beat 128 BPM)';

  document.getElementById('trackNameDisplay').textContent = state.audioFileName;
  document.getElementById('trackDurationDisplay').textContent = formatTime(state.totalDurationMs);
  document.getElementById('audioStatusPill').querySelector('.status-dot').classList.add('active');

  // Convert buffer to WAV Blob for HTML5 Audio playback
  const wavBlob = audioBufferToWavBlob(audioBuffer);
  state.audioElement.src = URL.createObjectURL(wavBlob);

  renderWaveform();
  renderRuler();
  renderCueMarkers();
}

function audioBufferToWavBlob(buffer) {
  const numChannels = buffer.numberOfChannels;
  const sampleRate = buffer.sampleRate;
  const format = 1; // PCM
  const bitDepth = 16;
  const bytesPerSample = bitDepth / 8;
  const blockAlign = numChannels * bytesPerSample;
  const dataLen = buffer.length * blockAlign;
  const bufferLen = 44 + dataLen;
  const ab = new ArrayBuffer(bufferLen);
  const view = new DataView(ab);

  // Write WAV header
  function writeString(offset, str) {
    for (let i = 0; i < str.length; i++) view.setUint8(offset + i, str.charCodeAt(i));
  }
  writeString(0, 'RIFF');
  view.setUint32(4, 36 + dataLen, true);
  writeString(8, 'WAVE');
  writeString(12, 'fmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, format, true);
  view.setUint16(22, numChannels, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * blockAlign, true);
  view.setUint16(32, blockAlign, true);
  view.setUint16(34, bitDepth, true);
  writeString(36, 'data');
  view.setUint32(40, dataLen, true);

  // Write PCM audio
  let offset = 44;
  for (let i = 0; i < buffer.length; i++) {
    for (let ch = 0; ch < numChannels; ch++) {
      let sample = buffer.getChannelData(ch)[i];
      sample = Math.max(-1, Math.min(1, sample));
      view.setInt16(offset, sample < 0 ? sample * 0x8000 : sample * 0x7FFF, true);
      offset += 2;
    }
  }
  return new Blob([ab], { type: 'audio/wav' });
}

// ================= TIMELINE & WAVEFORM RENDERING =================
function initTimeline() {
  const scrollWrap = document.getElementById('timelineScrollWrap');
  const viewport = document.getElementById('timelineViewport');

  // Play / Pause Button
  document.getElementById('btnPlayPause').addEventListener('click', togglePlayPause);
  document.getElementById('btnStopPlayback').addEventListener('click', stopPlayback);
  document.getElementById('btnJumpStart').addEventListener('click', () => seekToTime(0));
  document.getElementById('btnJumpEnd').addEventListener('click', () => seekToTime(state.totalDurationMs));

  // Stepping controls (move left/right by smallest unit)
  const btnStepBack = document.getElementById('btnStepBackward');
  if (btnStepBack) {
    btnStepBack.addEventListener('click', () => stepTimeline(-1));
  }
  const btnStepFwd = document.getElementById('btnStepForward');
  if (btnStepFwd) {
    btnStepFwd.addEventListener('click', () => stepTimeline(1));
  }

  // Zoom controls
  document.getElementById('btnZoomIn').addEventListener('click', () => setZoom(state.pxPerSec * 1.3));
  document.getElementById('btnZoomOut').addEventListener('click', () => setZoom(state.pxPerSec / 1.3));
  document.getElementById('btnZoomFit').addEventListener('click', fitZoomToScreen);

  // Magnet snap toggle
  const magnetBtn = document.getElementById('btnMagnetToggle');
  if (magnetBtn) {
    magnetBtn.addEventListener('click', () => {
      state.magnetEnabled = !state.magnetEnabled;
      magnetBtn.classList.toggle('active', state.magnetEnabled);
    });
  }

  // Clicking on timeline track seeks playhead
  viewport.addEventListener('click', (e) => {
    // Avoid seek if clicking on a cue marker block directly
    if (e.target.closest('.cue-marker-block')) return;

    const rect = viewport.getBoundingClientRect();
    const clickX = e.clientX - rect.left;
    const timeMs = Math.max(0, Math.floor((clickX / state.pxPerSec) * 1000));
    seekToTime(timeMs);
  });

  // Window resize re-renders canvas
  window.addEventListener('resize', () => {
    renderRuler();
    renderWaveform();
    renderCueMarkers();
  });

  // Initial draw
  updateTimelineWidth();
  renderRuler();
  renderWaveform();
}

function updateTimelineWidth() {
  const totalSec = state.totalDurationMs / 1000;
  const widthPx = Math.max(window.innerWidth - 300, totalSec * state.pxPerSec);
  const viewport = document.getElementById('timelineViewport');
  viewport.style.width = `${widthPx}px`;
  return widthPx;
}

function setZoom(newPxPerSec) {
  state.pxPerSec = Math.max(state.minPxPerSec, Math.min(state.maxPxPerSec, newPxPerSec));
  document.getElementById('zoomLevelDisplay').textContent = `${(state.pxPerSec / 80).toFixed(1)}x`;
  updateTimelineWidth();
  renderRuler();
  renderWaveform();
  renderCueMarkers();
  updatePlayheadPosition();
}

function fitZoomToScreen() {
  const wrapWidth = document.getElementById('timelineScrollWrap').clientWidth - 40;
  const totalSec = state.totalDurationMs / 1000;
  setZoom(wrapWidth / totalSec);
}

// Render Time Ruler (Ticks & Labels)
function renderRuler() {
  const canvas = document.getElementById('rulerCanvas');
  const width = updateTimelineWidth();
  const height = 26;

  canvas.width = width * window.devicePixelRatio;
  canvas.height = height * window.devicePixelRatio;
  canvas.style.width = `${width}px`;
  canvas.style.height = `${height}px`;

  const ctx = canvas.getContext('2d');
  ctx.scale(window.devicePixelRatio, window.devicePixelRatio);

  ctx.fillStyle = '#080b12';
  ctx.fillRect(0, 0, width, height);

  ctx.fillStyle = '#64748b';
  ctx.font = '10px JetBrains Mono, monospace';

  // Major ticks every 1s or 5s depending on zoom
  const stepSec = state.pxPerSec > 80 ? 1 : (state.pxPerSec > 40 ? 2 : 5);
  const totalSec = Math.ceil(state.totalDurationMs / 1000);

  for (let s = 0; s <= totalSec; s += stepSec) {
    const x = s * state.pxPerSec;
    // Major tick line
    ctx.strokeStyle = '#334155';
    ctx.beginPath();
    ctx.moveTo(x, 14);
    ctx.lineTo(x, height);
    ctx.stroke();

    // Time text
    ctx.fillText(`${s}s`, x + 4, 12);

    // Minor ticks
    if (state.pxPerSec > 60) {
      for (let sub = 1; sub < 4; sub++) {
        const subX = x + (sub * (state.pxPerSec * stepSec)) / 4;
        ctx.strokeStyle = '#1e293b';
        ctx.beginPath();
        ctx.moveTo(subX, 20);
        ctx.lineTo(subX, height);
        ctx.stroke();
      }
    }
  }
}

// Render Audio Waveform
function renderWaveform() {
  const canvas = document.getElementById('waveformCanvas');
  const width = updateTimelineWidth();
  const height = 70;

  canvas.width = width * window.devicePixelRatio;
  canvas.height = height * window.devicePixelRatio;
  canvas.style.width = `${width}px`;
  canvas.style.height = `${height}px`;

  const ctx = canvas.getContext('2d');
  ctx.scale(window.devicePixelRatio, window.devicePixelRatio);

  ctx.clearRect(0, 0, width, height);

  if (!state.audioBuffer) {
    // Placeholder faint rhythm pattern if no audio loaded
    ctx.fillStyle = 'rgba(0, 243, 255, 0.1)';
    for (let x = 0; x < width; x += 6) {
      const h = (Math.sin(x * 0.05) * 0.5 + 0.5) * 30 + 5;
      ctx.fillRect(x, height / 2 - h / 2, 3, h);
    }
    return;
  }

  // Draw decoded audio waveform
  const channelData = state.audioBuffer.getChannelData(0);
  const totalSamples = channelData.length;
  const samplesPerPixel = totalSamples / width;

  // Waveform glowing gradient
  const gradient = ctx.createLinearGradient(0, 0, 0, height);
  gradient.addColorStop(0, '#00f3ff');
  gradient.addColorStop(0.5, '#00b4d8');
  gradient.addColorStop(1, '#ff007f');

  ctx.fillStyle = gradient;

  for (let x = 0; x < width; x++) {
    const startSample = Math.floor(x * samplesPerPixel);
    const endSample = Math.floor((x + 1) * samplesPerPixel);

    let min = 1.0;
    let max = -1.0;

    for (let i = startSample; i < endSample; i += 4) {
      const val = channelData[i];
      if (val < min) min = val;
      if (val > max) max = val;
    }

    const yMin = ((1 + min) / 2) * height;
    const yMax = ((1 + max) / 2) * height;
    ctx.fillRect(x, yMin, 1, Math.max(2, yMax - yMin));
  }
}

// ================= CUE MARKERS TRACK RENDERING (WITH DYNAMIC DOWNWARD LAYER STACKING) =================
// Calculates non-overlapping vertical layers so that concurrent/overlapping cues stack downwards
function calculateCueLayers(visibleCues) {
  // Sort cues chronologically by timeMs, then duration
  const sorted = [...visibleCues].sort((a, b) => a.timeMs - b.timeMs || a.durationMs - b.durationMs);
  const layerEndTimes = []; // Tracks the end timestamp of the latest cue in each layer

  sorted.forEach(cue => {
    let assignedLayer = -1;
    const start = cue.timeMs;
    const end = cue.timeMs + cue.durationMs;

    // Check existing layers to find the first one that has finished before this cue starts
    for (let l = 0; l < layerEndTimes.length; l++) {
      if (layerEndTimes[l] <= start) {
        assignedLayer = l;
        layerEndTimes[l] = end;
        break;
      }
    }

    // If all existing layers overlap, create a new layer downwards
    if (assignedLayer === -1) {
      assignedLayer = layerEndTimes.length;
      layerEndTimes.push(end);
    }

    cue.layer = assignedLayer;
  });

  return Math.max(1, layerEndTimes.length);
}

function renderCueMarkers() {
  // Refresh cached per-cue target/section indices before rendering
  for (let i = 0; i < state.cues.length; i++) indexCueTargets(state.cues[i]);

  const track = document.getElementById('cuesTrack');
  const viewport = document.getElementById('timelineViewport');
  track.innerHTML = '';

  // Filter cues according to dancer selection
  const visibleCues = state.cues.filter(cue => {
    if (state.activeDancerFilter !== 0 && !cue.dancers.includes(0) && !cue.dancers.includes(state.activeDancerFilter)) {
      return false;
    }
    return true;
  });

  // Calculate dynamic layers for overlapping cues
  const totalLayers = calculateCueLayers(visibleCues);

  const CUE_HEIGHT = 26;
  const CUE_GAP = 5;
  const trackHeight = Math.max(130, totalLayers * (CUE_HEIGHT + CUE_GAP) + 35);
  track.style.height = `${trackHeight}px`;
  viewport.style.height = `${96 + trackHeight}px`;

  visibleCues.forEach(cue => {
    const leftPx = (cue.timeMs / 1000) * state.pxPerSec;
    const widthPx = Math.max(28, (cue.durationMs / 1000) * state.pxPerSec);
    const topPx = (cue.layer || 0) * (CUE_HEIGHT + CUE_GAP);

    const block = document.createElement('div');
    block.className = `cue-marker-block ${cue.id === state.selectedCueId ? 'selected' : ''}`;
    block.style.left = `${leftPx}px`;
    block.style.top = `${topPx}px`;
    block.style.width = `${widthPx}px`;
    block.style.backgroundColor = cue.color;

    // High contrast text based on brightness
    const rgb = hexToRgb(cue.color);
    const brightness = (rgb.r * 299 + rgb.g * 587 + rgb.b * 114) / 1000;
    const textColor = brightness > 140 ? '#07090e' : '#ffffff';
    block.style.color = textColor;

    // Dancer badge text & section summary
    const dancerLabel = cue.dancers.includes(0) ? 'ALL' : `D${cue.dancers.join(',')}`;
    let secLabel = 'SUIT';
    if (cue.sections.length === 1) {
      const secKey = cue.sections[0];
      secLabel = SECTIONS[secKey] ? SECTIONS[secKey].name.split('/')[0].trim() : secKey;
    } else if (cue.sections.length < 8) {
      secLabel = `${cue.sections.length} Secs`;
    }

    const timeLabel = widthPx > 70 ? `<span class="cue-time-badge" style="color:${textColor}; border:1px solid ${textColor}40;">${cue.timeMs}-${cue.timeMs + cue.durationMs}ms</span>` : '';

    block.innerHTML = `
      <span style="font-weight:700;">${secLabel}</span>
      ${timeLabel}
      <span class="cue-badge" style="color:${textColor}; border:1px solid ${textColor}40;">${dancerLabel}</span>
    `;

    block.title = `${secLabel} (${dancerLabel}) • Start: ${cue.timeMs}ms | End: ${cue.timeMs + cue.durationMs}ms (Length: ${cue.durationMs}ms) • Click to edit, drag box to move, drag edges to resize`;

    // Resize handles (drag left/right edges to shrink or expand)
    const hLeft = document.createElement('div');
    hLeft.className = 'resize-handle left';
    const hRight = document.createElement('div');
    hRight.className = 'resize-handle right';
    block.appendChild(hLeft);
    block.appendChild(hRight);

    attachResizeHandle(hLeft, cue, 'left', block);
    attachResizeHandle(hRight, cue, 'right', block);

    // Drag-to-move cue box horizontally across timeline
    attachDragToMove(block, cue);

    // Click marker to inspect & edit
    block.addEventListener('click', (e) => {
      e.stopPropagation();
      selectCue(cue.id);
    });

    track.appendChild(block);
  });

  const layerInfo = totalLayers > 1 ? ` (${totalLayers} Layers)` : '';
  document.getElementById('cueCounterBadge').textContent = `${state.cues.length} Cues${layerInfo}`;
}

// Attach interactive drag-to-move horizontally along the timeline
function attachDragToMove(block, cue) {
  block.addEventListener('pointerdown', (e) => {
    // If clicking on a resize handle, let resize handle operate
    if (e.target.classList.contains('resize-handle')) return;

    e.stopPropagation();
    e.preventDefault();

    const startClientX = e.clientX;
    const initialTimeMs = cue.timeMs;
    const durationMs = cue.durationMs;
    const pxPerSec = state.pxPerSec;
    const thresholdMs = Math.max(10, (8 / pxPerSec) * 1000);

    let hasMoved = false;
    document.body.classList.add('resizing');
    block.style.cursor = 'grabbing';

    const onMove = (ev) => {
      const dxPx = ev.clientX - startClientX;
      if (Math.abs(dxPx) > 3) hasMoved = true;

      const dxMs = (dxPx / pxPerSec) * 1000;
      let newTime = initialTimeMs + dxMs;

      if (state.magnetEnabled) {
        newTime = snapValue(newTime, cue, thresholdMs);
      } else {
        const stepMs = currentGridStepMs();
        newTime = Math.round(newTime / stepMs) * stepMs;
      }

      newTime = Math.max(0, Math.min(state.totalDurationMs - durationMs, newTime));
      cue.timeMs = Math.round(newTime);

      updateCueBlockVisual(cue, block);
      if (state.selectedCueId === cue.id) {
        updateCueInspectorFields(cue);
      }
      renderStageLightingAtCurrentTime();
    };

    const onUp = () => {
      window.removeEventListener('pointermove', onMove);
      window.removeEventListener('pointerup', onUp);
      document.body.classList.remove('resizing');
      block.style.cursor = '';

      if (hasMoved) {
        state.cues.sort((a, b) => a.timeMs - b.timeMs);
        renderCueMarkers();
        selectCue(cue.id);
      }
    };

    window.addEventListener('pointermove', onMove);
    window.addEventListener('pointerup', onUp);
  });
}

// ================= CUE BLOCK RESIZE (DRAG EDGES) =================
// Live-updates a block's position/width while dragging a left or right handle,
// converting pixel deltas to milliseconds via the current pxPerSec zoom.
function currentGridStepMs() {
  const sel = document.getElementById('stepUnitSelect');
  return (sel && parseInt(sel.value)) || 25;
}

// Collect candidate snap points: grid lines, timeline bounds, other cue
// boundaries, and the current playhead position.
function collectSnapPoints(excludeCue) {
  const stepMs = currentGridStepMs();
  const points = [0, state.totalDurationMs];
  for (let ms = stepMs; ms < state.totalDurationMs; ms += stepMs) points.push(ms);
  points.push(state.currentTimeMs);
  for (let i = 0; i < state.cues.length; i++) {
    const c = state.cues[i];
    if (c === excludeCue) continue;
    points.push(c.timeMs);
    points.push(c.timeMs + c.durationMs);
  }
  return points;
}

// Snap a millisecond value to the nearest snap point within the given
// threshold (in ms). Returns the original value if nothing is close enough.
function snapValue(valueMs, excludeCue, thresholdMs) {
  const points = collectSnapPoints(excludeCue);
  let best = valueMs;
  let bestDist = thresholdMs;
  for (let i = 0; i < points.length; i++) {
    const d = Math.abs(valueMs - points[i]);
    if (d <= bestDist) { bestDist = d; best = points[i]; }
  }
  return best;
}

function updateCueBlockVisual(cue, block) {
  block.style.left = `${(cue.timeMs / 1000) * state.pxPerSec}px`;
  block.style.width = `${Math.max(28, (cue.durationMs / 1000) * state.pxPerSec)}px`;
}

function updateCueInspectorFields(cue) {
  const timeInput = document.getElementById('cueTimeInput');
  const durInput = document.getElementById('cueDurationInput');
  const endInput = document.getElementById('cueEndTimeInput');

  if (timeInput) timeInput.value = cue.timeMs;
  if (durInput) durInput.value = cue.durationMs;
  if (endInput) endInput.value = cue.timeMs + cue.durationMs;
}

function attachResizeHandle(handle, cue, side, block) {
  handle.addEventListener('pointerdown', (e) => {
    e.stopPropagation();
    e.preventDefault();

    const startX = e.clientX;
    const startTimeMs = cue.timeMs;
    const startDurMs = cue.durationMs;
    const pxPerSec = state.pxPerSec;
    const MIN_MS = 25;

    document.body.classList.add('resizing');

    const onMove = (ev) => {
      const dxMs = ((ev.clientX - startX) / pxPerSec) * 1000;
      const endMs = startTimeMs + startDurMs; // original right edge
      const thresholdMs = Math.max(MIN_MS, (8 / pxPerSec) * 1000);

      let newTime = startTimeMs;
      let newDur = startDurMs;

      if (side === 'right') {
        // Right handle: snap the moving edge (end), keep start fixed
        let edge = endMs + dxMs;
        if (state.magnetEnabled) edge = snapValue(edge, cue, thresholdMs);
        newDur = edge - startTimeMs;
        if (newDur < MIN_MS) newDur = MIN_MS;
        cue.timeMs = startTimeMs;
        cue.durationMs = Math.round(newDur);
      } else {
        // Left handle: snap the moving edge (start), keep the right edge fixed
        let edge = startTimeMs + dxMs;
        if (state.magnetEnabled) edge = snapValue(edge, cue, thresholdMs);
        newTime = edge;
        let dur = endMs - edge;
        if (newTime < 0) { newTime = 0; dur = endMs; }
        if (dur < MIN_MS) { dur = MIN_MS; newTime = endMs - MIN_MS; }
        cue.timeMs = Math.round(newTime);
        cue.durationMs = Math.round(dur);
      }

      updateCueBlockVisual(cue, block);
      if (state.selectedCueId === cue.id) updateCueInspectorFields(cue);
      renderStageLightingAtCurrentTime();
    };

    const onUp = () => {
      window.removeEventListener('pointermove', onMove);
      window.removeEventListener('pointerup', onUp);
      document.body.classList.remove('resizing');
      state.cues.sort((a, b) => a.timeMs - b.timeMs);
      renderCueMarkers();
      if (state.selectedCueId === cue.id) selectCue(cue.id);
    };

    window.addEventListener('pointermove', onMove);
    window.addEventListener('pointerup', onUp);
  });

  // Prevent the block's click handler from firing when interacting with a handle
  handle.addEventListener('click', (e) => e.stopPropagation());
}

// Stepping helper: moves playhead left or right by the selected unit (10ms, 25ms, 50ms, 100ms)
function stepTimeline(direction) {
  const unitSelect = document.getElementById('stepUnitSelect');
  const stepMs = unitSelect ? parseInt(unitSelect.value) || 25 : 25;
  const newTime = Math.max(0, Math.min(state.totalDurationMs, state.currentTimeMs + direction * stepMs));
  seekToTime(newTime);
}

// ================= PLAYBACK CONTROLS =================
function togglePlayPause() {
  if (state.isPlaying) {
    // Audacity-style: when paused, return to where playback was originally placed
    pauseAndReturnToStart();
  } else {
    startPlayback();
  }
}

// Pause and scrub back to the position where playback was started.
function pauseAndReturnToStart() {
  const returnMs = (state.playbackStartMs != null) ? state.playbackStartMs : state.currentTimeMs;
  pausePlayback();
  seekToTime(returnMs);
}

function startPlayback() {
  // Remember the position playback began, so pausing returns here
  state.playbackStartMs = state.currentTimeMs;
  state.isPlaying = true;
  document.querySelector('.icon-play').style.display = 'none';
  document.querySelector('.icon-pause').style.display = 'block';

  // Fallback clock keeps the rAF loop valid even while audio autoplay is resolving
  state.playbackStartTime = performance.now() - state.currentTimeMs;

  if (state.audioLoaded && state.audioElement.src) {
    // Start HTML5 audio (its currentTime is read by the rAF loop as the time source)
    state.audioElement.currentTime = state.currentTimeMs / 1000;
    state.audioElement.play().catch(e => console.warn(e));
  }
  requestAnimationFrame(playbackAnimationLoop);
}

function pausePlayback() {
  state.isPlaying = false;
  document.querySelector('.icon-play').style.display = 'block';
  document.querySelector('.icon-pause').style.display = 'none';

  if (state.audioElement) {
    state.audioElement.pause();
  }
}

// Unified per-frame playback driver. Reads the authoritative clock
// (HTML5 audio currentTime, or performance.now fallback) and only re-renders
// when the millisecond timestamp actually advances, keeping the stage preview smooth.
let lastPlaybackRenderMs = -1;
function playbackAnimationLoop() {
  if (!state.isPlaying) return;

  let newTimeMs;
  if (state.audioLoaded && state.audioElement && !state.audioElement.paused) {
    newTimeMs = Math.floor(state.audioElement.currentTime * 1000);
  } else {
    newTimeMs = Math.floor(performance.now() - state.playbackStartTime);
  }

  if (Number.isFinite(newTimeMs) && newTimeMs !== lastPlaybackRenderMs) {
    lastPlaybackRenderMs = newTimeMs;
    if (newTimeMs >= state.totalDurationMs) {
      stopPlayback();
      return;
    }
    state.currentTimeMs = newTimeMs;
    updatePlayheadPosition();
    renderStageLightingAtCurrentTime();
  }

  requestAnimationFrame(playbackAnimationLoop);
}

function stopPlayback() {
  pausePlayback();
  seekToTime(0);
}

function seekToTime(timeMs) {
  state.currentTimeMs = Math.max(0, Math.min(state.totalDurationMs, timeMs));
  if (state.audioElement && state.audioLoaded) {
    state.audioElement.currentTime = state.currentTimeMs / 1000;
  }
  updatePlayheadPosition();
  renderStageLightingAtCurrentTime();
}

function updatePlayheadPosition() {
  const playhead = document.getElementById('playhead');
  const x = (state.currentTimeMs / 1000) * state.pxPerSec;
  playhead.style.left = `${x}px`;

  // Update Timecode displays
  document.getElementById('timecodeCurrent').textContent = formatTime(state.currentTimeMs);
  document.getElementById('timecodeMs').textContent = `${state.currentTimeMs} ms`;
  document.getElementById('stageOverlayTime').textContent = `${formatTime(state.currentTimeMs)} (${state.currentTimeMs} ms)`;

  // Auto-scroll timeline to keep playhead in view
  const scrollWrap = document.getElementById('timelineScrollWrap');
  const scrollLeft = scrollWrap.scrollLeft;
  const wrapWidth = scrollWrap.clientWidth;

  if (x > scrollLeft + wrapWidth - 80 || x < scrollLeft) {
    scrollWrap.scrollLeft = x - wrapWidth * 0.3;
  }
}

function formatTime(ms) {
  const totalSec = Math.floor(ms / 1000);
  const m = Math.floor(totalSec / 60);
  const s = totalSec % 60;
  const milli = ms % 1000;
  return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}.${String(milli).padStart(3, '0')}`;
}

// ================= STAGE LIGHTING ENGINE =================
// Computes and renders active LED colors for all 5 dancers at currentTimeMs
// Precompute per-cue target dancer lists (expanded) whenever cues change,
// so the render loop never re-checks `includes(0)` per frame.
function indexCueTargets(cue) {
  cue._targetsAll = cue.dancers.includes(0);
  cue._sectionsIdx = cue.sections.map(sec => SECTION_INDEX[sec]).filter(i => i !== undefined);
}

// Applies a cue's lighting mutation to the dancers' current states.
// Blackout / OFF cues (r=g=b=0 or #000000) reset the state to null (unlit).
function applyCue(cue, dancerStates) {
  if (!cue._sectionsIdx) indexCueTargets(cue);
  const isOff = (cue.r === 0 && cue.g === 0 && cue.b === 0) || cue.color === '#000000';
  const color = isOff ? null : cue.color;
  const secIdx = cue._sectionsIdx;

  if (cue._targetsAll) {
    for (let d = 1; d <= 5; d++) {
      const ds = dancerStates[d];
      for (let s = 0; s < secIdx.length; s++) {
        ds[secIdx[s]] = color;
      }
    }
  } else {
    const targets = cue.dancers;
    for (let k = 0; k < targets.length; k++) {
      const ds = dancerStates[targets[k]];
      if (!ds) continue;
      for (let s = 0; s < secIdx.length; s++) {
        ds[secIdx[s]] = color;
      }
    }
  }
}

function renderStageLightingAtCurrentTime() {
  const t = state.currentTimeMs;

  // Flat color state arrays: dancerStates[d][sectionIndex], default null (unlit)
  const dancerStates = [
    null,                     // index 0 unused (dancers are 1-5)
    new Array(SECTION_KEYS.length).fill(null),
    new Array(SECTION_KEYS.length).fill(null),
    new Array(SECTION_KEYS.length).fill(null),
    new Array(SECTION_KEYS.length).fill(null),
    new Array(SECTION_KEYS.length).fill(null)
  ];

  // Iterate cues that apply at current time
  const n = state.cues.length;
  for (let ci = 0; ci < n; ci++) {
    const cue = state.cues[ci];
    if (cue.timeMs <= t && t < cue.timeMs + cue.durationMs) {
      applyCue(cue, dancerStates);
    }
  }

  // Apply to cached SVG DOM elements
  for (let d = 1; d <= 5; d++) {
    const ds = dancerStates[d];
    const elements = ledElements[d];
    for (let s = 0; s < SECTION_KEYS.length; s++) {
      const el = elements[SECTION_KEYS[s]];
      const color = ds[s];
      if (color && color !== '#000000') {
        el.style.stroke = color;
        el.style.strokeWidth = '4.5px';
        el.style.filter = `drop-shadow(0 0 6px ${color}) drop-shadow(0 0 12px ${color})`;
        el.classList.add('lit');
      } else {
        el.style.stroke = '#1e293b';
        el.style.strokeWidth = '4px';
        el.style.filter = 'none';
        el.classList.remove('lit');
      }
    }
  }
}

// Flash Test Button
document.getElementById('btnTestFlashSuit').addEventListener('click', () => {
  for (let d = 1; d <= 5; d++) {
    const elements = ledElements[d];
    for (let s = 0; s < SECTION_KEYS.length; s++) {
      const el = elements[SECTION_KEYS[s]];
      if (el) {
        el.style.stroke = '#ffffff';
        el.style.filter = 'drop-shadow(0 0 10px #ffffff)';
      }
    }
  }
  setTimeout(() => {
    renderStageLightingAtCurrentTime();
  }, 1000);
});

// Blackout All Button
document.getElementById('btnBlackoutAll').addEventListener('click', () => {
  pausePlayback();
  for (let d = 1; d <= 5; d++) {
    const elements = ledElements[d];
    for (let s = 0; s < SECTION_KEYS.length; s++) {
      const el = elements[SECTION_KEYS[s]];
      if (el) {
        el.style.stroke = '#1e293b';
        el.style.filter = 'none';
      }
    }
  }
});

// Stage Dancer Filter Toggles
document.querySelectorAll('#dancerToggles .toggle-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('#dancerToggles .toggle-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');

    const d = parseInt(btn.dataset.dancer);
    state.activeDancerFilter = d;

    // Dim non-targeted dancers
    for (let i = 1; i <= 5; i++) {
      const node = document.getElementById(`dancerNode${i}`);
      if (d === 0 || d === i) {
        node.classList.remove('dimmed');
      } else {
        node.classList.add('dimmed');
      }
    }

    renderCueMarkers();
  });
});

// ================= CUE INSPECTOR & BUILDER =================
function initInspector() {
  const timeInput = document.getElementById('cueTimeInput');
  const durInput = document.getElementById('cueDurationInput');
  const endInput = document.getElementById('cueEndTimeInput');

  function liveSyncCueInputs(changedField) {
    let t = parseInt(timeInput.value) || 0;
    let d = parseInt(durInput.value) || 250;
    let end = endInput ? (parseInt(endInput.value) || (t + d)) : (t + d);

    if (changedField === 'time') {
      end = t + d;
      if (endInput) endInput.value = end;
    } else if (changedField === 'duration') {
      d = Math.max(25, d);
      durInput.value = d;
      end = t + d;
      if (endInput) endInput.value = end;
    } else if (changedField === 'end') {
      end = Math.max(t + 25, end);
      d = end - t;
      durInput.value = d;
    }

    if (state.selectedCueId) {
      const cue = state.cues.find(c => c.id === state.selectedCueId);
      if (cue) {
        cue.timeMs = t;
        cue.durationMs = d;
        renderCueMarkers();
        renderStageLightingAtCurrentTime();
      }
    }
  }

  timeInput.addEventListener('input', () => liveSyncCueInputs('time'));
  durInput.addEventListener('input', () => liveSyncCueInputs('duration'));
  if (endInput) {
    endInput.addEventListener('input', () => liveSyncCueInputs('end'));
  }

  // Snap to playhead
  document.getElementById('btnSnapPlayhead').addEventListener('click', () => {
    timeInput.value = state.currentTimeMs;
    liveSyncCueInputs('time');
  });

  // Duration Chips
  document.querySelectorAll('.quick-chips .chip').forEach(chip => {
    chip.addEventListener('click', () => {
      document.querySelectorAll('.quick-chips .chip').forEach(c => c.classList.remove('active'));
      chip.classList.add('active');
      const dur = parseInt(chip.dataset.dur) || 1000;
      durInput.value = dur;
      liveSyncCueInputs('duration');
    });
  });

  // Dancer Multi-Select Chips
  document.querySelectorAll('#dancerSelectGrid .select-chip').forEach(chip => {
    chip.addEventListener('click', () => {
      const target = parseInt(chip.dataset.target);
      if (target === 0) {
        // Clicked ALL: deselect others
        document.querySelectorAll('#dancerSelectGrid .select-chip').forEach(c => c.classList.remove('active'));
        chip.classList.add('active');
      } else {
        // Deselect ALL when picking specific
        const allChip = document.querySelector('#dancerSelectGrid .select-chip[data-target="0"]');
        if (allChip) allChip.classList.remove('active');
        chip.classList.toggle('active');

        // If none selected, default back to ALL
        const activeCount = document.querySelectorAll('#dancerSelectGrid .select-chip.active').length;
        if (activeCount === 0 && allChip) allChip.classList.add('active');
      }
    });
  });

  // Section Chips toggle
  document.querySelectorAll('#sectionsPickerGrid .section-chip').forEach(chip => {
    chip.addEventListener('click', () => {
      chip.classList.toggle('active');
    });
  });

  // Section Presets
  document.getElementById('btnSelectFullSuit').addEventListener('click', () => {
    document.querySelectorAll('#sectionsPickerGrid .section-chip').forEach(c => c.classList.add('active'));
  });
  document.getElementById('btnSelectAllTop').addEventListener('click', () => {
    document.querySelectorAll('#sectionsPickerGrid .section-chip').forEach(c => {
      const sec = c.dataset.sec;
      c.classList.toggle('active', ['head', 'left-hand', 'right-hand', 'left-zipper', 'right-zipper'].includes(sec));
    });
  });
  document.getElementById('btnSelectAllBottom').addEventListener('click', () => {
    document.querySelectorAll('#sectionsPickerGrid .section-chip').forEach(c => {
      const sec = c.dataset.sec;
      c.classList.toggle('active', ['belt', 'left-leg', 'right-leg'].includes(sec));
    });
  });
  document.getElementById('btnClearSections').addEventListener('click', () => {
    document.querySelectorAll('#sectionsPickerGrid .section-chip').forEach(c => c.classList.remove('active'));
  });

  // Color Swatches & Native Picker
  const colorPicker = document.getElementById('cueColorPicker');
  colorPicker.addEventListener('input', (e) => setColor(e.target.value));

  document.querySelectorAll('#colorSwatches .swatch').forEach(sw => {
    sw.addEventListener('click', () => {
      setColor(sw.dataset.color);
      document.querySelectorAll('#colorSwatches .swatch').forEach(s => s.classList.remove('active'));
      sw.classList.add('active');
    });
  });

  // Save Cue Button
  document.getElementById('btnSaveCue').addEventListener('click', saveCurrentCue);
  document.getElementById('btnDuplicateCue').addEventListener('click', duplicateSelectedCue);
  document.getElementById('btnDeleteCue').addEventListener('click', deleteSelectedCue);

  // Close / deselect cue builder
  document.getElementById('btnCloseInspector').addEventListener('click', closeInspector);

  // Add Marker at Playhead Button (Toolbar): adds a real cue, then opens it for editing
  document.getElementById('btnAddMarkerAtPlayhead').addEventListener('click', () => {
    addCueAtPlayhead();
  });
}

function setColor(hex) {
  document.getElementById('cueColorPicker').value = hex;
  document.getElementById('colorPreviewDot').style.background = hex;
  document.getElementById('colorHexText').textContent = hex.toUpperCase();
}

function setInspectorTargetDancers(dancers) {
  document.querySelectorAll('#dancerSelectGrid .select-chip').forEach(c => {
    const val = parseInt(c.dataset.target);
    c.classList.toggle('active', dancers.includes(val));
  });
}

// Show / hide the cue builder panel
function setInspectorVisible(visible) {
  document.getElementById('cueInspectorPanel').classList.toggle('visible', !!visible);
}

// Deselect the current cue and hide the builder
function closeInspector() {
  state.selectedCueId = null;
  setInspectorVisible(false);
  renderCueMarkers();
}

function selectCue(cueId) {
  state.selectedCueId = cueId;
  const cue = state.cues.find(c => c.id === cueId);
  if (!cue) return;

  setInspectorVisible(true);

  document.getElementById('inspectorTitle').textContent = 'EDIT LIGHT CUE';
  document.getElementById('selectedCueBadge').textContent = `Cue ID: ${cueId.slice(-4)}`;
  document.getElementById('btnSaveCueText').textContent = 'Update Cue';
  document.getElementById('btnDeleteCue').style.display = 'inline-flex';

  document.getElementById('cueTimeInput').value = cue.timeMs;
  document.getElementById('cueDurationInput').value = cue.durationMs;
  setColor(cue.color);

  // Dancers
  setInspectorTargetDancers(cue.dancers);

  // Sections
  document.querySelectorAll('#sectionsPickerGrid .section-chip').forEach(c => {
    c.classList.toggle('active', cue.sections.includes(c.dataset.sec));
  });

  renderCueMarkers();
  seekToTime(cue.timeMs);
}

// Add a new cue at the current playhead using the current inspector settings,
// then select it so the builder opens for immediate editing.
function addCueAtPlayhead() {
  const timeMs = state.currentTimeMs;
  const durationMs = parseInt(document.getElementById('cueDurationInput').value) || 1000;
  const color = document.getElementById('cueColorPicker').value;

  // Selected Dancers (default to ALL)
  const dancers = [];
  document.querySelectorAll('#dancerSelectGrid .select-chip.active').forEach(c => {
    dancers.push(parseInt(c.dataset.target));
  });
  if (dancers.length === 0) dancers.push(0);

  // Selected Sections (default to full suit)
  const sections = [];
  document.querySelectorAll('#sectionsPickerGrid .section-chip.active').forEach(c => {
    sections.push(c.dataset.sec);
  });
  if (sections.length === 0) sections = Object.keys(SECTIONS);

  const rgb = hexToRgb(color);
  const newCue = {
    id: 'cue_' + Date.now() + '_' + Math.random().toString(36).substr(2, 4),
    timeMs,
    durationMs,
    dancers,
    sections,
    color,
    r: rgb.r,
    g: rgb.g,
    b: rgb.b
  };

  state.cues.push(newCue);
  state.cues.sort((a, b) => a.timeMs - b.timeMs);
  renderCueMarkers();
  selectCue(newCue.id);
}

function saveCurrentCue() {
  const timeMs = parseInt(document.getElementById('cueTimeInput').value) || 0;
  const durationMs = parseInt(document.getElementById('cueDurationInput').value) || 1000;
  const color = document.getElementById('cueColorPicker').value;

  // Selected Dancers
  const dancers = [];
  document.querySelectorAll('#dancerSelectGrid .select-chip.active').forEach(c => {
    dancers.push(parseInt(c.dataset.target));
  });
  if (dancers.length === 0) dancers.push(0);

  // Selected Sections
  const sections = [];
  document.querySelectorAll('#sectionsPickerGrid .section-chip.active').forEach(c => {
    sections.push(c.dataset.sec);
  });
  if (sections.length === 0) {
    alert('Please select at least one body section to illuminate.');
    return;
  }

  // Parse RGB
  const rgb = hexToRgb(color);

  if (state.selectedCueId) {
    // Update existing
    const idx = state.cues.findIndex(c => c.id === state.selectedCueId);
    if (idx !== -1) {
      state.cues[idx] = {
        ...state.cues[idx],
        timeMs,
        durationMs,
        importedFromCpp: durationMs === 0 ? state.cues[idx].importedFromCpp : false,
        dancers,
        sections,
        color,
        r: rgb.r,
        g: rgb.g,
        b: rgb.b
      };
    }
  } else {
    // Add new cue
    const newCue = {
      id: 'cue_' + Date.now() + '_' + Math.random().toString(36).substr(2, 4),
      timeMs,
      durationMs,
      dancers,
      sections,
      color,
      r: rgb.r,
      g: rgb.g,
      b: rgb.b
    };
    state.cues.push(newCue);
  }

  // Sort cues by timestamp
  state.cues.sort((a, b) => a.timeMs - b.timeMs);

  renderCueMarkers();
  renderStageLightingAtCurrentTime();
}

function duplicateSelectedCue() {
  if (!state.selectedCueId) return;
  const cue = state.cues.find(c => c.id === state.selectedCueId);
  if (!cue) return;

  const clone = {
    ...cue,
    id: 'cue_' + Date.now() + '_' + Math.random().toString(36).substr(2, 4),
    timeMs: cue.timeMs + 500 // Offset slightly
  };
  state.cues.push(clone);
  state.cues.sort((a, b) => a.timeMs - b.timeMs);

  renderCueMarkers();
  selectCue(clone.id);
}

function deleteSelectedCue() {
  if (!state.selectedCueId) return;
  state.cues = state.cues.filter(c => c.id !== state.selectedCueId);
  state.selectedCueId = null;
  setInspectorVisible(false);
  renderCueMarkers();
  renderStageLightingAtCurrentTime();
}

function hexToRgb(hex) {
  const cleanHex = hex.replace('#', '');
  const bigint = parseInt(cleanHex, 16);
  return {
    r: (bigint >> 16) & 255,
    g: (bigint >> 8) & 255,
    b: bigint & 255
  };
}

// ================= EXPORT & IMPORT =================
function initModals() {
  // Export Modal
  document.getElementById('btnExportModal').addEventListener('click', openExportModal);
  document.getElementById('btnCloseExportModal').addEventListener('click', () => {
    document.getElementById('exportModal').style.display = 'none';
  });

  // Import Modal
  document.getElementById('btnImportModal').addEventListener('click', openImportModal);
  document.getElementById('btnCloseImportModal').addEventListener('click', () => {
    document.getElementById('importModal').style.display = 'none';
  });

  // Backdrop click to close modals
  document.getElementById('exportModal').addEventListener('click', (e) => {
    if (e.target.id === 'exportModal') document.getElementById('exportModal').style.display = 'none';
  });
  document.getElementById('importModal').addEventListener('click', (e) => {
    if (e.target.id === 'importModal') document.getElementById('importModal').style.display = 'none';
  });

  // Escape key closes modals
  window.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
      document.getElementById('exportModal').style.display = 'none';
      document.getElementById('importModal').style.display = 'none';
    }
  });

  // Modal Tabs Switching
  document.querySelectorAll('.modal-tabs .tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const modal = btn.closest('.modal-card');
      modal.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      modal.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));

      btn.classList.add('active');
      const targetContent = modal.querySelector(`#${btn.dataset.tab}`);
      if (targetContent) targetContent.classList.add('active');
    });
  });

  // Copy C++
  document.getElementById('btnCopyCpp').addEventListener('click', () => {
    const text = document.getElementById('cppOutputBox').value;
    navigator.clipboard.writeText(text).then(() => alert('C++ Choreography copied to clipboard!'));
  });

  // Download C++ .ino snippet
  document.getElementById('btnDownloadCppSnippet').addEventListener('click', () => {
    const text = document.getElementById('cppOutputBox').value;
    downloadFile('choreography_timeline.ino', text, 'text/plain');
  });

  // Copy JSON
  document.getElementById('btnCopyJson').addEventListener('click', () => {
    const text = document.getElementById('jsonOutputBox').value;
    navigator.clipboard.writeText(text).then(() => alert('JSON Project copied to clipboard!'));
  });

  // Download JSON
  document.getElementById('btnDownloadJson').addEventListener('click', () => {
    const text = document.getElementById('jsonOutputBox').value;
    downloadFile('light_dance_project.json', text, 'application/json');
  });

  // Confirm Import JSON
  document.getElementById('btnConfirmImportJson').addEventListener('click', () => {
    const raw = document.getElementById('jsonInputBox').value.trim();
    if (!raw) return;
    try {
      const data = JSON.parse(raw);
      if (Array.isArray(data.cues)) {
        state.cues = data.cues.map(c => {
          const color = c.color || (c.r !== undefined ? rgbToHex(c.r, c.g, c.b) : '#00ffff');
          const rgb = hexToRgb(color);
          return {
            id: c.id || ('cue_' + Date.now() + '_' + Math.random().toString(36).substr(2, 4)),
            timeMs: parseInt(c.timeMs, 10) || 0,
            durationMs: parseInt(c.durationMs, 10) || 1000,
            dancers: Array.isArray(c.dancers) ? c.dancers : (c.dancer !== undefined ? [c.dancer] : [0]),
            sections: Array.isArray(c.sections) ? c.sections : Object.keys(SECTIONS),
            color: color,
            r: rgb.r,
            g: rgb.g,
            b: rgb.b,
            preferredMacro: c.preferredMacro || undefined
          };
        });

        if (data.totalDurationMs) {
          state.totalDurationMs = data.totalDurationMs;
        } else {
          const maxCueTime = state.cues.reduce((max, c) => Math.max(max, c.timeMs + c.durationMs), 0);
          if (maxCueTime > state.totalDurationMs) {
            state.totalDurationMs = Math.ceil((maxCueTime + 3000) / 1000) * 1000;
          }
        }

        if (data.audioFileName) {
          state.audioFileName = data.audioFileName;
          document.getElementById('trackNameDisplay').textContent = data.audioFileName;
        }

        document.getElementById('trackDurationDisplay').textContent = formatTime(state.totalDurationMs);

        state.cues.sort((a, b) => a.timeMs - b.timeMs);
        state.selectedCueId = null;
        setInspectorVisible(false);

        updateTimelineWidth();
        renderRuler();
        renderWaveform();
        renderCueMarkers();
        renderStageLightingAtCurrentTime();
        seekToTime(0);

        document.getElementById('importModal').style.display = 'none';
        alert(`Successfully imported ${state.cues.length} cues!`);
      } else {
        alert('Invalid JSON structure: missing cues array.');
      }
    } catch (err) {
      alert('Error parsing JSON: ' + err.message);
    }
  });

  // JSON File upload input
  document.getElementById('jsonFileInput').addEventListener('change', (e) => {
    if (e.target.files && e.target.files.length > 0) {
      const reader = new FileReader();
      reader.onload = (ev) => {
        document.getElementById('jsonInputBox').value = ev.target.result;
      };
      reader.readAsText(e.target.files[0]);
    }
  });

  // Confirm Import C++
  document.getElementById('btnConfirmImportCpp').addEventListener('click', parseAndImportCppCode);
}

function openExportModal() {
  document.getElementById('exportModal').style.display = 'flex';
  
  // Generate Arduino C++ code
  document.getElementById('cppOutputBox').value = generateArduinoCppCode();

  // Generate JSON project
  const projectData = {
    project: 'Light Dance Choreography',
    version: '1.0',
    audioFileName: state.audioFileName,
    totalDurationMs: state.totalDurationMs,
    dancersCount: 5,
    cues: state.cues
  };
  document.getElementById('jsonOutputBox').value = JSON.stringify(projectData, null, 2);
}

function openImportModal() {
  document.getElementById('importModal').style.display = 'flex';
}

function generateArduinoCppCode() {
  // Sort cues chronologically
  const packets = [];

  state.cues.forEach((cue, cueIdx) => {
    const dancers = cue.dancers.includes(0) ? [0] : cue.dancers;
    const isFullSuit = (cue.sections.length === 8);
    const isTopOnly = (cue.sections.length === 5 && !cue.sections.includes('belt') && !cue.sections.includes('left-leg') && !cue.sections.includes('right-leg'));
    const isBottomOnly = (cue.sections.length === 3 && cue.sections.includes('belt') && cue.sections.includes('left-leg') && cue.sections.includes('right-leg'));
    const isBothLegs = (cue.preferredMacro === 'CUE_BOTH_LEGS' || (cue.sections.length === 2 && cue.sections.includes('left-leg') && cue.sections.includes('right-leg') && cue.preferredMacro !== 'individual'));
    const isBlackoutCue = (cue.r === 0 && cue.g === 0 && cue.b === 0) || cue.color === '#000000';

    dancers.forEach(dancer => {
      const dancerMacro = dancer === 0 ? 'SLAVE_ALL' : `SLAVE_${dancer}`;

      // 1. Emit packet at cue.timeMs
      if (isFullSuit) {
        packets.push({
          timeMs: cue.timeMs,
          isOff: isBlackoutCue,
          line: `    CUE_FULL_SUIT(${cue.timeMs}, ${dancerMacro}, ${cue.r}, ${cue.g}, ${cue.b}), // Cue #${cueIdx+1}: Full Suit${isBlackoutCue ? ' OFF' : ''}`,
        });
      } else if (isTopOnly) {
        packets.push({
          timeMs: cue.timeMs,
          isOff: isBlackoutCue,
          line: `    CUE_UPPER_BODY(${cue.timeMs}, ${dancerMacro}, ${cue.r}, ${cue.g}, ${cue.b}), // Cue #${cueIdx+1}: Top Body${isBlackoutCue ? ' OFF' : ''}`,
        });
      } else if (isBottomOnly) {
        packets.push({
          timeMs: cue.timeMs,
          isOff: isBlackoutCue,
          line: `    CUE_LOWER_BODY(${cue.timeMs}, ${dancerMacro}, ${cue.r}, ${cue.g}, ${cue.b}), // Cue #${cueIdx+1}: Bottom Body${isBlackoutCue ? ' OFF' : ''}`,
        });
      } else if (isBothLegs) {
        packets.push({
          timeMs: cue.timeMs,
          isOff: isBlackoutCue,
          line: `    CUE_BOTH_LEGS(${cue.timeMs}, ${dancerMacro}, ${cue.r}, ${cue.g}, ${cue.b}), // Cue #${cueIdx+1}: Both Legs${isBlackoutCue ? ' OFF' : ''}`,
        });
      } else {
        cue.sections.forEach(secKey => {
          const sec = SECTIONS[secKey];
          if (sec) {
            packets.push({
              timeMs: cue.timeMs,
              isOff: isBlackoutCue,
              line: `    ${sec.macro}(${cue.timeMs}, ${dancerMacro}, ${cue.r}, ${cue.g}, ${cue.b}), // Cue #${cueIdx+1}: ${sec.name}${isBlackoutCue ? ' OFF' : ''}`,
            });
          }
        });
      }

      // 2. Turn-OFF packets (blackout when duration expires)
      if (cue.durationMs > 0 && !isBlackoutCue) {
        const offTime = cue.timeMs + cue.durationMs;
        if (isFullSuit) {
          packets.push({
            timeMs: offTime,
            isOff: true,
            line: `    CUE_FULL_SUIT(${offTime}, ${dancerMacro}, 0, 0, 0), // Cue #${cueIdx+1} OFF`,
          });
        } else if (isTopOnly) {
          packets.push({
            timeMs: offTime,
            isOff: true,
            line: `    CUE_UPPER_BODY(${offTime}, ${dancerMacro}, 0, 0, 0), // Cue #${cueIdx+1} Top OFF`,
          });
        } else if (isBottomOnly) {
          packets.push({
            timeMs: offTime,
            isOff: true,
            line: `    CUE_LOWER_BODY(${offTime}, ${dancerMacro}, 0, 0, 0), // Cue #${cueIdx+1} Bottom OFF`,
          });
        } else if (isBothLegs) {
          packets.push({
            timeMs: offTime,
            isOff: true,
            line: `    CUE_BOTH_LEGS(${offTime}, ${dancerMacro}, 0, 0, 0), // Cue #${cueIdx+1} Both Legs OFF`,
          });
        } else {
          cue.sections.forEach(secKey => {
            const sec = SECTIONS[secKey];
            if (sec) {
              packets.push({
                timeMs: offTime,
                isOff: true,
                line: `    ${sec.macro}(${offTime}, ${dancerMacro}, 0, 0, 0), // Cue #${cueIdx+1} ${sec.name} OFF`,
              });
            }
          });
        }
      }
    });
  });

  // Sort chronologically by timeMs, placing OFF packets before ON packets if timestamps are identical
  packets.sort((a, b) => {
    if (a.timeMs !== b.timeMs) return a.timeMs - b.timeMs;
    if (a.isOff !== b.isOff) return a.isOff ? -1 : 1;
    return 0;
  });

  let out = `// ================= CHOREOGRAPHY TIMELINE GENERATED BY LIGHT DANCE STUDIO =================\n`;
  out += `// Total Steps: ${packets.length} | Generated on: ${new Date().toLocaleString()}\n`;
  out += `Packet choreography[] = {\n\n`;
  packets.forEach(p => {
    out += p.line + '\n';
  });
  out += `};\n\n`;
  out += `const int TOTAL_STEPS = sizeof(choreography) / sizeof(Packet);\n`;
  return out;
}

// Helpers for Parsing C++ Choreography Code
function resolveDancerFromStr(str) {
  const s = String(str).trim().toUpperCase();
  if (s === '0' || s === 'SLAVE_ALL' || s === 'ALL' || s === 'SLAVE_0') return 0;
  const m = s.match(/[1-5]/);
  return m ? parseInt(m[0], 10) : 0;
}

function resolveSectionsFromMacro(macroName) {
  const m = macroName.toUpperCase();
  if (m === 'CUE_FULL_SUIT' || m === 'CUE_ALL') {
    return { sections: Object.keys(SECTIONS), preferredMacro: 'CUE_FULL_SUIT' };
  }
  if (m === 'CUE_UPPER_BODY' || m === 'CUE_TOP_BODY' || m === 'CUE_UPPER') {
    return { sections: ['head', 'left-hand', 'right-hand', 'left-zipper', 'right-zipper'], preferredMacro: 'CUE_UPPER_BODY' };
  }
  if (m === 'CUE_LOWER_BODY' || m === 'CUE_BOTTOM_BODY' || m === 'CUE_LOWER') {
    return { sections: ['belt', 'left-leg', 'right-leg'], preferredMacro: 'CUE_LOWER_BODY' };
  }
  if (m === 'CUE_BOTH_LEGS') {
    return { sections: ['left-leg', 'right-leg'], preferredMacro: 'CUE_BOTH_LEGS' };
  }
  if (m === 'CUE_HEAD' || m === 'CUE_HOOD') {
    return { sections: ['head'], preferredMacro: undefined };
  }
  if (m === 'CUE_LEFT_HAND' || m === 'CUE_LEFT_ARM') {
    return { sections: ['left-hand'], preferredMacro: undefined };
  }
  if (m === 'CUE_RIGHT_HAND' || m === 'CUE_RIGHT_ARM') {
    return { sections: ['right-hand'], preferredMacro: undefined };
  }
  if (m === 'CUE_BOTH_HANDS' || m === 'CUE_BOTH_ARMS') {
    return { sections: ['left-hand', 'right-hand'], preferredMacro: undefined };
  }
  if (m === 'CUE_LEFT_ZIPPER') {
    return { sections: ['left-zipper'], preferredMacro: undefined };
  }
  if (m === 'CUE_RIGHT_ZIPPER') {
    return { sections: ['right-zipper'], preferredMacro: undefined };
  }
  if (m === 'CUE_BOTH_ZIPPERS') {
    return { sections: ['left-zipper', 'right-zipper'], preferredMacro: undefined };
  }
  if (m === 'CUE_LEFT_LEG') {
    return { sections: ['left-leg'], preferredMacro: undefined };
  }
  if (m === 'CUE_RIGHT_LEG') {
    return { sections: ['right-leg'], preferredMacro: undefined };
  }
  if (m === 'CUE_BELT') {
    return { sections: ['belt'], preferredMacro: undefined };
  }

  // Check matching section in SECTIONS
  for (const [key, val] of Object.entries(SECTIONS)) {
    if (val.macro === m) {
      return { sections: [key], preferredMacro: undefined };
    }
  }

  return { sections: Object.keys(SECTIONS), preferredMacro: undefined };
}

function resolveSectionsFromStruct(stripStr, startLedStr, endLedStr) {
  const stripUp = String(stripStr).trim().toUpperCase();
  const stripId = (stripUp.includes('BOTH') || stripUp === '2') ? 2 : ((stripUp.includes('LOWER') || stripUp === '1') ? 1 : 0);

  if (stripId === 2) {
    return { sections: Object.keys(SECTIONS), preferredMacro: 'CUE_FULL_SUIT' };
  }

  const startDigits = String(startLedStr).replace(/\D/g, '');
  const endDigits = String(endLedStr).replace(/\D/g, '');
  const startNum = startDigits !== '' ? parseInt(startDigits, 10) : 0;
  const endNum = endDigits !== '' ? parseInt(endDigits, 10) : 299;

  if (stripId === 0) {
    // Upper strip (0-299)
    if (startNum <= 0 && endNum >= 290) {
      return { sections: ['head', 'left-hand', 'right-hand', 'left-zipper', 'right-zipper'], preferredMacro: 'CUE_UPPER_BODY' };
    }
    if (startNum === 0 && endNum <= 35) return { sections: ['left-zipper'], preferredMacro: undefined };
    if (startNum >= 30 && endNum <= 105) return { sections: ['left-hand'], preferredMacro: undefined };
    if (startNum >= 100 && endNum <= 155) return { sections: ['head'], preferredMacro: undefined };
    if (startNum >= 150 && endNum <= 225) return { sections: ['right-hand'], preferredMacro: undefined };
    if (startNum >= 225 && endNum >= 290) return { sections: ['right-zipper'], preferredMacro: undefined };
    return { sections: ['head', 'left-hand', 'right-hand', 'left-zipper', 'right-zipper'], preferredMacro: 'CUE_UPPER_BODY' };
  } else {
    // Lower strip (0-299)
    if (startNum <= 0 && endNum >= 290) {
      return { sections: ['belt', 'left-leg', 'right-leg'], preferredMacro: 'CUE_LOWER_BODY' };
    }
    if (startNum === 0 && endNum <= 115) return { sections: ['left-leg'], preferredMacro: undefined };
    if (startNum >= 110 && endNum <= 224) return { sections: ['right-leg'], preferredMacro: undefined };
    if (startNum <= 5 && endNum >= 220 && endNum <= 230) {
      return { sections: ['left-leg', 'right-leg'], preferredMacro: 'CUE_BOTH_LEGS' };
    }
    if (startNum >= 220 && endNum >= 290) return { sections: ['belt'], preferredMacro: undefined };
    return { sections: ['belt', 'left-leg', 'right-leg'], preferredMacro: 'CUE_LOWER_BODY' };
  }
}

function parseAndImportCppCode() {
  const code = document.getElementById('cppInputBox').value;
  if (!code.trim()) return;

  const lines = code.split('\n');
  const rawPackets = [];

  const macroRegex = /(CUE_[A-Z0-9_]+)\s*\(\s*(\d+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)/i;
  const structRegex = /\{\s*(\d+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}/i;

  for (let i = 0; i < lines.length; i++) {
    const rawLine = lines[i];
    const stripped = rawLine.trim();
    if (!stripped || stripped.startsWith('#')) continue;

    let comment = '';
    const commentIdx = rawLine.indexOf('//');
    if (commentIdx !== -1) {
      comment = rawLine.substring(commentIdx + 2).trim();
    }

    const cueMatch = comment.match(/Cue\s*#?\s*(\d+)/i);
    const cueNum = cueMatch ? parseInt(cueMatch[1], 10) : null;
    const isOffComment = /\b(OFF|off|blackout)\b/i.test(comment);

    const m = stripped.match(macroRegex);
    if (m) {
      const macroName = m[1].toUpperCase();
      const timeMs = parseInt(m[2], 10);
      const dancer = resolveDancerFromStr(m[3]);
      const r = parseInt(m[4], 10);
      const g = parseInt(m[5], 10);
      const b = parseInt(m[6], 10);
      const { sections, preferredMacro } = resolveSectionsFromMacro(macroName);
      const isOff = (r === 0 && g === 0 && b === 0);

      rawPackets.push({
        timeMs,
        dancer,
        sections,
        r, g, b,
        isOff,
        cueNum,
        isOffComment,
        preferredMacro,
        macroName
      });
      continue;
    }

    const s = stripped.match(structRegex);
    if (s) {
      const timeMs = parseInt(s[1], 10);
      const dancer = resolveDancerFromStr(s[2]);
      const stripStr = s[3];
      const startLedStr = s[4];
      const endLedStr = s[5];
      const r = parseInt(s[6], 10);
      const g = parseInt(s[7], 10);
      const b = parseInt(s[8], 10);
      const { sections, preferredMacro } = resolveSectionsFromStruct(stripStr, startLedStr, endLedStr);
      const isOff = (r === 0 && g === 0 && b === 0);

      rawPackets.push({
        timeMs,
        dancer,
        sections,
        r, g, b,
        isOff,
        cueNum,
        isOffComment,
        preferredMacro,
        macroName: null
      });
    }
  }

  if (rawPackets.length === 0) {
    alert('Could not find valid choreography packets or CUE_... macros in the provided C++ code.');
    return;
  }

  // Group / pair packets to calculate the true active duration for full-length boxes
  const hasCueNums = rawPackets.some(p => p.cueNum !== null && !p.isOff);
  let importedCues = [];

  if (hasCueNums) {
    const grouped = new Map();
    rawPackets.forEach(p => {
      const key = p.cueNum !== null ? `cue_${p.cueNum}` : `time_${p.timeMs}_${p.dancer}_${p.r}_${p.g}_${p.b}`;
      if (!grouped.has(key)) {
        grouped.set(key, { on: [], off: [] });
      }
      if (p.isOff) {
        grouped.get(key).off.push(p);
      } else {
        grouped.get(key).on.push(p);
      }
    });

    grouped.forEach(grp => {
      const onList = grp.on;
      if (onList.length === 0) return;
      const offList = grp.off;

      const startMs = Math.min(...onList.map(p => p.timeMs));
      const firstOn = onList[0];
      const dancerSet = new Set(onList.map(p => p.dancer));
      const dancers = dancerSet.has(0) ? [0] : Array.from(dancerSet).sort((a, b) => a - b);
      const sectionSet = new Set();
      onList.forEach(p => p.sections.forEach(s => sectionSet.add(s)));
      const sections = Array.from(sectionSet);

      let durationMs = 1000;
      if (offList.length > 0) {
        const offMs = Math.max(...offList.map(p => p.timeMs));
        durationMs = Math.max(50, offMs - startMs);
      }

      importedCues.push({
        id: 'cue_' + Date.now() + '_' + Math.random().toString(36).substr(2, 6),
        timeMs: startMs,
        durationMs,
        dancers,
        sections,
        color: rgbToHex(firstOn.r, firstOn.g, firstOn.b),
        r: firstOn.r,
        g: firstOn.g,
        b: firstOn.b,
        preferredMacro: firstOn.preferredMacro
      });
    });
  } else {
    // Unlabelled C++ code: pair ON packets with OFF packets or next state mutations chronologically
    rawPackets.sort((a, b) => {
      if (a.timeMs !== b.timeMs) return a.timeMs - b.timeMs;
      if (a.isOff !== b.isOff) return a.isOff ? -1 : 1;
      return 0;
    });

    const activeOns = new Map(); // key `${dancer}_${section}` -> onPacket
    const onCues = [];

    rawPackets.forEach(p => {
      const dancerList = p.dancer === 0 ? [1, 2, 3, 4, 5] : [p.dancer];

      if (!p.isOff) {
        // ON packet
        dancerList.forEach(d => {
          p.sections.forEach(s => {
            const key = `${d}_${s}`;
            if (activeOns.has(key)) {
              const prev = activeOns.get(key);
              if (!prev.endMs) prev.endMs = p.timeMs;
              activeOns.delete(key);
            }
          });
        });

        p.endMs = null;
        onCues.push(p);

        dancerList.forEach(d => {
          p.sections.forEach(s => {
            activeOns.set(`${d}_${s}`, p);
          });
        });
      } else {
        // OFF packet
        dancerList.forEach(d => {
          p.sections.forEach(s => {
            const key = `${d}_${s}`;
            if (activeOns.has(key)) {
              const prev = activeOns.get(key);
              if (!prev.endMs) prev.endMs = p.timeMs;
              activeOns.delete(key);
            }
          });
        });
      }
    });

    // Close any unclosed cues
    onCues.forEach((cue, idx) => {
      if (!cue.endMs) {
        let nextTime = null;
        for (let j = idx + 1; j < onCues.length; j++) {
          if (onCues[j].timeMs > cue.timeMs) {
            nextTime = onCues[j].timeMs;
            break;
          }
        }
        cue.endMs = nextTime !== null ? nextTime : cue.timeMs + 1000;
      }
      cue.durationMs = Math.max(50, cue.endMs - cue.timeMs);
    });

    // Merge concurrent identical onCues (e.g. CUE_LEFT_LEG and CUE_RIGHT_LEG at same timeMs and duration)
    const merged = [];
    onCues.forEach(onP => {
      let found = false;
      for (let i = 0; i < merged.length; i++) {
        const m = merged[i];
        if (m.timeMs === onP.timeMs && m.durationMs === onP.durationMs && m.r === onP.r && m.g === onP.g && m.b === onP.b) {
          if (m.dancers.length === 1 && m.dancers[0] === onP.dancer) {
            onP.sections.forEach(s => {
              if (!m.sections.includes(s)) m.sections.push(s);
            });
            if (m.sections.includes('left-leg') && m.sections.includes('right-leg')) {
              if (m.sections.includes('belt')) {
                m.preferredMacro = 'CUE_LOWER_BODY';
              } else if (m.sections.length === 2) {
                m.preferredMacro = 'CUE_BOTH_LEGS';
              }
            }
            found = true;
            break;
          }
          const sameSecs = (m.sections.length === onP.sections.length && m.sections.every(s => onP.sections.includes(s)));
          if (sameSecs) {
            if (!m.dancers.includes(onP.dancer)) m.dancers.push(onP.dancer);
            if (m.dancers.includes(0)) m.dancers = [0];
            found = true;
            break;
          }
        }
      }

      if (!found) {
        merged.push({
          timeMs: onP.timeMs,
          durationMs: onP.durationMs,
          dancers: [onP.dancer],
          sections: [...onP.sections],
          color: rgbToHex(onP.r, onP.g, onP.b),
          r: onP.r,
          g: onP.g,
          b: onP.b,
          preferredMacro: onP.preferredMacro
        });
      }
    });

    importedCues = merged.map((m, idx) => ({
      id: 'cue_' + Date.now() + '_' + Math.random().toString(36).substr(2, 6) + '_' + idx,
      timeMs: m.timeMs,
      durationMs: m.durationMs,
      dancers: m.dancers,
      sections: m.sections,
      color: m.color,
      r: m.r,
      g: m.g,
      b: m.b,
      preferredMacro: m.preferredMacro
    }));
  }

  if (importedCues.length > 0) {
    importedCues.sort((a, b) => a.timeMs - b.timeMs);
    state.cues = importedCues;

    // Check if timeline duration needs expanding
    const maxCueEnd = state.cues.reduce((max, c) => Math.max(max, c.timeMs + c.durationMs), 0);
    if (maxCueEnd > state.totalDurationMs) {
      state.totalDurationMs = Math.ceil((maxCueEnd + 3000) / 1000) * 1000;
    }
    document.getElementById('trackDurationDisplay').textContent = formatTime(state.totalDurationMs);

    state.selectedCueId = null;
    setInspectorVisible(false);

    updateTimelineWidth();
    renderRuler();
    renderWaveform();
    renderCueMarkers();
    renderStageLightingAtCurrentTime();
    seekToTime(0);

    document.getElementById('importModal').style.display = 'none';
    alert(`Successfully parsed and imported ${importedCues.length} full-length lighting cues from C++!`);
  } else {
    alert('No valid lighting cues could be extracted from the provided C++ code.');
  }
}

function rgbToHex(r, g, b) {
  return '#' + [r, g, b].map(x => {
    const hex = x.toString(16);
    return hex.length === 1 ? '0' + hex : hex;
  }).join('');
}

function downloadFile(filename, content, type) {
  const blob = new Blob([content], { type });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
}

// ================= KEYBOARD SHORTCUTS =================
function initKeyboardShortcuts() {
  window.addEventListener('keydown', (e) => {
    // Avoid shortcuts if typing in text inputs or textareas
    if (['INPUT', 'TEXTAREA', 'SELECT'].includes(e.target.tagName)) return;

    if (e.code === 'Space') {
      e.preventDefault();
      togglePlayPause();
    } else if (e.code === 'ArrowLeft') {
      e.preventDefault();
      stepTimeline(e.shiftKey ? -4 : -1);
    } else if (e.code === 'ArrowRight') {
      e.preventDefault();
      stepTimeline(e.shiftKey ? 4 : 1);
    } else if (e.code === 'KeyM') {
      e.preventDefault();
      addCueAtPlayhead();
    } else if (e.code === 'Delete' || e.code === 'Backspace') {
      if (state.selectedCueId) {
        deleteSelectedCue();
      }
    } else if (e.code === 'Home') {
      seekToTime(0);
    }
  });
}

// ================= DEFAULT CHOREOGRAPHY ROUTINE =================
// Pre-loads the user's choreography routine with layer stacking
function loadDefaultRoutine() {
  state.cues = [
    // User's latest choreography cues
    { id: 'c1', timeMs: 1437, durationMs: 500, dancers: [1], sections: ['belt', 'left-leg', 'right-leg'], color: '#00ff66', r: 0, g: 255, b: 102 },
    { id: 'c2', timeMs: 1937, durationMs: 250, dancers: [1], sections: ['right-leg'], color: '#ff00bb', r: 255, g: 0, b: 187 },
    { id: 'c3', timeMs: 2175, durationMs: 250, dancers: [1], sections: ['left-leg'], color: '#9900ff', r: 153, g: 0, b: 255 },
    { id: 'c4', timeMs: 2412, durationMs: 250, dancers: [1], sections: ['belt'], color: '#0066ff', r: 0, g: 102, b: 255 },
    { id: 'c5', timeMs: 2687, durationMs: 500, dancers: [1], sections: ['left-leg', 'right-leg'], color: '#ffee00', r: 255, g: 238, b: 0 },
    { id: 'c6', timeMs: 2962, durationMs: 500, dancers: [1], sections: ['belt'], color: '#ff0044', r: 255, g: 0, b: 68 },
    { id: 'c7', timeMs: 3337, durationMs: 500, dancers: [1], sections: ['belt', 'left-leg'], color: '#00ff66', r: 0, g: 255, b: 102 },
    { id: 'c8', timeMs: 3612, durationMs: 500, dancers: [1], sections: ['belt', 'left-leg', 'right-leg'], color: '#00ff66', r: 0, g: 255, b: 102 },

    // Synchronized Troupe Cues (All 5 Dancers)
    { id: 'c9',  timeMs: 4500, durationMs: 1500, dancers: [0], sections: ['head', 'left-hand', 'right-hand'], color: '#00ffff', r: 0, g: 255, b: 255 },
    { id: 'c10', timeMs: 6000, durationMs: 1500, dancers: [0], sections: ['left-zipper', 'right-zipper'], color: '#00f3ff', r: 0, g: 243, b: 255 },
    { id: 'c11', timeMs: 7500, durationMs: 2000, dancers: [0], sections: Object.keys(SECTIONS), color: '#ff0044', r: 255, g: 0, b: 68 },
    { id: 'c12', timeMs: 9500, durationMs: 2000, dancers: [0], sections: Object.keys(SECTIONS), color: '#00ff88', r: 0, g: 255, b: 136 },
    { id: 'c13', timeMs: 12000, durationMs: 2500, dancers: [0], sections: Object.keys(SECTIONS), color: '#ffffff', r: 255, g: 255, b: 255 }
  ];

  renderCueMarkers();
  renderStageLightingAtCurrentTime();
}

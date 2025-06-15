import React, { useState, useEffect, useRef, useCallback } from 'react';
import './App.css';
import { FaPlay, FaPause, FaStepBackward, FaStepForward } from 'react-icons/fa';

// --- MAIN APP COMPONENT (FINAL UNIVERSAL VERSION) ---
function App() {
  // --- State Management & Hooks ---
  const [rawInput, setRawInput] = useState('');
  const [wasmModule, setWasmModule] = useState(null);
  const visualizeMyLogicRef = useRef(null);
  const [history, setHistory] = useState([]);
  const [currentFrameIndex, setCurrentFrameIndex] = useState(0);
  const [isPlaying, setIsPlaying] = useState(false);
  const [isGenerating, setIsGenerating] = useState(false);

  useEffect(() => {
    if (window.createAlgoModule) {
      window.createAlgoModule().then(Module => {
        setWasmModule(Module);
        visualizeMyLogicRef.current = Module.visualizeMyLogic;
      });
    }
  }, []);

  const handleVisualize = useCallback(() => {
    const funcToRun = visualizeMyLogicRef.current;
    if (!funcToRun || !wasmModule) return;

    setIsGenerating(true); setIsPlaying(false);
    setTimeout(() => {
      // --- The only thing React does is pass the raw string ---
      let historyJson = '';
      try {
        historyJson = funcToRun(rawInput);
      } catch (e) {
        console.error("Error from C++ execution:", e);
        alert("An error occurred in the C++ code. Check the console.");
      }

      if (historyJson) {
        const parsedHistory = JSON.parse(historyJson);
        setHistory(parsedHistory);
        setCurrentFrameIndex(0);
      }
      setIsGenerating(false);
    }, 50);
  }, [rawInput, wasmModule]);

  useEffect(() => {
    if (isPlaying && history.length > 0) {
      const timer = setTimeout(() => {
        setCurrentFrameIndex(prev => Math.min(prev + 1, history.length - 1));
        if (currentFrameIndex === history.length - 1) setIsPlaying(false);
      }, 400);
      return () => clearTimeout(timer);
    }
  }, [isPlaying, currentFrameIndex, history]);

  const currentFrame = history[currentFrameIndex] || { objects: {}, message: "Write your C++ logic and click Visualize!" };
  const objectEntries = Object.entries(currentFrame.objects || {});

  return (
    <div className="main-layout">
      <aside className="config-panel">
        <h1 className="panel-title">Configuration</h1>
        <div className="control-group">
          <label>Algorithm: Custom Logic (v-cpp)</label>
          <p className="subtitle">Edit `algorithms.cpp` to change the logic.</p>
        </div>
        <div className="control-group">
          <label htmlFor="data-input">Universal Input</label>
          <textarea
            id="data-input"
            value={rawInput}
            onChange={(e) => setRawInput(e.target.value)}
            disabled={isGenerating}
            placeholder={
              `Welcome to the v-cpp Universal Input Parser!

--- HOW IT WORKS ---

1.  Define your input here using 'key=value' pairs.
2.  In your C++ code, use the 'v' handle to get data
    by its key.

The key in the input MUST MATCH the key in the code.

--- EXAMPLES ---

// For Scalars (Numbers, Strings, Booleans)
count=100, rate=3.14, name="Saumy", enabled=true

// For Vectors, Lists, Sets, Deques
arr={10, 20, 30}
names={"apple", "banana"}

// For a Matrix (Nested Arrays)
matrix={{1, 2}, {3, 4}}

// For Maps (An array of {key,value} pairs)
scores={ {"player1", 100}, {"player2", 95} }

// A Complex Example
grid_points={ {{1,1},{1,2}}, {{2,1},{2,2}} }, id=99, is_active=true

--- CONNECTING TO C++ (in run_my_algorithm) ---

// Input: my_array={10, 20, 30}, count=3

// C++ Code:
auto arr = v.get_vector<int>("my_array");
auto total = v.get_scalar<int>("count");

// This works because "my_array" matches "my_array"
// and "count" matches "count".`
            }
          />
        </div>
        <button className="visualize-button" onClick={handleVisualize} disabled={isGenerating || !wasmModule}>
          {isGenerating ? 'Generating...' : (wasmModule ? 'Visualize!' : 'Loading Engine...')}
        </button>
      </aside>

      <main className="visualization-wrapper">
        {history.length > 0 && (
          <div className="playback-controls">
            <button onClick={() => setCurrentFrameIndex(prev => Math.max(0, prev - 1))}><FaStepBackward /></button>
            <button onClick={() => setIsPlaying(!isPlaying)}>{isPlaying ? <FaPause /> : <FaPlay />}</button>
            <button onClick={() => setCurrentFrameIndex(prev => Math.min(history.length - 1, prev + 1))}><FaStepForward /></button>
            <input type="range" min="0" max={history.length - 1} value={currentFrameIndex} onChange={(e) => setCurrentFrameIndex(Number(e.target.value))} />
          </div>
        )}
        <div className="visualization-area">
          <div className="status-message">{currentFrame.message}</div>
          <div className="objects-grid">{objectEntries.map(([name, obj]) => (<VisualObject key={name} name={name} obj={obj} />))}</div>
        </div>
      </main>
    </div>
  );
}

// The component hierarchy is universal and does not need to change.
const VisualObject = ({ name, obj }) => (
  <div className="ds-container">
    <div className="ds-title">{name} ({obj.type})</div>
    <div className="ds-body">
      {obj.type === 'scalar' && <ScalarView data={obj.data} highlights={obj.highlights} />}
      {obj.type === 'string' && <StringView data={obj.data} highlights={obj.highlights} />}
      {obj.type === 'bool' && <BoolView data={obj.data} highlights={obj.highlights} />}
      {(obj.type === 'vector' || obj.type === 'list' || obj.type === 'deque') && <VectorView data={obj.data} highlights={obj.highlights} />}
      {(obj.type === 'set' || obj.type === 'multiset' || obj.type === 'unordered_set' || obj.type === 'unordered_multiset') && <SetView data={obj.data} highlights={obj.highlights} />}
      {obj.type === 'matrix' && <MatrixView data={obj.data} highlights={obj.highlights} />}
      {(obj.type === 'map' || obj.type === 'multimap' || obj.type === 'unordered_map' || obj.type === 'unordered_multimap') && <MapView data={obj.data} highlights={obj.highlights} />}
      {obj.type === 'stack' && <StackView data={obj.data} highlights={obj.highlights} />}
      {obj.type === 'queue' && <QueueView data={obj.data} highlights={obj.highlights} />}
      {obj.type === 'priority_queue' && <PriorityQueueView data={obj.data} highlights={obj.highlights} />}
      {obj.type === 'pair' && <PairView data={obj.data} highlights={obj.highlights} />}
      {obj.type === 'tuple' && <TupleView data={obj.data} highlights={obj.highlights} />}
    </div>
  </div>
);
const ScalarView = ({ data, highlights }) => <div className="array-cell scalar-cell" data-state={highlights?.['0'] || 'default'}>{data}</div>;
const StringView = ({ data, highlights }) => <div className="array-cell string-cell" data-state={highlights?.['0'] || 'default'}>"{data}"</div>;
const BoolView = ({ data, highlights }) => <div className="array-cell scalar-cell" data-state={highlights?.['0'] || 'default'}>{data ? 'true' : 'false'}</div>;
const VectorView = ({ data, highlights }) => <div className="vector-wrapper">{data.map((value, idx) => (<div className="cell-block" key={idx}><div className="array-cell" data-state={highlights?.[`${idx}`] || 'default'}>{value}</div><div className="cell-index">{idx}</div></div>))}</div>;
const SetView = ({ data, highlights }) => { const sortedData = Array.isArray(data) ? [...data].sort((a, b) => a - b) : []; return <div className="set-wrapper">{sortedData.map((value, idx) => (<div className="array-cell" key={idx} data-state={highlights?.[`${value}`] || 'default'}>{value}</div>))}</div> };
const MatrixView = ({ data, highlights }) => <div className="matrix-grid">{(Array.isArray(data) ? data : []).map((row, r_idx) => (<div className="matrix-row" key={r_idx}>{row.map((value, c_idx) => (<div className="array-cell" key={c_idx} data-state={highlights?.[`${r_idx}-${c_idx}`] || 'default'}>{value}</div>))}</div>))}</div>;
const MapView = ({ data, highlights }) => <div className="map-wrapper">{(Array.isArray(data) ? data : []).sort((a, b) => a.key - b.key).map(({ key, value }, idx) => (<div className="map-pair" key={idx}><div className="array-cell map-key">{key}</div><div className="map-separator">→</div><div className="array-cell map-value" data-state={highlights?.[`${key}`] || 'default'}>{value}</div></div>))}</div>;
const StackView = ({ data, highlights }) => <div className="stack-wrapper"><div className="stack-top-label">TOP</div>{(Array.isArray(data) ? [...data] : []).reverse().map((value, idx) => (<div className="array-cell" key={idx} data-state={idx === 0 ? highlights?.['top'] : 'default'}>{value}</div>))}</div>;
const QueueView = ({ data, highlights }) => <div className="queue-wrapper"><div className="queue-label">FRONT</div>{(Array.isArray(data) ? data : []).map((value, idx) => (<div className="array-cell" key={idx} data-state={(idx === 0 ? highlights?.['front'] : (idx === data.length - 1 ? highlights?.['back'] : 'default')) || 'default'}>{value}</div>))}<div className="queue-label">BACK</div></div>;
const PriorityQueueView = ({ data, highlights }) => <div className="pq-wrapper"><div className="pq-top-label">MAX HEAP (TOP)</div>{(Array.isArray(data) ? [...data] : []).sort((a, b) => b - a).map((value, idx) => (<div className="array-cell pq-cell" key={idx} data-state={idx === 0 ? highlights?.['top'] : 'default'}>{value}</div>))}</div>;
const PairView = ({ data, highlights }) => <div className="pair-wrapper"><div className="array-cell" data-state={highlights?.['0'] || 'default'}>{data?.[0]}</div><div className="array-cell" data-state={highlights?.['1'] || 'default'}>{data?.[1]}</div></div>;
const TupleView = ({ data, highlights }) => <div className="tuple-wrapper">{(Array.isArray(data) ? data : []).map((value, idx) => <div className="array-cell" key={idx} data-state={highlights?.[`${idx}`] || 'default'}>{value}</div>)}</div>;

export default App;
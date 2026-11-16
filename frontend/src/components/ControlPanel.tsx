import React from 'react';
import type { SimulationConfig, ConnectionStatus } from '../types/market';

interface ControlPanelProps {
  config: SimulationConfig;
  connectionStatus?: ConnectionStatus;
  mode?: 'backend' | 'mock';
  onSetMode?: (mode: 'backend' | 'mock') => void;
  onReconnect?: () => void;
  onUpdateConfig: (cfg: Partial<SimulationConfig>) => void;
  onResetBook: () => void;
  onClearData?: () => void;
}

export const ControlPanel: React.FC<ControlPanelProps> = ({
  config,
  connectionStatus = 'connected',
  mode = 'backend',
  onSetMode,
  onReconnect,
  onUpdateConfig,
  onResetBook,
  onClearData,
}) => {
  const getSpeedLabel = (ms: number) => {
    if (ms <= 5) return 'Turbo (~2000+/s)';
    if (ms <= 20) return 'Very Fast (~300/s)';
    if (ms <= 50) return 'Fast (~40/s)';
    if (ms <= 100) return 'Medium (10/s)';
    return `${ms}ms`;
  };

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 space-y-4">
      {/* Panel Header */}
      <div className="flex items-center justify-between pb-2 border-b border-slate-800">
        <h2 className="text-xs font-semibold uppercase tracking-wider text-slate-300">
          Engine Controls
        </h2>
        <div className="flex items-center space-x-2">
          {onClearData && (
            <button
              onClick={onClearData}
              className="text-xs text-slate-300 hover:text-white px-2 py-0.5 rounded border border-slate-700 bg-slate-800 hover:bg-slate-700 transition"
              title="Clear all orders and start with an empty book"
            >
              Clear Book
            </button>
          )}
          <button
            onClick={onResetBook}
            className="text-xs text-slate-400 hover:text-white px-2 py-0.5 rounded border border-slate-700 bg-slate-800 transition"
            title="Clear and reset engine state"
          >
            Reset
          </button>
        </div>
      </div>

      {/* Mode Switcher */}
      {onSetMode && (
        <div className="space-y-1.5">
          <label className="text-xs text-slate-400">Execution Mode</label>
          <div className="grid grid-cols-2 gap-1.5 bg-slate-800 p-1 rounded border border-slate-700 text-xs">
            <button
              onClick={() => onSetMode('backend')}
              className={`py-1 rounded text-center transition ${
                mode === 'backend'
                  ? 'bg-slate-900 text-white font-medium shadow-sm'
                  : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              C++ Backend
            </button>
            <button
              onClick={() => onSetMode('mock')}
              className={`py-1 rounded text-center transition ${
                mode === 'mock'
                  ? 'bg-slate-900 text-white font-medium shadow-sm'
                  : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Mock Simulator
            </button>
          </div>
        </div>
      )}

      {/* Mode Info */}
      {mode === 'backend' ? (
        <div className="space-y-2 text-xs bg-slate-800/60 p-3 rounded border border-slate-800 text-slate-300">
          <div className="flex justify-between items-center text-[11px]">
            <span className="text-slate-400">Backend API:</span>
            <span className="font-mono text-slate-300">http://localhost:4000</span>
          </div>
          <div className="flex justify-between items-center text-[11px]">
            <span className="text-slate-400">WebSocket Feed:</span>
            <span className="font-mono text-slate-300">ws://localhost:4001</span>
          </div>
          <div className="flex justify-between items-center text-[11px] pt-1 border-t border-slate-700/60">
            <span className="text-slate-400">Status:</span>
            <span
              className={`font-semibold ${
                connectionStatus === 'connected'
                  ? 'text-green-400'
                  : connectionStatus === 'connecting'
                  ? 'text-amber-400'
                  : 'text-red-400'
              }`}
            >
              {connectionStatus === 'connected'
                ? 'Online'
                : connectionStatus === 'connecting'
                ? 'Connecting...'
                : 'Offline'}
            </span>
          </div>

          {connectionStatus === 'disconnected' && onReconnect && (
            <button
              onClick={onReconnect}
              className="w-full mt-2 py-1 text-xs text-center rounded bg-slate-700 hover:bg-slate-600 text-white transition"
            >
              Reconnect to Backend
            </button>
          )}
        </div>
      ) : null}

      {/* Flow Simulator Controls (Available in both Backend & Mock modes) */}
      <div className="space-y-3 pt-1 border-t border-slate-800/80 text-xs">
        <div className="flex items-center justify-between">
          <span className="text-slate-300 font-medium">
            {mode === 'backend' ? 'Engine Flow Simulator' : 'Mock Simulator'}
          </span>
          <button
            onClick={() => onUpdateConfig({ isRunning: !config.isRunning })}
            className={`px-2.5 py-1 rounded font-medium text-xs transition ${
              config.isRunning
                ? 'bg-amber-600/20 text-amber-300 border border-amber-600/40 hover:bg-amber-600/30'
                : 'bg-green-600/20 text-green-300 border border-green-600/40 hover:bg-green-600/30'
            }`}
          >
            {config.isRunning ? 'Pause Sim' : 'Start Sim'}
          </button>
        </div>

        {/* Speed Presets */}
        <div className="space-y-1">
          <label className="text-[11px] text-slate-400">Speed Presets</label>
          <div className="grid grid-cols-4 gap-1 text-[11px]">
            {[
              { label: 'Normal', ms: 250 },
              { label: 'Fast', ms: 50 },
              { label: 'High', ms: 10 },
              { label: 'Turbo', ms: 2 },
            ].map((preset) => (
              <button
                key={preset.ms}
                onClick={() => onUpdateConfig({ updateSpeedMs: preset.ms })}
                className={`py-0.5 rounded border transition font-mono ${
                  config.updateSpeedMs === preset.ms
                    ? 'border-slate-500 bg-slate-700 text-white font-semibold'
                    : 'border-slate-700 bg-slate-800 text-slate-400 hover:text-slate-200'
                }`}
              >
                {preset.label}
              </button>
            ))}
          </div>
        </div>

        {/* Speed Slider */}
        <div className="space-y-1">
          <div className="flex justify-between text-slate-400">
            <span>Interval Rate:</span>
            <span className="text-white font-mono text-[11px]">{getSpeedLabel(config.updateSpeedMs)}</span>
          </div>
          <input
            type="range"
            min="2"
            max="1000"
            step="2"
            value={config.updateSpeedMs}
            onChange={(e) => onUpdateConfig({ updateSpeedMs: parseInt(e.target.value, 10) })}
            className="w-full h-1.5 bg-slate-800 rounded accent-slate-400 cursor-pointer"
          />
        </div>

        {/* Bot Count */}
        <div className="space-y-1">
          <div className="flex justify-between text-slate-400">
            <span>Active Bots:</span>
            <span className="text-white font-mono">{config.activeBots}</span>
          </div>
          <input
            type="range"
            min="1"
            max="50"
            value={config.activeBots}
            onChange={(e) => onUpdateConfig({ activeBots: parseInt(e.target.value, 10) })}
            className="w-full h-1.5 bg-slate-800 rounded accent-slate-400 cursor-pointer"
          />
        </div>

        <p className="text-[11px] text-slate-400 pt-1 leading-normal">
          {mode === 'backend'
            ? 'Orders are dispatched directly into the C++ engine stdin and matched in real-time.'
            : 'Simulated locally inside React state for offline testing.'}
        </p>
      </div>
    </div>
  );
};

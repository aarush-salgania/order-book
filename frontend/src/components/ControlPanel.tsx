import React from 'react';
import { Play, Pause, RotateCcw, Bot, Gauge, Zap, Sparkles } from 'lucide-react';
import type { SimulationConfig } from '../types/market';

interface ControlPanelProps {
  config: SimulationConfig;
  onUpdateConfig: (cfg: Partial<SimulationConfig>) => void;
  onResetBook: () => void;
}

export const ControlPanel: React.FC<ControlPanelProps> = ({
  config,
  onUpdateConfig,
  onResetBook,
}) => {
  const toggleRunning = () => {
    onUpdateConfig({ isRunning: !config.isRunning });
  };

  const handleBotsChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    onUpdateConfig({ activeBots: parseInt(e.target.value, 10) });
  };

  const handleSpeedChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    onUpdateConfig({ updateSpeedMs: parseInt(e.target.value, 10) });
  };

  return (
    <div className="bg-terminal-panel border border-terminal-border rounded-lg p-4 shadow-md flex flex-col space-y-4">
      {/* Title & Engine Toggle */}
      <div className="flex items-center justify-between pb-3 border-b border-terminal-border">
        <div className="flex items-center space-x-2">
          <Gauge className="w-4 h-4 text-cyan-400" />
          <h2 className="text-xs font-bold uppercase tracking-wider text-slate-200">
            Simulator Controls
          </h2>
        </div>

        {/* Start / Pause Primary Toggle Button */}
        <button
          onClick={toggleRunning}
          className={`flex items-center space-x-1.5 px-3 py-1.5 rounded text-xs font-bold transition-all shadow-sm ${
            config.isRunning
              ? 'bg-amber-500/10 hover:bg-amber-500/20 text-amber-400 border border-amber-500/30'
              : 'bg-bid/10 hover:bg-bid/20 text-bid border border-bid/40'
          }`}
          title={config.isRunning ? 'Pause simulation ticks' : 'Start simulation ticks'}
        >
          {config.isRunning ? (
            <>
              <Pause className="w-3.5 h-3.5 fill-current" />
              <span>PAUSE SIM</span>
            </>
          ) : (
            <>
              <Play className="w-3.5 h-3.5 fill-current" />
              <span>START SIM</span>
            </>
          )}
        </button>
      </div>

      {/* Control 1: Active Bots Slider */}
      <div className="space-y-1.5">
        <div className="flex justify-between items-center text-xs">
          <span className="flex items-center space-x-1.5 text-slate-300 font-medium">
            <Bot className="w-3.5 h-3.5 text-slate-400" />
            <span>Active Bots</span>
          </span>
          <span className="bg-terminal-card border border-terminal-border px-2 py-0.5 rounded text-cyan-400 font-bold tabular-nums">
            {config.activeBots} <span className="text-terminal-muted font-normal">bots</span>
          </span>
        </div>

        <input
          type="range"
          min="1"
          max="100"
          value={config.activeBots}
          onChange={handleBotsChange}
          className="w-full h-1.5 bg-terminal-card rounded-lg appearance-none cursor-pointer accent-cyan-400 border border-terminal-border"
        />

        {/* Bot Presets */}
        <div className="flex justify-between items-center pt-1 text-[10px] text-terminal-muted">
          <span>1 bot</span>
          <div className="flex space-x-1">
            {[5, 25, 50, 100].map((preset) => (
              <button
                key={preset}
                onClick={() => onUpdateConfig({ activeBots: preset })}
                className={`px-1.5 py-0.5 rounded border transition-colors ${
                  config.activeBots === preset
                    ? 'border-cyan-500 text-cyan-400 bg-cyan-950/40 font-bold'
                    : 'border-terminal-border hover:border-slate-500 text-slate-400'
                }`}
              >
                {preset}
              </button>
            ))}
          </div>
          <span>100 bots</span>
        </div>
      </div>

      {/* Control 2: Market Speed Slider */}
      <div className="space-y-1.5">
        <div className="flex justify-between items-center text-xs">
          <span className="flex items-center space-x-1.5 text-slate-300 font-medium">
            <Zap className="w-3.5 h-3.5 text-amber-400" />
            <span>Market Speed</span>
          </span>
          <span className="bg-terminal-card border border-terminal-border px-2 py-0.5 rounded text-amber-400 font-bold tabular-nums">
            {config.updateSpeedMs} <span className="text-terminal-muted font-normal">ms/tick</span>
          </span>
        </div>

        <input
          type="range"
          min="100"
          max="2000"
          step="50"
          value={config.updateSpeedMs}
          onChange={handleSpeedChange}
          className="w-full h-1.5 bg-terminal-card rounded-lg appearance-none cursor-pointer accent-amber-400 border border-terminal-border"
        />

        {/* Speed Presets */}
        <div className="flex justify-between items-center pt-1 text-[10px] text-terminal-muted">
          <div className="flex space-x-1.5 w-full justify-between">
            <button
              onClick={() => onUpdateConfig({ updateSpeedMs: 100 })}
              className={`px-2 py-0.5 rounded border transition-colors ${
                config.updateSpeedMs === 100
                  ? 'border-amber-500 text-amber-400 bg-amber-950/40 font-bold'
                  : 'border-terminal-border hover:border-slate-500 text-slate-400'
              }`}
            >
              100ms (Max)
            </button>
            <button
              onClick={() => onUpdateConfig({ updateSpeedMs: 250 })}
              className={`px-2 py-0.5 rounded border transition-colors ${
                config.updateSpeedMs === 250
                  ? 'border-amber-500 text-amber-400 bg-amber-950/40 font-bold'
                  : 'border-terminal-border hover:border-slate-500 text-slate-400'
              }`}
            >
              250ms (Norm)
            </button>
            <button
              onClick={() => onUpdateConfig({ updateSpeedMs: 1000 })}
              className={`px-2 py-0.5 rounded border transition-colors ${
                config.updateSpeedMs === 1000
                  ? 'border-amber-500 text-amber-400 bg-amber-950/40 font-bold'
                  : 'border-terminal-border hover:border-slate-500 text-slate-400'
              }`}
            >
              1000ms (Slow)
            </button>
          </div>
        </div>
      </div>

      {/* Action Footer: Reset Order Book */}
      <div className="pt-2 border-t border-terminal-border flex items-center justify-between">
        <span className="text-[10px] text-terminal-muted flex items-center space-x-1">
          <Sparkles className="w-3 h-3 text-cyan-400" />
          <span>Synthesizing Micro-Ticks</span>
        </span>

        <button
          onClick={onResetBook}
          className="flex items-center space-x-1 px-2 py-1 text-[11px] font-medium text-slate-400 hover:text-slate-200 bg-terminal-card hover:bg-terminal-border/40 border border-terminal-border rounded transition-colors"
          title="Reset book to clean snapshot"
        >
          <RotateCcw className="w-3 h-3" />
          <span>Reset Book</span>
        </button>
      </div>
    </div>
  );
};

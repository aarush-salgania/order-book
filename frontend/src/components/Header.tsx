import React from 'react';
import type { OrderBookSnapshot, SimulationConfig, ConnectionStatus, EngineStats } from '../types/market';

interface HeaderProps {
  snapshot: OrderBookSnapshot;
  config: SimulationConfig;
  connectionStatus?: ConnectionStatus;
  mode?: 'backend' | 'mock';
  engineStats?: EngineStats;
  onReconnect?: () => void;
}

function formatLatency(ns?: number): string {
  if (ns == null || ns === 0) return '—';
  if (ns < 1000) return `${ns} ns`;
  if (ns < 1000000) return `${(ns / 1000).toFixed(1)} µs`;
  return `${(ns / 1000000).toFixed(2)} ms`;
}

export const Header: React.FC<HeaderProps> = ({
  snapshot,
  connectionStatus = 'connected',
  mode = 'backend',
  engineStats,
  onReconnect,
}) => {
  const isPriceUp = snapshot.priceChangePercent24h >= 0;

  return (
    <header className="bg-slate-900 border-b border-slate-800 px-4 py-3 flex flex-wrap items-center justify-between gap-4">
      {/* Title & Project Info */}
      <div className="flex items-center space-x-3">
        <div>
          <h1 className="text-sm font-semibold text-white">Order Matching Engine</h1>
          <p className="text-xs text-slate-400">Minor Project &bull; C++ Backend with React</p>
        </div>
      </div>

      {/* Stock Ticker & Current Price */}
      <div className="flex items-center space-x-4 bg-slate-800/80 px-3.5 py-1.5 rounded border border-slate-700/80 text-xs">
        <span className="font-bold text-white">NVDA / INR</span>
        <span className="text-slate-600">|</span>
        <div className="flex items-center space-x-1.5">
          <span className="text-slate-400">Last Price:</span>
          <span className={`font-mono font-semibold ${isPriceUp ? 'text-green-400' : 'text-red-400'}`}>
            ₹{snapshot.lastPrice.toFixed(2)}
          </span>
        </div>
        <span className="text-slate-600">|</span>
        <div className="flex items-center space-x-1.5">
          <span className="text-slate-400">Spread:</span>
          <span className="font-mono text-slate-200">
            ₹{snapshot.spread.toFixed(2)}
          </span>
        </div>
      </div>

      {/* Engine, Tail Latency & Mempool Stats */}
      <div className="hidden md:flex items-center space-x-3.5 bg-slate-800/80 px-3.5 py-1.5 rounded border border-slate-700/80 text-xs">
        {/* Current Match */}
        <div className="flex items-center space-x-1.5">
          <span className="text-slate-400">Match:</span>
          <span className="font-mono text-slate-200 font-medium">
            {formatLatency(engineStats?.latencyNs)}
          </span>
        </div>

        <span className="text-slate-600">|</span>

        {/* Tail Latencies: p50 / p90 / p99 / p99.9 */}
        <div className="flex items-center space-x-2.5">
          <div className="flex items-center space-x-1" title="Median Latency (50th Percentile)">
            <span className="text-slate-400 text-[11px]">p50:</span>
            <span className="font-mono text-emerald-400 font-medium">
              {formatLatency(engineStats?.p50Ns)}
            </span>
          </div>

          <div className="flex items-center space-x-1" title="90th Percentile Latency">
            <span className="text-slate-400 text-[11px]">p90:</span>
            <span className="font-mono text-slate-200">
              {formatLatency(engineStats?.p90Ns)}
            </span>
          </div>

          <div className="flex items-center space-x-1" title="99th Percentile Latency">
            <span className="text-slate-400 text-[11px]">p99:</span>
            <span className="font-mono text-sky-400 font-medium">
              {formatLatency(engineStats?.p99Ns)}
            </span>
          </div>

          <div className="flex items-center space-x-1" title="99.9th Percentile Tail Latency">
            <span className="text-slate-400 text-[11px]">p99.9:</span>
            <span className="font-mono text-amber-400 font-semibold">
              {formatLatency(engineStats?.p999Ns)}
            </span>
          </div>
        </div>

        <span className="text-slate-600">|</span>

        {/* Mempool Info */}
        <div className="flex items-center space-x-1.5">
          <span className="text-slate-400">Mempool:</span>
          <span className="font-mono text-slate-200">
            {engineStats?.poolUsed != null ? `${engineStats.poolUsed} / 100k` : '0 / 100k'}
          </span>
          <span className="text-slate-500 font-mono text-[11px]">(2.3 ns/alloc)</span>
        </div>
      </div>

      {/* Backend Status */}
      <div className="flex items-center space-x-2 text-xs">
        <span
          className={`w-2.5 h-2.5 rounded-full ${
            mode === 'backend'
              ? connectionStatus === 'connected'
                ? 'bg-green-500'
                : connectionStatus === 'connecting'
                ? 'bg-amber-400'
                : 'bg-red-500'
              : 'bg-blue-400'
          }`}
        />
        <span className="text-slate-300 font-medium">
          {mode === 'backend'
            ? connectionStatus === 'connected'
              ? 'Backend Connected (4001)'
              : connectionStatus === 'connecting'
              ? 'Connecting to Backend...'
              : 'Backend Offline'
            : 'Mock Mode'}
        </span>
        {mode === 'backend' && connectionStatus === 'disconnected' && onReconnect && (
          <button
            onClick={onReconnect}
            className="text-[11px] text-blue-400 hover:text-blue-300 underline ml-1"
          >
            Reconnect
          </button>
        )}
      </div>
    </header>
  );
};

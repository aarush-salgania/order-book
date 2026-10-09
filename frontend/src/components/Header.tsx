import React, { useState, useEffect } from 'react';
import { Activity, Cpu } from 'lucide-react';
import type { OrderBookSnapshot, SimulationConfig } from '../types/market';

interface HeaderProps {
  snapshot: OrderBookSnapshot;
  config: SimulationConfig;
  connectionStatus?: ConnectionStatus;
  mode?: 'backend' | 'mock';
  onReconnect?: () => void;
}

export const Header: React.FC<HeaderProps> = ({
  snapshot,
  config,
  connectionStatus = 'connected',
  mode = 'backend',
  onReconnect,
}) => {
  const [currentTime, setCurrentTime] = useState<string>('');

  useEffect(() => {
    const updateTime = () => {
      const now = new Date();
      setCurrentTime(
        now.toISOString().replace('T', ' ').substring(0, 23) + ' UTC'
      );
    };
    updateTime();
    const interval = setInterval(updateTime, 50);
    return () => clearInterval(interval);
  }, []);

  const isPriceUp = snapshot.priceChangePercent24h >= 0;

  return (
    <header className="bg-terminal-panel border-b border-terminal-border px-4 py-2.5 flex flex-wrap items-center justify-between gap-4 shadow-lg select-none">
      {/* Brand & Market Identifier */}
      <div className="flex items-center space-x-3">
        <div className="flex items-center space-x-2 bg-terminal-card border border-terminal-border px-2.5 py-1 rounded">
          <Activity className="w-4 h-4 text-cyan-400 animate-pulse" />
          <span className="font-bold tracking-wider text-sm text-white">LOB // SIMULATOR</span>
          <span className="text-[10px] bg-cyan-950 text-cyan-400 font-semibold px-1.5 py-0.5 rounded border border-cyan-800">
            L2 BOOK
          </span>
        </div>

        <div className="flex items-center space-x-2 pl-2">
          <span className="text-base font-bold text-white tracking-wide">NVDA/INR</span>
          <span className="text-xs text-terminal-muted bg-terminal-card px-1.5 py-0.5 rounded border border-terminal-border">
            EQUITY
          </span>
        </div>
      </div>

      {/* Market Statistics Bar */}
      <div className="flex items-center space-x-6 text-xs tabular-nums">
        <div className="flex flex-col">
          <span className="text-[10px] text-terminal-muted uppercase">Last Price</span>
          <span
            className={`font-bold text-sm ${
              isPriceUp ? 'text-bid' : 'text-ask'
            }`}
          >
            ₹{snapshot.lastPrice.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
          </span>
        </div>

        <div className="flex flex-col">
          <span className="text-[10px] text-terminal-muted uppercase">24h Change</span>
          <span className={`font-semibold ${isPriceUp ? 'text-bid' : 'text-ask'}`}>
            {isPriceUp ? '+' : ''}{snapshot.priceChangePercent24h.toFixed(2)}%
          </span>
        </div>

        <div className="hidden md:flex flex-col">
          <span className="text-[10px] text-terminal-muted uppercase">24h High</span>
          <span className="text-slate-300 font-medium">
            ₹{snapshot.high24h.toLocaleString(undefined, { minimumFractionDigits: 2 })}
          </span>
        </div>

        <div className="hidden md:flex flex-col">
          <span className="text-[10px] text-terminal-muted uppercase">24h Low</span>
          <span className="text-slate-300 font-medium">
            ₹{snapshot.low24h.toLocaleString(undefined, { minimumFractionDigits: 2 })}
          </span>
        </div>

        <div className="hidden lg:flex flex-col">
          <span className="text-[10px] text-terminal-muted uppercase">24h Vol (Shares)</span>
          <span className="text-slate-300 font-medium">
            {snapshot.volume24h.toLocaleString(undefined, { minimumFractionDigits: 0, maximumFractionDigits: 0 })}
          </span>
        </div>
      </div>

      {/* System Status & C++ Backend State */}
      <div className="flex items-center space-x-3 text-xs">
        {/* Dynamic C++ Backend WebSocket Status Badge */}
        {mode === 'backend' ? (
          <div
            onClick={connectionStatus === 'disconnected' ? onReconnect : undefined}
            title={
              connectionStatus === 'connected'
                ? 'Connected to C++ Backend on ws://localhost:4001'
                : 'Click to attempt reconnect to ws://localhost:4001'
            }
            className={`flex items-center space-x-1.5 bg-terminal-card border px-2 py-1 rounded transition-colors ${
              connectionStatus === 'connected'
                ? 'border-emerald-500/40 text-emerald-300 shadow-[0_0_8px_rgba(16,185,129,0.2)]'
                : connectionStatus === 'connecting'
                ? 'border-amber-500/40 text-amber-300'
                : 'border-rose-500/40 text-rose-300 cursor-pointer hover:bg-rose-950/20'
            }`}
          >
            <Cpu
              className={`w-3.5 h-3.5 ${
                connectionStatus === 'connected'
                  ? 'text-emerald-400'
                  : connectionStatus === 'connecting'
                  ? 'text-amber-400 animate-spin'
                  : 'text-rose-400'
              }`}
            />
            <span className="text-[11px] font-bold">
              CPP WS:
            </span>
            <span className="text-[11px] uppercase tracking-wide">
              {connectionStatus === 'connected'
                ? 'ONLINE (4001)'
                : connectionStatus === 'connecting'
                ? 'CONNECTING...'
                : 'OFFLINE (RETRY)'}
            </span>
          </div>
        ) : (
          <div className="flex items-center space-x-1.5 bg-terminal-card border border-cyan-800 px-2 py-1 rounded text-cyan-300">
            <Cpu className="w-3.5 h-3.5 text-cyan-400" />
            <span className="text-[11px] font-medium">MODE:</span>
            <span className="text-[11px] font-semibold text-cyan-400">LOCAL MOCK</span>
          </div>
        )}

        {/* Live Engine Status */}
        <div className="flex items-center space-x-1.5 bg-terminal-card border border-terminal-border px-2 py-1 rounded">
          <span
            className={`w-2 h-2 rounded-full ${
              config.isRunning ? 'bg-bid animate-pulse-fast shadow-[0_0_8px_#00e676]' : 'bg-ask'
            }`}
          />
          <span className="text-[11px] font-semibold text-slate-200">
            {config.isRunning ? 'ENGINE LIVE' : 'ENGINE PAUSED'}
          </span>
          <span className="text-[10px] text-terminal-muted ml-1">
            ({config.updateSpeedMs}ms)
          </span>
        </div>

        {/* Live Clock */}
        <div className="hidden xl:block text-[11px] font-mono text-terminal-muted bg-terminal-card/80 px-2 py-1 rounded border border-terminal-border">
          {currentTime}
        </div>
      </div>
    </header>
  );
};


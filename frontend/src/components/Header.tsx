import React from 'react';
import type { OrderBookSnapshot, SimulationConfig, ConnectionStatus } from '../types/market';

interface HeaderProps {
  snapshot: OrderBookSnapshot;
  config: SimulationConfig;
  connectionStatus?: ConnectionStatus;
  mode?: 'backend' | 'mock';
  onReconnect?: () => void;
}

export const Header: React.FC<HeaderProps> = ({
  snapshot,
  connectionStatus = 'connected',
  mode = 'backend',
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

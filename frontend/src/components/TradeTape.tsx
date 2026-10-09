import React, { memo } from 'react';
import { History, ArrowUpRight, ArrowDownRight } from 'lucide-react';
import type { Trade } from '../types/market';

interface TradeTapeProps {
  trades: Trade[];
}

/**
 * Format timestamp into terminal-style HH:mm:ss.SSS
 */
function formatTime(timestamp: number): string {
  const date = new Date(timestamp);
  const hours = date.getHours().toString().padStart(2, '0');
  const minutes = date.getMinutes().toString().padStart(2, '0');
  const seconds = date.getSeconds().toString().padStart(2, '0');
  const ms = date.getMilliseconds().toString().padStart(3, '0');
  return `${hours}:${minutes}:${seconds}.${ms}`;
}

const TradeRow = memo(({ trade, isLatest }: { trade: Trade; isLatest: boolean }) => {
  const isBuy = trade.side === 'buy';

  return (
    <div
      className={`grid grid-cols-4 px-3 py-1 text-xs font-mono tabular-nums transition-colors items-center hover:bg-white/5 ${
        isLatest ? (isBuy ? 'animate-flash-green' : 'animate-flash-red') : ''
      }`}
    >
      {/* Time */}
      <span className="text-[11px] text-terminal-muted truncate">
        {formatTime(trade.timestamp)}
      </span>

      {/* Side Badge */}
      <div className="flex items-center space-x-1">
        {isBuy ? (
          <ArrowUpRight className="w-3 h-3 text-bid" />
        ) : (
          <ArrowDownRight className="w-3 h-3 text-ask" />
        )}
        <span
          className={`text-[10px] font-bold px-1 rounded ${
            isBuy ? 'bg-bid/10 text-bid' : 'bg-ask/10 text-ask'
          }`}
        >
          {isBuy ? 'BUY' : 'SELL'}
        </span>
      </div>

      {/* Price */}
      <span
        className={`font-semibold text-right ${
          isBuy ? 'text-bid' : 'text-ask'
        }`}
      >
        ₹{trade.price.toFixed(2)}
      </span>

      {/* Quantity */}
      <span className="text-slate-300 text-right">
        {Math.round(trade.quantity)}
      </span>
    </div>
  );
});

TradeRow.displayName = 'TradeRow';

export const TradeTape: React.FC<TradeTapeProps> = ({ trades }) => {
  // Compute buy vs sell distribution from recent trades
  const buyCount = trades.filter((t) => t.side === 'buy').length;
  const totalCount = trades.length || 1;
  const buyPercent = Math.round((buyCount / totalCount) * 100);

  return (
    <div className="bg-terminal-panel border border-terminal-border rounded-lg shadow-md flex flex-col h-full overflow-hidden">
      {/* Tape Header */}
      <div className="px-4 py-2.5 border-b border-terminal-border flex items-center justify-between bg-terminal-panel/90">
        <div className="flex items-center space-x-2">
          <History className="w-4 h-4 text-cyan-400" />
          <h2 className="text-xs font-bold uppercase tracking-wider text-slate-200">
            Market Trades (Tape)
          </h2>
        </div>

        <span className="text-[10px] text-terminal-muted font-mono bg-terminal-card px-2 py-0.5 rounded border border-terminal-border">
          {trades.length} RECENT
        </span>
      </div>

      {/* Trade Flow Ratio Bar */}
      <div className="px-3 py-1.5 border-b border-terminal-border/60 bg-terminal-card/30 flex flex-col space-y-1">
        <div className="flex justify-between text-[10px] text-terminal-muted font-mono">
          <span className="text-bid font-semibold">BUY {buyPercent}%</span>
          <span className="text-ask font-semibold">SELL {100 - buyPercent}%</span>
        </div>
        <div className="w-full h-1 bg-ask/40 rounded-full overflow-hidden flex">
          <div
            className="bg-bid h-full transition-all duration-300"
            style={{ width: `${buyPercent}%` }}
          />
        </div>
      </div>

      {/* Column Headers */}
      <div className="grid grid-cols-4 px-3 py-1.5 border-b border-terminal-border text-[11px] font-semibold text-terminal-muted uppercase tracking-wider bg-terminal-card/50">
        <span>Time</span>
        <span>Side</span>
        <span className="text-right">Price</span>
        <span className="text-right">Size (Shares)</span>
      </div>

      {/* Scrolling Trade Stream */}
      <div className="flex-1 min-h-[460px] max-h-[580px] overflow-y-auto divide-y divide-terminal-border/20">
        {trades.map((trade, idx) => (
          <TradeRow
            key={trade.id}
            trade={trade}
            isLatest={idx === 0}
          />
        ))}

        {trades.length === 0 && (
          <div className="flex flex-col items-center justify-center h-48 text-terminal-muted text-xs">
            <span>Awaiting simulated executions...</span>
          </div>
        )}
      </div>

      {/* Footer Info */}
      <div className="px-3 py-1.5 border-t border-terminal-border bg-terminal-card/60 flex items-center justify-between text-[10px] text-terminal-muted font-mono">
        <span>FEED: SIMULATED TAPE</span>
        <span className="text-cyan-400">TICK RESOLUTION: 1ms</span>
      </div>
    </div>
  );
};

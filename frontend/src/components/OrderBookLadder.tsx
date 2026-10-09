import React, { memo } from 'react';
import { Layers, ArrowUp, ArrowDown } from 'lucide-react';
import type { OrderBookLevel, OrderBookSnapshot } from '../types/market';

interface OrderBookLadderProps {
  snapshot: OrderBookSnapshot;
}

/**
 * Memoized Bid Row to avoid full-ladder re-renders during high-frequency micro-ticks
 */
const BidRow = memo(({ level }: { level: OrderBookLevel }) => {
  return (
    <div className="relative flex items-center justify-between text-xs py-1 px-3 hover:bg-white/5 font-mono tabular-nums transition-colors group">
      {/* Cumulative Depth Background Bar */}
      <div
        className="absolute inset-y-0 right-0 bg-bid-bar pointer-events-none transition-all duration-150 ease-out"
        style={{ width: `${level.depthPercent}%` }}
      />

      <span className="relative z-10 text-slate-400 group-hover:text-slate-200">
        {Math.round(level.total)}
      </span>
      <span className="relative z-10 text-slate-300 font-medium">
        {Math.round(level.quantity)}
      </span>
      <span className="relative z-10 text-bid font-bold text-right">
        {level.price.toFixed(2)}
      </span>
    </div>
  );
});

BidRow.displayName = 'BidRow';

/**
 * Memoized Ask Row to avoid full-ladder re-renders during high-frequency micro-ticks
 */
const AskRow = memo(({ level }: { level: OrderBookLevel }) => {
  return (
    <div className="relative flex items-center justify-between text-xs py-1 px-3 hover:bg-white/5 font-mono tabular-nums transition-colors group">
      {/* Cumulative Depth Background Bar */}
      <div
        className="absolute inset-y-0 left-0 bg-ask-bar pointer-events-none transition-all duration-150 ease-out"
        style={{ width: `${level.depthPercent}%` }}
      />

      <span className="relative z-10 text-ask font-bold">
        {level.price.toFixed(2)}
      </span>
      <span className="relative z-10 text-slate-300 font-medium text-center">
        {Math.round(level.quantity)}
      </span>
      <span className="relative z-10 text-slate-400 group-hover:text-slate-200 text-right">
        {Math.round(level.total)}
      </span>
    </div>
  );
});

AskRow.displayName = 'AskRow';

export const OrderBookLadder: React.FC<OrderBookLadderProps> = ({ snapshot }) => {
  const { bids, asks, spread, spreadPercent, midPrice, lastPrice } = snapshot;

  return (
    <div className="bg-terminal-panel border border-terminal-border rounded-lg shadow-md flex flex-col h-full overflow-hidden">
      {/* Terminal Order Book Top Bar */}
      <div className="px-4 py-2.5 border-b border-terminal-border flex items-center justify-between bg-terminal-panel/90">
        <div className="flex items-center space-x-2">
          <Layers className="w-4 h-4 text-cyan-400" />
          <h2 className="text-xs font-bold uppercase tracking-wider text-slate-200">
            Order Book Depth (L2)
          </h2>
        </div>

        {/* Real-time Spread & Mid-price display */}
        <div className="flex items-center space-x-4 text-xs font-mono tabular-nums">
          <div className="flex items-center space-x-1.5 bg-terminal-card border border-terminal-border px-2.5 py-1 rounded">
            <span className="text-[11px] text-terminal-muted uppercase">Spread:</span>
            <span className="font-bold text-slate-200">₹{spread.toFixed(2)}</span>
            <span className="text-[10px] text-terminal-muted">({spreadPercent.toFixed(3)}%)</span>
          </div>

          <div className="flex items-center space-x-1 bg-terminal-card border border-terminal-border px-2.5 py-1 rounded">
            <span className="text-[11px] text-terminal-muted uppercase">Mid:</span>
            <span className="font-bold text-cyan-400">₹{midPrice.toFixed(2)}</span>
          </div>
        </div>
      </div>

      {/* Two-Column Depth Table Header */}
      <div className="grid grid-cols-2 border-b border-terminal-border text-[11px] font-semibold text-terminal-muted uppercase tracking-wider bg-terminal-card/40">
        {/* Bids Header */}
        <div className="grid grid-cols-3 px-3 py-1.5 border-r border-terminal-border text-center">
          <span className="text-left">Cumul</span>
          <span>Size (Shares)</span>
          <span className="text-right text-bid">Bid (INR)</span>
        </div>

        {/* Asks Header */}
        <div className="grid grid-cols-3 px-3 py-1.5 text-center">
          <span className="text-left text-ask">Ask (INR)</span>
          <span>Size (Shares)</span>
          <span className="text-right">Cumul</span>
        </div>
      </div>

      {/* Ladder Content - Two Columns Side-by-Side */}
      <div className="grid grid-cols-2 flex-1 min-h-[460px] max-h-[580px] overflow-y-auto no-scrollbar">
        {/* Left Column: BIDS (Price descending) */}
        <div className="border-r border-terminal-border divide-y divide-terminal-border/20">
          {bids.map((level) => (
            <BidRow key={`bid-${level.price}`} level={level} />
          ))}
          {bids.length === 0 && (
            <div className="text-center py-12 text-terminal-muted text-xs">
              No resting bids
            </div>
          )}
        </div>

        {/* Right Column: ASKS (Price ascending) */}
        <div className="divide-y divide-terminal-border/20">
          {asks.map((level) => (
            <AskRow key={`ask-${level.price}`} level={level} />
          ))}
          {asks.length === 0 && (
            <div className="text-center py-12 text-terminal-muted text-xs">
              No resting asks
            </div>
          )}
        </div>
      </div>

      {/* Spread Bar Divider Footer */}
      <div className="px-4 py-2 border-t border-terminal-border bg-terminal-card/60 flex items-center justify-between text-xs font-mono">
        <div className="flex items-center space-x-2">
          <span className="text-terminal-muted text-[11px]">Best Bid:</span>
          <span className="font-bold text-bid tabular-nums">
            ₹{snapshot.bestBid.toFixed(2)}
          </span>
        </div>

        <div className="flex items-center space-x-1 font-bold">
          {lastPrice >= midPrice ? (
            <ArrowUp className="w-3.5 h-3.5 text-bid animate-bounce" />
          ) : (
            <ArrowDown className="w-3.5 h-3.5 text-ask animate-bounce" />
          )}
          <span className={lastPrice >= midPrice ? 'text-bid' : 'text-ask'}>
            ₹{lastPrice.toFixed(2)}
          </span>
        </div>

        <div className="flex items-center space-x-2">
          <span className="text-terminal-muted text-[11px]">Best Ask:</span>
          <span className="font-bold text-ask tabular-nums">
            ₹{snapshot.bestAsk.toFixed(2)}
          </span>
        </div>
      </div>
    </div>
  );
};

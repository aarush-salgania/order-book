import React, { memo } from 'react';
import type { OrderBookLevel, OrderBookSnapshot } from '../types/market';

interface OrderBookLadderProps {
  snapshot: OrderBookSnapshot;
}

const BidRow = memo(({ level }: { level: OrderBookLevel }) => {
  return (
    <div className="relative flex items-center justify-between text-xs py-1 px-3 hover:bg-slate-800/60 font-mono transition">
      {/* Subtle depth bar */}
      <div
        className="absolute inset-y-0 right-0 bg-green-500/10 pointer-events-none"
        style={{ width: `${level.depthPercent}%` }}
      />
      <span className="relative z-10 text-slate-400">
        {Math.round(level.total)}
      </span>
      <span className="relative z-10 text-slate-300">
        {Math.round(level.quantity)}
      </span>
      <span className="relative z-10 text-green-400 font-medium text-right">
        {level.price.toFixed(2)}
      </span>
    </div>
  );
});

BidRow.displayName = 'BidRow';

const AskRow = memo(({ level }: { level: OrderBookLevel }) => {
  return (
    <div className="relative flex items-center justify-between text-xs py-1 px-3 hover:bg-slate-800/60 font-mono transition">
      {/* Subtle depth bar */}
      <div
        className="absolute inset-y-0 left-0 bg-red-500/10 pointer-events-none"
        style={{ width: `${level.depthPercent}%` }}
      />
      <span className="relative z-10 text-red-400 font-medium">
        {level.price.toFixed(2)}
      </span>
      <span className="relative z-10 text-slate-300 text-center">
        {Math.round(level.quantity)}
      </span>
      <span className="relative z-10 text-slate-400 text-right">
        {Math.round(level.total)}
      </span>
    </div>
  );
});

AskRow.displayName = 'AskRow';

export const OrderBookLadder: React.FC<OrderBookLadderProps> = ({ snapshot }) => {
  const { bids, asks, spread, midPrice, lastPrice } = snapshot;

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-lg shadow-sm flex flex-col h-full overflow-hidden">
      {/* Order Book Header */}
      <div className="px-4 py-2.5 border-b border-slate-800 flex items-center justify-between bg-slate-900">
        <div>
          <h2 className="text-xs font-semibold uppercase tracking-wider text-slate-200">
            Order Book
          </h2>
          <p className="text-[11px] text-slate-400">Level 2 Depth</p>
        </div>

        <div className="flex items-center space-x-3 text-xs">
          <div className="text-slate-400">
            Spread: <span className="font-mono text-slate-200 font-medium">₹{spread.toFixed(2)}</span>
          </div>
          <div className="text-slate-400">
            Mid: <span className="font-mono text-slate-200 font-medium">₹{midPrice.toFixed(2)}</span>
          </div>
        </div>
      </div>

      {/* Two-Column Table Header */}
      <div className="grid grid-cols-2 border-b border-slate-800 text-[11px] font-medium text-slate-400 bg-slate-800/40">
        {/* Bids Header */}
        <div className="grid grid-cols-3 px-3 py-1.5 border-r border-slate-800 text-center">
          <span className="text-left">Cumul</span>
          <span>Shares</span>
          <span className="text-right text-green-400 font-semibold">Bid (₹)</span>
        </div>

        {/* Asks Header */}
        <div className="grid grid-cols-3 px-3 py-1.5 text-center">
          <span className="text-left text-red-400 font-semibold">Ask (₹)</span>
          <span>Shares</span>
          <span className="text-right">Cumul</span>
        </div>
      </div>

      {/* Ladder Content */}
      <div className="grid grid-cols-2 flex-1 min-h-[460px] max-h-[580px] overflow-y-auto">
        {/* BIDS */}
        <div className="border-r border-slate-800 divide-y divide-slate-800/40">
          {bids.map((level) => (
            <BidRow key={`bid-${level.price}`} level={level} />
          ))}
          {bids.length === 0 && (
            <div className="text-center py-12 text-slate-500 text-xs">
              No active bids
            </div>
          )}
        </div>

        {/* ASKS */}
        <div className="divide-y divide-slate-800/40">
          {asks.map((level) => (
            <AskRow key={`ask-${level.price}`} level={level} />
          ))}
          {asks.length === 0 && (
            <div className="text-center py-12 text-slate-500 text-xs">
              No active asks
            </div>
          )}
        </div>
      </div>

      {/* Footer Info */}
      <div className="px-4 py-2 border-t border-slate-800 bg-slate-800/40 flex items-center justify-between text-xs">
        <div className="text-slate-400 text-[11px]">
          Best Bid: <span className="font-mono text-green-400 font-medium">₹{snapshot.bestBid.toFixed(2)}</span>
        </div>

        <div className="text-slate-300 font-mono text-xs">
          Last: <span className="font-medium text-white">₹{lastPrice.toFixed(2)}</span>
        </div>

        <div className="text-slate-400 text-[11px]">
          Best Ask: <span className="font-mono text-red-400 font-medium">₹{snapshot.bestAsk.toFixed(2)}</span>
        </div>
      </div>
    </div>
  );
};

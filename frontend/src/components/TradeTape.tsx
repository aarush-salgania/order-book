import React, { memo } from 'react';
import type { Trade } from '../types/market';

interface TradeTapeProps {
  trades: Trade[];
}

function formatTime(timestamp: number): string {
  const date = new Date(timestamp);
  const hours = date.getHours().toString().padStart(2, '0');
  const minutes = date.getMinutes().toString().padStart(2, '0');
  const seconds = date.getSeconds().toString().padStart(2, '0');
  return `${hours}:${minutes}:${seconds}`;
}

const TradeRow = memo(({ trade }: { trade: Trade }) => {
  const isBuy = trade.side === 'buy';

  return (
    <div className="grid grid-cols-4 px-3 py-1.5 text-xs font-mono items-center hover:bg-slate-800/40 transition">
      <span className="text-slate-400 text-[11px]">
        {formatTime(trade.timestamp)}
      </span>

      <div>
        <span
          className={`text-[11px] font-medium px-1.5 py-0.5 rounded ${
            isBuy ? 'text-green-400 bg-green-950/60' : 'text-red-400 bg-red-950/60'
          }`}
        >
          {isBuy ? 'BUY' : 'SELL'}
        </span>
      </div>

      <span className={`text-right font-medium ${isBuy ? 'text-green-400' : 'text-red-400'}`}>
        ₹{trade.price.toFixed(2)}
      </span>

      <span className="text-slate-300 text-right">
        {Math.round(trade.quantity)}
      </span>
    </div>
  );
});

TradeRow.displayName = 'TradeRow';

export const TradeTape: React.FC<TradeTapeProps> = ({ trades }) => {
  return (
    <div className="bg-slate-900 border border-slate-800 rounded-lg shadow-sm flex flex-col h-full overflow-hidden">
      {/* Tape Header */}
      <div className="px-4 py-2.5 border-b border-slate-800 flex items-center justify-between bg-slate-900">
        <div>
          <h2 className="text-xs font-semibold uppercase tracking-wider text-slate-200">
            Recent Trades
          </h2>
          <p className="text-[11px] text-slate-400">Execution Stream</p>
        </div>

        <span className="text-[11px] text-slate-400 bg-slate-800 px-2 py-0.5 rounded border border-slate-700">
          {trades.length} trades
        </span>
      </div>

      {/* Column Headers */}
      <div className="grid grid-cols-4 px-3 py-1.5 border-b border-slate-800 text-[11px] font-medium text-slate-400 bg-slate-800/40">
        <span>Time</span>
        <span>Side</span>
        <span className="text-right">Price (₹)</span>
        <span className="text-right">Shares</span>
      </div>

      {/* Scrolling Trade Stream */}
      <div className="flex-1 min-h-[460px] max-h-[580px] overflow-y-auto divide-y divide-slate-800/40">
        {trades.map((trade) => (
          <TradeRow key={trade.id} trade={trade} />
        ))}

        {trades.length === 0 && (
          <div className="flex items-center justify-center h-48 text-slate-500 text-xs">
            No executions yet. Submit an order to trade.
          </div>
        )}
      </div>
    </div>
  );
};

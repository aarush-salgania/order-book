import React, { useState } from 'react';
import { Send, ArrowUpRight, ArrowDownRight, CheckCircle2 } from 'lucide-react';
import type { ManualOrderPayload, OrderBookSnapshot, OrderSide } from '../types/market';

interface ManualOrderFormProps {
  snapshot: OrderBookSnapshot;
  onSubmitOrder: (order: ManualOrderPayload) => Promise<{ success: boolean; message: string }>;
}

export const ManualOrderForm: React.FC<ManualOrderFormProps> = ({
  snapshot,
  onSubmitOrder,
}) => {
  const [side, setSide] = useState<OrderSide>('buy');
  const [price, setPrice] = useState<string>('');
  const [quantity, setQuantity] = useState<string>('0.25');
  const [feedback, setFeedback] = useState<{ message: string; isError?: boolean } | null>(null);
  const [isSubmitting, setIsSubmitting] = useState(false);

  // Quick price fill buttons
  const fillBestBid = () => setPrice(snapshot.bestBid.toFixed(2));
  const fillMidPrice = () => setPrice(snapshot.midPrice.toFixed(2));
  const fillBestAsk = () => setPrice(snapshot.bestAsk.toFixed(2));

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    const numericPrice = parseFloat(price);
    const numericQty = parseFloat(quantity);

    if (isNaN(numericPrice) || numericPrice <= 0) {
      setFeedback({ message: 'Enter a valid positive price.', isError: true });
      return;
    }

    if (isNaN(numericQty) || numericQty <= 0) {
      setFeedback({ message: 'Enter a valid positive quantity.', isError: true });
      return;
    }

    setIsSubmitting(true);
    setFeedback(null);

    try {
      /* ========================================================================
       * WEBSOCKET HOOK:
       * In the future, this triggers the WebSocket client to send a LimitOrder
       * payload directly to the low-latency C++ matching engine socket.
       * ======================================================================== */
      const res = await onSubmitOrder({
        price: numericPrice,
        quantity: numericQty,
        side,
        type: 'limit',
      });

      setFeedback({ message: res.message, isError: !res.success });
      // Clear feedback after 4 seconds
      setTimeout(() => setFeedback(null), 4000);
    } catch (err) {
      setFeedback({ message: 'Failed to submit order.', isError: true });
    } finally {
      setIsSubmitting(false);
    }
  };

  const numericPrice = parseFloat(price) || 0;
  const numericQty = parseFloat(quantity) || 0;
  const estimatedTotal = numericPrice * numericQty;

  return (
    <div className="bg-terminal-panel border border-terminal-border rounded-lg p-4 shadow-md flex flex-col space-y-3.5">
      {/* Header */}
      <div className="flex items-center justify-between pb-2 border-b border-terminal-border">
        <h2 className="text-xs font-bold uppercase tracking-wider text-slate-200">
          Manual Order Entry
        </h2>
        <div className="flex items-center space-x-1">
          <span className="text-[10px] text-cyan-400 bg-cyan-950/40 border border-cyan-800/60 px-1.5 py-0.5 rounded font-mono">
            POST /api/order
          </span>
          <span className="text-[10px] text-emerald-400 bg-emerald-950/40 border border-emerald-800/60 px-1.5 py-0.5 rounded font-mono">
            LIMIT
          </span>
        </div>
      </div>

      <form onSubmit={handleSubmit} className="space-y-3">
        {/* Side Toggle: Buy / Sell */}
        <div className="grid grid-cols-2 gap-2 p-1 bg-terminal-card rounded border border-terminal-border">
          <button
            type="button"
            onClick={() => setSide('buy')}
            className={`flex items-center justify-center space-x-1.5 py-1.5 text-xs font-bold rounded transition-all ${
              side === 'buy'
                ? 'bg-bid text-black shadow-md'
                : 'text-slate-400 hover:text-slate-200'
            }`}
          >
            <ArrowUpRight className="w-3.5 h-3.5" />
            <span>BUY / BID</span>
          </button>

          <button
            type="button"
            onClick={() => setSide('sell')}
            className={`flex items-center justify-center space-x-1.5 py-1.5 text-xs font-bold rounded transition-all ${
              side === 'sell'
                ? 'bg-ask text-white shadow-md'
                : 'text-slate-400 hover:text-slate-200'
            }`}
          >
            <ArrowDownRight className="w-3.5 h-3.5" />
            <span>SELL / ASK</span>
          </button>
        </div>

        {/* Price Input & Quick Pickers */}
        <div className="space-y-1">
          <div className="flex justify-between items-center text-xs">
            <label className="text-terminal-muted font-medium">Order Price (USDT)</label>
            <div className="flex space-x-1 text-[10px]">
              <button
                type="button"
                onClick={fillBestBid}
                className="text-bid hover:underline px-1"
                title="Fill Best Bid"
              >
                Bid
              </button>
              <button
                type="button"
                onClick={fillMidPrice}
                className="text-slate-400 hover:underline px-1"
                title="Fill Mid-Price"
              >
                Mid
              </button>
              <button
                type="button"
                onClick={fillBestAsk}
                className="text-ask hover:underline px-1"
                title="Fill Best Ask"
              >
                Ask
              </button>
            </div>
          </div>

          <div className="relative">
            <input
              type="number"
              step="0.01"
              placeholder={snapshot.midPrice ? snapshot.midPrice.toFixed(2) : '64250.00'}
              value={price}
              onChange={(e) => setPrice(e.target.value)}
              className="w-full bg-terminal-card border border-terminal-border rounded px-3 py-1.5 text-xs text-white focus:outline-none focus:border-cyan-500 font-mono tabular-nums"
              required
            />
            <span className="absolute right-3 top-1.5 text-[11px] text-terminal-muted pointer-events-none">
              USDT
            </span>
          </div>
        </div>

        {/* Quantity Input & Percentage Buttons */}
        <div className="space-y-1">
          <div className="flex justify-between items-center text-xs">
            <label className="text-terminal-muted font-medium">Quantity (BTC)</label>
            <div className="flex space-x-1 text-[10px]">
              {['0.1', '0.25', '0.5', '1.0'].map((q) => (
                <button
                  key={q}
                  type="button"
                  onClick={() => setQuantity(q)}
                  className="text-slate-400 hover:text-white px-1"
                >
                  {q}
                </button>
              ))}
            </div>
          </div>

          <div className="relative">
            <input
              type="number"
              step="0.001"
              placeholder="0.25"
              value={quantity}
              onChange={(e) => setQuantity(e.target.value)}
              className="w-full bg-terminal-card border border-terminal-border rounded px-3 py-1.5 text-xs text-white focus:outline-none focus:border-cyan-500 font-mono tabular-nums"
              required
            />
            <span className="absolute right-3 top-1.5 text-[11px] text-terminal-muted pointer-events-none">
              BTC
            </span>
          </div>
        </div>

        {/* Order Value Estimation */}
        <div className="bg-terminal-card/60 border border-terminal-border/60 rounded px-2.5 py-1.5 flex justify-between items-center text-[11px] tabular-nums">
          <span className="text-terminal-muted">Estimated Value:</span>
          <span className="font-medium text-slate-200">
            ${estimatedTotal.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}{' '}
            <span className="text-terminal-muted text-[10px]">USDT</span>
          </span>
        </div>

        {/* Submission Button */}
        <button
          type="submit"
          disabled={isSubmitting}
          className={`w-full py-2 rounded text-xs font-bold transition-all flex items-center justify-center space-x-1.5 shadow-md ${
            side === 'buy'
              ? 'bg-bid hover:bg-bid-hover text-black'
              : 'bg-ask hover:bg-ask-hover text-white'
          }`}
        >
          <Send className="w-3.5 h-3.5" />
          <span>
            {isSubmitting
              ? 'TRANSMITTING...'
              : `SUBMIT ${side.toUpperCase()} ORDER`}
          </span>
        </button>
      </form>

      {/* Submission Feedback Alert */}
      {feedback && (
        <div
          className={`p-2 rounded border text-xs flex items-start space-x-1.5 animate-fadeIn ${
            feedback.isError
              ? 'bg-red-950/40 border-red-800 text-red-300'
              : 'bg-emerald-950/40 border-emerald-800 text-emerald-300'
          }`}
        >
          <CheckCircle2 className="w-3.5 h-3.5 mt-0.5 shrink-0" />
          <span className="break-all font-mono text-[11px]">{feedback.message}</span>
        </div>
      )}
    </div>
  );
};

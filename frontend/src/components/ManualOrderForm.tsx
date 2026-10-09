import React, { useState } from 'react';
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
  const [quantity, setQuantity] = useState<string>('1');
  const [feedback, setFeedback] = useState<{ message: string; isError?: boolean } | null>(null);
  const [isSubmitting, setIsSubmitting] = useState(false);

  const fillBestBid = () => setPrice(snapshot.bestBid.toFixed(2));
  const fillMidPrice = () => setPrice(snapshot.midPrice.toFixed(2));
  const fillBestAsk = () => setPrice(snapshot.bestAsk.toFixed(2));

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    const numericPrice = parseFloat(price);
    const numericQty = parseInt(quantity, 10);

    if (isNaN(numericPrice) || numericPrice <= 0) {
      setFeedback({ message: 'Please enter a valid positive price.', isError: true });
      return;
    }

    if (isNaN(numericQty) || numericQty < 1 || !Number.isInteger(numericQty)) {
      setFeedback({ message: 'Please enter a valid whole number of shares (min: 1).', isError: true });
      return;
    }

    setIsSubmitting(true);
    setFeedback(null);

    try {
      const res = await onSubmitOrder({
        price: numericPrice,
        quantity: numericQty,
        side,
        type: 'limit',
      });

      setFeedback({ message: res.message, isError: !res.success });
      setTimeout(() => setFeedback(null), 4000);
    } catch {
      setFeedback({ message: 'Failed to submit order.', isError: true });
    } finally {
      setIsSubmitting(false);
    }
  };

  const numericPrice = parseFloat(price) || 0;
  const numericQty = parseInt(quantity, 10) || 0;
  const estimatedTotal = numericPrice * numericQty;

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 space-y-3.5">
      {/* Header */}
      <div className="flex items-center justify-between pb-2 border-b border-slate-800">
        <div>
          <h2 className="text-xs font-semibold uppercase tracking-wider text-slate-200">
            Order Entry
          </h2>
          <p className="text-[11px] text-slate-400">Limit Order</p>
        </div>
      </div>

      <form onSubmit={handleSubmit} className="space-y-3 text-xs">
        {/* Side Toggle: Buy / Sell */}
        <div className="grid grid-cols-2 gap-2 p-1 bg-slate-800/80 rounded border border-slate-700">
          <button
            type="button"
            onClick={() => setSide('buy')}
            className={`py-1.5 rounded font-medium transition ${
              side === 'buy'
                ? 'bg-green-600 text-white shadow-sm'
                : 'text-slate-400 hover:text-white'
            }`}
          >
            Buy
          </button>
          <button
            type="button"
            onClick={() => setSide('sell')}
            className={`py-1.5 rounded font-medium transition ${
              side === 'sell'
                ? 'bg-red-600 text-white shadow-sm'
                : 'text-slate-400 hover:text-white'
            }`}
          >
            Sell
          </button>
        </div>

        {/* Price Input */}
        <div className="space-y-1">
          <div className="flex justify-between items-center text-slate-400">
            <label>Price (₹)</label>
            <div className="flex space-x-1.5 text-[11px]">
              <button
                type="button"
                onClick={fillBestBid}
                className="hover:text-green-400 transition"
              >
                Bid ({snapshot.bestBid.toFixed(2)})
              </button>
              <span>&bull;</span>
              <button
                type="button"
                onClick={fillMidPrice}
                className="hover:text-slate-200 transition"
              >
                Mid
              </button>
              <span>&bull;</span>
              <button
                type="button"
                onClick={fillBestAsk}
                className="hover:text-red-400 transition"
              >
                Ask ({snapshot.bestAsk.toFixed(2)})
              </button>
            </div>
          </div>

          <input
            type="number"
            step="0.05"
            placeholder={snapshot.midPrice ? snapshot.midPrice.toFixed(2) : '10500.00'}
            value={price}
            onChange={(e) => setPrice(e.target.value)}
            className="w-full bg-slate-800 border border-slate-700 rounded px-2.5 py-1.5 text-white font-mono focus:outline-none focus:border-slate-500"
            required
          />
        </div>

        {/* Quantity Input */}
        <div className="space-y-1">
          <div className="flex justify-between items-center text-slate-400">
            <label>Shares (Whole Units)</label>
            <div className="flex space-x-1 text-[11px]">
              {['1', '5', '10', '25', '50'].map((q) => (
                <button
                  key={q}
                  type="button"
                  onClick={() => setQuantity(q)}
                  className="px-1 text-slate-400 hover:text-white transition"
                >
                  {q}
                </button>
              ))}
            </div>
          </div>

          <input
            type="number"
            step="1"
            min="1"
            placeholder="1"
            value={quantity}
            onChange={(e) => {
              const val = e.target.value;
              if (val === '' || /^\d+$/.test(val)) {
                setQuantity(val);
              }
            }}
            className="w-full bg-slate-800 border border-slate-700 rounded px-2.5 py-1.5 text-white font-mono focus:outline-none focus:border-slate-500"
            required
          />
        </div>

        {/* Total Estimation */}
        <div className="flex justify-between items-center bg-slate-800/50 p-2 rounded border border-slate-800 text-[11px]">
          <span className="text-slate-400">Total Value:</span>
          <span className="font-mono font-medium text-slate-200">
            ₹{estimatedTotal.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
          </span>
        </div>

        {/* Submit Button */}
        <button
          type="submit"
          disabled={isSubmitting}
          className={`w-full py-2 rounded font-medium transition text-white ${
            side === 'buy'
              ? 'bg-green-600 hover:bg-green-500'
              : 'bg-red-600 hover:bg-red-500'
          }`}
        >
          {isSubmitting
            ? 'Sending...'
            : `Submit ${side === 'buy' ? 'Buy' : 'Sell'} Order`}
        </button>
      </form>

      {/* Feedback Message */}
      {feedback && (
        <div
          className={`p-2 rounded text-xs border ${
            feedback.isError
              ? 'bg-red-900/30 border-red-800 text-red-200'
              : 'bg-green-900/30 border-green-800 text-green-200'
          }`}
        >
          {feedback.message}
        </div>
      )}
    </div>
  );
};

import { useOrderBook } from './hooks/useOrderBook';
import { Header } from './components/Header';
import { ControlPanel } from './components/ControlPanel';
import { ManualOrderForm } from './components/ManualOrderForm';
import { OrderBookLadder } from './components/OrderBookLadder';
import { TradeTape } from './components/TradeTape';

/**
 * Main Application Dashboard: High-Performance Limit Order Book Simulator
 * 
 * =========================================================================================
 * FUTURE WEBSOCKET INTEGRATION POINT:
 * In the future, swap `useOrderBook()` default service with a WebSocket service instance:
 * 
 *   const wsService = useMemo(() => new WebSocketOrderBookService('ws://localhost:8080'), []);
 *   const { snapshot, recentTrades, config, updateConfig, sendOrder, resetBook } = useOrderBook(wsService);
 * 
 * The UI components, data structures, and state hooks are 100% decoupling-compatible.
 * =========================================================================================
 */
export function App() {
  const {
    snapshot,
    recentTrades,
    config,
    connectionStatus,
    mode,
    setMode,
    reconnect,
    updateConfig,
    sendOrder,
    resetBook,
  } = useOrderBook();

  return (
    <div className="min-h-screen bg-terminal-bg text-terminal-text flex flex-col font-mono selection:bg-cyan-900 selection:text-white">
      {/* 1. Header with market stats & C++ backend status */}
      <Header
        snapshot={snapshot}
        config={config}
        connectionStatus={connectionStatus}
        mode={mode}
        onReconnect={reconnect}
      />

      {/* 2. Main 3-Column Terminal Layout */}
      <main className="flex-1 p-3 md:p-4 max-w-[1920px] w-full mx-auto">
        <div className="grid grid-cols-1 lg:grid-cols-12 gap-4 h-full">
          {/* Left Column (25% on 12-col grid = col-span-3): Controls & Order Injection */}
          <div className="lg:col-span-3 flex flex-col space-y-4">
            <ControlPanel
              config={config}
              connectionStatus={connectionStatus}
              mode={mode}
              onSetMode={setMode}
              onReconnect={reconnect}
              onUpdateConfig={updateConfig}
              onResetBook={resetBook}
            />
            <ManualOrderForm
              snapshot={snapshot}
              onSubmitOrder={sendOrder}
            />
          </div>

          {/* Middle Column (50% on 12-col grid = col-span-6): Two-Column Depth Ladder */}
          <div className="lg:col-span-6 h-full">
            <OrderBookLadder snapshot={snapshot} />
          </div>

          {/* Right Column (25% on 12-col grid = col-span-3): Real-time Trade Tape */}
          <div className="lg:col-span-3 h-full">
            <TradeTape trades={recentTrades} />
          </div>
        </div>
      </main>

      {/* Terminal Status Bar Footer */}
      <footer className="bg-terminal-panel/80 border-t border-terminal-border px-4 py-1.5 flex items-center justify-between text-[11px] text-terminal-muted tabular-nums">
        <div className="flex items-center space-x-4">
          <span className="flex items-center space-x-1.5">
            <span
              className={`w-2 h-2 rounded-full ${
                mode === 'backend'
                  ? connectionStatus === 'connected'
                    ? 'bg-emerald-400 animate-pulse'
                    : connectionStatus === 'connecting'
                    ? 'bg-amber-400'
                    : 'bg-rose-400'
                  : 'bg-cyan-400'
              }`}
            ></span>
            <span>
              {mode === 'backend'
                ? `DATA-PROVIDER: LIVE_WS (ws://localhost:4001)`
                : `DATA-PROVIDER: MOCK_FEED_V1`}
            </span>
          </span>
          <span className="hidden sm:inline text-slate-500">|</span>
          <span className="hidden sm:inline">
            {mode === 'backend'
              ? `ENGINE: CPP_BACKEND (HTTP :4000 / WS :4001)`
              : `ENGINE: LOCAL_SIM`}
          </span>
        </div>

        <div className="flex items-center space-x-3">
          <span className="text-slate-400">
            SPREAD: <span className="text-white">₹{snapshot.spread.toFixed(2)}</span>
          </span>
          <span className="text-slate-500">|</span>
          <span className="text-slate-400">
            TOTAL BIDS: <span className="text-bid">{snapshot.bids.length}</span> / ASKS: <span className="text-ask">{snapshot.asks.length}</span>
          </span>
        </div>
      </footer>
    </div>
  );
}

export default App;

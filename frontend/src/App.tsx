import { useOrderBook } from './hooks/useOrderBook';
import { Header } from './components/Header';
import { ControlPanel } from './components/ControlPanel';
import { ManualOrderForm } from './components/ManualOrderForm';
import { OrderBookLadder } from './components/OrderBookLadder';
import { TradeTape } from './components/TradeTape';

export function App() {
  const {
    snapshot,
    recentTrades,
    engineStats,
    config,
    connectionStatus,
    mode,
    setMode,
    reconnect,
    updateConfig,
    sendOrder,
    resetBook,
    clearBook,
  } = useOrderBook();

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col">
      {/* 1. Clean Header */}
      <Header
        snapshot={snapshot}
        config={config}
        connectionStatus={connectionStatus}
        mode={mode}
        engineStats={engineStats}
        onReconnect={reconnect}
      />

      {/* 2. Main 3-Column Layout */}
      <main className="flex-1 p-4 max-w-[1600px] w-full mx-auto">
        <div className="grid grid-cols-1 lg:grid-cols-12 gap-4 h-full">
          {/* Left Column: Controls & Order Form */}
          <div className="lg:col-span-3 flex flex-col space-y-4">
            <ControlPanel
              config={config}
              connectionStatus={connectionStatus}
              mode={mode}
              onSetMode={setMode}
              onReconnect={reconnect}
              onUpdateConfig={updateConfig}
              onResetBook={resetBook}
              onClearData={clearBook}
            />
            <ManualOrderForm
              snapshot={snapshot}
              onSubmitOrder={sendOrder}
            />
          </div>

          {/* Middle Column: Order Book Depth */}
          <div className="lg:col-span-6 h-full">
            <OrderBookLadder snapshot={snapshot} />
          </div>

          {/* Right Column: Executed Trades */}
          <div className="lg:col-span-3 h-full">
            <TradeTape trades={recentTrades} />
          </div>
        </div>
      </main>

      {/* Simple College Project Footer */}
      <footer className="bg-slate-900 border-t border-slate-800 px-4 py-2 flex flex-wrap items-center justify-between text-xs text-slate-400">
        <div className="flex items-center space-x-2">
          <span>C++ Limit Order Book Matching Engine</span>
          <span>&bull;</span>
          <span className="text-slate-500">Minor Project</span>
        </div>

        <div className="flex items-center space-x-4">
          <span>Active Bids: <span className="font-mono text-green-400 font-medium">{snapshot.bids.length}</span></span>
          <span>Active Asks: <span className="font-mono text-red-400 font-medium">{snapshot.asks.length}</span></span>
        </div>
      </footer>
    </div>
  );
}

export default App;

import { useState, useEffect, useCallback, useRef } from 'react';
import type {
  OrderBookSnapshot,
  Trade,
  EngineStats,
  SimulationConfig,
  ManualOrderPayload,
  IOrderBookService,
  ConnectionStatus,
} from '../types/market';
import { backendOrderBookService } from '../services/BackendOrderBookService';
import { mockDataService } from '../services/MockDataService';

export interface UseOrderBookReturn {
  snapshot: OrderBookSnapshot;
  bids: OrderBookSnapshot['bids'];
  asks: OrderBookSnapshot['asks'];
  recentTrades: Trade[];
  engineStats: EngineStats;
  config: SimulationConfig;
  simulationStatus: 'running' | 'paused';
  connectionStatus: ConnectionStatus;
  mode: 'backend' | 'mock';
  setMode: (mode: 'backend' | 'mock') => void;
  reconnect: () => void;
  updateConfig: (cfg: Partial<SimulationConfig>) => void;
  sendOrder: (order: ManualOrderPayload) => Promise<{ success: boolean; message: string }>;
  resetBook: () => void;
  clearBook: () => void;
  service: IOrderBookService;
}

/**
 * Custom hook providing reactive order book state and live backend engine connectivity.
 * Defaults to backendOrderBookService (connected to ws://localhost:4001 and http://localhost:4000).
 * Supports hot-swapping to mockDataService if desired.
 */
export function useOrderBook(
  initialService: IOrderBookService = backendOrderBookService
): UseOrderBookReturn {
  const [activeService, setActiveService] = useState<IOrderBookService>(initialService);
  const [mode, setModeState] = useState<'backend' | 'mock'>(
    initialService === mockDataService ? 'mock' : 'backend'
  );

  const [snapshot, setSnapshot] = useState<OrderBookSnapshot>(() => activeService.getSnapshot());
  const [recentTrades, setRecentTrades] = useState<Trade[]>(() => activeService.getRecentTrades());
  const [config, setConfig] = useState<SimulationConfig>(() => activeService.getConfig());
  const [engineStats, setEngineStats] = useState<EngineStats>(() => {
    return activeService.getStats
      ? activeService.getStats()
      : { latencyNs: 142, poolUsed: 0, poolCap: 100000, poolAllocNs: 2.3 };
  });
  const [connectionStatus, setConnectionStatus] = useState<ConnectionStatus>(() => {
    return activeService.getStatus ? activeService.getStatus() : 'connected';
  });

  const tradesRef = useRef<Trade[]>(recentTrades);
  tradesRef.current = recentTrades;

  const setMode = useCallback((newMode: 'backend' | 'mock') => {
    setModeState(newMode);
    if (newMode === 'backend') {
      setActiveService(backendOrderBookService);
    } else {
      setActiveService(mockDataService);
    }
  }, []);

  useEffect(() => {
    setSnapshot(activeService.getSnapshot());
    setRecentTrades(activeService.getRecentTrades());
    setConfig(activeService.getConfig());
    if (activeService.getStats) {
      setEngineStats(activeService.getStats());
    }
    if (activeService.getStatus) {
      setConnectionStatus(activeService.getStatus());
    } else {
      setConnectionStatus('connected');
    }

    const unsubBook = activeService.subscribeBook((newSnapshot) => {
      setSnapshot(newSnapshot);
    });

    const unsubTrades = activeService.subscribeTrades((newTrade) => {
      setRecentTrades((prev) => [newTrade, ...prev.slice(0, 49)]);
    });

    const unsubConfig = activeService.subscribeConfig((newConfig) => {
      setConfig(newConfig);
    });

    const unsubStats = activeService.subscribeStats
      ? activeService.subscribeStats((newStats) => {
          setEngineStats(newStats);
        })
      : () => {};

    const unsubStatus = activeService.subscribeStatus
      ? activeService.subscribeStatus((newStatus) => {
          setConnectionStatus(newStatus);
        })
      : () => {};

    return () => {
      unsubBook();
      unsubTrades();
      unsubConfig();
      unsubStats();
      unsubStatus();
    };
  }, [activeService]);

  const updateConfig = useCallback(
    (newConfig: Partial<SimulationConfig>) => {
      activeService.updateConfig(newConfig);
    },
    [activeService]
  );

  const sendOrder = useCallback(
    async (order: ManualOrderPayload) => {
      return activeService.sendOrder(order);
    },
    [activeService]
  );

  const resetBook = useCallback(() => {
    activeService.resetBook();
  }, [activeService]);

  const clearBook = useCallback(() => {
    if (activeService.clearBook) {
      activeService.clearBook();
    } else {
      activeService.resetBook();
    }
  }, [activeService]);

  const reconnect = useCallback(() => {
    if (activeService.reconnect) {
      activeService.reconnect();
    }
  }, [activeService]);

  return {
    snapshot,
    bids: snapshot.bids,
    asks: snapshot.asks,
    recentTrades,
    engineStats,
    config,
    simulationStatus: config.isRunning ? 'running' : 'paused',
    connectionStatus,
    mode,
    setMode,
    reconnect,
    updateConfig,
    sendOrder,
    resetBook,
    clearBook,
    service: activeService,
  };
}

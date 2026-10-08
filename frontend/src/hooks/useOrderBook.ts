import { useState, useEffect, useCallback, useRef } from 'react';
import type {
  OrderBookSnapshot,
  Trade,
  SimulationConfig,
  ManualOrderPayload,
  IOrderBookService,
} from '../types/market';
import { mockDataService } from '../services/MockDataService';

export interface UseOrderBookReturn {
  snapshot: OrderBookSnapshot;
  bids: OrderBookSnapshot['bids'];
  asks: OrderBookSnapshot['asks'];
  recentTrades: Trade[];
  config: SimulationConfig;
  simulationStatus: 'running' | 'paused';
  updateConfig: (cfg: Partial<SimulationConfig>) => void;
  sendOrder: (order: ManualOrderPayload) => Promise<{ success: boolean; message: string }>;
  resetBook: () => void;
  service: IOrderBookService;
}

/**
 * Custom hook providing reactive order book state and simulation control.
 * Injects mockDataService by default, with support for swapping to any IOrderBookService.
 */
export function useOrderBook(service: IOrderBookService = mockDataService): UseOrderBookReturn {
  const [snapshot, setSnapshot] = useState<OrderBookSnapshot>(() => service.getSnapshot());
  const [recentTrades, setRecentTrades] = useState<Trade[]>(() => service.getRecentTrades());
  const [config, setConfig] = useState<SimulationConfig>(() => service.getConfig());

  const tradesRef = useRef<Trade[]>(recentTrades);
  tradesRef.current = recentTrades;

  useEffect(() => {
    const unsubBook = service.subscribeBook((newSnapshot) => {
      setSnapshot(newSnapshot);
    });

    const unsubTrades = service.subscribeTrades((newTrade) => {
      setRecentTrades((prev) => [newTrade, ...prev.slice(0, 49)]);
    });

    const unsubConfig = service.subscribeConfig((newConfig) => {
      setConfig(newConfig);
    });

    return () => {
      unsubBook();
      unsubTrades();
      unsubConfig();
    };
  }, [service]);

  const updateConfig = useCallback(
    (newConfig: Partial<SimulationConfig>) => {
      service.updateConfig(newConfig);
    },
    [service]
  );

  const sendOrder = useCallback(
    async (order: ManualOrderPayload) => {
      return service.sendOrder(order);
    },
    [service]
  );

  const resetBook = useCallback(() => {
    service.resetBook();
  }, [service]);

  return {
    snapshot,
    bids: snapshot.bids,
    asks: snapshot.asks,
    recentTrades,
    config,
    simulationStatus: config.isRunning ? 'running' : 'paused',
    updateConfig,
    sendOrder,
    resetBook,
    service,
  };
}

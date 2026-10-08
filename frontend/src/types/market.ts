export type OrderSide = 'buy' | 'sell';

export type OrderType = 'limit' | 'market';

export interface OrderBookLevel {
  price: number;
  quantity: number;
  total: number;
  depthPercent: number;
}

export interface OrderBookSnapshot {
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
  bestBid: number;
  bestAsk: number;
  spread: number;
  spreadPercent: number;
  midPrice: number;
  lastPrice: number;
  priceChangePercent24h: number;
  high24h: number;
  low24h: number;
  volume24h: number;
  timestamp: number;
}

export interface Trade {
  id: string;
  price: number;
  quantity: number;
  side: OrderSide;
  timestamp: number;
}

export interface ManualOrderPayload {
  price: number;
  quantity: number;
  side: OrderSide;
  type: OrderType;
}

export interface SimulationConfig {
  isRunning: boolean;
  activeBots: number; // 1 - 100
  updateSpeedMs: number; // 100 - 2000 ms
}

export interface OrderBookStats {
  totalBidLiquidity: number;
  totalAskLiquidity: number;
  tradesPerMinute: number;
  lastTradeSide?: OrderSide;
}

export type OrderBookListener = (snapshot: OrderBookSnapshot) => void;
export type TradeListener = (trade: Trade) => void;
export type ConfigListener = (config: SimulationConfig) => void;

/**
 * Service interface decoupling the UI from data generation.
 * This contract enables 1:1 swapping between MockDataService and the future C++ WebSocket client.
 */
export interface IOrderBookService {
  subscribeBook(listener: OrderBookListener): () => void;
  subscribeTrades(listener: TradeListener): () => void;
  subscribeConfig(listener: ConfigListener): () => void;
  updateConfig(config: Partial<SimulationConfig>): void;
  sendOrder(order: ManualOrderPayload): Promise<{ success: boolean; message: string }>;
  resetBook(): void;
  getConfig(): SimulationConfig;
  getSnapshot(): OrderBookSnapshot;
  getRecentTrades(): Trade[];
}

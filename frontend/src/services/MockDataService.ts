import type {
  IOrderBookService,
  OrderBookLevel,
  OrderBookListener,
  OrderBookSnapshot,
  Trade,
  TradeListener,
  ConfigListener,
  StatsListener,
  EngineStats,
  SimulationConfig,
  ManualOrderPayload,
  OrderSide,
} from '../types/market';

/**
 * MockDataService simulates market feeds for testing the frontend visualization.
 * 
 * =========================================================================================
 * FUTURE C++ INTEGRATION ARCHITECTURE:
 * When connecting to the low-latency C++ Limit Order Book backend:
 * 1. Replace this class or create `WebSocketOrderBookService implements IOrderBookService`.
 * 2. Connect to the C++ WebSocket gateway (e.g., ws://localhost:8080/orderbook).
 * 3. Ingest high-frequency binary/JSON book deltas and parse into OrderBookSnapshot.
 * 4. Dispatch manual orders directly to the C++ order intake pipe.
 * =========================================================================================
 */
export class MockDataService implements IOrderBookService {
  private config: SimulationConfig = {
    isRunning: true,
    activeBots: 15,
    updateSpeedMs: 250,
  };

  private basePrice = 10520.0;
  private lastPrice = 10520.0;
  private high24h = 10680.0;
  private low24h = 10410.0;
  private volume24h = 245890;
  private priceChangePercent24h = 1.05;

  private bids: Map<number, number> = new Map(); // price -> quantity
  private asks: Map<number, number> = new Map(); // price -> quantity
  private recentTrades: Trade[] = [];

  private bookListeners: Set<OrderBookListener> = new Set();
  private tradeListeners: Set<TradeListener> = new Set();
  private configListeners: Set<ConfigListener> = new Set();
  private statsListeners: Set<StatsListener> = new Set();

  private stats: EngineStats = {
    latencyNs: 145,
    poolUsed: 18,
    poolCap: 100000,
    poolAllocNs: 2.3,
  };

  private intervalTimer: ReturnType<typeof setInterval> | null = null;
  private readonly depthLevels = 18;
  private readonly tickSize = 0.5;

  constructor() {
    this.initializeOrderBook();
    this.seedInitialTrades();
    this.startSimulation();
  }

  // ============================================================================
  // WEBSOCKET HOOK: Future connection lifecycle for C++ gateway
  // ============================================================================
  /*
  private ws: WebSocket | null = null;

  public connectWebSocket(url = 'ws://localhost:8080/ws') {
    this.ws = new WebSocket(url);
    this.ws.binaryType = 'arraybuffer';
    this.ws.onopen = () => console.info('[C++ WS] Connected to matching engine feed');
    this.ws.onmessage = (event) => this.handleCppMessage(event.data);
    this.ws.onclose = () => console.warn('[C++ WS] Disconnected, scheduling reconnect...');
  }

  private handleCppMessage(data: ArrayBuffer | string) {
    // Parse binary/JSON packets sent by C++ matching engine
    // and trigger this.notifyBookListeners() / this.notifyTradeListeners()
  }
  */

  /**
   * Generates initial snapshot of bids and asks around the base price.
   */
  private initializeOrderBook(): void {
    this.bids.clear();
    this.asks.clear();

    // Generate bids descending from basePrice - tickSize
    for (let i = 1; i <= this.depthLevels; i++) {
      const price = +(this.basePrice - i * this.tickSize).toFixed(2);
      const baseQty = Math.round(5 + i * 2 + Math.random() * 15);
      this.bids.set(price, baseQty);
    }

    // Generate asks ascending from basePrice + tickSize
    for (let i = 1; i <= this.depthLevels; i++) {
      const price = +(this.basePrice + i * this.tickSize).toFixed(2);
      const baseQty = Math.round(5 + i * 2 + Math.random() * 15);
      this.asks.set(price, baseQty);
    }
  }

  /**
   * Seeds historical trades for initial view.
   */
  private seedInitialTrades(): void {
    const now = Date.now();
    for (let i = 20; i >= 0; i--) {
      const side: OrderSide = Math.random() > 0.48 ? 'buy' : 'sell';
      const offset = (Math.random() - 0.5) * 6;
      const price = +(this.basePrice + offset).toFixed(2);
      const quantity = Math.round(1 + Math.random() * 20);

      this.recentTrades.push({
        id: `trade-seed-${i}-${Math.random().toString(36).substring(2, 7)}`,
        price,
        quantity,
        side,
        timestamp: now - i * 1200,
      });
    }
  }

  /**
   * Starts the simulation interval timer.
   */
  private startSimulation(): void {
    if (this.intervalTimer) {
      clearInterval(this.intervalTimer);
      this.intervalTimer = null;
    }

    if (!this.config.isRunning) return;

    this.intervalTimer = setInterval(() => {
      this.simulateTick();
    }, this.config.updateSpeedMs);
  }

  /**
   * Core simulation tick: modifies book quantities, shifts spread slightly, and generates fake trades.
   */
  private simulateTick(): void {
    const { activeBots } = this.config;
    if (activeBots <= 0) return;

    // Number of micro-actions this tick scales with activeBots
    const actionsCount = Math.max(1, Math.min(6, Math.round(activeBots / 16)));

    for (let a = 0; a < actionsCount; a++) {
      const isBidUpdate = Math.random() > 0.5;
      const targetMap = isBidUpdate ? this.bids : this.asks;
      const prices = Array.from(targetMap.keys());

      if (prices.length === 0) continue;

      // Pick a random level to mutate
      const targetPrice = prices[Math.floor(Math.random() * prices.length)];
      const currentQty = targetMap.get(targetPrice) || 0.5;

      // Random quantity fluctuation: add or remove volume
      const delta = (Math.random() - 0.48) * 0.4;
      const newQty = Math.max(0.05, +(currentQty + delta).toFixed(4));
      targetMap.set(targetPrice, newQty);
    }

    // Dynamic price walk: slight pressure on base price
    const drift = (Math.random() - 0.5) * this.tickSize;
    if (Math.random() < 0.25) {
      this.basePrice = +(this.basePrice + drift).toFixed(2);
    }

    // Occasional trade generation (scaled by bot count)
    const tradeChance = Math.min(0.85, 0.2 + (activeBots / 100) * 0.6);
    if (Math.random() < tradeChance) {
      this.generateSimulatedTrade();
    }

    // Notify UI subscribers of updated book
    this.notifyBookListeners();
  }

  /**
   * Emits a simulated executed trade.
   */
  private generateSimulatedTrade(): void {
    const side: OrderSide = Math.random() > 0.5 ? 'buy' : 'sell';
    const bestBid = this.getBestBid();
    const bestAsk = this.getBestAsk();

    if (side === 'buy' && (bestAsk <= 0 || bestAsk >= 50000)) return;
    if (side === 'sell' && (bestBid <= 0 || bestBid >= 50000)) return;

    const price = side === 'buy' ? bestAsk : bestBid;
    const quantity = Math.round(1 + Math.random() * 15);

    this.lastPrice = price;
    this.volume24h += quantity;

    // Decrement resting order on opposite side
    if (side === 'buy') {
      const curAsk = this.asks.get(price) || 0;
      if (curAsk <= quantity) this.asks.delete(price);
      else this.asks.set(price, Math.round(curAsk - quantity));
    } else {
      const curBid = this.bids.get(price) || 0;
      if (curBid <= quantity) this.bids.delete(price);
      else this.bids.set(price, Math.round(curBid - quantity));
    }

    const latencyNs = Math.floor(130 + Math.random() * 35);
    const poolUsed = Math.floor(14 + Math.random() * 12);

    const trade: Trade = {
      id: `tr-${Date.now()}-${Math.random().toString(36).substring(2, 6)}`,
      price,
      quantity,
      side,
      timestamp: Date.now(),
      latencyNs,
      poolUsed,
      poolCap: 100000,
      poolAllocNs: 2.3,
    };

    this.stats = {
      latencyNs,
      poolUsed,
      poolCap: 100000,
      poolAllocNs: 2.3,
    };
    this.notifyStatsListeners(this.stats);

    // Keep rolling buffer of latest 50 trades
    this.recentTrades = [trade, ...this.recentTrades.slice(0, 49)];
    this.notifyTradeListeners(trade);
    this.notifyBookListeners();
  }

  private getBestBid(): number {
    const prices = Array.from(this.bids.keys());
    return prices.length > 0 ? Math.max(...prices) : +(this.basePrice - this.tickSize).toFixed(2);
  }

  private getBestAsk(): number {
    const prices = Array.from(this.asks.keys());
    return prices.length > 0 ? Math.min(...prices) : +(this.basePrice + this.tickSize).toFixed(2);
  }

  /**
   * Computes clean OrderBookSnapshot with depth percent bars and spread calculations.
   */
  public getSnapshot(): OrderBookSnapshot {
    // Sort Bids descending (highest buy offer first)
    const sortedBidPrices = Array.from(this.bids.keys()).sort((a, b) => b - a).slice(0, this.depthLevels);
    let cumulativeBidTotal = 0;
    const rawBids: { price: number; quantity: number; total: number }[] = [];

    for (const price of sortedBidPrices) {
      const quantity = this.bids.get(price) || 0;
      cumulativeBidTotal += quantity;
      rawBids.push({ price, quantity, total: +cumulativeBidTotal.toFixed(4) });
    }

    // Sort Asks ascending (lowest sell offer first)
    const sortedAskPrices = Array.from(this.asks.keys()).sort((a, b) => a - b).slice(0, this.depthLevels);
    let cumulativeAskTotal = 0;
    const rawAsks: { price: number; quantity: number; total: number }[] = [];

    for (const price of sortedAskPrices) {
      const quantity = this.asks.get(price) || 0;
      cumulativeAskTotal += quantity;
      rawAsks.push({ price, quantity, total: +cumulativeAskTotal.toFixed(4) });
    }

    // Max depth across both sides to normalize cumulative volume bars (0 - 100%)
    const maxDepth = Math.max(cumulativeBidTotal, cumulativeAskTotal, 0.01);

    const bids: OrderBookLevel[] = rawBids.map((b) => ({
      ...b,
      depthPercent: Math.min(100, Math.round((b.total / maxDepth) * 100)),
    }));

    const asks: OrderBookLevel[] = rawAsks.map((a) => ({
      ...a,
      depthPercent: Math.min(100, Math.round((a.total / maxDepth) * 100)),
    }));

    const bestBid = bids.length > 0 ? bids[0].price : +(this.basePrice - this.tickSize).toFixed(2);
    const bestAsk = asks.length > 0 ? asks[0].price : +(this.basePrice + this.tickSize).toFixed(2);
    const spread = +(bestAsk - bestBid).toFixed(2);
    const midPrice = +((bestBid + bestAsk) / 2).toFixed(2);
    const spreadPercent = midPrice > 0 ? +((spread / midPrice) * 100).toFixed(3) : 0;

    return {
      bids,
      asks,
      bestBid,
      bestAsk,
      spread,
      spreadPercent,
      midPrice,
      lastPrice: this.lastPrice,
      priceChangePercent24h: this.priceChangePercent24h,
      high24h: this.high24h,
      low24h: this.low24h,
      volume24h: +this.volume24h.toFixed(2),
      timestamp: Date.now(),
    };
  }

  public getRecentTrades(): Trade[] {
    return [...this.recentTrades];
  }

  public getConfig(): SimulationConfig {
    return { ...this.config };
  }

  // ============================================================================
  // MANUAL ORDER INGESTION (WebSocket Emitter Hook)
  // ============================================================================
  public async sendOrder(order: ManualOrderPayload): Promise<{ success: boolean; message: string }> {
    const price = +order.price.toFixed(2);
    let qty = Math.max(1, Math.round(order.quantity));

    if (order.side === 'buy') {
      const bestAsk = this.getBestAsk();
      if (bestAsk > 0 && price >= bestAsk && this.asks.has(bestAsk)) {
        // Cross against best ask
        const restingAskQty = this.asks.get(bestAsk) || 0;
        const fillQty = Math.min(qty, restingAskQty);
        if (restingAskQty <= fillQty) this.asks.delete(bestAsk);
        else this.asks.set(bestAsk, restingAskQty - fillQty);

        qty -= fillQty;
        this.lastPrice = bestAsk;
        this.recentTrades = [
          {
            id: `tr-${Date.now()}-${Math.random().toString(36).substring(2, 6)}`,
            price: bestAsk,
            quantity: fillQty,
            side: 'buy',
            timestamp: Date.now(),
            latencyNs: 140,
            poolUsed: 15,
            poolCap: 100000,
            poolAllocNs: 2.3,
          },
          ...this.recentTrades.slice(0, 49),
        ];
        this.notifyTradeListeners(this.recentTrades[0]);
      }
      if (qty > 0) {
        const existing = this.bids.get(price) || 0;
        this.bids.set(price, existing + qty);
      }
    } else {
      const bestBid = this.getBestBid();
      if (bestBid > 0 && price <= bestBid && this.bids.has(bestBid)) {
        // Cross against best bid
        const restingBidQty = this.bids.get(bestBid) || 0;
        const fillQty = Math.min(qty, restingBidQty);
        if (restingBidQty <= fillQty) this.bids.delete(bestBid);
        else this.bids.set(bestBid, restingBidQty - fillQty);

        qty -= fillQty;
        this.lastPrice = bestBid;
        this.recentTrades = [
          {
            id: `tr-${Date.now()}-${Math.random().toString(36).substring(2, 6)}`,
            price: bestBid,
            quantity: fillQty,
            side: 'sell',
            timestamp: Date.now(),
            latencyNs: 140,
            poolUsed: 15,
            poolCap: 100000,
            poolAllocNs: 2.3,
          },
          ...this.recentTrades.slice(0, 49),
        ];
        this.notifyTradeListeners(this.recentTrades[0]);
      }
      if (qty > 0) {
        const existing = this.asks.get(price) || 0;
        this.asks.set(price, existing + qty);
      }
    }

    this.notifyBookListeners();

    return {
      success: true,
      message: `[Simulated] Order processed: ${order.side.toUpperCase()} ${order.quantity} shares @ ₹${order.price.toFixed(2)}`,
    };
  }

  public updateConfig(newConfig: Partial<SimulationConfig>): void {
    const prevSpeed = this.config.updateSpeedMs;
    const prevRunning = this.config.isRunning;

    this.config = { ...this.config, ...newConfig };

    if (prevSpeed !== this.config.updateSpeedMs || prevRunning !== this.config.isRunning) {
      this.startSimulation();
    }

    this.notifyConfigListeners();
  }

  public resetBook(): void {
    this.initializeOrderBook();
    this.notifyBookListeners();
  }

  public clearBook(): void {
    this.bids.clear();
    this.asks.clear();
    this.recentTrades = [];
    this.volume24h = 0;
    this.stats = {
      latencyNs: 0,
      poolUsed: 0,
      poolCap: 100000,
      poolAllocNs: 2.3,
    };
    this.notifyBookListeners();
    this.notifyStatsListeners(this.stats);
  }

  // ============================================================================
  // Subscription Listeners (Observer Pattern)
  // ============================================================================
  public subscribeBook(listener: OrderBookListener): () => void {
    this.bookListeners.add(listener);
    listener(this.getSnapshot());
    return () => this.bookListeners.delete(listener);
  }

  public subscribeTrades(listener: TradeListener): () => void {
    this.tradeListeners.add(listener);
    return () => this.tradeListeners.delete(listener);
  }

  public subscribeConfig(listener: ConfigListener): () => void {
    this.configListeners.add(listener);
    listener(this.getConfig());
    return () => this.configListeners.delete(listener);
  }

  public subscribeStats(listener: StatsListener): () => void {
    this.statsListeners.add(listener);
    listener(this.stats);
    return () => this.statsListeners.delete(listener);
  }

  public getStats(): EngineStats {
    return { ...this.stats };
  }

  private notifyStatsListeners(stats: EngineStats): void {
    this.stats = { ...stats };
    this.statsListeners.forEach((l) => l(this.stats));
  }

  private notifyBookListeners(): void {
    if (this.bookListeners.size === 0) return;
    const snapshot = this.getSnapshot();
    this.bookListeners.forEach((listener) => listener(snapshot));
  }

  private notifyTradeListeners(trade: Trade): void {
    this.tradeListeners.forEach((listener) => listener(trade));
  }

  private notifyConfigListeners(): void {
    const config = this.getConfig();
    this.configListeners.forEach((listener) => listener(config));
  }
}

export const mockDataService = new MockDataService();

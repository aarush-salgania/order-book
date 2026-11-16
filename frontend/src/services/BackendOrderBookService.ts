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
  ConnectionListener,
  ConnectionStatus,
  SimulationConfig,
  ManualOrderPayload,
  OrderSide,
} from '../types/market';

/**
 * BackendOrderBookService coordinates real-time communication between
 * the React frontend and the backend engine running on:
 * - Express HTTP API: http://localhost:4000/api/order
 * - WebSocket Server: ws://localhost:4001
 */
export class BackendOrderBookService implements IOrderBookService {
  private httpUrl: string;
  private wsUrl: string;
  private ws: WebSocket | null = null;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private isDestroyed = false;

  private status: ConnectionStatus = 'disconnected';

  private config: SimulationConfig = {
    isRunning: false,
    activeBots: 0,
    updateSpeedMs: 500,
  };

  private basePrice = 10520.0;
  private lastPrice = 10520.0;
  private high24h = 10680.0;
  private low24h = 10410.0;
  private volume24h = 245890;
  private priceChangePercent24h = 1.05;

  private bids: Map<number, number> = new Map();
  private asks: Map<number, number> = new Map();
  private recentTrades: Trade[] = [];

  private bookListeners: Set<OrderBookListener> = new Set();
  private tradeListeners: Set<TradeListener> = new Set();
  private configListeners: Set<ConfigListener> = new Set();
  private statusListeners: Set<ConnectionListener> = new Set();
  private statsListeners: Set<StatsListener> = new Set();

  private currentStats: EngineStats = {
    latencyNs: 142,
    poolUsed: 0,
    poolCap: 100000,
    poolAllocNs: 2.3,
  };

  private simInterval: ReturnType<typeof setInterval> | null = null;
  private readonly depthLevels = 18;
  private readonly tickSize = 0.5;

  constructor(
    httpUrl = 'http://localhost:4000',
    wsUrl = 'ws://localhost:4001'
  ) {
    this.httpUrl = httpUrl;
    this.wsUrl = wsUrl;

    this.initializeOrderBook();
    this.seedInitialTrades();
    this.startBackgroundSim();
    this.connectWebSocket();
  }

  // ============================================================================
  // WebSocket Lifecycle
  // ============================================================================
  public connectWebSocket(): void {
    if (this.isDestroyed) return;

    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
      return;
    }

    this.setStatus('connecting');

    try {
      this.ws = new WebSocket(this.wsUrl);

      this.ws.onopen = () => {
        console.info(`[Backend WS] Connected to ${this.wsUrl}`);
        this.setStatus('connected');
      };

      this.ws.onmessage = (event: MessageEvent) => {
        this.handleIncomingMessage(event.data);
      };

      this.ws.onclose = () => {
        if (!this.isDestroyed) {
          console.warn(`[Backend WS] Connection closed, will retry...`);
          this.setStatus('disconnected');
          this.scheduleReconnect();
        }
      };

      this.ws.onerror = (err) => {
        console.warn(`[Backend WS] Error connecting to ${this.wsUrl}:`, err);
        this.setStatus('disconnected');
      };
    } catch (err) {
      console.error('[Backend WS] Socket instantiation failed:', err);
      this.setStatus('disconnected');
      this.scheduleReconnect();
    }
  }

  private scheduleReconnect(): void {
    if (this.reconnectTimer || this.isDestroyed) return;
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connectWebSocket();
    }, 3000);
  }

  public reconnect(): void {
    if (this.ws) {
      try {
        this.ws.close();
      } catch {
        // ignore
      }
      this.ws = null;
    }
    this.connectWebSocket();
  }

  private setStatus(status: ConnectionStatus): void {
    if (this.status === status) return;
    this.status = status;
    this.statusListeners.forEach((listener) => listener(status));
  }

  public getStatus(): ConnectionStatus {
    return this.status;
  }

  public getHttpUrl(): string {
    return this.httpUrl;
  }

  public getWsUrl(): string {
    return this.wsUrl;
  }

  // ============================================================================
  // Message Parser & Processing
  // ============================================================================
  private handleIncomingMessage(rawData: string | ArrayBuffer): void {
    if (typeof rawData !== 'string') return;

    const trimmed = rawData.trim();
    if (!trimmed) return;

    console.log(`[Backend WS Message Received]:`, trimmed);

    // 0. Check if backend sent ENGINE_STATS: ...
    if (trimmed.startsWith('ENGINE_STATS:')) {
      try {
        const payload = trimmed.substring('ENGINE_STATS:'.length).trim();
        const obj = JSON.parse(payload);
        this.notifyStatsListeners({
          latencyNs: Number(obj.latency_ns) || this.currentStats.latencyNs,
          poolUsed: obj.pool_used !== undefined ? Number(obj.pool_used) : this.currentStats.poolUsed,
          poolCap: Number(obj.pool_cap) || this.currentStats.poolCap,
          poolAllocNs: Number(obj.pool_alloc_ns) || this.currentStats.poolAllocNs,
        });
      } catch (e) {
        console.warn('Failed to parse ENGINE_STATS:', e);
      }
      return;
    }

    // 1. Check if backend sent TRADE_EXECUTED: ... (from dummy.cpp or similar)
    if (trimmed.startsWith('TRADE_EXECUTED:')) {
      const payload = trimmed.substring('TRADE_EXECUTED:'.length).trim();
      this.processTradePayload(payload);
      return;
    }

    // 2. Check if backend sent JSON
    if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
      try {
        const parsed = JSON.parse(trimmed);

        if (parsed.type === 'trade' || parsed.event === 'trade') {
          this.applyTrade({
            price: Number(parsed.price) || this.lastPrice,
            quantity: Number(parsed.quantity) || 0.1,
            side: (parsed.side === 'sell' ? 'sell' : 'buy') as OrderSide,
            latencyNs: parsed.latency_ns !== undefined ? Number(parsed.latency_ns) : undefined,
            poolUsed: parsed.pool_used !== undefined ? Number(parsed.pool_used) : undefined,
            poolCap: parsed.pool_cap !== undefined ? Number(parsed.pool_cap) : undefined,
            poolAllocNs: parsed.pool_alloc_ns !== undefined ? Number(parsed.pool_alloc_ns) : undefined,
          });
          return;
        }

        if (parsed.type === 'stats' || parsed.type === 'engine_stats') {
          this.notifyStatsListeners({
            latencyNs: Number(parsed.latency_ns) || this.currentStats.latencyNs,
            poolUsed: parsed.pool_used !== undefined ? Number(parsed.pool_used) : this.currentStats.poolUsed,
            poolCap: Number(parsed.pool_cap) || this.currentStats.poolCap,
            poolAllocNs: Number(parsed.pool_alloc_ns) || this.currentStats.poolAllocNs,
          });
          return;
        }

        if (parsed.type === 'snapshot' || (parsed.bids && parsed.asks)) {
          // Custom book snapshot update
          if (Array.isArray(parsed.bids) && Array.isArray(parsed.asks)) {
            this.bids.clear();
            this.asks.clear();
            for (const b of parsed.bids) this.bids.set(Number(b.price), Number(b.quantity));
            for (const a of parsed.asks) this.asks.set(Number(a.price), Number(a.quantity));
            this.notifyBookListeners();
          }
          return;
        }

        // Generic object with side, price, qty
        if (parsed.side && (parsed.price !== undefined || parsed.quantity !== undefined)) {
          this.applyTrade({
            price: Number(parsed.price) || this.lastPrice,
            quantity: Number(parsed.quantity) || 0.1,
            side: parsed.side === 'sell' ? 'sell' : 'buy',
            latencyNs: parsed.latency_ns !== undefined ? Number(parsed.latency_ns) : undefined,
            poolUsed: parsed.pool_used !== undefined ? Number(parsed.pool_used) : undefined,
            poolCap: parsed.pool_cap !== undefined ? Number(parsed.pool_cap) : undefined,
            poolAllocNs: parsed.pool_alloc_ns !== undefined ? Number(parsed.pool_alloc_ns) : undefined,
          });
          return;
        }
      } catch (e) {
        console.warn('Failed to parse JSON packet:', e);
      }
    }

    // 3. Fallback: Parse text-based trade or log
    this.processTradePayload(trimmed);
  }

  /**
   * Parses string payload like:
   * '{"side":"buy","price":64250,"quantity":0.25}' or 'BUY 0.25 64250'
   */
  private processTradePayload(payload: string): void {
    // Try JSON first
    try {
      if (payload.startsWith('{')) {
        const obj = JSON.parse(payload);
        const price = Number(obj.price) || this.lastPrice;
        const qty = Number(obj.quantity || obj.qty) || 0.25;
        const side: OrderSide = obj.side === 'sell' ? 'sell' : 'buy';
        const latencyNs = obj.latency_ns !== undefined ? Number(obj.latency_ns) : undefined;
        const poolUsed = obj.pool_used !== undefined ? Number(obj.pool_used) : undefined;
        const poolCap = obj.pool_cap !== undefined ? Number(obj.pool_cap) : undefined;
        const poolAllocNs = obj.pool_alloc_ns !== undefined ? Number(obj.pool_alloc_ns) : undefined;

        if (latencyNs !== undefined || poolUsed !== undefined) {
          this.notifyStatsListeners({
            latencyNs: latencyNs ?? this.currentStats.latencyNs,
            poolUsed: poolUsed ?? this.currentStats.poolUsed,
            poolCap: poolCap ?? this.currentStats.poolCap,
            poolAllocNs: poolAllocNs ?? this.currentStats.poolAllocNs,
          });
        }

        this.applyTrade({
          price,
          quantity: qty,
          side,
          latencyNs,
          poolUsed,
          poolCap,
          poolAllocNs,
        });
        return;
      }
    } catch {
      // not JSON
    }

    // Try regex matching: BUY/SELL <qty> <price> or BUY/SELL <price> <qty>
    const match = payload.match(/(BUY|SELL)\s+([0-9.]+)\s+([0-9.]+)/i);
    if (match) {
      const side: OrderSide = match[1].toLowerCase() === 'sell' ? 'sell' : 'buy';
      const val1 = parseFloat(match[2]);
      const val2 = parseFloat(match[3]);
      // Generally larger number is price, smaller is quantity
      const price = Math.max(val1, val2);
      const quantity = Math.min(val1, val2);
      this.applyTrade({ price, quantity, side });
      return;
    }

    // Generic trade fallback for unformatted engine output
    this.applyTrade({
      price: this.lastPrice,
      quantity: 0.1,
      side: payload.toLowerCase().includes('sell') ? 'sell' : 'buy',
    });
  }

  private applyTrade(details: {
    price: number;
    quantity: number;
    side: OrderSide;
    latencyNs?: number;
    poolUsed?: number;
    poolCap?: number;
    poolAllocNs?: number;
  }): void {
    const price = +details.price.toFixed(2);
    const quantity = Math.max(1, Math.round(details.quantity));
    const side = details.side;

    this.lastPrice = price;
    this.volume24h += quantity;

    if (price > this.high24h) this.high24h = price;
    if (price < this.low24h) this.low24h = price;

    // Adjust resting orders in ladder
    if (side === 'buy') {
      const askQty = this.asks.get(price);
      if (askQty !== undefined) {
        if (askQty <= quantity) this.asks.delete(price);
        else this.asks.set(price, Math.round(askQty - quantity));
      }
    } else {
      const bidQty = this.bids.get(price);
      if (bidQty !== undefined) {
        if (bidQty <= quantity) this.bids.delete(price);
        else this.bids.set(price, Math.round(bidQty - quantity));
      }
    }

    const trade: Trade = {
      id: `exec-${Date.now()}-${Math.random().toString(36).substring(2, 6)}`,
      price,
      quantity,
      side,
      timestamp: Date.now(),
      latencyNs: details.latencyNs,
      poolUsed: details.poolUsed,
      poolCap: details.poolCap,
      poolAllocNs: details.poolAllocNs,
    };

    this.recentTrades = [trade, ...this.recentTrades.slice(0, 49)];
    this.notifyTradeListeners(trade);
    this.notifyBookListeners();
  }

  // ============================================================================
  // Order Ingestion (HTTP & WebSocket)
  // ============================================================================
  public async sendOrder(
    order: ManualOrderPayload
  ): Promise<{ success: boolean; message: string }> {
    const formattedPrice = +order.price.toFixed(2);
    const formattedQty = Math.max(1, Math.round(order.quantity));

    // Structure the payload for the C++ engine
    const orderData = {
      side: order.side,
      price: formattedPrice,
      quantity: formattedQty,
      type: order.type,
      timestamp: Date.now(),
    };

    const orderString = JSON.stringify(orderData);

    // Add to local book optimistically so user sees their resting order immediately
    if (order.side === 'buy') {
      const existing = this.bids.get(formattedPrice) || 0;
      this.bids.set(formattedPrice, existing + formattedQty);
    } else {
      const existing = this.asks.get(formattedPrice) || 0;
      this.asks.set(formattedPrice, existing + formattedQty);
    }
    this.notifyBookListeners();

    // 1. Try sending via HTTP POST to /api/order
    try {
      const response = await fetch(`${this.httpUrl}/api/order`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ order: orderString }),
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const resJson = await response.json();
      return {
        success: true,
        message: `Order sent to C++ Engine: ${order.side.toUpperCase()} ${formattedQty} shares @ ₹${formattedPrice.toFixed(2)} (${resJson.status || 'Success'})`,
      };
    } catch (err: unknown) {
      const errMsg = err instanceof Error ? err.message : String(err);
      console.warn(`[Backend HTTP Error] POST ${this.httpUrl}/api/order failed:`, errMsg);

      // 2. Try sending directly over WebSocket if open
      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        try {
          this.ws.send(JSON.stringify({ action: 'order', order: orderString }));
          return {
            success: true,
            message: `Dispatched via WebSocket fallback: ${order.side.toUpperCase()} ${formattedQty} shares @ ₹${formattedPrice.toFixed(2)}`,
          };
        } catch (wsErr) {
          console.error('[Backend WS Error] Fallback send failed:', wsErr);
        }
      }

      return {
        success: false,
        message: `Backend connection error (${errMsg}). Check if backend server is running on ${this.httpUrl}.`,
      };
    }
  }

  // ============================================================================
  // Background Liquidity Simulation
  // ============================================================================
  private startBackgroundSim(): void {
    if (this.simInterval) {
      clearInterval(this.simInterval);
      this.simInterval = null;
    }

    if (!this.config.isRunning) return;

    this.simInterval = setInterval(() => {
      this.simulateTick();
    }, this.config.updateSpeedMs);
  }

  private simulateTick(): void {
    if (!this.config.isRunning) return;

    // Maintain realistic order book spread and resting orders around base price
    if (this.bids.size < 10 || this.asks.size < 10) {
      this.initializeOrderBook();
      this.notifyBookListeners();
      return;
    }

    // Micro fluctuations in existing levels
    const isBid = Math.random() > 0.5;
    const targetMap = isBid ? this.bids : this.asks;
    const prices = Array.from(targetMap.keys());
    if (prices.length > 0) {
      const price = prices[Math.floor(Math.random() * prices.length)];
      const current = targetMap.get(price) || 0.5;
      const delta = (Math.random() - 0.48) * 0.2;
      const updated = Math.max(0.05, +(current + delta).toFixed(4));
      targetMap.set(price, updated);
      this.notifyBookListeners();
    }
  }

  private initializeOrderBook(): void {
    this.bids.clear();
    this.asks.clear();

    for (let i = 1; i <= this.depthLevels; i++) {
      const price = +(this.basePrice - i * this.tickSize).toFixed(2);
      const baseQty = Math.round(5 + i * 2 + Math.random() * 15);
      this.bids.set(price, baseQty);
    }

    for (let i = 1; i <= this.depthLevels; i++) {
      const price = +(this.basePrice + i * this.tickSize).toFixed(2);
      const baseQty = Math.round(5 + i * 2 + Math.random() * 15);
      this.asks.set(price, baseQty);
    }
  }

  private seedInitialTrades(): void {
    const now = Date.now();
    for (let i = 15; i >= 0; i--) {
      const side: OrderSide = Math.random() > 0.5 ? 'buy' : 'sell';
      const price = +(this.basePrice + (Math.random() - 0.5) * 5).toFixed(2);
      const quantity = Math.round(1 + Math.random() * 20);

      this.recentTrades.push({
        id: `seed-${i}-${Math.random().toString(36).substring(2, 6)}`,
        price,
        quantity,
        side,
        timestamp: now - i * 2000,
      });
    }
  }

  // ============================================================================
  // Snapshot Generation
  // ============================================================================
  public getSnapshot(): OrderBookSnapshot {
    const sortedBidPrices = Array.from(this.bids.keys()).sort((a, b) => b - a).slice(0, this.depthLevels);
    let cumulativeBidTotal = 0;
    const rawBids: { price: number; quantity: number; total: number }[] = [];

    for (const price of sortedBidPrices) {
      const quantity = this.bids.get(price) || 0;
      cumulativeBidTotal += quantity;
      rawBids.push({ price, quantity, total: +cumulativeBidTotal.toFixed(4) });
    }

    const sortedAskPrices = Array.from(this.asks.keys()).sort((a, b) => a - b).slice(0, this.depthLevels);
    let cumulativeAskTotal = 0;
    const rawAsks: { price: number; quantity: number; total: number }[] = [];

    for (const price of sortedAskPrices) {
      const quantity = this.asks.get(price) || 0;
      cumulativeAskTotal += quantity;
      rawAsks.push({ price, quantity, total: +cumulativeAskTotal.toFixed(4) });
    }

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

  public updateConfig(newConfig: Partial<SimulationConfig>): void {
    const prevSpeed = this.config.updateSpeedMs;
    const prevRunning = this.config.isRunning;

    this.config = { ...this.config, ...newConfig };

    if (prevSpeed !== this.config.updateSpeedMs || prevRunning !== this.config.isRunning) {
      this.startBackgroundSim();
    }

    this.configListeners.forEach((l) => l(this.config));
  }

  public resetBook(): void {
    this.initializeOrderBook();
    this.notifyBookListeners();
  }

  public clearBook(): void {
    this.bids.clear();
    this.asks.clear();
    this.recentTrades = [];
    this.lastPrice = 10520.0;
    this.volume24h = 0;
    this.currentStats = {
      latencyNs: 0,
      poolUsed: 0,
      poolCap: 100000,
      poolAllocNs: 2.3,
    };
    this.notifyBookListeners();
    this.notifyStatsListeners(this.currentStats);

    // Send clear signal to C++ engine
    try {
      fetch(`${this.httpUrl}/api/order`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ order: '{"action":"clear"}' }),
      }).catch(() => {});
    } catch {}
  }

  // ============================================================================
  // Subscription Listeners
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

  public subscribeStatus(listener: ConnectionListener): () => void {
    this.statusListeners.add(listener);
    listener(this.status);
    return () => this.statusListeners.delete(listener);
  }

  public subscribeStats(listener: StatsListener): () => void {
    this.statsListeners.add(listener);
    listener(this.currentStats);
    return () => this.statsListeners.delete(listener);
  }

  public getStats(): EngineStats {
    return { ...this.currentStats };
  }

  private notifyStatsListeners(stats: EngineStats): void {
    this.currentStats = { ...stats };
    this.statsListeners.forEach((l) => l(this.currentStats));
  }

  private notifyBookListeners(): void {
    if (this.bookListeners.size === 0) return;
    const snap = this.getSnapshot();
    this.bookListeners.forEach((l) => l(snap));
  }

  private notifyTradeListeners(trade: Trade): void {
    this.tradeListeners.forEach((l) => l(trade));
  }

  public destroy(): void {
    this.isDestroyed = true;
    if (this.reconnectTimer) clearTimeout(this.reconnectTimer);
    if (this.simInterval) clearInterval(this.simInterval);
    if (this.ws) {
      try {
        this.ws.close();
      } catch {
        // ignore
      }
    }
  }
}

export const backendOrderBookService = new BackendOrderBookService();

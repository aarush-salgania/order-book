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
 * the React frontend and the C++ backend matching engine running on:
 * - Express HTTP API: http://localhost:4000
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
    activeBots: 15,
    updateSpeedMs: 250,
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
    p50Ns: 0,
    p90Ns: 0,
    p99Ns: 0,
    p999Ns: 0,
    opsCount: 0,
  };

  private latencyHistory: number[] = [];
  private totalOpsCount = 0;

  private simInterval: ReturnType<typeof setInterval> | null = null;
  private readonly depthLevels = 18;
  private readonly tickSize = 0.5;

  private notifyScheduled = false;

  constructor(
    httpUrl = 'http://localhost:4000',
    wsUrl = 'ws://localhost:4001'
  ) {
    this.httpUrl = httpUrl;
    this.wsUrl = wsUrl;

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
        // Request current state from engine
        try {
          this.ws?.send(JSON.stringify({ action: 'snapshot' }));
        } catch {}
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
      } catch {}
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
  // Message Parser & Processing (Single Source of Truth from C++)
  // ============================================================================
  private handleIncomingMessage(rawData: string | ArrayBuffer): void {
    if (typeof rawData !== 'string') return;

    const trimmed = rawData.trim();
    if (!trimmed) return;

    // 0. MERKLE_ROOT: {"batch":1,"count":64,"root":"0x...","timestamp":...}
    if (trimmed.startsWith('MERKLE_ROOT:')) {
      try {
        const payload = trimmed.substring('MERKLE_ROOT:'.length).trim();
        const obj = JSON.parse(payload);
        if (obj.root) {
          this.notifyStatsListeners({
            merkleRoot: String(obj.root),
            merkleBatch: Number(obj.batch) || undefined,
          });
        }
      } catch (e) {
        console.warn('Failed to parse MERKLE_ROOT:', e);
      }
      return;
    }

    // 1. BOOK_SNAPSHOT: {"bids":[{"price":...,"quantity":...}],"asks":[...]}
    if (trimmed.startsWith('BOOK_SNAPSHOT:')) {
      try {
        const payload = trimmed.substring('BOOK_SNAPSHOT:'.length).trim();
        const obj = JSON.parse(payload);
        if (Array.isArray(obj.bids) && Array.isArray(obj.asks)) {
          this.bids.clear();
          this.asks.clear();
          for (const b of obj.bids) {
            if (b.price > 0 && b.quantity > 0) this.bids.set(Number(b.price), Number(b.quantity));
          }
          for (const a of obj.asks) {
            if (a.price > 0 && a.quantity > 0) this.asks.set(Number(a.price), Number(a.quantity));
          }
          this.scheduleBookNotify();
        }
      } catch (e) {
        console.warn('Failed to parse BOOK_SNAPSHOT:', e);
      }
      return;
    }

    // 1. ORDER_ACCEPTED: {"side":"buy","price":10520.00,"quantity":10,"latency_ns":...}
    if (trimmed.startsWith('ORDER_ACCEPTED:')) {
      try {
        const payload = trimmed.substring('ORDER_ACCEPTED:'.length).trim();
        const obj = JSON.parse(payload);
        const side: OrderSide = (obj.side === 'sell' || obj.side === 'S') ? 'sell' : 'buy';
        const price = +Number(obj.price).toFixed(2);
        const qty = Math.max(1, Math.round(Number(obj.quantity)));

        if (side === 'buy') {
          const current = this.bids.get(price) || 0;
          this.bids.set(price, current + qty);
        } else {
          const current = this.asks.get(price) || 0;
          this.asks.set(price, current + qty);
        }

        if (obj.latency_ns !== undefined || obj.pool_used !== undefined) {
          this.notifyStatsListeners({
            latencyNs: Number(obj.latency_ns) || this.currentStats.latencyNs,
            poolUsed: obj.pool_used !== undefined ? Number(obj.pool_used) : this.currentStats.poolUsed,
            poolCap: Number(obj.pool_cap) || this.currentStats.poolCap,
            poolAllocNs: Number(obj.pool_alloc_ns) || this.currentStats.poolAllocNs,
          });
        }
        this.scheduleBookNotify();
      } catch (e) {
        console.warn('Failed to parse ORDER_ACCEPTED:', e);
      }
      return;
    }

    // 2. ENGINE_STATS: {"latency_ns":0,"pool_used":0,"pool_cap":100000,"pool_alloc_ns":2.3}
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

    // 3. TRADE_EXECUTED: ...
    if (trimmed.startsWith('TRADE_EXECUTED:')) {
      const payload = trimmed.substring('TRADE_EXECUTED:'.length).trim();
      this.processTradePayload(payload);
      return;
    }

    // 4. Generic JSON fallback
    if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
      try {
        const parsed = JSON.parse(trimmed);
        if (parsed.type === 'trade' || parsed.event === 'trade' || parsed.side) {
          this.applyTrade({
            price: Number(parsed.price) || this.lastPrice,
            orderPrice: parsed.order_price !== undefined ? Number(parsed.order_price) : undefined,
            quantity: Number(parsed.quantity) || 1,
            remaining: parsed.remaining !== undefined ? Number(parsed.remaining) : 0,
            side: (parsed.side === 'sell' || parsed.side === 'S') ? 'sell' : 'buy',
            latencyNs: parsed.latency_ns !== undefined ? Number(parsed.latency_ns) : undefined,
            poolUsed: parsed.pool_used !== undefined ? Number(parsed.pool_used) : undefined,
            poolCap: parsed.pool_cap !== undefined ? Number(parsed.pool_cap) : undefined,
            poolAllocNs: parsed.pool_alloc_ns !== undefined ? Number(parsed.pool_alloc_ns) : undefined,
          });
          return;
        }
      } catch {}
    }
  }

  private processTradePayload(payload: string): void {
    try {
      if (payload.startsWith('{')) {
        const obj = JSON.parse(payload);
        const price = Number(obj.price) || this.lastPrice;
        const orderPrice = obj.order_price !== undefined ? Number(obj.order_price) : price;
        const qty = Number(obj.quantity || obj.qty) || 1;
        const remaining = obj.remaining !== undefined ? Number(obj.remaining) : 0;
        const side: OrderSide = (obj.side === 'sell' || obj.side === 'S') ? 'sell' : 'buy';
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
          orderPrice,
          quantity: qty,
          remaining,
          side,
          latencyNs,
          poolUsed,
          poolCap,
          poolAllocNs,
        });
        return;
      }
    } catch {}

    // Regex fallback
    const match = payload.match(/(BUY|SELL)\s+([0-9.]+)\s+([0-9.]+)/i);
    if (match) {
      const side: OrderSide = match[1].toLowerCase() === 'sell' ? 'sell' : 'buy';
      const val1 = parseFloat(match[2]);
      const val2 = parseFloat(match[3]);
      const price = Math.max(val1, val2);
      const quantity = Math.min(val1, val2);
      this.applyTrade({ price, quantity, side });
    }
  }

  private applyTrade(details: {
    price: number;
    orderPrice?: number;
    quantity: number;
    remaining?: number;
    side: OrderSide;
    latencyNs?: number;
    poolUsed?: number;
    poolCap?: number;
    poolAllocNs?: number;
  }): void {
    const price = +details.price.toFixed(2);
    const quantity = Math.max(1, Math.round(details.quantity));
    const side = details.side;
    const remaining = details.remaining ? Math.round(details.remaining) : 0;
    const orderPrice = details.orderPrice ? +details.orderPrice.toFixed(2) : price;

    this.lastPrice = price;
    this.volume24h += quantity;

    if (price > this.high24h) this.high24h = price;
    if (price < this.low24h) this.low24h = price;

    // Adjust resting orders in ladder:
    // The aggressive order was on `side`. It filled against the OPPOSITE side at `price`.
    if (side === 'buy') {
      const askQty = this.asks.get(price);
      if (askQty !== undefined) {
        if (askQty <= quantity) this.asks.delete(price);
        else this.asks.set(price, Math.round(askQty - quantity));
      }
      // If the incoming buy order had remaining unfilled shares, they rest as a bid at orderPrice
      if (remaining > 0) {
        const curBid = this.bids.get(orderPrice) || 0;
        this.bids.set(orderPrice, curBid + remaining);
      }
    } else {
      const bidQty = this.bids.get(price);
      if (bidQty !== undefined) {
        if (bidQty <= quantity) this.bids.delete(price);
        else this.bids.set(price, Math.round(bidQty - quantity));
      }
      // If the incoming sell order had remaining unfilled shares, they rest as an ask at orderPrice
      if (remaining > 0) {
        const curAsk = this.asks.get(orderPrice) || 0;
        this.asks.set(orderPrice, curAsk + remaining);
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
    this.scheduleBookNotify();
  }

  private scheduleBookNotify(): void {
    if (this.notifyScheduled) return;
    this.notifyScheduled = true;
    requestAnimationFrame(() => {
      this.notifyScheduled = false;
      this.notifyBookListeners();
    });
  }

  // ============================================================================
  // Order Ingestion (Direct to C++ Engine without optimistic ghosting)
  // ============================================================================
  public async sendOrder(
    order: ManualOrderPayload
  ): Promise<{ success: boolean; message: string }> {
    const formattedPrice = +order.price.toFixed(2);
    const formattedQty = Math.max(1, Math.round(order.quantity));

    const orderData = {
      side: order.side,
      price: formattedPrice,
      quantity: formattedQty,
      type: order.type,
      timestamp: Date.now(),
    };

    const orderString = JSON.stringify(orderData);

    // 1. Try sending directly over WebSocket
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      try {
        this.ws.send(orderString);
        return {
          success: true,
          message: `Dispatched to C++ Engine: ${order.side.toUpperCase()} ${formattedQty} shares @ ₹${formattedPrice.toFixed(2)}`,
        };
      } catch (wsErr) {
        console.warn('[Backend WS Error] Fallback to HTTP:', wsErr);
      }
    }

    // 2. HTTP POST fallback
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

      return {
        success: true,
        message: `Order sent to C++ Engine: ${order.side.toUpperCase()} ${formattedQty} shares @ ₹${formattedPrice.toFixed(2)}`,
      };
    } catch (err: unknown) {
      const errMsg = err instanceof Error ? err.message : String(err);
      return {
        success: false,
        message: `Backend connection error (${errMsg}). Check if backend server is running on ${this.httpUrl}.`,
      };
    }
  }

  // ============================================================================
  // High-Throughput Realistic Market Flow Simulator (Through C++ Engine)
  // ============================================================================
  private startBackgroundSim(): void {
    if (this.simInterval) {
      clearInterval(this.simInterval);
      this.simInterval = null;
    }

    if (!this.config.isRunning) return;

    this.simInterval = setInterval(() => {
      this.simulateTick();
    }, Math.max(1, this.config.updateSpeedMs));
  }

  private simulateTick(): void {
    if (!this.config.isRunning) return;

    const sortedBids = Array.from(this.bids.keys()).sort((a, b) => b - a);
    const sortedAsks = Array.from(this.asks.keys()).sort((a, b) => a - b);

    const bestBid = sortedBids.length > 0 ? sortedBids[0] : 0;
    const bestAsk = sortedAsks.length > 0 ? sortedAsks[0] : 0;

    let mid = 10520.0;
    if (bestBid > 0 && bestAsk > 0 && bestAsk > bestBid) {
      mid = (bestBid + bestAsk) / 2;
    } else if (bestBid > 0) {
      mid = bestBid + 0.5;
    } else if (bestAsk > 0) {
      mid = bestAsk - 0.5;
    } else {
      mid = this.lastPrice || 10520.0;
    }

    // Dynamic price drift with mean reversion
    const drift = (Math.random() - 0.49) * this.tickSize;
    if (Math.abs(mid - 10520.0) > 80) {
      mid += (10520.0 - mid) * 0.05;
    } else {
      mid += drift;
    }
    mid = Math.round(mid * 2) / 2;

    // High throughput scaling: batch orders per tick when interval is small
    let batch = 1;
    if (this.config.updateSpeedMs <= 5) {
      batch = Math.max(10, Math.min(50, Math.round(this.config.activeBots * 1.5)));
    } else if (this.config.updateSpeedMs <= 20) {
      batch = Math.max(3, Math.min(15, Math.round(this.config.activeBots * 0.5)));
    } else if (this.config.updateSpeedMs <= 50) {
      batch = 2;
    }

    const ordersToSend: any[] = [];

    for (let i = 0; i < batch; i++) {
      const needBids = this.bids.size < 5;
      const needAsks = this.asks.size < 5;

      let isAggressive = Math.random() < 0.40;
      if (needBids || needAsks) isAggressive = false;

      let isBuy = Math.random() > 0.5;
      if (needBids && !needAsks) isBuy = true;
      else if (needAsks && !needBids) isBuy = false;

      let orderPrice: number;
      const qtyOptions = [1, 2, 5, 10, 15, 20, 25, 50];
      const orderQty = qtyOptions[Math.floor(Math.random() * qtyOptions.length)];

      if (isAggressive && bestBid > 0 && bestAsk > 0 && bestAsk >= bestBid) {
        // Taker crossing spread: triggers immediate match in C++
        if (isBuy) {
          orderPrice = bestAsk;
        } else {
          orderPrice = bestBid;
        }
      } else {
        // Passive maker order: provides depth around mid-price
        const offset = Math.floor(1 + Math.random() * 8) * this.tickSize;
        if (isBuy) {
          orderPrice = Math.max(0.5, +(mid - offset).toFixed(2));
        } else {
          orderPrice = +(mid + offset).toFixed(2);
        }
      }

      orderPrice = Math.round(orderPrice * 2) / 2;

      ordersToSend.push({
        side: isBuy ? 'buy' : 'sell',
        price: orderPrice,
        quantity: orderQty,
        type: 'limit',
        timestamp: Date.now(),
      });
    }

    this.dispatchSimOrders(ordersToSend);
  }

  private dispatchSimOrders(orders: any[]): void {
    if (orders.length === 0) return;

    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      if (orders.length === 1) {
        this.ws.send(JSON.stringify(orders[0]));
      } else {
        this.ws.send(JSON.stringify(orders));
      }
      return;
    }

    try {
      fetch(`${this.httpUrl}/api/orders`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ orders }),
      }).catch(() => {});
    } catch {}
  }

  // ============================================================================
  // Book Lifecycle (Reset & Clear)
  // ============================================================================
  public resetBook(): void {
    this.bids.clear();
    this.asks.clear();
    this.recentTrades = [];
    this.lastPrice = 10520.0;
    this.notifyBookListeners();

    try {
      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        this.ws.send(JSON.stringify({ action: 'clear' }));
      } else {
        fetch(`${this.httpUrl}/api/order`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ order: '{"action":"clear"}' }),
        }).catch(() => {});
      }
    } catch {}
  }

  public clearBook(): void {
    this.bids.clear();
    this.asks.clear();
    this.recentTrades = [];
    this.lastPrice = 10520.0;
    this.volume24h = 0;
    this.latencyHistory = [];
    this.totalOpsCount = 0;
    this.currentStats = {
      latencyNs: 0,
      poolUsed: 0,
      poolCap: 100000,
      poolAllocNs: 2.3,
      p50Ns: 0,
      p90Ns: 0,
      p99Ns: 0,
      p999Ns: 0,
      opsCount: 0,
      merkleRoot: undefined,
      merkleBatch: 0,
    };
    this.notifyBookListeners();
    this.notifyStatsListeners(this.currentStats);

    try {
      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        this.ws.send(JSON.stringify({ action: 'clear' }));
      } else {
        fetch(`${this.httpUrl}/api/order`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ order: '{"action":"clear"}' }),
        }).catch(() => {});
      }
    } catch {}
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

    const bestBid = bids.length > 0 ? bids[0].price : 0;
    const bestAsk = asks.length > 0 ? asks[0].price : 0;
    const spread = (bestBid > 0 && bestAsk > 0) ? +(bestAsk - bestBid).toFixed(2) : 0;
    const midPrice = (bestBid > 0 && bestAsk > 0) ? +((bestBid + bestAsk) / 2).toFixed(2) : this.lastPrice;
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

  private recordLatency(ns?: number): void {
    if (ns == null || ns <= 0) return;
    this.totalOpsCount++;
    this.latencyHistory.push(ns);
    if (this.latencyHistory.length > 2000) {
      this.latencyHistory.shift();
    }
  }

  private getPercentiles(): { p50: number; p90: number; p99: number; p999: number } {
    if (this.latencyHistory.length === 0) {
      return { p50: 0, p90: 0, p99: 0, p999: 0 };
    }
    const sorted = [...this.latencyHistory].sort((a, b) => a - b);
    const n = sorted.length;
    return {
      p50: sorted[Math.floor(n * 0.50)] || 0,
      p90: sorted[Math.floor(n * 0.90)] || sorted[n - 1] || 0,
      p99: sorted[Math.floor(n * 0.99)] || sorted[n - 1] || 0,
      p999: sorted[Math.floor(n * 0.999)] || sorted[n - 1] || 0,
    };
  }

  private notifyStatsListeners(partial: Partial<EngineStats>): void {
    if (partial.latencyNs && partial.latencyNs > 0) {
      this.recordLatency(partial.latencyNs);
    }
    const p = this.getPercentiles();
    this.currentStats = {
      ...this.currentStats,
      ...partial,
      p50Ns: p.p50,
      p90Ns: p.p90,
      p99Ns: p.p99,
      p999Ns: p.p999,
      opsCount: this.totalOpsCount,
    };
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
      } catch {}
    }
  }
}

export const backendOrderBookService = new BackendOrderBookService();

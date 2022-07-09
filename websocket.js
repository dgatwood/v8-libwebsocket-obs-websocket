
class WebSocket {
  constructor(url, protocols) {
    if (typeof(protocols)==='string') {
      this.construct(url, [protocols]);
    } else {
      this.construct(url, protocols);
    }
  }

  STATE_CONNECTING = 0;
  STATE_CONNECTED = 1;
  STATE_CLOSING = 2;
  STATE_CLOSED = 3;

  construct(url, protocols) {
    Object.defineProperty(this, "url", {
      value: url,
      writable: false,
      enumerable: true,
      configurable: true
    });

    const internal_connection_id =
        connectWebSocket(this, url, protocols);
    Object.defineProperty(this, "internal_connection_id", {
      value: internal_connection_id,
      writable: false,
      enumerable: true,
      configurable: true
    });

    this.readyState = WebSocket.STATE_CLOSED;
    this.lastEventID = 0;

    this.openEventListeners = new Array();
    this.messageEventListeners = new Array();
    this.closeEventListeners = new Array();
    this.errorEventListeners = new Array();
  }

  onClose(handler) {
    this.closeHandler = handler;
  }

  onError(handler) {
    this.errorHandler = handler;
  }

  onMessage(handler) {
    this.messageHandler = handler;
  }

  onOpen(handler) {
    this.openHandler = handler;
  }

  callHandlers(handler, eventListeners, eventData) {
    var event = new Event();
    for (propertyName in Object.keys(eventData)) {
      event[propertyName] = eventData[propertyName];
    }
    if (handler) handler(event);
    for (listener in eventListeners) {
      listener(event);
    }
  }

  addEventListener(type, callback, options) {
    var newcallback = callback;
    assert(!options.once, "One-shot event listeners are NOT supported.");
    if (type == "open") {
      this.openEventListeners.push(callback);
    } else if (type == "message") {
      this.messageEventListeners.push(callback);
    } else if (type == "error") {
      this.errorEventListeners.push(callback);
    } else if (type == "close") {
      this.closeEventListeners.push(callback);
    }
  }

  removeEventListener(type, callback, options) {
    if (type == "open") {
      this.openEventListeners.words.filter(value => value != callback);
    } else if (type == "message") {
      this.messageEventListeners.filter(value => value != callback);
    } else if (type == "error") {
      this.errorEventListeners.filter(value => value != callback);
    } else if (type == "close") {
      this.closeEventListeners.filter(value => value != callback);
    }
  }

  dispatchEvent(event) {
    if (event.type == "open") {
        this.callHandlers(this.openHandler, this.openEventListeners, event);
    } else if (event.type == "message") {
        this.callHandlers(this.messageHandler, this.messageEventListeners, event);
    } else if (event.type == "error") {
        this.callHandlers(this.errorHandler, this.errorEventListeners, event);
    } else if (event.type == "close") {
        this.callHandlers(this.closeHandler, this.closeEventListeners, event);
    }
  }

  send(data) {
    if (this.readyState == STATE_CONNECTING) {
      exception = new DOMException();
      exception.code = 11;
      exception.message = "Can't send while websocket in connecting state";
      exception.name = "InvalidStateError";
      throw exception;
      return;
    }
    sendWebSocketData(this.internal_connection_id, data);
  }

  close(code, reason) {
    closeWebSocket(this.internal_connection_id);

    this.callHandlers(this.closeHandler,
                      this.closeEventListeners, {
      type: "close",
      code: code,
      reason: reason ? reason : "Unknown",
      wasClean: (code == 1000),
      lastEventId: this.lastEventID++
    });
  }

  didReceiveError() {
    this.callHandlers(this.errorHandler,
                      this.errorEventListeners, {
      type: "error",
      lastEventId: this.lastEventID++
    });
  }

  deliverMessage(data, origin) {
    this.callHandlers(this.messageHandler,
                      this.messageEventListeners, {
      type: "message",
      data: data,
      origin: origin,
      lastEventId: this.lastEventID++,
      source: this,
      ports: new Array()
    });
  }

  didOpen() {
    this.callHandlers(this.openHandler,
                      this.openEventListeners, {
      type: "open",
      lastEventId: this.lastEventID++,
    });
  }

  internal_binary_type = "blob";

  get binaryType() {
    return this.internal_binary_type;
  }

  set binaryType(newBinaryType) {
    this.internal_binary_type = newBinaryType;
    setWebSocketBinaryType(this.internal_connection_id, newBinaryType);
  }

  get bufferedAmount() {
    return getWebSocketBufferedAmount();
  }

  get extensions() {
    return getWebSocketExtensions();
  }
}

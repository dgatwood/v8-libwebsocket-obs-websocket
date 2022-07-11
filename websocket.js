
const console = {
  log: (message) => {
    logMessage(message);
  }
}

const process = {
  env: {
    DEBUG: "*"
  }
}

class Event {
  constructor(type, options = {}) {
    this.type = type;
  }
}

class WebSocket {
  constructor(url, protocols = "websocket") {
    logMessage("Constructor called.\n");
    if (typeof(protocols)==='string') {
      this.construct(url, [protocols]);
    } else {
      this.construct(url, protocols);
    }
  }

  CONNECTING = 0;
  CONNECTED = 1;
  CLOSING = 2;
  CLOSED = 3;

  construct(url, protocols) {
    logMessage("Secondary constructor called.\n");
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

    this.lastEventID = 0;

    this.openEventListeners = new Array();
    this.messageEventListeners = new Array();
    this.closeEventListeners = new Array();
    this.errorEventListeners = new Array();

    this.openHandler = undefined;
    this.messageHandler = undefined;
    this.errorHandler = undefined;
    this.closeHandler = undefined;
  }

  get onopen() {
    return this.openHandler;
  }

  get onOpen() {
    return this.openHandler;
  }

  get onerror() {
    return this.errorHandler;
  }

  get onError() {
    return this.errorHandler;
  }

  get onMessage() {
    return this.messageHandler;
  }

  get onmessage() {
    return this.messageHandler;
  }

  get onClose() {
    return this.closeHandler;
  }

  get onclose() {
    return this.closeHandler;
  }

  set onClose(handler) {
    logMessage("@@@ set onClose called");
    this.closeHandler = handler;
  }

  set onclose(handler) {
    logMessage("@@@ set onclose called");
    this.closeHandler = handler;
  }

  set onError(handler) {
    logMessage("@@@ set onError called");
    this.errorHandler = handler;
  }

  set onerror(handler) {
    logMessage("@@@ set onerror called");
    this.errorHandler = handler;
  }

  set onMessage(handler) {
    logMessage("@@@ set onMessage called");
    this.messageHandler = handler;
  }

  set onmessage(handler) {
    logMessage("@@@ set onmessage called");
    this.messageHandler = handler;
  }

  set onOpen(handler) {
    logMessage("@@@ set onOpen called");
    this.openHandler = handler;
  }

  set onopen(handler) {
    logMessage("@@@ set onopen called");
    this.openHandler = handler;
  }

  callHandlers(handler, eventListeners, event) {
    if (handler) handler(event);
    for (var listener in eventListeners) {
      listener(event);
    }
  }

  addEventListener(type, callback, options) {
    logMessage("@@@ addEventListener called");
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
    logMessage("@@@ removeEventListener called");
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
    logMessage("dispatchEvent called");
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
    logMessage("@@@ send called with payload: " + data);
    if (this.readyState == this.CONNECTING) {
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
    logMessage("@@@ close called");
    closeWebSocket(this.internal_connection_id);
  }

  _connectionDidReceiveData(data) {
    logMessage("@@@ _connectionDidReceiveData called");
    this._deliverMessage(data, this.url);
  }

  _connectionDidClose(code, reason) {
    logMessage("_connectionDidClose called");

    var event = new Event("close");
    event.code = code;
    event.reason = reason ? reason : "Unknown";
    event.wasClean = (code == 1000);
    event.lastEventId = this.lastEventID++;

    this.callHandlers(this.closeHandler, this.closeEventListeners, event);
  }

  _didReceiveError() {
    logMessage("_didReceiveError called");

    var event = new Event("error");
    event.lastEventId = this.lastEventID++;
    this.callHandlers(this.errorHandler, this.errorEventListeners, event);
  }

  _deliverMessage(data, origin) {
    logMessage("_deliverMessage called with data: " + data + " origin: " + origin);
    var event = new Event("message");
    event.data = data;
    event.origin = origin;
    event.lastEventId = this.lastEventID++;
    event.source = this;
    event.ports = new Array();

    logMessage("event.data: " + event.data);
    this.callHandlers(this.messageHandler, this.messageEventListeners, event);
  }

  _didOpen() {
    logMessage("_didOpen called");
    var event = new Event("open");
    event.lastEventId = this.lastEventID++;
    this.callHandlers(this.openHandler, this.openEventListeners, event);
  }

  internal_binary_type = "blob";

  get readyState() {
    var retval = getWebSocketConnectionState(this.internal_connection_id);
    logMessage("readyState called.  Returning "+retval);
    return retval;
  }

  get binaryType() {
    logMessage("readyState called");
    return this.internal_binary_type;
  }

  set binaryType(newBinaryType) {
    logMessage("set binaryType called");
    this.internal_binary_type = newBinaryType;
    setWebSocketBinaryType(this.internal_connection_id, newBinaryType);
  }

  get bufferedAmount() {
    logMessage("get bufferedAmount called");
    return getWebSocketBufferedAmount(this.internal_connection_id);
  }

  get extensions() {
    logMessage("get extensions called");
    return getWebSocketExtensions(this.internal_connection_id);
  }

  get protocol() {
    logMessage("get protocol called.");
    var protocol = getWebSocketActiveProtocol(this.internal_connection_id);
    logMessage("get protocol called.  Returning "+protocol);
    return protocol;
  }
}

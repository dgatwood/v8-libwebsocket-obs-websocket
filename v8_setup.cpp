#define _GNU_SOURCE  // For asprintf

#include <libplatform/libplatform.h>
#include <libwebsockets.h>
#include <map>
#include <set>
#include <stdio.h>
#include <sys/param.h>
#include <v8.h>

// Might work on Linux.  Doesn't work in macOS.
#undef SUPPORT_DEFLATE

#if 0
#define CBDEBUG(args...) fprintf(stderr, args)
#define FUNCDEBUG(args...) fprintf(stderr, args)
#define GENERALDEBUG(args...) fprintf(stderr, args)
#define WSIDEBUG(args...) fprintf(stderr, args)
#else
#define CBDEBUG(args...)
#define FUNCDEBUG(args...)
#define GENERALDEBUG(args...)
#define WSIDEBUG(args...)
#endif

// Can't figure out how to determine when this is needed, and lots
// of websockets code expects strings, so....
#undef SEND_AS_BINARY

#ifdef USE_NODE
#include <node/node.h>
#endif

#include "v8_setup.h"

// using namespace node;
// using namespace v8;

extern char *password;

std::recursive_mutex connection_mutex;

// States:
// 0 = Connecting
// 1 = Connected
// 2 = Closing
// 3 = Closed
enum {
  kConnectionStateConnecting = 0,
  kConnectionStateConnected = 1,
  kConnectionStateClosing = 2,
  kConnectionStateClosed = 3
};

// Stores a copy of buf.  Storage is scoped to the object.
class WebSocketsDataItem {
  public:
    WebSocketsDataItem(uint8_t *buf, size_t length, bool isBinary);
    ~WebSocketsDataItem(void);

    size_t GetLength();
    uint8_t *GetBuf();
    bool IsBinary();

  private:
    uint8_t *rawBuf = NULL;
    size_t rawLength = 0;
    bool rawIsBinary = false;
};

typedef struct websocketsDataItemChain {
  WebSocketsDataItem *item;
  struct websocketsDataItemChain *next;
} websocketsDataItemChain_t;


class DataProvider {
  public:
    DataProvider(const char *name);
    void addPendingData(WebSocketsDataItem *item);
    bool getPendingData(WebSocketsDataItem **returnItem);
    uint32_t PendingBytes(void);
    void SetWSI(struct lws *wsi);
    const char *name;
  private:
    std::recursive_mutex mutex;
    websocketsDataItemChain_t *firstItem = NULL;
    struct lws *wsi = nullptr;
};

class WebSocketsContextData {
  public:
    WebSocketsContextData(v8::Persistent<v8::Object> *jsObject,
                          struct lws_protocols * protocols, 
                          v8::Isolate *isolate);
    ~WebSocketsContextData(void);

    v8::Persistent<v8::Object> *jsObject = nullptr;

    DataProvider incomingData = DataProvider("incomingData");
    DataProvider outgoingData = DataProvider("outgoingData");

    v8::Isolate *isolate;

    bool isBinary = false;

    bool shouldCloseConnection = false;
    bool connectionDidOpen = false;
    bool connectionDidClose = false;
    bool hasConnectionError = false;

    std::string *activeProtocolName = nullptr;
    const struct lws_protocols *protocols;

    int connectionState = kConnectionStateConnecting;
    int codeNumber = 0;
    std::string *reason = nullptr;

    void SetWSI(struct lws *wsi);
    struct lws *wsi = nullptr;
};

WebSocketsContextData::WebSocketsContextData(v8::Persistent<v8::Object> *jsObject,
                                             struct lws_protocols * protocols,
                                             v8::Isolate *isolate) {
  this->jsObject = jsObject;
  this->protocols = protocols;
  this->isolate = isolate;
}

WebSocketsContextData::~WebSocketsContextData(void) {
  if (this->activeProtocolName) {
    delete this->activeProtocolName;
  }
  if (this->jsObject) {
    delete this->jsObject;
  }
  if (this->reason) {
    delete this->reason;
  }
  if (this->protocols) {
    free((void *)this->protocols);
  }
}

void WebSocketsContextData::SetWSI(struct lws *wsi) {
  WSIDEBUG("Top level: Setting WSI to 0x%p\n", wsi);
  this->wsi = wsi;
  this->outgoingData.SetWSI(wsi);
}

void setConnectionState(uint32_t connectionID, int state);

std::map<uint32_t, WebSocketsContextData *> connectionData;

std::unique_ptr<v8::Platform> platform;
// v8::Isolate *gIsolate;
v8::Local<v8::ObjectTemplate> globals;

std::vector<std::string> programScenes;
std::vector<std::string> previewScenes;
void updateScenes(std::vector<std::string> newPreviewScenes, std::vector<std::string> newProgramScenes);
void PasswordGetter(v8::Local<v8::String> property,
              const v8::PropertyCallbackInfo<v8::Value>& info);

v8::MaybeLocal<v8::Module> resolveCallback(v8::Local<v8::Context> context,
                                           v8::Local<v8::String> specifier,
                                           v8::Local<v8::FixedArray> import_assertions,
                                           v8::Local<v8::Module> referrer);

void logMessage(const v8::FunctionCallbackInfo<v8::Value>& args);
void connectWebSocket(const v8::FunctionCallbackInfo<v8::Value>& args);
void sendWebSocketData(const v8::FunctionCallbackInfo<v8::Value>& args);
void closeWebSocket(const v8::FunctionCallbackInfo<v8::Value>& args);
void getWebSocketBufferedAmount(const v8::FunctionCallbackInfo<v8::Value>& args);
void getWebSocketExtensions(const v8::FunctionCallbackInfo<v8::Value>& args);
void setWebSocketBinaryType(const v8::FunctionCallbackInfo<v8::Value>& args);
void getWebSocketConnectionState(const v8::FunctionCallbackInfo<v8::Value>& args);
void getWebSocketActiveProtocol(const v8::FunctionCallbackInfo<v8::Value>& args);
bool connectWebSocket(std::string URL, struct lws_protocols *protocols,
                      uint32_t connectionID);
struct lws_protocols *createProtocols(std::vector<std::string> protocols);

uint32_t connectionIDForWSI(struct lws *wsi);

extern "C" {
void setSceneIsProgram(const char *sceneName);
void setSceneIsPreview(const char *sceneName);
void setSceneIsInactive(const char *sceneName);
};

void setProgramAndPreviewScenes(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  v8::Isolate *isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  FUNCDEBUG("setProgramAndPreviewScenes called.\n");

  if(args.Length() != 2 || !args[0]->IsArray() || !args[1]->IsArray()) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, "Error: Two arrays expected").ToLocalChecked()));
    return;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Handle<v8::Array> previewSceneArray = v8::Handle<v8::Array>::Cast(args[0]);

  std::vector<std::string> newPreviewScenes;

  for (int i = 0; i < previewSceneArray->Length(); i++) {
    v8::Local<v8::Value> element = previewSceneArray->Get(context, i).ToLocalChecked();
    v8::String::Utf8Value previewSceneUTF8(v8::Isolate::GetCurrent(), element);
    std::string previewSceneCPPString(*previewSceneUTF8);
    newPreviewScenes.push_back(previewSceneCPPString);
  }

  v8::Handle<v8::Array> programSceneArray = v8::Handle<v8::Array>::Cast(args[1]);

  std::vector<std::string> newProgramScenes;

  for (int i = 0; i < programSceneArray->Length(); i++) {
    v8::Local<v8::Value> element = programSceneArray->Get(context, i).ToLocalChecked();
    v8::String::Utf8Value programSceneUTF8(v8::Isolate::GetCurrent(), element);
    std::string programSceneCPPString(*programSceneUTF8);
    newProgramScenes.push_back(programSceneCPPString);
  }

  updateScenes(newPreviewScenes, newProgramScenes);
}

void *v8_setup(void) {
  v8::V8::InitializeICUDefaultLocation("viscaptz");
  v8::V8::InitializeExternalStartupData("viscaptz");

#ifdef USE_NODE
  std::unique_ptr<node::MultiIsolatePlatform> platform =
      node::MultiIsolatePlatform::Create(4);
  v8::V8::InitializePlatform(platform.get());
#else
  platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());

#endif
  v8::V8::Initialize();

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();

  v8::Isolate *gIsolate = v8::Isolate::New(create_params);
  gIsolate->Enter();

  v8::Isolate::Scope isolate_scope(gIsolate);

  // Create a stack-allocated handle scope.
  v8::HandleScope handle_scope(gIsolate);

  globals = v8::ObjectTemplate::New(gIsolate);

  globals->SetAccessor(v8::String::NewFromUtf8(gIsolate, "obsPassword", v8::NewStringType::kNormal).ToLocalChecked(),
                       PasswordGetter, nullptr);

  globals->Set(v8::String::NewFromUtf8(gIsolate, "setProgramAndPreviewScenes").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, setProgramAndPreviewScenes));

  globals->Set(v8::String::NewFromUtf8(gIsolate, "logMessage").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, logMessage));

  globals->Set(v8::String::NewFromUtf8(gIsolate, "connectWebSocket").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, connectWebSocket));

  globals->Set(v8::String::NewFromUtf8(gIsolate, "sendWebSocketData").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, sendWebSocketData));

  globals->Set(v8::String::NewFromUtf8(gIsolate, "closeWebSocket").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, closeWebSocket));

  globals->Set(v8::String::NewFromUtf8(gIsolate, "getWebSocketBufferedAmount").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, getWebSocketBufferedAmount));

  globals->Set(v8::String::NewFromUtf8(gIsolate, "getWebSocketExtensions").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, getWebSocketExtensions));

  globals->Set(v8::String::NewFromUtf8(gIsolate, "setWebSocketBinaryType").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, setWebSocketBinaryType));

  globals->Set(v8::String::NewFromUtf8(gIsolate, "getWebSocketConnectionState").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, getWebSocketConnectionState));

  globals->Set(v8::String::NewFromUtf8(gIsolate, "getWebSocketActiveProtocol").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, getWebSocketActiveProtocol));

  // Create a new context.
  v8::Local<v8::Context> context = v8::Context::New(gIsolate, nullptr, globals);
  context->Enter();

  return (void *)gIsolate;
}

void callConnectionDidOpen(int connectionID, v8::Isolate *isolate);
void callConnectionDidClose(int connectionID, v8::Isolate *isolate, int codeNumber,
                            std::string *reason);
void callHasConnectionError(int connectionID, v8::Isolate *isolate);
void sendPendingDataToClient(int connectionID, v8::Isolate *isolate);

void v8_runLoopCallback(void *isolateVoid) {
  GENERALDEBUG("@@@ v8_runLoopCallback\n");

  v8::Isolate *isolate = (v8::Isolate *)isolateVoid;
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);

  std::vector<int32_t> connectionIDsToDelete;

  int connectionCount = MAX(connectionData.size(), 1);
  int contextWaitTime = MAX(500 / connectionCount, 1);

  for (std::pair<int32_t, WebSocketsContextData *> element :
       connectionData) {
    int32_t connectionID = element.first;
    WebSocketsContextData *connection = element.second;

    if (connection->wsi != nullptr) {
      struct lws_context *context = lws_get_context(connection->wsi);
      GENERALDEBUG("Waiting for events (%d milliseconds)\n", contextWaitTime);
      lws_service(context, contextWaitTime);
      GENERALDEBUG("Done waiting for events\n");
    } else {
      GENERALDEBUG("Connection %d ignored because wsi is NULL\n", connectionID);
    }

    if (connection->connectionDidOpen) {
      connection->connectionDidOpen = false;
      callConnectionDidOpen(connectionID, isolate);
    }

    if (connection->incomingData.PendingBytes() > 0) {
      GENERALDEBUG("@@@ Sending data to client.\n");
      sendPendingDataToClient(connectionID, isolate);
      GENERALDEBUG("@@@ Done.\n");
    }

    if (connection->hasConnectionError) {
      connection->hasConnectionError = false;
      callHasConnectionError(connectionID, isolate);
    }
    if (connection->connectionDidClose) {
      connection->connectionDidClose = false;
      callConnectionDidClose(connectionID, isolate, connection->codeNumber,
                             connection->reason);
      connectionIDsToDelete.push_back(connectionID);
    }
  }
  for (int32_t connectionID : connectionIDsToDelete) {
    connectionData.erase(connectionID);
  }
}

void runScript(char *scriptString) {
  auto isolate = v8::Isolate::GetCurrent();

  // Create a stack-allocated handle scope.
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();  // v8::Context::New(isolate, nullptr, globals);
  context->Enter();

  // Enter the context for compiling and running scripts.
  v8::Context::Scope context_scope(context);

  // Create a string containing the JavaScript source code.
  // printf("%s\n", scriptString);
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), scriptString,
                              v8::NewStringType::kNormal, strlen(scriptString))
          .ToLocalChecked();

  // Compile the source code.
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source).ToLocalChecked();
  // Run the script to get the result.
  v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
  // Convert the result to an UTF8 string and print it.
  v8::String::Utf8Value utf8(v8::Isolate::GetCurrent(), result);
  printf("%s\n", *utf8);
}

bool runScriptAsModule(char *moduleName, char *scriptString) {
  auto isolate = v8::Isolate::GetCurrent();

  // Create a stack-allocated handle scope.
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  context->Enter();

  // Enter the context for compiling and running scripts.
  v8::Context::Scope context_scope(context);

  // Create a string containing the JavaScript source code.
  // printf("%s\n", scriptString);
  v8::Local<v8::String> sourceCode =
      v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), scriptString,
                              v8::NewStringType::kNormal, strlen(scriptString))
          .ToLocalChecked();

  v8::ScriptOrigin origin(
      isolate,
      v8::String::NewFromUtf8(isolate, moduleName).ToLocalChecked() /* resource_name */,
      0 /* resource_line_offset */,
      0 /* resource_column_offset */, false /* resource_is_shared_cross_origin */,
      -1 /* script_id */, v8::Local<v8::Value>() /* source_map_url */,
      false /* resource_is_opaque */, false /* is_wasm */, true /* is_module*/
      /* omitted Local< Data > host_defined_options=Local< Data >() */);

  v8::ScriptCompiler::Source source(sourceCode, origin);

  // Compile the source code.
  v8::MaybeLocal<v8::Module> loadedModule =
      // v8::Script::Compile(context, sourceCode).ToLocalChecked();
      v8::ScriptCompiler::CompileModule(isolate, &source);


  v8::Local<v8::Module> verifiedModule;
  if (!loadedModule.ToLocal(&verifiedModule)) {
    fprintf(stderr, "Error loading module!\n");
    return false;
  }

  v8::Maybe<bool> instantiationResult =
      verifiedModule->InstantiateModule(context, resolveCallback);
  if (instantiationResult.IsNothing()) {
    fprintf(stderr, "Unable to instantiate module.\n");
    return false;
  }

  // Run the module to get the result.
  v8::Local<v8::Value> result;
  if (!verifiedModule->Evaluate(context).ToLocal(&result)) {
    fprintf(stderr, "Module evaluation failed.\n");
    return false;
  }

  // Run the script to get the result.
  // v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

  // Convert the result to a UTF8 string and print it.
  v8::String::Utf8Value utf8(v8::Isolate::GetCurrent(), result);
  printf("%s\n", *utf8);

  return true;
}

// Stub method.  If this ever gets called, it will crash.
v8::MaybeLocal<v8::Module> resolveCallback(v8::Local<v8::Context> context,
                                           v8::Local<v8::String> specifier,
                                           v8::Local<v8::FixedArray> import_assertions,
                                           v8::Local<v8::Module> referrer) {
  v8::Isolate *isolate = context->GetIsolate();
  isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Exception message").ToLocalChecked());
  return v8::MaybeLocal<v8::Module>();
}

void v8_teardown(void) {
  // Dispose the isolate and tear down V8.
  // isolate->Dispose();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  // delete create_params.array_buffer_allocator;
}


#pragma mark - Synthesized getters and setters

void PasswordGetter(v8::Local<v8::String> property,
              const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate *isolate = v8::Isolate::GetCurrent();
  FUNCDEBUG("PasswordGetter called.\n");
  
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::String> passwordV8String =
      v8::String::NewFromUtf8(isolate, password).ToLocalChecked();
  info.GetReturnValue().Set(passwordV8String);
}

void updateScenes(std::vector<std::string> newPreviewScenes, std::vector<std::string> newProgramScenes) {
  std::set<std::string> inactiveScenes;

  // Mark all of the old preview and program scenes as possibly inactive.
  for (std::string scene : previewScenes) {
    inactiveScenes.insert(scene);
  }
  for (std::string scene : programScenes) {
    inactiveScenes.insert(scene);
  }

  // Remove any new preview and program scenes so that they won't be marked as inactive,
  // and notify the main code that the scenes are active.
  for (std::string scene : newPreviewScenes) {
    inactiveScenes.erase(scene);
    setSceneIsPreview(scene.c_str());
  }
  for (std::string scene : newProgramScenes) {
    inactiveScenes.erase(scene);
    setSceneIsProgram(scene.c_str());
  }

  // Notify the main code that any previously active scenes are no longer active.
  for (std::string scene : inactiveScenes) {
    setSceneIsInactive(scene.c_str());
  }
}

#pragma mark - Websocket support

// JavaScript methods to implement:
//
// + WebSocket(url, [ (protocolString | array of protocolString) ])
// + close([code [, reason ]])
// + send(data) - data is String, ArrayBuffer, Blob, TypedArray, or DataView
// + addEventListener(type [, listener [, (optionsObject | useCapture) ])
// + removeEventListener(type [, listener [, (optionsObject | useCapture) ])
// + dispatchEvent(event)
//
// Events to implement:
// + close
// + error
// + message
// + open
//
// Properties to implement;
//
// + binaryType - property: either "blob" or "arraybuffer".
// x bufferedAmount - read-only property: bytes queued for writing but not sent.
// x extensions - read-only property: empty string or list of extensions from server.
// - protocol - read-only property: the sub-protocol selected by the server.
// - readyState - read-only property: 0 = connecting, 1 = connected, 2 = closing, 3 = closed.
// + url - read-only property: the URL from the constructor call.


// Support to add on C++ side:
//
// connectWebSocket(this, URL, protocols)
// sendWebSocketData(this.internal_connection_id, data)
// closeWebSocket(this.internal_connection_id)
// getWebSocketBufferedAmount()
// getWebSocketExtensions()
// setWebSocketBinaryType(typeString)
//
// On open, call:
//
// _didOpen()
//
// When data is received, call:
//
// deliverMessage(data, origin)
//
// On error, call:
//
// didReceiveError()
//
// On close, call:
//
// close(code, reason)


// Open a socket.
// connectWebSocket(this, URL, protocols)
void connectWebSocket(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate *isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  static uint32_t newConnectionIdentifier = 0;

  FUNCDEBUG("connectWebSocket called.\n");

  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Handle<v8::Object> objectHandle = v8::Handle<v8::Object>::Cast(args[0]);
  v8::Persistent<v8::Object> *persistentObject = new v8::Persistent<v8::Object>();
  persistentObject->Reset(isolate, objectHandle);

  v8::String::Utf8Value URLV8(v8::Isolate::GetCurrent(), args[1]);
  std::string URL(*URLV8);

  v8::Handle<v8::Array> protocolStringsArray = v8::Handle<v8::Array>::Cast(args[2]);

  std::vector<std::string> protocolStringsStdArray;

  for (int i = 0; i < protocolStringsArray->Length(); i++) {
    v8::Local<v8::Value> element = protocolStringsArray->Get(context, i).ToLocalChecked();
    v8::String::Utf8Value protocolStringUTF8(v8::Isolate::GetCurrent(), element);
    std::string protocolString(*protocolStringUTF8);
    protocolStringsStdArray.push_back(protocolString);
  }

  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  struct lws_protocols *protocols = createProtocols(protocolStringsStdArray);
  connectionData[newConnectionIdentifier] =
      new WebSocketsContextData(persistentObject, protocols, isolate);
  bool success = connectWebSocket(URL, protocols, newConnectionIdentifier);

  args.GetReturnValue().Set(newConnectionIdentifier++);
}

bool sendWebSocketData(uint32_t connectionID, uint8_t *data, uint64_t length, bool isUTF8);

// sendWebSocketData(this.internal_connection_id, data);
void sendWebSocketData(const v8::FunctionCallbackInfo<v8::Value>& args) {

  FUNCDEBUG("Called sendWebSocketData\n");

  v8::Isolate *isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Uint32> connectionIDV8 = v8::Handle<v8::Uint32>::Cast(args[0]);
  uint32_t connectionID = connectionIDV8->Uint32Value(context).ToChecked();

  // In theory, we need to support String, ArrayBuffer, Blob, TypedArray,
  // and DataView objects as the data object (args[1]).
  //
  // For now, support strings and byte arrays.  Nothing else.  Eventually,
  // we will convert everything else to one of those forms on the JavaScript
  // side.
  bool retval = true;
  if (args[1]->IsString()) { 
    v8::String::Utf8Value protocolStringUTF8(v8::Isolate::GetCurrent(), args[1]->ToString(context).ToLocalChecked());
    std::string protocolString(*protocolStringUTF8);
    retval = sendWebSocketData(connectionID, (uint8_t *)protocolString.c_str(), protocolString.length(), true);
  } else if (args[1]->IsArray()) {
    v8::Handle<v8::Array> byteArray = v8::Handle<v8::Array>::Cast(args[1]);

    uint8_t *data = (uint8_t *)malloc(byteArray->Length());
    for (int i = 0; i < byteArray->Length(); i++) {
      v8::Handle<v8::Uint32> byteValue = v8::Handle<v8::Uint32>::Cast(byteArray->Get(context, i).ToLocalChecked());
      data[i] = byteValue->Uint32Value(context).ToChecked() & 0xff;
    }
    retval = sendWebSocketData(connectionID, data, byteArray->Length(), false);
    free(data);
  }
  args.GetReturnValue().Set(retval);
}

// closeWebSocket(this.internal_connection_id);
void closeWebSocket(const v8::FunctionCallbackInfo<v8::Value>& args) {
  FUNCDEBUG("@@@ called closeWebSocket\n");

  v8::Isolate *isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Uint32> connectionIDV8 = v8::Handle<v8::Uint32>::Cast(args[0]);
  uint32_t connectionID = connectionIDV8->Uint32Value(context).ToChecked();

  setConnectionState(connectionID, kConnectionStateClosing);

  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];
  if (dataProviderGroup != nullptr) {
    dataProviderGroup->shouldCloseConnection = true;
  }
}

// getWebSocketBufferedAmount()
void getWebSocketBufferedAmount(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate *isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Uint32> connectionIDV8 = v8::Handle<v8::Uint32>::Cast(args[0]);
  uint32_t connectionID = connectionIDV8->Uint32Value(context).ToChecked();

  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];
  uint32_t bufferCount = 0;

  if (dataProviderGroup != nullptr) {
    bufferCount = dataProviderGroup->outgoingData.PendingBytes();
  }

  args.GetReturnValue().Set(bufferCount);
}

// getWebSocketExtensions()
void getWebSocketExtensions(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate *isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Uint32> connectionIDV8 = v8::Handle<v8::Uint32>::Cast(args[0]);
  uint32_t connectionID = connectionIDV8->Uint32Value(context).ToChecked();


  v8::Local<v8::Array> array = v8::Local<v8::Array>::New(isolate, v8::Array::New(isolate));

#ifdef SUPPORT_DEFLATE
  array->Set(context, v8::Number::New(isolate, 0),
             v8::String::NewFromUtf8(isolate, "permessage-deflate").ToLocalChecked()).Check();
  array->Set(context, v8::Number::New(isolate, 1),
                 v8::String::NewFromUtf8(isolate, "deflate-frame").ToLocalChecked()).Check();
#endif

  args.GetReturnValue().Set(array);
}

// setWebSocketBinaryType(typeString)
void setWebSocketBinaryType(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate *isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Uint32> connectionIDV8 = v8::Handle<v8::Uint32>::Cast(args[0]);
  uint32_t connectionID = connectionIDV8->Uint32Value(context).ToChecked();

  v8::String::Utf8Value binaryTypeStringV8(v8::Isolate::GetCurrent(), args[1]->ToString(context).ToLocalChecked());
  std::string binaryTypeString(*binaryTypeStringV8);

  // Update with data:
  // uint32_t connectionID
  // string binaryTypeString

  // @@@
  FUNCDEBUG("Got setWebSocketBinaryType (unsupported)\n");
  exit(1);
}

void getWebSocketActiveProtocol(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate *isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Uint32> connectionIDV8 = v8::Handle<v8::Uint32>::Cast(args[0]);
  uint32_t connectionID = connectionIDV8->Uint32Value(context).ToChecked();
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];

  bool isValid = (dataProviderGroup->activeProtocolName != NULL);
  if (isValid) {
    GENERALDEBUG("In getWebSocketActiveProtocol: protocol is %s\n",
                 dataProviderGroup->activeProtocolName->c_str());
  } else {
    GENERALDEBUG("In getWebSocketActiveProtocol: protocol is NULL!!!\n");
  }

  args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, isValid ?
      dataProviderGroup->activeProtocolName->c_str() : "").ToLocalChecked());
}

// Makes a permanent copy of a C++ string using malloc.
char *mallocString(std::string string) {
  char *buf = NULL;
  asprintf(&buf, "%s", string.c_str());
  return buf;
}

#ifdef SUPPORT_DEFLATE
static const struct lws_extension supportedExtensions[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate; client_no_context_takeover"
	},
	{
		"deflate-frame",
		lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{ NULL, NULL, NULL /* terminator */ }
};
#endif

int websocketLWSCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

struct lws_protocols *createProtocols(std::vector<std::string> protocols) {
  size_t count = protocols.size();
  struct lws_protocols *data = (struct lws_protocols *)malloc(sizeof(struct lws_protocols) * (count + 1));

  for (size_t i = 0; i < count; i++) {
    GENERALDEBUG("Protocol %zu: %s\n", i, protocols[i].c_str());
    data[i].name = mallocString(protocols[i]);
    data[i].callback = websocketLWSCallback;
    data[i].per_session_data_size = 0;
    data[i].rx_buffer_size = 65536;
    data[i].id = 0;
    data[i].user = NULL,
    data[i].tx_packet_size = 0;
  }

  data[count] = LWS_PROTOCOL_LIST_TERM;

  return data;
};

bool connectWebSocket(std::string URL, struct lws_protocols *protocols,
                      uint32_t connectionID) {
  struct lws_context_creation_info info;

  bzero(&info, sizeof(info));

  info.protocols = protocols;

  uint32_t *connectionIDRef = (uint32_t *)malloc(sizeof(uint32_t));
  *connectionIDRef = connectionID;

  info.user = connectionIDRef;
  info.uid = -1;
  info.gid = -1;
#ifdef SUPPORT_DEFLATE
  info.extensions = supportedExtensions;
#endif
  info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  info.options |= LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW;

  struct lws_context *context = lws_create_context(&info);

  struct lws_client_connect_info connectInfo;
  bzero(&connectInfo, sizeof(connectInfo));

  connectInfo.opaque_user_data = (void *)connectionIDRef;

  const char *URLProtocol = NULL, *URLPath = NULL;
  char *tempURL = mallocString(URL);

  if (lws_parse_uri(tempURL, &URLProtocol, &connectInfo.address, &connectInfo.port, &URLPath)) {
    return false;
  }

  GENERALDEBUG("Will connect to %s port %d with path %s using protocol %s\n",
               connectInfo.address, connectInfo.port, URLPath, URLProtocol);

  bool use_ssl = false;
  if (!strcmp(URLProtocol, "https") || !strcmp(URLProtocol, "wss")) {
    use_ssl = LCCSCF_USE_SSL;
  }

  /* Add a leading / on the path if it is missing. */
  char *path = NULL;
  asprintf(&path, "%s%s", (URLPath[0] != '/') ? "/" : "", URLPath);
  connectInfo.path = path;

  connectInfo.context = context;
  connectInfo.ssl_connection = use_ssl;
  connectInfo.host = connectInfo.address;
  connectInfo.origin = connectInfo.address;
  connectInfo.ietf_version_or_minus_one = -1;

  // Probably don't do any of this.
  // connectInfo.method = "POST";
  // connectInfo.method = "GET";
  // connectInfo.method = "RAW";

  connectInfo.method = NULL; // "RAW";
  // connectInfo.protocol = mallocString(protocols[0]);

  lws_client_connect_via_info(&connectInfo);

// lws_set_opaque_user_data

  free(tempURL);

  return true;
}

bool sendWebSocketData(uint32_t connectionID, uint8_t *data, uint64_t length, bool isUTF8) {
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];

  WebSocketsDataItem *item = new WebSocketsDataItem(data, length, !isUTF8);

  if (dataProviderGroup == nullptr) {
    fprintf(stderr, "No provider group.  Failing.\n");
    return false;
  }
  dataProviderGroup->outgoingData.addPendingData(item);
  return true;
}

void setConnectionState(uint32_t connectionID, int state) {
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];
  dataProviderGroup->connectionState = state;
}

void getWebSocketConnectionState(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate *isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Uint32> connectionIDV8 = v8::Handle<v8::Uint32>::Cast(args[0]);
  uint32_t connectionID = connectionIDV8->Uint32Value(context).ToChecked();

  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];
  if (dataProviderGroup == nullptr) {
    args.GetReturnValue().Set(kConnectionStateClosed);
  } else {
    args.GetReturnValue().Set(dataProviderGroup->connectionState);
  }
}

void logMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::String::Utf8Value messageV8(v8::Isolate::GetCurrent(), args[0]);
  std::string messageString(*messageV8);
  fprintf(stderr, "%s\n", messageString.c_str());
}

void callConnectionError(uint32_t connectionID) {
  // Call didReceiveError on object.
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];
  if (dataProviderGroup != nullptr) {
    dataProviderGroup->hasConnectionError = true;
  }
}

void callConnectionDidOpen(int connectionID, v8::Isolate *isolate) {
  // @@@
  v8::HandleScope handle_scope(isolate);
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];

  v8::Local<v8::String> methodName =
      v8::String::NewFromUtf8(isolate, "_didOpen").ToLocalChecked();

  v8::Persistent<v8::Object> *object = dataProviderGroup->jsObject;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Function> method = v8::Local<v8::Function>::Cast(object->Get(isolate)->Get(context, methodName).ToLocalChecked());

  v8::Local<v8::Object> localObject = v8::Local<v8::Object>::New(isolate, *object);
  v8::Local<v8::Value> result = method->Call(context, localObject, 0, nullptr).ToLocalChecked();
}

void callConnectionDidClose(int connectionID, v8::Isolate *isolate, int codeNumber,
                            std::string *reason) {
  v8::HandleScope handle_scope(isolate);
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];

  v8::Local<v8::String> methodName =
      v8::String::NewFromUtf8(isolate, "_connectionDidClose").ToLocalChecked();

  v8::Local<v8::Integer> code =
      v8::Local<v8::Integer>::New(isolate, v8::Integer::NewFromUnsigned(isolate, codeNumber));

  std::string *reportedReason = reason ?: new std::string("Unknown");
  v8::Local<v8::String> reasonV8 =
      v8::String::NewFromUtf8(isolate, reportedReason->c_str()).ToLocalChecked();

  v8::Persistent<v8::Object> *object = dataProviderGroup->jsObject;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Function> method = v8::Local<v8::Function>::Cast(object->Get(isolate)->Get(context, methodName).ToLocalChecked());

  v8::Local<v8::Value> args[2];
  args[0] = code;
  args[1] = reasonV8;

  v8::Local<v8::Object> localObject = v8::Local<v8::Object>::New(isolate, *object);
  v8::Local<v8::Value> result = method->Call(context, localObject, 2, args).ToLocalChecked();

  if (reason == nullptr) {
    delete reportedReason;
  }
}

void callHasConnectionError(int connectionID, v8::Isolate *isolate) {
  // @@@
  v8::HandleScope handle_scope(isolate);
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];

  v8::Local<v8::String> methodName =
      v8::String::NewFromUtf8(isolate, "_didReceiveError").ToLocalChecked();

  v8::Persistent<v8::Object> *object = dataProviderGroup->jsObject;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Function> method = v8::Local<v8::Function>::Cast(object->Get(isolate)->Get(context, methodName).ToLocalChecked());

  v8::Local<v8::Object> localObject = v8::Local<v8::Object>::New(isolate, *object);
  v8::Local<v8::Value> result = method->Call(context, localObject, 0, nullptr).ToLocalChecked();
}

void sendPendingDataToClient(int connectionID, v8::Isolate *isolate) {
  v8::HandleScope handle_scope(isolate);
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];

  v8::Local<v8::String> methodName =
      v8::String::NewFromUtf8(isolate, "_connectionDidReceiveData").ToLocalChecked();

  v8::Persistent<v8::Object> *object = dataProviderGroup->jsObject;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Function> method = v8::Local<v8::Function>::Cast(object->Get(isolate)->Get(context, methodName).ToLocalChecked());

  WebSocketsDataItem *dataItem;
  while (dataProviderGroup->incomingData.getPendingData(&dataItem)) {

    uint8_t *buf = dataItem->GetBuf();
    size_t length = dataItem->GetLength();

#ifdef SEND_AS_BINARY
    v8::Local<v8::ArrayBuffer> dataArray =
        v8::Local<v8::ArrayBuffer>::New(isolate, v8::ArrayBuffer::New(isolate, length));

    for (size_t i = 0; i < length; i++) {
      dataArray->Set(context, v8::Number::New(isolate, i),
                 v8::Integer::New(isolate, buf[i])).Check();
    }
#else
    // char *dataCString = malloc(length + 1);
    // bcopy(buf, dataCString);
    // buf[length] '\0';

    v8::Local<v8::String> dataString =
        v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), (const char *)buf,
                                v8::NewStringType::kNormal, length)
            .ToLocalChecked();
#endif

    v8::Local<v8::Value> args[1];
#ifdef SEND_AS_BINARY
    args[0] = dataArray;
#else
    args[0] = dataString;
#endif

    v8::Local<v8::Object> localObject = v8::Local<v8::Object>::New(isolate, *object);
    v8::Local<v8::Value> result = method->Call(context, localObject, 1, args).ToLocalChecked();
  }
}


int websocketLWSCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t length) {
  uint32_t connectionID = connectionIDForWSI(wsi);

  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];

  if (dataProviderGroup == nullptr) {
    GENERALDEBUG("Closing connection because data provider group is NULL.\n");
    return -1;
  }
  if (wsi) {
    WSIDEBUG("Will set WSI to 0x%p\n", wsi);
    dataProviderGroup->SetWSI(wsi);
  }

  switch (reason) {
    case LWS_CALLBACK_WSI_CREATE:
      CBDEBUG("@@@ Got callback LWS_CALLBACK_WSI_CREATE\n");
      dataProviderGroup->SetWSI(wsi);
      break;
    case LWS_CALLBACK_WSI_DESTROY:
      CBDEBUG("@@@ Got callback LWS_CALLBACK_WSI_DESTROY\n");
      dataProviderGroup->SetWSI(nullptr);
      break;
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      CBDEBUG("@@@ Got callback LWS_CALLBACK_CLIENT_ESTABLISHED\n");
    case LWS_CALLBACK_RAW_CONNECTED:
      CBDEBUG("@@@ Got callback LWS_CALLBACK_RAW_CONNECTED\n");
      setConnectionState(connectionID, kConnectionStateConnected);
      dataProviderGroup->connectionDidOpen = true;
      if (dataProviderGroup->activeProtocolName != NULL) {
        break;
      }
      // Fall through and set the active protocol.
    case LWS_CALLBACK_CLIENT_HTTP_BIND_PROTOCOL:
    case LWS_CALLBACK_WS_CLIENT_BIND_PROTOCOL:
      CBDEBUG("@@@ Got callback LWS_CALLBACK_WS_CLIENT_BIND_PROTOCOL\n");
    case LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL:
    {
      CBDEBUG("@@@ Got callback LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL\n");
      const struct lws_protocols *protocol = lws_get_protocol(wsi);
      dataProviderGroup->activeProtocolName = new std::string(protocol->name);
      break;
    }
    case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
    {
      uint16_t *value = (uint16_t *)in;
      dataProviderGroup->codeNumber = htons(*value);

      if (length > 2) {
        char *tempString = (char *)malloc(length - 1);
        bcopy((void *)((uint8_t *)in + 2), tempString, length - 2);
        tempString[length - 2] = '\0';
        dataProviderGroup->reason = new std::string(tempString);
        free(tempString);
      }
      // Fall through.
    }
    case LWS_CALLBACK_CLOSED:
      CBDEBUG("@@@ Got callback LWS_CALLBACK_CLOSED\n");
    case LWS_CALLBACK_RAW_CLOSE:
      CBDEBUG("@@@ Got callback LWS_CALLBACK_RAW_CLOSED\n");
      setConnectionState(connectionID, kConnectionStateClosed);
      dataProviderGroup->connectionDidClose = true;
      break;
    case LWS_CALLBACK_CLIENT_RECEIVE:
    {
      CBDEBUG("@@@ Got callback LWS_CALLBACK_CLIENT_RECEIVE\n");
      WebSocketsDataItem *item = new WebSocketsDataItem((uint8_t *)in, length, true);
      CBDEBUG("@@@ Mid-callback.\n");
      dataProviderGroup->incomingData.addPendingData(item);
      CBDEBUG("@@@ Leaving callback.\n");

      break;
    }
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      CBDEBUG("@@@ Got callback LWS_CALLBACK_CLIENT_CONNECTION_ERROR\n");
      callConnectionError(connectionID);
      setConnectionState(connectionID, kConnectionStateClosed);
      break;
    case LWS_CALLBACK_RAW_WRITEABLE:
      CBDEBUG("@@@ Got callback LWS_CALLBACK_RAW_WRITEABLE\n");
    case LWS_CALLBACK_CLIENT_WRITEABLE:
    {
      CBDEBUG("@@@ Got callback LWS_CALLBACK_CLIENT_WRITEABLE\n");
      WebSocketsDataItem *item = NULL;
      if (dataProviderGroup->outgoingData.getPendingData(&item)) {
        size_t bytesWritten = (int)lws_write(wsi, (unsigned char *)item->GetBuf(),
                                item->GetLength(),
                                item->IsBinary() ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);
          if (bytesWritten < 0) {
            CBDEBUG("Closing connection because of write failure.\n");
            return -1;
          } else if (bytesWritten < item->GetLength()) {
            lwsl_err("Partial write LWS_CALLBACK_CLIENT_WRITEABLE\n");
          }
      }
      break;
    }
    case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
      CBDEBUG("@@@ Got callback LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED\n");
#ifdef SUPPORT_DEFLATE
      if ((strcmp((const char *)in, "deflate-stream") == 0) &&
        deny_deflate) {
          lwsl_notice("denied deflate-stream extension\n");
          CBDEBUG("Extension deflate-stream detected.  Returning 1.\n");
          return 1;
      }
      if ((strcmp((const char *)in, "x-webkit-deflate-frame") == 0)) {
        CBDEBUG("Extension x-webkit-deflate-frame detected.  Returning 1.\n");
        return 1;
      }
      if ((strcmp((const char *)in, "deflate-frame") == 0)) {
        CBDEBUG("Extension deflate-frame detected.  Returning 1.\n");
        return 1;
      }
#endif
      return 0;
    case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
      // If certs don't verify on their own, let them fail.
      CBDEBUG("Ignoring SSL callback.\n");
      return 0;

    // Ignore all of these.
    case LWS_CALLBACK_CONNECTING:  // 105
        CBDEBUG("Ignoring callback LWS_CALLBACK_CONNECTING\n");
        break;
    case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
        CBDEBUG("Ignoring callback LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP\n");
        break;
    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
        CBDEBUG("Ignoring callback LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ\n");
        break;
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
        CBDEBUG("Ignoring callback LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER\n");
        break;
    case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
        CBDEBUG("Ignoring callback LWS_CALLBACK_CLIENT_HTTP_WRITEABLE\n");
        break;
    case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
        CBDEBUG("Ignoring callback LWS_CALLBACK_COMPLETED_CLIENT_HTTP\n");
        break;
    case LWS_CALLBACK_PROTOCOL_INIT:
        CBDEBUG("Ignoring callback LWS_CALLBACK_PROTOCOL_INIT\n");
        break;
    case LWS_CALLBACK_VHOST_CERT_AGING:
        CBDEBUG("Ignoring callback LWS_CALLBACK_VHOST_CERT_AGING\n");
        break;
    case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
        CBDEBUG("Ignoring callback LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH\n");
        break;
    default:
        CBDEBUG("Ignoring callback %d\n", reason);
        break;
  }
  if (dataProviderGroup == nullptr || dataProviderGroup->shouldCloseConnection) {
    GENERALDEBUG("Closing connection because of client request.\n");
    return -1;
  }
  return 0;
}
// Ignoring callback 31 LWS_CALLBACK_GET_THREAD_ID 
// Ignoring callback 71 LWS_CALLBACK_EVENT_WAIT_CANCELLED
// Ignoring callback 61
// Ignoring callback 72 LWS_CALLBACK_VHOST_CERT_AGING

uint32_t connectionIDForWSI(struct lws *wsi) {
  struct lws_context *context = lws_get_context(wsi);
  uint32_t *connectionIDRef = (uint32_t *)lws_context_user(context);
  return *connectionIDRef;
}

#pragma mark - Data provider methods

DataProvider::DataProvider(const char *name) {
  this->name = name;
}

void DataProvider::SetWSI(struct lws *wsi) {
  WSIDEBUG("DataProvider: Setting WSI to 0x%p for 0x%p\n", wsi, this);
  this->wsi = wsi;
}

void DataProvider::addPendingData(WebSocketsDataItem *item) {
  std::lock_guard<std::recursive_mutex> guard(this->mutex);

  websocketsDataItemChain_t *chainItem =
      (websocketsDataItemChain_t *)malloc(sizeof(websocketsDataItemChain_t));
  chainItem->item = item;
  chainItem->next = nullptr;

  if (this->firstItem == nullptr) {
    this->firstItem = chainItem;
  } else {
    websocketsDataItemChain_t *position = this->firstItem;
    while (position && position->next) {
      position = position->next;
    }
    position->next = chainItem;
  }

  GENERALDEBUG("Appended to provider %s.  Queue length now %d\n",
               this->name, this->PendingBytes());

  if (this->wsi != nullptr) {
    GENERALDEBUG("Requesting callback on writable.\n");
    lws_callback_on_writable(this->wsi);
  } else {
    GENERALDEBUG("Not requesting callback from 0x%p because wsi is NULL.\n", this);
  }
}

bool DataProvider::getPendingData(WebSocketsDataItem **returnItem) {
  std::lock_guard<std::recursive_mutex> guard(this->mutex);

  websocketsDataItemChain_t *chainItem = this->firstItem;
  if (chainItem == nullptr) {
    return false;
  }
  *returnItem = chainItem->item;
  this->firstItem = chainItem->next;

  free(chainItem);

  return true;
}

uint32_t DataProvider::PendingBytes(void) {
  std::lock_guard<std::recursive_mutex> guard(this->mutex);

  uint32_t pendingBytes = 0;
  for (websocketsDataItemChain_t *chainItem = this->firstItem; chainItem; chainItem = chainItem->next) {
    pendingBytes = chainItem->item->GetLength();
  }

  return pendingBytes;
}

size_t WebSocketsDataItem::GetLength() {
  return this->rawLength;
}

uint8_t * WebSocketsDataItem::GetBuf() {
  return this->rawBuf;
}

bool WebSocketsDataItem::IsBinary() {
  return this->rawIsBinary;
}

WebSocketsDataItem::WebSocketsDataItem(uint8_t *buf, size_t length, bool isBinary) {

  this->rawBuf = (uint8_t *)malloc(length);
  this->rawLength = length;
  this->rawIsBinary = isBinary;

  bcopy(buf, this->rawBuf, length);
}

WebSocketsDataItem::~WebSocketsDataItem(void) {
  free(this->rawBuf);
}


// Future issues:
//
// 1.  Ideally, we should run LWS code in a different thread per context so that
//     the waits don't have to be so short.  This also involves figuring out how to
//     dispatch new connection requests to that thread from the JS thread so that
//     everything works.
//
// 2.  Ideally, for a more generally useful integration, we should probably have
//     support for setTimeout() and setInteval().

#define _GNU_SOURCE  // For asprintf

#include <libplatform/libplatform.h>
#include <libwebsockets.h>
#include <map>
#include <set>
#include <stdio.h>
#include <v8.h>

// Might work on Linux.  Doesn't work in macOS.
#undef SUPPORT_DEFLATE

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
    void addPendingData(WebSocketsDataItem *item);
    bool getPendingData(WebSocketsDataItem **returnItem);
    uint32_t PendingBytes(void);
    void SetWSI(struct lws *wsi);
  private:
    std::mutex mutex;
    websocketsDataItemChain_t *firstItem = NULL;
    struct lws *wsi = nullptr;
};

class WebSocketsContextData {
  public:
    WebSocketsContextData(v8::Persistent<v8::Object> *jsObject,
                          struct lws_protocols * protocols);
    ~WebSocketsContextData(void);

    v8::Persistent<v8::Object> *jsObject = nullptr;

    DataProvider incomingData;
    DataProvider outgoingData;

    bool isBinary = false;
    bool shouldCloseConnection = false;
    bool didCloseConnection = false;
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
                                             struct lws_protocols * protocols) {
  this->jsObject = jsObject;
  this->protocols = protocols;
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
  this->wsi = wsi;
  this->incomingData.SetWSI(wsi);
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

  fprintf(stderr, "setProgramAndPreviewScenes called.\n");

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

  // Create a new context.
  v8::Local<v8::Context> context = v8::Context::New(gIsolate, nullptr, globals);
  context->Enter();

  return (void *)gIsolate;
}

void callConnectionDidClose(int connectionID, v8::Isolate *isolate, int codeNumber,
                            std::string *reason);
void callHasConnectionError(int connectionID, v8::Isolate *isolate);
void sendPendingDataToClient(int connectionID, v8::Isolate *isolate);

void v8_runLoopCallback(void *isolateVoid) {
  v8::Isolate *isolate = (v8::Isolate *)isolateVoid;
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);

  std::vector<int32_t> connectionIDsToDelete;
  for (std::pair<int32_t, WebSocketsContextData *> element :
       connectionData) {
    int32_t connectionID = element.first;
    WebSocketsContextData *connection = element.second;
    if (connection->didCloseConnection) {
      callConnectionDidClose(connectionID, isolate, connection->codeNumber,
                             connection->reason);
      connectionIDsToDelete.push_back(connectionID);
      continue;
    }
    if (connection->hasConnectionError) {
      callHasConnectionError(connectionID, isolate);
      continue;
    }
    if (connection->incomingData.PendingBytes() > 0) {
      sendPendingDataToClient(connectionID, isolate);
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
  fprintf(stderr, "PasswordGetter called.\n");
  
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
// didOpen()
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

  fprintf(stderr, "connectWebSocket called.\n");

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
      new WebSocketsContextData(persistentObject, protocols);
  bool success = connectWebSocket(URL, protocols, newConnectionIdentifier);

  args.GetReturnValue().Set(newConnectionIdentifier++);
}

bool sendWebSocketData(uint32_t connectionID, uint8_t *data, uint64_t length, bool isUTF8);

// sendWebSocketData(this.internal_connection_id, data);
void sendWebSocketData(const v8::FunctionCallbackInfo<v8::Value>& args) {

fprintf(stderr, "Called sendWebSocketData\n");

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
  fprintf(stderr, "@@@ called closeWebSocket\n");

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
  fprintf(stderr, "Got setWebSocketBinaryType (unsupported)\n");
  exit(1);
}

void getWebSocketActiveProtocol(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate *isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Uint32> connectionIDV8 = v8::Handle<v8::Uint32>::Cast(args[0]);
  uint32_t connectionID = connectionIDV8->Uint32Value(context).ToChecked();
  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];

  args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate,
      dataProviderGroup->activeProtocolName->c_str()).ToLocalChecked());
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
    fprintf(stderr, "Protocol %zu: %s\n", i, protocols[i].c_str());
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

  fprintf(stderr, "Will connect to %s port %d with path %s using protocol %s\n",
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

  connectInfo.method = "RAW";
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

  v8::Local<v8::Value> result = method->Call(context, context->Global(), 2, args).ToLocalChecked();

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

  v8::Local<v8::Value> result = method->Call(context, context->Global(), 0, nullptr).ToLocalChecked();
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

    v8::Local<v8::Array> dataArray = v8::Local<v8::Array>::New(isolate, v8::Array::New(isolate));
    uint8_t *buf = dataItem->GetBuf();
    size_t length = dataItem->GetLength();

    for (size_t i = 0; i < length; i++) {
      dataArray->Set(context, v8::Number::New(isolate, 1),
                 v8::Integer::New(isolate, buf[i])).Check();
    }

    v8::Local<v8::Value> args[1];
    args[0] = dataArray;

    v8::Local<v8::Value> result = method->Call(context, context->Global(), 1, args).ToLocalChecked();
  }
}


int websocketLWSCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t length) {
  uint32_t connectionID = connectionIDForWSI(wsi);

  std::lock_guard<std::recursive_mutex> guard(connection_mutex);
  WebSocketsContextData *dataProviderGroup = connectionData[connectionID];

  dataProviderGroup->SetWSI(wsi);

  if (dataProviderGroup == nullptr) {
    fprintf(stderr, "Closing connection because data provider group is NULL.\n");
    return -1;
  }
  if (dataProviderGroup == nullptr || dataProviderGroup->shouldCloseConnection) {
    fprintf(stderr, "Closing connection because of client request.\n");
    return -1;
  }

  switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      fprintf(stderr, "@@@ Got callback LWS_CALLBACK_CLIENT_ESTABLISHED\n");
      setConnectionState(connectionID, kConnectionStateConnected);
      break;
    case LWS_CALLBACK_CLOSED:
      fprintf(stderr, "@@@ Got callback LWS_CALLBACK_CLOSED\n");
      setConnectionState(connectionID, kConnectionStateClosed);
      dataProviderGroup->didCloseConnection = true;
      break;
    case LWS_CALLBACK_CLIENT_RECEIVE:
    {
      fprintf(stderr, "@@@ Got callback LWS_CALLBACK_CLIENT_RECEIVE\n");
      WebSocketsDataItem *item = new WebSocketsDataItem((uint8_t *)in, length, true);
      dataProviderGroup->incomingData.addPendingData(item);

      break;
    }
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      fprintf(stderr, "@@@ Got callback LWS_CALLBACK_CLIENT_CONNECTION_ERROR\n");
      callConnectionError(connectionID);
      setConnectionState(connectionID, kConnectionStateClosed);
      break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
    {
      fprintf(stderr, "@@@ Got callback LWS_CALLBACK_CLIENT_WRITEABLE\n");
      WebSocketsDataItem *item = NULL;
      if (dataProviderGroup->outgoingData.getPendingData(&item)) {
        size_t bytesWritten = (int)lws_write(wsi, (unsigned char *)item->GetBuf(),
                                item->GetLength(),
                                item->IsBinary() ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);
          if (bytesWritten < 0) {
            fprintf(stderr, "Closing connection because of write failure.\n");
            return -1;
          } else if (bytesWritten < item->GetLength()) {
            lwsl_err("Partial write LWS_CALLBACK_CLIENT_WRITEABLE\n");
          }
      }
      break;
    }
    case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
      fprintf(stderr, "@@@ Got callback LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED\n");
#ifdef SUPPORT_DEFLATE
      if ((strcmp((const char *)in, "deflate-stream") == 0) &&
        deny_deflate) {
          lwsl_notice("denied deflate-stream extension\n");
          fprintf(stderr, "Extension deflate-stream detected.  Returning 1.\n");
          return 1;
      }
      if ((strcmp((const char *)in, "x-webkit-deflate-frame") == 0)) {
        fprintf(stderr, "Extension x-webkit-deflate-frame detected.  Returning 1.\n");
        return 1;
      }
      if ((strcmp((const char *)in, "deflate-frame") == 0)) {
        fprintf(stderr, "Extension deflate-frame detected.  Returning 1.\n");
        return 1;
      }
#endif
      return 0;

    case LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL:
    {
      fprintf(stderr, "@@@ Got callback LWS_CALLBACK_RAW_SKT_BIND_PROTOCOL\n");
      const struct lws_protocols *protocol = lws_get_protocol(wsi);
      dataProviderGroup->activeProtocolName = new std::string(protocol->name);
      break;
    }
    case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
      // If certs don't verify on their own, let them fail.
      fprintf(stderr, "Ignoring SSL callback.\n");
      return 0;

    // Ignore all of these.
    case LWS_CALLBACK_CONNECTING:  // 105
    case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
    case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
    case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
    default:
        fprintf(stderr, "Ignoring callback %d\n", reason);
        break;
  }
  return 0;
}
// LWS_CALLBACK_PROTOCOL_INIT 27

uint32_t connectionIDForWSI(struct lws *wsi) {
  struct lws_context *context = lws_get_context(wsi);
  uint32_t *connectionIDRef = (uint32_t *)lws_context_user(context);
  return *connectionIDRef;
}

#pragma mark - Data provider methods

void DataProvider::SetWSI(struct lws *wsi) {
  this->wsi = wsi;
}

void DataProvider::addPendingData(WebSocketsDataItem *item) {
  std::lock_guard<std::mutex> guard(this->mutex);

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

  if (this->wsi != nullptr) {
    lws_callback_on_writable(this->wsi);
  }
}

bool DataProvider::getPendingData(WebSocketsDataItem **returnItem) {
  std::lock_guard<std::mutex> guard(this->mutex);

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
  std::lock_guard<std::mutex> guard(this->mutex);

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

// In main loop:
// if (hasConnectionError) { call didReceiveError() on object. }

// WebSocketsContextData *dataProviderGroup = connectionData[connectionID];
// if (dataProviderGroup != nullptr && dataProviderGroup->incomingData.PendingBytes()

// if (hasConnectionError) { call didReceiveError() on object. }


// PROBLEMS:
//
// 1.  Need to call                                 lws_callback_on_writable_all_protocol(context,
//                                            &protocols[PROTOCOL_DUMB_INCREMENT]);
//
// for each connection.
//
// 2.  Need to call                 lws_service(context, 500);
//
// 3.  Need to run LWS code in a different thread so that it won't interfere with the V8 threading model.
//
// 4.  Need to figure out how to dispatch new connection requests to that thread from the JS thread
//     so that everything works.

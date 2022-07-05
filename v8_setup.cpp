#include <libplatform/libplatform.h>
#include <v8.h>

static v8::Isolate::CreateParams create_params;

std::vector<std::string> programScenes;
std::vector<std::string> previewScenes;

void setSceneIsProgram(char *sceneName);
void setSceneIsPreview(char *sceneName);
void setSceneIsInactive(char *sceneName);

void ProgramSceneGetter(v8::Local<v8::String> property,
              const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  v8::Local<v8::Array> programScenesV8 = v8::Array::New(isolate, programScenes.size());

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (int i = 0; i < programScenes.size(); i++) {
    v8::Local<v8::String> programSceneV8String =
        v8::String::NewFromUtf8(isolate, programScenes[i].c_str()).ToLocalChecked();
    programScenesV8->Set(context, i, programSceneV8String);
  }
  info.GetReturnValue().Set(programScenesV8);
}

void ProgramSceneSetter(v8::Local<v8::String> property, v8::Local<v8::Value> value,
             const v8::PropertyCallbackInfo<void>& info) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  if(!value->IsArray()) {
    return;
  }

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
  int length = array->Length();

  std::vector<std::string> newProgramScenes;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  for(int i = 0; i < length; i++) {
    v8::Local<v8::Value> element = array->Get(context, i).ToLocalChecked();

    v8::String::Utf8Value programSceneUTF8(isolate, value);
    std::string programSceneCPPString(*programSceneUTF8);
    programScenes[i] = programSceneCPPString;
  }
}

void PreviewSceneGetter(v8::Local<v8::String> property,
              const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  v8::Local<v8::Array> previewScenesV8 = v8::Array::New(isolate, previewScenes.size());

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (int i = 0; i < previewScenes.size(); i++) {
    v8::Local<v8::String> previewSceneV8String =
        v8::String::NewFromUtf8(isolate, previewScenes[i].c_str()).ToLocalChecked();
    previewScenesV8->Set(context, i, previewSceneV8String);
  }
  info.GetReturnValue().Set(previewScenesV8);
}

void PreviewSceneSetter(v8::Local<v8::String> property, v8::Local<v8::Value> value,
             const v8::PropertyCallbackInfo<void>& info) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  if(!value->IsArray()) {
    return;
  }

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
  int length = array->Length();

  std::vector<std::string> newPreviewScenes;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  for(int i = 0; i < length; i++) {
    v8::Local<v8::Value> element = array->Get(context, i).ToLocalChecked();

    v8::String::Utf8Value previewSceneUTF8(isolate, value);
    std::string previewSceneCPPString(*previewSceneUTF8);
    previewScenes[i] = previewSceneCPPString;
  }
}
void v8_setup(void) {
  v8::V8::InitializeICUDefaultLocation("viscaptz");
  v8::V8::InitializeExternalStartupData("viscaptz");
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  // YGetter/YSetter are so similar they are omitted for brevity

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->SetAccessor(v8::String::NewFromUtf8(isolate, "ProgramScene").ToLocalChecked(),
                      ProgramSceneGetter, ProgramSceneSetter);
  global->SetAccessor(v8::String::NewFromUtf8(isolate, "PreviewScene").ToLocalChecked(),
                      PreviewSceneGetter, PreviewSceneSetter);

  v8::Isolate::Scope isolate_scope(isolate);
  // Create a stack-allocated handle scope.
  v8::HandleScope handle_scope(isolate);

  // Create a new context.
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);

  // Enter the context for compiling and running scripts.
  v8::Context::Scope context_scope(context);
}


void runScript(char *scriptString) {
  // Create a string containing the JavaScript source code.
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(isolate, scriptString,
                              v8::NewStringType::kNormal)
          .ToLocalChecked();
  // Compile the source code.
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source).ToLocalChecked();
  // Run the script to get the result.
  v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
  // Convert the result to an UTF8 string and print it.
  v8::String::Utf8Value utf8(isolate, result);
  printf("%s\n", *utf8);
}

void v8_teardown(void) {
  // Dispose the isolate and tear down V8.
  // isolate->Dispose();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  delete create_params.array_buffer_allocator;
}

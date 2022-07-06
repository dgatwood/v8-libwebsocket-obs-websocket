#include <libplatform/libplatform.h>
#include <set>
#include <v8.h>

#include "v8_setup.h"

std::unique_ptr<v8::Platform> platform;
// v8::Isolate *gIsolate;
v8::Local<v8::ObjectTemplate> globals;

std::vector<std::string> programScenes;
std::vector<std::string> previewScenes;
void updateScenes(std::vector<std::string> newPreviewScenes, std::vector<std::string> newProgramScenes);

extern "C" {
void setSceneIsProgram(const char *sceneName);
void setSceneIsPreview(const char *sceneName);
void setSceneIsInactive(const char *sceneName);
};

void ProgramSceneGetter(v8::Local<v8::String> property,
              const v8::PropertyCallbackInfo<v8::Value>& info);
void ProgramSceneSetter(v8::Local<v8::String> property, v8::Local<v8::Value> value,
             const v8::PropertyCallbackInfo<void>& info);
void PreviewSceneGetter(v8::Local<v8::String> property,
              const v8::PropertyCallbackInfo<v8::Value>& info);
void PreviewSceneSetter(v8::Local<v8::String> property, v8::Local<v8::Value> value,
             const v8::PropertyCallbackInfo<void>& info);

void testFunction(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  fprintf(stderr, "Test function called.\n");
}

void v8_setup(void) {
  v8::V8::InitializeICUDefaultLocation("viscaptz");
  v8::V8::InitializeExternalStartupData("viscaptz");
  platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
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
  globals->SetAccessor(v8::String::NewFromUtf8(gIsolate, "ProgramScene", v8::NewStringType::kNormal).ToLocalChecked(),
                       ProgramSceneGetter, ProgramSceneSetter);
  globals->SetAccessor(v8::String::NewFromUtf8(gIsolate, "PreviewScene", v8::NewStringType::kNormal).ToLocalChecked(),
                       PreviewSceneGetter, PreviewSceneSetter);

  globals->Set(v8::String::NewFromUtf8(gIsolate, "testFunction").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, testFunction));

  // Create a new context.
  v8::Local<v8::Context> context = v8::Context::New(gIsolate, nullptr, globals);
  context->Enter();
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

void v8_teardown(void) {
  // Dispose the isolate and tear down V8.
  // isolate->Dispose();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  // delete create_params.array_buffer_allocator;
}


#pragma mark - Synthesized getters and setters

void ProgramSceneGetter(v8::Local<v8::String> property,
              const v8::PropertyCallbackInfo<v8::Value>& info) {
  fprintf(stderr, "ProgramSceneGetter called.\n");

  // Create a stack-allocated handle scope.
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  v8::Local<v8::Array> programScenesV8 = v8::Array::New(v8::Isolate::GetCurrent(), programScenes.size());

  v8::Local<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext();
  for (int i = 0; i < programScenes.size(); i++) {
    v8::Local<v8::String> programSceneV8String =
        v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), programScenes[i].c_str()).ToLocalChecked();
    programScenesV8->Set(context, i, programSceneV8String);
  }
  info.GetReturnValue().Set(programScenesV8);
}

void ProgramSceneSetter(v8::Local<v8::String> property, v8::Local<v8::Value> value,
             const v8::PropertyCallbackInfo<void>& info) {
  fprintf(stderr, "ProgramSceneSetter called.\n");

  // Create a stack-allocated handle scope.
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  if(!value->IsArray()) {
    fprintf(stderr, "ProgramSceneSetter called with non-array.\n");
    return;
  }

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
  int length = array->Length();

  std::vector<std::string> newProgramScenes;
  v8::Local<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext();

  for (int i = 0; i < length; i++) {
    v8::Local<v8::Value> element = array->Get(context, i).ToLocalChecked();

    v8::String::Utf8Value programSceneUTF8(v8::Isolate::GetCurrent(), value);
    std::string programSceneCPPString(*programSceneUTF8);
    newProgramScenes[i] = programSceneCPPString;
  }

  updateScenes(previewScenes, newProgramScenes);
}

void PreviewSceneGetter(v8::Local<v8::String> property,
              const v8::PropertyCallbackInfo<v8::Value>& info) {
  fprintf(stderr, "PreviewSceneGetter called.\n");

  // Create a stack-allocated handle scope.
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  v8::Local<v8::Array> previewScenesV8 = v8::Array::New(v8::Isolate::GetCurrent(), previewScenes.size());

  v8::Local<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext();
  for (int i = 0; i < previewScenes.size(); i++) {
    v8::Local<v8::String> previewSceneV8String =
        v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), previewScenes[i].c_str()).ToLocalChecked();
    previewScenesV8->Set(context, i, previewSceneV8String);
  }
  info.GetReturnValue().Set(previewScenesV8);
}

void PreviewSceneSetter(v8::Local<v8::String> property, v8::Local<v8::Value> value,
             const v8::PropertyCallbackInfo<void>& info) {
  fprintf(stderr, "ProgramSceneSetter called.\n");

  // Create a stack-allocated handle scope.
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  if(!value->IsArray()) {
    fprintf(stderr, "PreviewSceneSetter called with non-array.\n");
    return;
  }

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
  int length = array->Length();

  std::vector<std::string> newPreviewScenes;
  v8::Local<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext();

  for (int i = 0; i < length; i++) {
    v8::Local<v8::Value> element = array->Get(context, i).ToLocalChecked();

    v8::String::Utf8Value previewSceneUTF8(v8::Isolate::GetCurrent(), value);
    std::string previewSceneCPPString(*previewSceneUTF8);
    newPreviewScenes[i] = previewSceneCPPString;
  }

  updateScenes(newPreviewScenes, programScenes);
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

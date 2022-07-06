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

void setProgramAndPreviewScenes(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  fprintf(stderr, "setProgramAndPreviewScenes called.\n");

  if(args.Length() != 2 || !args[0]->IsArray() || !args[1]->IsArray()) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, "Error: Two arrays expected").ToLocalChecked()));
    return;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Handle<v8::Array> previewSceneArray = v8::Handle<v8::Array>::Cast(args[0]);

  std::vector<std::string> newProgramScenes;

  for (int i = 0; i < previewSceneArray->Length(); i++) {
    v8::Local<v8::Value> element = previewSceneArray->Get(context, i).ToLocalChecked();
    v8::String::Utf8Value programSceneUTF8(v8::Isolate::GetCurrent(), element);
    std::string programSceneCPPString(*programSceneUTF8);
    newProgramScenes.push_back(programSceneCPPString);
  }

  v8::Handle<v8::Array> programSceneArray = v8::Handle<v8::Array>::Cast(args[1]);

  std::vector<std::string> newPreviewScenes;

  for (int i = 0; i < programSceneArray->Length(); i++) {
    v8::Local<v8::Value> element = programSceneArray->Get(context, i).ToLocalChecked();
    v8::String::Utf8Value previewSceneUTF8(v8::Isolate::GetCurrent(), element);
    std::string previewSceneCPPString(*previewSceneUTF8);
    newPreviewScenes.push_back(previewSceneCPPString);
  }

  updateScenes(newPreviewScenes, newProgramScenes);
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

  globals->Set(v8::String::NewFromUtf8(gIsolate, "setProgramAndPreviewScenes").ToLocalChecked(),
               v8::FunctionTemplate::New(gIsolate, setProgramAndPreviewScenes));

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

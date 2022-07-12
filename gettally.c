#include "bin/gettally.h"
#include "bin/obs-websocket.h"
#include "bin/websocket.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uuid/uuid.h>


#include "v8_setup.h"

// This supports ONLY the new 5.0 protocol.

void (*gProgramCallback)(const char *sceneName);
void (*gPreviewCallback)(const char *sceneName);
void (*gInactiveCallback)(const char *sceneName);

void registerOBSProgramCallback(void (*callbackPointer)(const char *sceneName)) {
  gProgramCallback = callbackPointer;
}

void registerOBSPreviewCallback(void (*callbackPointer)(const char *sceneName)) {
  gPreviewCallback = callbackPointer;
}

void registerOBSInactiveCallback(void (*callbackPointer)(const char *sceneName)) {
  gInactiveCallback = callbackPointer;
}

void runOBSTally(char *password) {
  setOBSPassword(password);

#if 1
  void *isolate = v8_setup();
#if 1
  runScript(websocket_js);
  runScript(obs_websocket_js);
  runScript(gettally_js);
#else
  runScriptAsModule("websocket_js", websocket_js);
  runScriptAsModule("obs_websocket_js", obs_websocket_js);
  runScriptAsModule("gettally_js", gettally_js);
#endif

  while (true) {
    v8_runLoopCallback(isolate);
    usleep(1000);  // 1000 callbacks per second.
  }
#endif
}

void _setSceneIsProgram(const char *sceneName) {
  if (gProgramCallback != NULL) {
    gProgramCallback(sceneName);
  }
}

void _setSceneIsPreview(const char *sceneName) {
  if (gPreviewCallback != NULL) {
    gPreviewCallback(sceneName);
  }
}

void _setSceneIsInactive(const char *sceneName) {
  if (gInactiveCallback != NULL) {
    gInactiveCallback(sceneName);
  }
}

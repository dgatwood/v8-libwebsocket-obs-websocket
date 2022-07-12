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
void (*gPreviewCallback)(const char *sceneName, bool alsoOnProgram);
void (*gInactiveCallback)(const char *sceneName);

void registerOBSProgramCallback(void (*callbackPointer)(const char *sceneName)) {
  gProgramCallback = callbackPointer;
}

void registerOBSPreviewCallback(void (*callbackPointer)(const char *sceneName,
                                                        bool alsoOnProgram)) {
  gPreviewCallback = callbackPointer;
}

void registerOBSInactiveCallback(void (*callbackPointer)(const char *sceneName)) {
  gInactiveCallback = callbackPointer;
}

void runOBSTally(char *OBSWebSocketURL, char *password) {
  setOBSURL(OBSWebSocketURL);
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
  fprintf(stderr, "Calling program callback.\n");
  if (gProgramCallback != NULL) {
    gProgramCallback(sceneName);
  } else {
    fprintf(stderr, "Not setting program (no callback)\n");
  }
  fprintf(stderr, "Done.\n");
}

void _setSceneIsPreview(const char *sceneName, bool alsoOnProgram) {
  fprintf(stderr, "Calling preview callback.\n");
  if (gPreviewCallback != NULL) {
    gPreviewCallback(sceneName, alsoOnProgram);
  } else {
    fprintf(stderr, "Not setting preview (no callback)\n");
  }
  fprintf(stderr, "Done.\n");
}

void _setSceneIsInactive(const char *sceneName) {
  fprintf(stderr, "Calling inactive callback.\n");
  if (gInactiveCallback != NULL) {
    gInactiveCallback(sceneName);
  } else {
    fprintf(stderr, "Not setting inactive (no callback)\n");
  }
  fprintf(stderr, "Done.\n");
}

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

bool onPreview = false;
bool onProgram = false;

void getInitialScenes(void);
void subscribeToStateUpdates(void);


// Returns a UUID string.  The caller is responsible for freeing
// the returned pointer.
char *generate_uuid_string(void);

char *password = "";

int main(int argc, char *argv[]) {

  if (argc > 1) {
    password = argv[1];
  }

#if 1
  v8_setup();
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
    v8_runLoopCallback();
    usleep(1000);  // 1000 callbacks per second.
  }
#endif

  return 0;
}


void getInitialScenes(void) {
  // Obtain initial state:
  //     GetCurrentProgramScene
  //     GetCurrentPreviewScene

}

void subscribeToStateUpdates(void) {
  // Subscribe to:
  //     EventSubscription::Scenes

}

// Handle events:
//     SceneTransitionStarted
//     SceneTransitionEnded

// When we get:
// {
//   "obsWebSocketVersion": string,
//   "rpcVersion": number,
//   "authentication": {   // May be absent.
//     "challenge": "+IxH4CnCiqpX1rM9scsNynZzbOe4KhDeYcTNS3PDaeY=",
//     "salt": "lM1GncleQOaCu9lT1yeUZhFYnqhsLLP1G5lAGo3ixaI="
//   }
// }
//
// Send:
// {
//   "rpcVersion": number,
//   "authentication": string(optional),
//   "eventSubscriptions": number(optional) = (EventSubscription::All)
// }
//
// where authentication is:
//
// Concatenate the websocket password with the salt provided by the server (password + salt)
// Generate an SHA256 binary hash of the result and base64 encode it, known as a base64 secret.
// Concatenate the base64 secret with the challenge sent by the server (base64_secret + challenge)
// Generate a binary SHA256 hash of that result and base64 encode it.

char *generate_uuid_string(void) {
  uuid_t rawUUID;
  uuid_generate(rawUUID);
  char *buf = (char *)malloc(37);
  uuid_unparse(rawUUID, buf);
  return buf;
}

#pragma mark - V8 callbacks

void setSceneIsProgram(const char *sceneName) {
  fprintf(stderr, "Scene %s is on program.\n", sceneName);
}

void setSceneIsPreview(const char *sceneName) {
  fprintf(stderr, "Scene %s is on preview.\n", sceneName);
}

void setSceneIsInactive(const char *sceneName) {
  fprintf(stderr, "Scene %s is inactive.\n", sceneName);
}

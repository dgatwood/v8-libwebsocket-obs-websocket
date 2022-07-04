#include <libwebsockets.h>
#include <stdbool.h>
#include <stdint.h>
#include <uuid/uuid.h>

// This supports ONLY the new 5.0 protocol.

bool onPreview = false;
bool onProgram = false;

void getInitialScenes(void);
void subscribeToStateUpdates(void);

typedef struct simpleCommandPayload {
  const char      *requestType;
  const char      *requestID;
} simpleCommandPayload_t;

typedef struct simpleCommand {
  int op;
  simpleCommandPayload_t payload;
} simpleCommand_t;

const lws_struct_map_t simpleCommandPayload_map[] = {
    LSM_STRING_PTR  (simpleCommandPayload_t, requestType, "requestType"),
    LSM_STRING_PTR  (simpleCommandPayload_t, requestID, "requestId"),
};

const lws_struct_map_t simpleCommand[] = {
    LSM_UNSIGNED    (simpleCommand_t, op,     "op"),
    LSM_CHILD_PTR   (simpleCommand_t, payload, simpleCommandPayload_t,
                     NULL, simpleCommandPayload_map, "d"),
};

const lws_struct_map_t simpleCommandSchemaMap[] = {
    LSM_SCHEMA      (simpleCommand_t, NULL, simpleCommand, "simpleCommand"),
};

// Returns a UUID string.  The caller is responsible for freeing
// the returned pointer.
char *generate_uuid_string(void);

int main(int argc, char *argv[]) {
  getInitialScenes();

  return 0;
}


void getInitialScenes(void) {
  // Obtain initial state:
  //     GetCurrentProgramScene
  //     GetCurrentPreviewScene

  simpleCommand_t command;
  command.op = 6;
  command.payload.requestType = "GetCurrentProgramScene";

  char *tempUUID = generate_uuid_string();
  command.payload.requestID = tempUUID;

  // Generate and send JSON blob here. @@@
  // Read reply here and check UUID.

  free(tempUUID);

  tempUUID = generate_uuid_string();
  command.payload.requestID = tempUUID;

  // Generate and send JSON blob here. @@@
  // Read reply here and check UUID.

  free(tempUUID);
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
  char *buf = malloc(37);
  uuid_unparse(rawUUID, buf);
  return buf;
}

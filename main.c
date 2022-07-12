#include <stdio.h>

#include "gettally.h"

// This supports ONLY the new 5.0 protocol.

void setSceneIsProgram(const char *sceneName);
void setSceneIsPreview(const char *sceneName);
void setSceneIsInactive(const char *sceneName);

int main(int argc, char *argv[]) {
  char *password = "";
  if (argc > 1) {
    password = argv[1];
  }

  registerOBSProgramCallback(&setSceneIsProgram);
  registerOBSPreviewCallback(&setSceneIsPreview);
  registerOBSInactiveCallback(&setSceneIsInactive);

  runOBSTally(password);

  return 0;
}

#pragma mark - Module callbacks

void setSceneIsProgram(const char *sceneName) {
  fprintf(stderr, "PROGRAM SCENE: %s\n", sceneName);
}

void setSceneIsPreview(const char *sceneName) {
  fprintf(stderr, "PREVIEW SCENE: %s\n", sceneName);
}

void setSceneIsInactive(const char *sceneName) {
  fprintf(stderr, "INACTIVE SCENE: %s\n", sceneName);
}

var obs = undefined;

function connectOBS() {
  obs = new OBSWebSocket();

  obs.on('Identified', () => {
    console.log('Connection identified');
    updateInitialScenes();
  });

  obs.on('CurrentSceneChanged', () => {
    console.log('Current scene changed.');
  });

  obs.on('CurrentPreviewSceneChanged', data => {
    console.log('CurrentPreviewSceneChanged: ' + allKeys(data));
    setPreviewScene(data["sceneName"]);
  });

  obs.on('CurrentProgramSceneChanged', data => {
    console.log('CurrentPreviewSceneChanged: ' + allKeys(data));
    setProgramScene(data["sceneName"]);
  });

  obs.on('SceneTransitionStarted', data => {
    console.log('SceneTransitionStarted: ' + allKeys(data));
    setPreviewToProgram();
  });

  obs.connect('ws://127.0.0.1:4455', obsPassword, {
    eventSubscriptions: (1 << 2) | (1 << 4),  /* EventSubcription.Scenes and Transitions */
    rpcVersion: 1
  }).then((value) => {
    logMessage("OBS connected: " + allKeys(value));
    logMessage("WebSocket version: " + value.obsWebSocketVersion);
    logMessage("RPC version: " + value.negotiatedRpcVersion);
  }).catch((error) => {
    logMessage("OBS connection failed: "+allKeys(error) + " in " + 
        error.fileName + ":" + error.lineNumber + ":" + error.message  + error.stack);
    retryAfterTimeout();
  });
}

function allKeys(unknownObject) {
  return allKeysSub(unknownObject, "");
}

function allKeysSub(unknownObject, indent) {
  var string = "";
  var props = Object.keys(unknownObject);
  for (var i = 0; i < props.length; i++) {
    var value = unknownObject[props[i]];
    if (typeof value === 'object') {
      string += indent + props[i] + " : " + allKeys(unknownObject[props[i]], indent + "    ");
    } else {
      string += indent + props[i] + " : " + unknownObject[props[i]];
    }
  }
  return string;
}

var updateInitialScenes = async function() {
  const {currentProgramSceneName} = await obs.call('GetCurrentProgramScene');
  const {currentPreviewSceneName} = await obs.call('GetCurrentPreviewScene');

  setPreviewScene(currentPreviewSceneName);
  setProgramScene(currentProgramSceneName);
}

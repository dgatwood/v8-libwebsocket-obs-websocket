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

  obs.on('SwitchScenes', data => {
    console.log('SwitchScenes', data);
  });

  obs.connect('ws://127.0.0.1:4455', obsPassword, {
    eventSubscriptions: (1 << 2),  /* EventSubcription.Scenes - no idea why the constant won't load. */
    rpcVersion: 1
  }).then((value) => {
    setProgramAndPreviewScenes(["OBS connected: "], [allKeys(value)]);
    setProgramAndPreviewScenes(["WebSocket version: "], [value.obsWebSocketVersion]);
    setProgramAndPreviewScenes(["RPC version: "], [value.negotiatedRpcVersion]);
  }).catch((error) => {
    // console.error("OBS connection failed: "+error);
    setProgramAndPreviewScenes([ "OBS connection failed: "+allKeys(error) ], [ 
        error.fileName + ":" + error.lineNumber + ":" + error.message  + error.stack ] );
    setTimeout(connectOBS, 5000);
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

  setProgramAndPreviewScenes([ currentPreviewSceneName ], [ currentProgramSceneName ]);
}

connectOBS();

// Uncomment to debug in Chrome.

// ProgramScenes = [ "one", "two", "three"];
// PreviewScenes = [ "four", "five", "six"];
// setProgramAndPreviewScenes(PreviewScenes, ProgramScenes);

// setProgramAndPreviewScenes([ obsPassword ], [ "B" ] );

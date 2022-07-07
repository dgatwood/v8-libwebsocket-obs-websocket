const obs = new OBSWebSocket();

var updateInitialScenes = async function() {
  const {currentProgramSceneName} = await obs.call('GetCurrentProgramScene');
  const {currentPreviewSceneName} = await obs.call('GetCurrentPreviewScene');

  setProgramAndPreviewScenes([ currentPreviewSceneName ], [ currentProgramSceneName ]);
}

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

obs.connect('ws://192.168.1.63:4455', obsPassword, {
  eventSubscriptions: (1 << 2),  /* EventSubcription.Scenes - no idea why the constant won't load. */
  rpcVersion: 1
}).then((value) => {
  console.log("OBS connected: "+value);
  console.log("WebSocket version: "+value.obsWebSocketVersion);
  console.log("RPC version: "+value.negotiatedRpcVersion);

}).catch((error) => {
  console.error("OBS connection failed: "+error);
});

// ProgramScenes = [ "one", "two", "three"];
// PreviewScenes = [ "four", "five", "six"];

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

// Uncomment to debug in Chrome.
// function setProgramAndPreviewScenes(preview, program) {
  // console.log("preview: " + allKeys(preview));
  // console.log("program: " + allKeys(program));
// }

// setProgramAndPreviewScenes(PreviewScenes, ProgramScenes);

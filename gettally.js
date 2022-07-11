var obs = undefined;

var test_mode = 0;

if (test_mode) {
  // let socket = new WebSocket("wss://javascript.info/article/websocket/demo/hello");
  // let socket = new WebSocket("ws://javascript.info/article/websocket/demo/hello");
  let socket = new WebSocket("ws://localhost:8081/ws");

  socket.onopen = function(e) {
    console.log("[open] Connection established");
    console.log("Sending to server");
    socket.send("My name is John");
  };

  socket.onmessage = function(event) {
    console.log(`[message] Data received from server: ${event.data}`);
  };

  socket.onclose = function(event) {
    if (event.wasClean) {
      console.log(`[close] Connection closed cleanly, code=${event.code} reason=${event.reason}`);
    } else {
      // e.g. server process killed or network down
      // event.code is usually 1006 in this case
      console.log('[close] Connection died');
    }
  };

  socket.onerror = function(error) {
    console.log(`[error] ${error.message}`);
  };
}


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
    logMessage("OBS connected: " + allKeys(value));
    logMessage("WebSocket version: " + value.obsWebSocketVersion);
    logMessage("RPC version: " + value.negotiatedRpcVersion);
  }).catch((error) => {
    // console.error("OBS connection failed: "+error);
    logMessage("OBS connection failed: "+allKeys(error) + " in " + 
        error.fileName + ":" + error.lineNumber + ":" + error.message  + error.stack);
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

if (!test_mode) {
  connectOBS();
}




// Uncomment to debug in Chrome.

// ProgramScenes = [ "one", "two", "three"];
// PreviewScenes = [ "four", "five", "six"];
// setProgramAndPreviewScenes(PreviewScenes, ProgramScenes);

// setProgramAndPreviewScenes([ obsPassword ], [ "B" ] );

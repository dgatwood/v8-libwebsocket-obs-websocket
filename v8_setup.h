
#ifdef __cplusplus
extern "C" {
#endif

void setOBSURL(char *URL);
void setOBSPassword(char *password);
void *v8_setup(void);  // Returns isolate cast to void pointer.
void runScript(char *scriptString);
bool runScriptAsModule(char *moduleName, char *scriptString);
void v8_runLoopCallback(void *isolate);
void v8_teardown(void);

#ifdef __cplusplus
};
#endif

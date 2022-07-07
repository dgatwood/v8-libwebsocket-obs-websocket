
#ifdef __cplusplus
extern "C" {
#endif

void v8_setup(void);
void runScript(char *scriptString);
bool runScriptAsModule(char *moduleName, char *scriptString);
void v8_teardown(void);

#ifdef __cplusplus
};
#endif

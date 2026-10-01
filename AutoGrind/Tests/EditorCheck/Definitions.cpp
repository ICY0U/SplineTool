// The engine globals the editor module refers to, and an entry point, so the check links.
#include "UEStubAll.h"

UEngine* GEngine = nullptr;
UEditorEngine* GEditor = nullptr;
bool GAllowActorScriptExecutionInEditor = false;

FEditorModeTools& GLevelEditorModeTools()
{
	static FEditorModeTools Tools;
	return Tools;
}

int main()
{
	return 0;
}

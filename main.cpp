
#include <string>
#include <memory>
namespace Fae {
class ScriptContext {
public:

	ScriptContext();
	virtual ~ScriptContext();

	virtual void FunctionCall(std::string);
	virtual void LoadScriptFile(std::string f, std::string i);
};
}

int main(int argc, char**argv) {
	auto script = std::make_unique<Fae::ScriptContext>();
	script->LoadScriptFile("test", "init");
	//script->FunctionCall("init");
	return 0;
}


#pragma once
#include <string>
#include <memory>
#include <unordered_map>

namespace Fae {

struct ModuleContext;

class ScriptContext {
public:

	ScriptContext();
	virtual ~ScriptContext();

	virtual void FunctionCall(std::string);
	virtual void LoadScriptFile(std::string f, std::string i);
private:
	std::unordered_map<std::string, std::shared_ptr<ModuleContext>> script_map;
};
}


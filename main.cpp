
#include "script.hpp"

using namespace std::string_literals;
using namespace std::string_view_literals;
int main(int argc, char**argv) {
	auto script = std::make_unique<Fae::ScriptContext>();
	script->LoadScriptFile("test"s, "init"s);
	script->FunctionCall("init"s);
	return 0;
}


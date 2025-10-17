#include "Lorebox.h"

bool Lorebox::Load_KW_AoT() {
	aot_kw = RE::BGSKeyword::CreateKeyword(aot_kw_name);
	return aot_kw != nullptr;
}
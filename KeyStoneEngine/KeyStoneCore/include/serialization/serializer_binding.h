#pragma once

#pragma once

/*
	TODO: Convert from Ks_Json to Ks_Serialization_Node. In this way every node will be
	a json node, xml node, yaml node ecc ecc... depending on the serializer used. 
	We can move from ks_serializer_create() to ks_serializer_create(ks_str serialization_kind)
	where serialization_kind can be "json", "xml", "yaml" and more, in this way we are not limited to json
	and later when needed, following engine modularity, we can define new types of serialization maybe with
	ks_serialization_add_kind(ks_str serialization_kind_name, fn_ptr create_serializer, fn_ptr destroy_serializer, 
	fn_ptr create_node, fn_ptr destroy_node, fn_ptr append_node) creating a modular serialization pipeline
*/

#include "../script/script_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

KS_API ks_no_ret ks_serializer_lua_bind(Ks_Script_Ctx ctx);

#ifdef __cplusplus
}
#endif
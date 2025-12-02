#include "../../include/core/types_binding.h"

KS_API ks_no_ret ks_types_lua_bind(Ks_Script_Ctx ctx) {
    ks_script_begin_scope(ctx); {
        Ks_Script_Table types_tbl = ks_script_create_named_table(ctx, "type");
        ks_script_promote(ctx, types_tbl);
        ks_script_table_set(ctx, types_tbl, ks_script_create_cstring(ctx, "BOOL"), ks_script_create_number(ctx, KS_TYPE_BOOL));
        ks_script_table_set(ctx, types_tbl, ks_script_create_cstring(ctx, "CHAR"), ks_script_create_number(ctx, KS_TYPE_CHAR));
        ks_script_table_set(ctx, types_tbl, ks_script_create_cstring(ctx, "INT"), ks_script_create_number(ctx, KS_TYPE_INT));
        ks_script_table_set(ctx, types_tbl, ks_script_create_cstring(ctx, "UINT"), ks_script_create_number(ctx, KS_TYPE_UINT));
        ks_script_table_set(ctx, types_tbl, ks_script_create_cstring(ctx, "FLOAT"), ks_script_create_number(ctx, KS_TYPE_FLOAT));
        ks_script_table_set(ctx, types_tbl, ks_script_create_cstring(ctx, "CSTRING"), ks_script_create_number(ctx, KS_TYPE_CSTRING));
        ks_script_table_set(ctx, types_tbl, ks_script_create_cstring(ctx, "LSTRING"), ks_script_create_number(ctx, KS_TYPE_LSTRING));
        ks_script_table_set(ctx, types_tbl, ks_script_create_cstring(ctx, "PTR"), ks_script_create_number(ctx, KS_TYPE_PTR));
        ks_script_table_set(ctx, types_tbl, ks_script_create_cstring(ctx, "USERDATA"), ks_script_create_number(ctx, KS_TYPE_USERDATA));
        ks_script_table_set(ctx, types_tbl, ks_script_create_cstring(ctx, "TABLE"), ks_script_create_number(ctx, KS_TYPE_SCRIPT_TABLE));
    }ks_script_end_scope(ctx);
}
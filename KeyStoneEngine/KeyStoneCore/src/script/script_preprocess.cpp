#include "../../include/script/script_preprocess.h"
#include "../../include/memory/memory.h"
#include "../../include/script/script_engine.h"
#include <tao/pegtl.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <stdarg.h>
#include <sstream>

namespace peg = tao::pegtl;

struct Ks_StringBuilder_T { std::string data; };
KS_API ks_no_ret ks_sb_append(Ks_StringBuilder sb, ks_str text) { if (sb && text) ((Ks_StringBuilder_T*)sb)->data += text; }
KS_API ks_no_ret ks_sb_appendf(Ks_StringBuilder sb, ks_str fmt, ...) {
    if (!sb || !fmt) return;
    va_list args; va_start(args, fmt);
    int size = vsnprintf(nullptr, 0, fmt, args); va_end(args);
    if (size <= 0) return;
    char* buf = (char*)alloca(size + 1);
    va_start(args, fmt); vsnprintf(buf, size + 1, fmt, args); va_end(args);
    ((Ks_StringBuilder_T*)sb)->data += buf;
}

struct RegisteredRule { ks_preproc_transform_fn on_def, on_set, on_get, on_call; };
struct ArgPair { std::string key; std::string value; };
struct SymbolInfo { std::string name, decorator_name; std::vector<ArgPair> dec_args; };

struct Ks_Preprocessor_T {
    ks_ptr lua_ctx = nullptr;
    std::unordered_map<std::string, RegisteredRule> registry;
    std::map<std::string, SymbolInfo> tracked_symbols;
};

static std::string trim(std::string s) {
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    s.erase(s.find_last_not_of(" \t\n\r") + 1);
    return s;
}

static void fill_ctx(Ks_Preproc_Ctx& ctx, const std::vector<ArgPair>& args, ks_str** vals, ks_str** keys, ks_size* count) {
    if (args.empty()) return;
    *count = args.size();
    ks_str* v = (ks_str*)ks_alloc(sizeof(ks_str) * args.size(), KS_LT_FRAME, KS_TAG_INTERNAL_DATA);
    ks_str* k = (ks_str*)ks_alloc(sizeof(ks_str) * args.size(), KS_LT_FRAME, KS_TAG_INTERNAL_DATA);
    for (size_t i = 0; i < args.size(); ++i) {
        v[i] = (ks_str)args[i].value.c_str();
        k[i] = args[i].key.empty() ? nullptr : (ks_str)args[i].key.c_str();
    }
    *vals = v; *keys = k;
}

namespace ks_grammar {
    struct opt_sep : peg::star<peg::space> {};
    struct sep : peg::plus<peg::space> {};
    struct lua_id : peg::identifier {};
    struct comment_single : peg::seq<peg::string<'-', '-'>, peg::until<peg::eolf>> {};
    struct comment : comment_single {};

    struct str_double : peg::seq<peg::one<'"'>, peg::until<peg::one<'"'>>> {};
    struct str_single : peg::seq<peg::one<'\''>, peg::until<peg::one<'\''>>> {};
    struct str_literal : peg::sor<str_double, str_single> {};
    struct string_literal_usage : str_literal {};

    struct open_b : peg::one<'{'> {};
    struct close_b : peg::one<'}'> {};
    struct non_b : peg::not_one<'{', '}'> {};
    struct balanced_block;
    struct block_content : peg::star<peg::sor<balanced_block, non_b>> {};
    struct balanced_block : peg::seq<open_b, block_content, close_b> {};

    struct open_p : peg::one<'('> {};
    struct close_p : peg::one<')'> {};
    struct non_p : peg::not_one<'(', ')'> {};
    struct balanced_parens;
    struct parens_content : peg::star<peg::sor<balanced_parens, non_p>> {};
    struct balanced_parens : peg::seq<open_p, parens_content, close_p> {};

    struct raw_table_def : peg::seq<lua_id, opt_sep, balanced_block> {};

    struct func_params : balanced_parens {};

    struct local_def : peg::seq<peg::string<'l', 'o', 'c', 'a', 'l'>, sep, lua_id, opt_sep, peg::opt<peg::one<'='>, peg::until<peg::at<peg::sor<peg::eolf, peg::one<'\n'>>>>>> {};
    struct function_def : peg::seq<peg::string<'f', 'u', 'n', 'c', 't', 'i', 'o', 'n'>, sep, lua_id, opt_sep, func_params, peg::until<peg::string<'e', 'n', 'd'>>> {};

    struct dot_access : peg::seq<peg::one<'.'>, opt_sep, lua_id> {};
    struct colon_access : peg::seq<peg::one<':'>, opt_sep, lua_id> {};
    struct bracket_content : peg::plus<peg::sor<peg::alnum, peg::one<'_', '"', '\'', ' ', '\t', '.', '+', '-', '*', '/'>>> {};
    struct bracket_access : peg::seq<peg::one<'['>, opt_sep, bracket_content, opt_sep, peg::one<']'>> {};
    struct complex_ident_lvalue : peg::seq<lua_id, opt_sep, peg::star<peg::sor<dot_access, bracket_access>>> {};
    struct end_of_expr : peg::sor<peg::eolf, peg::one<'\n'>, peg::string<'t', 'h', 'e', 'n'>, peg::string<'e', 'n', 'd'>> {};
    struct assignment : peg::seq<complex_ident_lvalue, opt_sep, peg::one<'='>, peg::not_at<peg::one<'='>>, peg::until<peg::at<end_of_expr>>> {};

    struct complex_ident_usage : peg::seq<lua_id, opt_sep, peg::star<peg::sor<dot_access, bracket_access, colon_access>>> {};
    struct usage_id : complex_ident_usage {};

    struct macro_tag : peg::seq<peg::one<'@'>, lua_id, peg::opt<opt_sep, func_params>> {};

    struct grammar : peg::star<peg::sor<
        comment,
        macro_tag,
        local_def,
        function_def,
        raw_table_def,
        assignment,
        usage_id,
        string_literal_usage,

        peg::any
        >> {};
}

static void parse_lvalue(const std::string& raw, std::string& sym_name, Ks_AccessType& type, std::string& key) {
    size_t dot = raw.find('.'); size_t bracket = raw.find('[');
    size_t pos = std::string::npos;
    if (dot != std::string::npos) pos = dot;
    if (bracket != std::string::npos && (pos == std::string::npos || bracket < pos)) pos = bracket;
    if (pos == std::string::npos) { sym_name = trim(raw); type = KS_ACCESS_DIRECT; key = ""; return; }
    sym_name = trim(raw.substr(0, pos));
    if (raw[pos] == '.') { type = KS_ACCESS_DOT; key = trim(raw.substr(pos + 1)); }
    else { type = KS_ACCESS_BRACKET; size_t close = raw.find(']', pos); key = trim(raw.substr(pos + 1, close - pos - 1)); }
}
static void parse_access(const std::string& raw, std::string& sym_name, Ks_AccessType& type, std::string& key) {
    size_t dot = raw.find('.'); size_t bracket = raw.find('['); size_t colon = raw.find(':');
    size_t pos = std::string::npos;
    if (dot != std::string::npos) pos = dot;
    if (bracket != std::string::npos && (pos == std::string::npos || bracket < pos)) pos = bracket;
    if (colon != std::string::npos && (pos == std::string::npos || colon < pos)) pos = colon;
    if (pos == std::string::npos) { sym_name = trim(raw); type = KS_ACCESS_DIRECT; key = ""; return; }
    sym_name = trim(raw.substr(0, pos));
    if (raw[pos] == '.') { type = KS_ACCESS_DOT; key = trim(raw.substr(pos + 1)); }
    else if (raw[pos] == ':') { type = KS_ACCESS_COLON; key = trim(raw.substr(pos + 1)); }
    else { type = KS_ACCESS_BRACKET; size_t close = raw.find(']', pos); key = trim(raw.substr(pos + 1, close - pos - 1)); }
}

struct ParserState {
    Ks_Preprocessor_T* pp;
    Ks_StringBuilder_T output;
    struct { std::string name; std::vector<ArgPair> args; } pending;
    void flush_pending() { pending.name.clear(); pending.args.clear(); }
};

template<typename Rule> struct action : peg::nothing<Rule> {};

template<> struct action<ks_grammar::macro_tag> {
    template<typename ActionInput> static void apply(const ActionInput& in, ParserState& state) {
        std::string raw = in.string();
        state.flush_pending();

        size_t paren = raw.find('(');
        std::string name = (paren == std::string::npos) ? trim(raw.substr(1)) : trim(raw.substr(1, paren - 1));

        std::vector<ArgPair> args_parsed;
        if (paren != std::string::npos) {
            std::string args_content = raw.substr(paren + 1, raw.size() - paren - 2);
            std::stringstream ss(args_content);
            std::string item;
            while (std::getline(ss, item, ',')) {
                size_t colon = item.find(':');
                if (colon != std::string::npos) {
                    args_parsed.push_back({ trim(item.substr(0, colon)), trim(item.substr(colon + 1)) });
                }
                else {
                    args_parsed.push_back({ "", trim(item) });
                }
            }
        }

        auto it = state.pp->registry.find(name);
        if (it != state.pp->registry.end()) {
            auto& rule = it->second;

            if (rule.on_call) {
                Ks_Preproc_Ctx ctx = {};
                ctx.is_transformator_in_lua = state.pp->lua_ctx ? true : false;
                ctx.lua_ctx = state.pp->lua_ctx;
                ctx.symbol_name = (ks_str)name.c_str();
                ctx.decorator_name = (ks_str)name.c_str();
                ctx.access_type = KS_ACCESS_DIRECT;

                fill_ctx(ctx, args_parsed, &ctx.decorator_args, &ctx.decorator_arg_keys, &ctx.decorator_args_count);

                Ks_StringBuilder_T sb;
                if (rule.on_call(&ctx, (Ks_StringBuilder*)&sb)) {
                    state.output.data += sb.data;
                    return;
                }
            }

            state.pending.name = name;
            state.pending.args = args_parsed;
            return;
        }

        state.output.data += raw;
    }
};

template<> struct action<ks_grammar::raw_table_def> {
    template<typename ActionInput> static void apply(const ActionInput& in, ParserState& state) {
        std::string raw = in.string();
        size_t brace_open = raw.find('{');
        std::string name = trim(raw.substr(0, brace_open));
        std::string body = raw.substr(brace_open);

        if (!state.pending.name.empty()) {
            auto& rule = state.pp->registry[state.pending.name];
            if (rule.on_def) {
                std::string inner_content = "";
                if (body.length() >= 2) inner_content = body.substr(1, body.length() - 2);

                Ks_Preproc_Ctx ctx = {};
                ctx.is_transformator_in_lua = state.pp->lua_ctx ? true : false;
                ctx.lua_ctx = state.pp->lua_ctx;
                ctx.symbol_name = (ks_str)name.c_str();
                ctx.decorator_name = (ks_str)state.pending.name.c_str();
                ctx.is_table_def = true;
                ks_str field_cstr = (ks_str)inner_content.c_str();
                ctx.table_fields = &field_cstr;
                ctx.table_fields_count = 1;

                fill_ctx(ctx, state.pending.args, &ctx.decorator_args, &ctx.decorator_arg_keys, &ctx.decorator_args_count);
                Ks_StringBuilder_T sb;
                if (rule.on_def(&ctx, (Ks_StringBuilder*)&sb)) {
                    state.output.data += sb.data;
                    state.flush_pending(); return;
                }
            }
        }
        state.output.data += raw; state.flush_pending();
    }
};

template<> struct action<ks_grammar::string_literal_usage> {
    template<typename ActionInput> static void apply(const ActionInput& in, ParserState& state) {
        std::string raw = in.string();
        if (!state.pending.name.empty()) {
            auto it_rule = state.pp->registry.find(state.pending.name);
            if (it_rule != state.pp->registry.end()) {
                auto& rule = it_rule->second;
                if (rule.on_get) {
                    std::string content = "";
                    if (raw.size() >= 2) content = raw.substr(1, raw.size() - 2);
                    Ks_Preproc_Ctx ctx = {};
                    ctx.is_transformator_in_lua = state.pp->lua_ctx ? true : false;
                    ctx.lua_ctx = state.pp->lua_ctx;
                    ctx.symbol_name = (ks_str)content.c_str();
                    ctx.decorator_name = (ks_str)state.pending.name.c_str();
                    ctx.access_type = KS_ACCESS_DIRECT;
                    fill_ctx(ctx, state.pending.args, &ctx.decorator_args, &ctx.decorator_arg_keys, &ctx.decorator_args_count);
                    Ks_StringBuilder_T sb;
                    if (rule.on_get(&ctx, (Ks_StringBuilder*)&sb)) {
                        state.output.data += sb.data;
                        state.flush_pending();
                        return;
                    }
                }
            }
            state.flush_pending();
        }
        state.output.data += raw;
    }
};

template<> struct action<ks_grammar::usage_id> {
    template<typename ActionInput> static void apply(const ActionInput& in, ParserState& state) {
        std::string raw = in.string();
        std::string sym_name, member_key; Ks_AccessType type;
        parse_access(raw, sym_name, type, member_key);

        if (!state.pending.name.empty()) {
            auto it_rule = state.pp->registry.find(state.pending.name);
            if (it_rule != state.pp->registry.end()) {
                auto& rule = it_rule->second;
                if (rule.on_get) {
                    Ks_Preproc_Ctx ctx = {};
                    ctx.is_transformator_in_lua = state.pp->lua_ctx ? true : false;
                    ctx.lua_ctx = state.pp->lua_ctx;
                    ctx.symbol_name = (ks_str)sym_name.c_str();
                    ctx.decorator_name = (ks_str)state.pending.name.c_str();
                    ctx.access_type = type;
                    fill_ctx(ctx, state.pending.args, &ctx.decorator_args, &ctx.decorator_arg_keys, &ctx.decorator_args_count);
                    Ks_StringBuilder_T sb;
                    if (rule.on_get(&ctx, (Ks_StringBuilder*)&sb)) {
                        state.output.data += sb.data;
                        state.flush_pending(); return;
                    }
                }
            }
            state.flush_pending();
        }

        auto it = state.pp->tracked_symbols.find(sym_name);
        if (it != state.pp->tracked_symbols.end()) {
            auto& rule = state.pp->registry[it->second.decorator_name];
            if (type == KS_ACCESS_COLON && rule.on_call) {
                Ks_Preproc_Ctx ctx = {}; ctx.is_transformator_in_lua = state.pp->lua_ctx ? true : false;
                ctx.lua_ctx = state.pp->lua_ctx; ctx.symbol_name = (ks_str)sym_name.c_str();
                ctx.access_type = type; ctx.member_key = (ks_str)member_key.c_str();
                if (rule.on_call(&ctx, (Ks_StringBuilder*)&state.output)) return;
            }
            else if (rule.on_get) {
                Ks_Preproc_Ctx ctx = {}; ctx.is_transformator_in_lua = state.pp->lua_ctx ? true : false;
                ctx.lua_ctx = state.pp->lua_ctx; ctx.symbol_name = (ks_str)sym_name.c_str();
                ctx.access_type = type; ctx.member_key = (type == KS_ACCESS_DIRECT) ? nullptr : (ks_str)member_key.c_str();
                if (rule.on_get(&ctx, (Ks_StringBuilder*)&state.output)) return;
            }
        }

        auto it_rule = state.pp->registry.find(sym_name);
        if (it_rule != state.pp->registry.end()) {
            auto& rule = it_rule->second;
            if (rule.on_get) {
                Ks_Preproc_Ctx ctx = {}; ctx.is_transformator_in_lua = state.pp->lua_ctx ? true : false;
                ctx.lua_ctx = state.pp->lua_ctx; ctx.symbol_name = (ks_str)sym_name.c_str();
                ctx.access_type = type;
                if (rule.on_get(&ctx, (Ks_StringBuilder*)&state.output)) return;
            }
        }
        state.output.data += raw;
    }
};

template<> struct action<ks_grammar::comment> {
    template<typename ActionInput> static void apply(const ActionInput& in, ParserState& state) {
        state.output.data += in.string();
        state.flush_pending();
    }
};

template<> struct action<ks_grammar::local_def> {
    template<typename ActionInput> static void apply(const ActionInput& in, ParserState& state) {
        std::string raw = in.string();
        size_t eq_p = raw.find('=');
        std::string name = trim((eq_p == std::string::npos) ? raw.substr(6) : raw.substr(6, eq_p - 6));
        if (!state.pending.name.empty()) {
            state.pp->tracked_symbols[name] = { name, state.pending.name, state.pending.args };
            auto it = state.pp->registry.find(state.pending.name);
            if (it != state.pp->registry.end() && it->second.on_def) {
                Ks_Preproc_Ctx ctx = {}; ctx.is_transformator_in_lua = state.pp->lua_ctx ? true : false;
                ctx.lua_ctx = state.pp->lua_ctx; ctx.symbol_name = (ks_str)name.c_str();
                ctx.decorator_name = (ks_str)state.pending.name.c_str(); ctx.is_local_def = true;
                std::string val_raw = trim((eq_p == std::string::npos) ? "" : raw.substr(eq_p + 1));
                ks_str processed_val = ks_preprocessor_process((Ks_Preprocessor)state.pp, val_raw.c_str());
                ctx.assignment_value = processed_val;
                fill_ctx(ctx, state.pending.args, &ctx.decorator_args, &ctx.decorator_arg_keys, &ctx.decorator_args_count);
                Ks_StringBuilder_T sb;
                if (it->second.on_def(&ctx, (Ks_StringBuilder*)&sb)) {
                    state.output.data += "local " + sb.data;
                    ks_dealloc((void*)processed_val); state.flush_pending(); return;
                }
                ks_dealloc((void*)processed_val);
            }
        }
        else if (eq_p != std::string::npos) {
            std::string lhs = raw.substr(0, eq_p + 1);
            std::string rhs = raw.substr(eq_p + 1);
            ks_str processed_rhs = ks_preprocessor_process((Ks_Preprocessor)state.pp, rhs.c_str());
            state.output.data += lhs + processed_rhs;
            ks_dealloc((void*)processed_rhs);
            state.flush_pending(); return;
        }
        state.output.data += raw; state.flush_pending();
    }
};

template<> struct action<ks_grammar::function_def> {
    template<typename ActionInput> static void apply(const ActionInput& in, ParserState& state) {
        std::string raw = in.string();
        size_t f_p = raw.find("function") + 8;
        size_t open_p = raw.find('('); size_t close_p = raw.find(')');
        std::string name = trim(raw.substr(f_p, open_p - f_p));
        std::string args_raw = raw.substr(open_p + 1, close_p - open_p - 1);
        std::string body = raw.substr(close_p + 1);
        if (body.size() >= 3 && body.substr(body.size() - 3) == "end") body.erase(body.size() - 3);

        if (!state.pending.name.empty()) {
            state.pp->tracked_symbols[name] = { name, state.pending.name, state.pending.args };
            auto it = state.pp->registry.find(state.pending.name);
            if (it != state.pp->registry.end() && it->second.on_def) {
                Ks_Preproc_Ctx ctx = {}; ctx.is_transformator_in_lua = state.pp->lua_ctx ? true : false;
                ctx.lua_ctx = state.pp->lua_ctx; ctx.symbol_name = (ks_str)name.c_str();
                ctx.decorator_name = (ks_str)state.pending.name.c_str(); ctx.is_func_def = true;
                std::vector<std::string> f_args_vec;
                std::stringstream ss(args_raw); std::string item;
                while (std::getline(ss, item, ',')) f_args_vec.push_back(trim(item));
                std::vector<const char*> c_f_args; for (auto& s : f_args_vec) c_f_args.push_back(s.c_str());
                ctx.function_args = (ks_str*)c_f_args.data(); ctx.function_args_count = (ks_size)c_f_args.size();
                ks_str processed_body = ks_preprocessor_process((Ks_Preprocessor)state.pp, body.c_str());
                ctx.function_body = processed_body;
                fill_ctx(ctx, state.pending.args, &ctx.decorator_args, &ctx.decorator_arg_keys, &ctx.decorator_args_count);
                Ks_StringBuilder_T sb;
                if (it->second.on_def(&ctx, (Ks_StringBuilder*)&sb)) {
                    state.output.data += sb.data;
                    ks_dealloc((void*)processed_body); state.flush_pending(); return;
                }
                ks_dealloc((void*)processed_body);
            }
        }
        state.output.data += raw; state.flush_pending();
    }
};

template<> struct action<ks_grammar::assignment> {
    template<typename ActionInput> static void apply(const ActionInput& in, ParserState& state) {
        std::string raw = in.string(); size_t eq = raw.find('=');
        std::string lhs = trim(raw.substr(0, eq));
        std::string sym_name, member_key; Ks_AccessType type;
        parse_lvalue(lhs, sym_name, type, member_key);

        std::string val_raw = trim(raw.substr(eq + 1));
        ks_str processed_val = ks_preprocessor_process((Ks_Preprocessor)state.pp, val_raw.c_str());

        auto it = state.pp->tracked_symbols.find(sym_name);
        if (it != state.pp->tracked_symbols.end()) {
            auto& rule = state.pp->registry[it->second.decorator_name];
            if (rule.on_set) {
                Ks_Preproc_Ctx ctx = {};
                ctx.is_transformator_in_lua = state.pp->lua_ctx ? true : false;
                ctx.lua_ctx = state.pp->lua_ctx;
                ctx.symbol_name = (ks_str)sym_name.c_str();
                ctx.access_type = type; ctx.member_key = (type == KS_ACCESS_DIRECT) ? nullptr : (ks_str)member_key.c_str();
                ctx.assignment_value = processed_val;
                if (rule.on_set(&ctx, (Ks_StringBuilder*)&state.output)) {
                    ks_dealloc((void*)processed_val); return;
                }
            }
        }
        state.output.data += raw.substr(0, eq + 1) + processed_val;
        ks_dealloc((void*)processed_val);
    }
};

template<> struct action<peg::any> {
    template<typename ActionInput> static void apply(const ActionInput& in, ParserState& state) { state.output.data += in.string(); }
};

KS_API Ks_Preprocessor ks_preprocessor_create(ks_ptr lua_ctx) {
    void* mem = ks_alloc(sizeof(Ks_Preprocessor_T), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA);
    Ks_Preprocessor_T* pp = new(mem) Ks_Preprocessor_T();
    pp->lua_ctx = lua_ctx;
    return (Ks_Preprocessor)pp;
}
KS_API ks_no_ret ks_preprocessor_destroy(Ks_Preprocessor pp) {
    auto* p = (Ks_Preprocessor_T*)pp; p->~Ks_Preprocessor_T(); ks_dealloc(p);
}
KS_API ks_no_ret ks_preprocessor_register(Ks_Preprocessor pp, ks_str name, ks_preproc_transform_fn on_def, ks_preproc_transform_fn on_set, ks_preproc_transform_fn on_get, ks_preproc_transform_fn on_call) {
    ((Ks_Preprocessor_T*)pp)->registry[name] = { on_def, on_set, on_get, on_call };
}

KS_API ks_str ks_preprocessor_process(Ks_Preprocessor pp, ks_str source_code) {
    ParserState state; state.pp = (Ks_Preprocessor_T*)pp;
    peg::memory_input in(source_code, "preprocessor");
    peg::parse<ks_grammar::grammar, action>(in, state);
    size_t len = state.output.data.length();
    char* res = (char*)ks_alloc(len + 1, KS_LT_USER_MANAGED, KS_TAG_SCRIPT);
    memcpy(res, state.output.data.c_str(), len + 1); return res;
}
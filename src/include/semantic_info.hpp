#ifndef JANE_SEMANTIC_INFO
#define JANE_SEMANTIC_INFO

#include "codegen.hpp"
#include "hash_map.hpp"
#include "jane_llvm.hpp"
#include "parser.hpp"

struct FnTableEntry {
  LLVMValueRef fn_value;
  AstNode *proto_node;
  AstNode *fn_def_node;
  bool is_extern;
  bool internal_linkage;
  unsigned calling_convention;
};

enum TypeId {
  TypeIdUserDefined,
  TypeIdPointer,
  TypeIdU8,
  TypeIdI32,
  TypeIdVoid,
  TypeIdUnreachable,
};

struct TypeTableEntry {
  TypeId id;
  LLVMTypeRef type_ref;
  LLVMJaneDIType *di_type;

  TypeTableEntry *pointer_child;
  bool pointer_is_const;
  int user_defined_id;
  Buf name;
  TypeTableEntry *pointer_const_parent;
  TypeTableEntry *pointer_mut_parent;
};

struct CodeGen {
  LLVMModuleRef module;
  AstNode *root;
  JaneList<ErrorMsg> errors;
  LLVMBuilderRef builder;
  LLVMJaneDIBuilder *dbuilder;
  LLVMJaneDICompileUnit *compile_unit;
  HashMap<Buf *, FnTableEntry *, buf_hash, buf_eql_buf> fn_table;
  HashMap<Buf *, LLVMValueRef, buf_hash, buf_eql_buf> str_table;
  HashMap<Buf *, TypeTableEntry *, buf_hash, buf_eql_buf> type_table;
  HashMap<Buf *, bool, buf_hash, buf_eql_buf> link_table;
  TypeTableEntry *invalid_type_entry;
  LLVMTargetDataRef target_data_ref;
  unsigned pointer_size_bytes;
  bool is_static;
  bool strip_debug_symbols;
  CodeGenBuildType build_type;
  LLVMTargetMachineRef target_machine;
  bool is_native_target;
  Buf in_file;
  Buf in_dir;
  JaneList<LLVMJaneDIScope *> block_scopes;
  LLVMJaneDIFile *di_file;
  JaneList<FnTableEntry *> fn_defs;
  Buf *out_name;
  OutType out_type;
  FnTableEntry *cur_fn;
  bool c_stdint_used;
  AstNode *root_export_decl;
  int version_major;
  int version_minor;
  int version_patch;
};

struct TypeNode {
  TypeTableEntry *entry;
};

struct FnDefNode {
  bool add_implicit_return;
  bool skip;
  LLVMValueRef *params;
};

struct CodeGenNode {
  union {
    TypeNode type_node;
    FnDefNode fn_def_node;
  } data;
};

static inline Buf *hack_get_fn_call_name(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeSymbol);
  return &node->data.symbol;
}

#endif // JANE_SEMANTIC_INFO
// Copyright (C) 2020-2022 Free Software Foundation, Inc.

// This file is part of GCC.

// GCC is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3, or (at your option) any later
// version.

// GCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#ifndef RUST_NAME_RESOLVER_H
#define RUST_NAME_RESOLVER_H

#include "rust-system.h"
#include "rust-canonical-path.h"
#include "rust-hir-map.h"
#include "rust-hir-type-check.h"

namespace Rust {
namespace Resolver {

class Rib
{
public:
  // Rust uses local_def_ids assigned by def_collector on the AST
  // lets use NodeId instead
  Rib (CrateNum crateNum, NodeId node_id);

  // this takes the relative paths of items within a compilation unit for lookup
  void insert_name (
    const CanonicalPath &path, NodeId id, Location locus, bool shadow,
    std::function<void (const CanonicalPath &, NodeId, Location)> dup_cb);

  bool lookup_canonical_path (const NodeId &id, CanonicalPath *ident);
  bool lookup_name (const CanonicalPath &ident, NodeId *id);
  void clear_name (const CanonicalPath &ident, NodeId id);
  void append_reference_for_def (NodeId def, NodeId ref);
  bool have_references_for_node (NodeId def) const;
  bool decl_was_declared_here (NodeId def) const;

  CrateNum get_crate_num () const { return crate_num; }
  NodeId get_node_id () const { return node_id; }
  std::map<NodeId, Location> &get_declarations () { return decls_within_rib; }

private:
  CrateNum crate_num;
  NodeId node_id;
  std::map<CanonicalPath, NodeId> path_mappings;
  std::map<NodeId, CanonicalPath> reverse_path_mappings;
  std::map<NodeId, Location> decls_within_rib;
  std::map<NodeId, std::set<NodeId>> references;
  Analysis::Mappings *mappings;
};

class Scope
{
public:
  Scope (CrateNum crate_num);

  void
  insert (const CanonicalPath &ident, NodeId id, Location locus, bool shadow,
	  std::function<void (const CanonicalPath &, NodeId, Location)> dup_cb);

  void insert (const CanonicalPath &ident, NodeId id, Location locus);
  bool lookup (const CanonicalPath &ident, NodeId *id);

  void iterate (std::function<bool (Rib *)> cb);

  Rib *peek ();
  void push (NodeId id);
  Rib *pop ();

  void append_reference_for_def (NodeId refId, NodeId defId);

  CrateNum get_crate_num () const { return crate_num; }

private:
  CrateNum crate_num;
  std::vector<Rib *> stack;
};

// This can map simple NodeIds for names to their parent node
// for example:
//
// var x = y + 1;
//
// say y has node id=1 and the plus_expression has id=2
// then the Definition will have
// Definition { node=1, parent=2 }
// this will be used later to gather the ribs for the type inferences context
//
// if parent is UNKNOWN_NODEID then this is a root declaration
// say the var_decl hasa node_id=4;
// the parent could be a BLOCK_Expr node_id but lets make it UNKNOWN_NODE_ID
// so we know when it terminates
struct Definition
{
  NodeId node;
  NodeId parent;
  // add kind ?

  bool is_equal (const Definition &other)
  {
    return node == other.node && parent == other.parent;
  }
};

class Resolver
{
public:
  static Resolver *get ();
  ~Resolver () {}

  // these builtin types
  void insert_builtin_types (Rib *r);

  // these will be required for type resolution passes to
  // map back to tyty nodes
  std::vector<AST::Type *> &get_builtin_types ();

  void push_new_name_rib (Rib *r);
  void push_new_type_rib (Rib *r);
  void push_new_label_rib (Rib *r);
  void push_new_macro_rib (Rib *r);

  bool find_name_rib (NodeId id, Rib **rib);
  bool find_type_rib (NodeId id, Rib **rib);
  bool find_label_rib (NodeId id, Rib **rib);
  bool find_macro_rib (NodeId id, Rib **rib);

  void insert_new_definition (NodeId id, Definition def);
  bool lookup_definition (NodeId id, Definition *def);

  void insert_resolved_name (NodeId refId, NodeId defId);
  bool lookup_resolved_name (NodeId refId, NodeId *defId);

  void insert_resolved_type (NodeId refId, NodeId defId);
  bool lookup_resolved_type (NodeId refId, NodeId *defId);

  void insert_resolved_label (NodeId refId, NodeId defId);
  bool lookup_resolved_label (NodeId refId, NodeId *defId);

  void insert_resolved_macro (NodeId refId, NodeId defId);
  bool lookup_resolved_macro (NodeId refId, NodeId *defId);

  void mark_decl_mutability (NodeId id, bool mut);
  bool decl_is_mutable (NodeId id) const;
  void mark_assignment_to_decl (NodeId id, NodeId assignment);
  size_t get_num_assignments_to_decl (NodeId id) const;

  // proxy for scoping
  Scope &get_name_scope () { return name_scope; }
  Scope &get_type_scope () { return type_scope; }
  Scope &get_label_scope () { return label_scope; }
  Scope &get_macro_scope () { return macro_scope; }

  NodeId get_global_type_node_id () { return global_type_node_id; }
  void set_unit_type_node_id (NodeId id) { unit_ty_node_id = id; }
  NodeId get_unit_type_node_id () { return unit_ty_node_id; }

private:
  Resolver ();

  void generate_builtins ();

  Analysis::Mappings *mappings;
  TypeCheckContext *tyctx;

  std::vector<AST::Type *> builtins;

  Scope name_scope;
  Scope type_scope;
  Scope label_scope;
  Scope macro_scope;

  NodeId global_type_node_id;
  NodeId unit_ty_node_id;

  // map a AST Node to a Rib
  std::map<NodeId, Rib *> name_ribs;
  std::map<NodeId, Rib *> type_ribs;
  std::map<NodeId, Rib *> label_ribs;
  std::map<NodeId, Rib *> macro_ribs;

  // map any Node to its Definition
  // ie any name or type usage
  std::map<NodeId, Definition> name_definitions;

  // Rust uses DefIds to namespace these under a crate_num
  // but then it uses the def_collector to assign local_defids
  // to each ast node as well. not sure if this is going to fit
  // with gcc very well to compile a full crate in one go but we will
  // see.

  // these are of the form ref->Def-NodeId
  // we need two namespaces one for names and ones for types
  std::map<NodeId, NodeId> resolved_names;
  std::map<NodeId, NodeId> resolved_types;
  std::map<NodeId, NodeId> resolved_labels;
  std::map<NodeId, NodeId> resolved_macros;

  // map of resolved names mutability flag
  std::map<NodeId, bool> decl_mutability;
  // map of resolved names and set of assignments to the decl
  std::map<NodeId, std::set<NodeId>> assignment_to_decl;
};

} // namespace Resolver
} // namespace Rust

#endif // RUST_NAME_RESOLVER_H

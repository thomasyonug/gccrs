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

#ifndef RUST_HIR_MAP_H
#define RUST_HIR_MAP_H

#include "rust-optional.h"
#include "rust-system.h"
#include "rust-location.h"
#include "rust-mapping-common.h"
#include "rust-canonical-path.h"
#include "rust-ast-full-decls.h"
#include "rust-hir-full-decls.h"
#include "rust-lang-item.h"
#include "rust-privacy-common.h"

namespace Rust {
namespace Analysis {

class NodeMapping
{
public:
  NodeMapping (CrateNum crateNum, NodeId nodeId, HirId hirId,
	       LocalDefId localDefId)
    : crateNum (crateNum), nodeId (nodeId), hirId (hirId),
      localDefId (localDefId)
  {}

  static NodeMapping get_error ();

  CrateNum get_crate_num () const;
  NodeId get_nodeid () const;
  HirId get_hirid () const;
  LocalDefId get_local_defid () const;
  DefId get_defid () const;

  static DefId get_defid (CrateNum crate_num, LocalDefId local_defid);

  std::string as_string () const;

  bool is_equal (const NodeMapping &other) const
  {
    return get_crate_num () == other.get_crate_num ()
	   && get_nodeid () == other.get_nodeid ()
	   && get_hirid () == other.get_hirid ()
	   && get_local_defid () == other.get_local_defid ();
  }

private:
  CrateNum crateNum;
  NodeId nodeId;
  HirId hirId;
  LocalDefId localDefId;
};

class Mappings
{
public:
  static Mappings *get ();
  ~Mappings ();

  CrateNum get_next_crate_num ();
  void set_current_crate (CrateNum crateNum);
  CrateNum get_current_crate () const;
  CrateNum setup_crate_mappings (std::string crate_name);

  bool get_crate_name (CrateNum crate_num, std::string &name) const
  {
    auto it = crate_names.find (crate_num);
    if (it == crate_names.end ())
      return false;

    name.assign (it->second);
    return true;
  }

  // set crate name mid-compilation
  // don't use this if setting crate name before Session::parse_files
  bool set_crate_name (CrateNum crate_num, std::string name)
  {
    rust_assert (!name.empty ());
    auto it = crate_names.find (crate_num);
    if (it == crate_names.end ())
      return false;

    it->second.assign (name);
    return true;
  }

  std::string get_current_crate_name () const
  {
    std::string name;
    bool ok = get_crate_name (get_current_crate (), name);
    rust_assert (ok);
    return name;
  }

  NodeId get_next_node_id () { return get_next_node_id (get_current_crate ()); }
  NodeId get_next_node_id (CrateNum crateNum);

  HirId get_next_hir_id () { return get_next_hir_id (get_current_crate ()); }
  HirId get_next_hir_id (CrateNum crateNum);

  LocalDefId get_next_localdef_id (CrateNum crateNum);

  AST::Crate *get_ast_crate (CrateNum crateNum);
  void insert_ast_crate (AST::Crate *crate);

  HIR::Crate *get_hir_crate (CrateNum crateNum);
  void insert_hir_crate (HIR::Crate *crate);

  void insert_defid_mapping (DefId id, HIR::Item *item);
  HIR::Item *lookup_defid (DefId id);

  void insert_local_defid_mapping (CrateNum crateNum, LocalDefId id,
				   HIR::Item *item);
  HIR::Item *lookup_local_defid (CrateNum crateNum, LocalDefId id);

  void insert_hir_item (CrateNum crateNum, HirId id, HIR::Item *item);
  HIR::Item *lookup_hir_item (CrateNum crateNum, HirId id);

  void insert_hir_trait_item (CrateNum crateNum, HirId id,
			      HIR::TraitItem *item);
  HIR::TraitItem *lookup_hir_trait_item (CrateNum crateNum, HirId id);

  void insert_hir_extern_item (CrateNum crateNum, HirId id,
			       HIR::ExternalItem *item);
  HIR::ExternalItem *lookup_hir_extern_item (CrateNum crateNum, HirId id);

  void insert_hir_impl_block (CrateNum crateNum, HirId id,
			      HIR::ImplBlock *item);
  HIR::ImplBlock *lookup_hir_impl_block (CrateNum crateNum, HirId id);

  void insert_module (CrateNum crateNum, HirId id, HIR::Module *module);
  HIR::Module *lookup_module (CrateNum crateNum, HirId id);

  void insert_hir_implitem (CrateNum crateNum, HirId id, HirId parent_impl_id,
			    HIR::ImplItem *item);
  HIR::ImplItem *lookup_hir_implitem (CrateNum crateNum, HirId id,
				      HirId *parent_impl_id);

  void insert_hir_expr (CrateNum crateNum, HirId id, HIR::Expr *expr);
  HIR::Expr *lookup_hir_expr (CrateNum crateNum, HirId id);

  void insert_hir_path_expr_seg (CrateNum crateNum, HirId id,
				 HIR::PathExprSegment *expr);
  HIR::PathExprSegment *lookup_hir_path_expr_seg (CrateNum crateNum, HirId id);

  void insert_simple_path_segment (CrateNum crateNum, HirId id,
				   const AST::SimplePathSegment *path);
  const AST::SimplePathSegment *lookup_simple_path_segment (CrateNum crateNum,
							    HirId id);

  void insert_simple_path (CrateNum crateNum, HirId id,
			   const AST::SimplePath *path);
  const AST::SimplePath *lookup_simple_path (CrateNum crateNum, HirId id);

  void insert_hir_generic_param (CrateNum crateNum, HirId id,
				 HIR::GenericParam *expr);
  HIR::GenericParam *lookup_hir_generic_param (CrateNum crateNum, HirId id);

  void insert_hir_type (CrateNum crateNum, HirId id, HIR::Type *type);
  HIR::Type *lookup_hir_type (CrateNum crateNum, HirId id);

  void insert_hir_stmt (CrateNum crateNum, HirId id, HIR::Stmt *stmt);
  HIR::Stmt *lookup_hir_stmt (CrateNum crateNum, HirId id);

  void insert_hir_param (CrateNum crateNum, HirId id, HIR::FunctionParam *type);
  HIR::FunctionParam *lookup_hir_param (CrateNum crateNum, HirId id);

  void insert_hir_self_param (CrateNum crateNum, HirId id,
			      HIR::SelfParam *type);
  HIR::SelfParam *lookup_hir_self_param (CrateNum crateNum, HirId id);

  void insert_hir_struct_field (CrateNum crateNum, HirId id,
				HIR::StructExprField *type);
  HIR::StructExprField *lookup_hir_struct_field (CrateNum crateNum, HirId id);

  void insert_hir_pattern (CrateNum crateNum, HirId id, HIR::Pattern *pattern);
  HIR::Pattern *lookup_hir_pattern (CrateNum crateNum, HirId id);

  void walk_local_defids_for_crate (CrateNum crateNum,
				    std::function<bool (HIR::Item *)> cb);

  void insert_node_to_hir (CrateNum crate, NodeId id, HirId ref);
  bool lookup_node_to_hir (CrateNum crate, NodeId id, HirId *ref);
  bool lookup_hir_to_node (CrateNum crate, HirId id, NodeId *ref);

  void insert_location (CrateNum crate, HirId id, Location locus);
  Location lookup_location (CrateNum crate, HirId id);
  Location lookup_location (HirId id)
  {
    return lookup_location (get_current_crate (), id);
  }

  bool resolve_nodeid_to_stmt (CrateNum crate, NodeId id, HIR::Stmt **stmt);
  bool resolve_nodeid_to_stmt (NodeId id, HIR::Stmt **stmt)
  {
    return resolve_nodeid_to_stmt (get_current_crate (), id, stmt);
  }

  std::set<HirId> &get_hirids_within_crate (CrateNum crate)
  {
    return hirNodesWithinCrate[crate];
  }

  void insert_impl_item_mapping (HirId impl_item_id, HIR::ImplBlock *impl)
  {
    rust_assert (hirImplItemsToImplMappings.find (impl_item_id)
		 == hirImplItemsToImplMappings.end ());
    hirImplItemsToImplMappings[impl_item_id] = impl;
  }

  HIR::ImplBlock *lookup_associated_impl (HirId impl_item_id)
  {
    auto lookup = hirImplItemsToImplMappings.find (impl_item_id);
    rust_assert (lookup != hirImplItemsToImplMappings.end ());
    return lookup->second;
  }

  void iterate_impl_items (
    std::function<bool (HirId, HIR::ImplItem *, HIR::ImplBlock *)> cb);

  void iterate_impl_blocks (std::function<bool (HirId, HIR::ImplBlock *)> cb);

  void iterate_trait_items (
    std::function<bool (HIR::TraitItem *item, HIR::Trait *)> cb);

  bool is_impl_item (HirId id)
  {
    HirId parent_impl_block_id = UNKNOWN_HIRID;
    return lookup_hir_implitem (get_current_crate (), id, &parent_impl_block_id)
	   != nullptr;
  }

  void insert_trait_item_mapping (HirId trait_item_id, HIR::Trait *trait)
  {
    rust_assert (hirTraitItemsToTraitMappings.find (trait_item_id)
		 == hirTraitItemsToTraitMappings.end ());
    hirTraitItemsToTraitMappings[trait_item_id] = trait;
  }

  HIR::Trait *lookup_trait_item_mapping (HirId trait_item_id)
  {
    auto lookup = hirTraitItemsToTraitMappings.find (trait_item_id);
    rust_assert (lookup != hirTraitItemsToTraitMappings.end ());
    return lookup->second;
  }

  void insert_canonical_path (CrateNum crate, NodeId id,
			      const Resolver::CanonicalPath path)
  {
    const Resolver::CanonicalPath *p = nullptr;
    if (lookup_canonical_path (crate, id, &p))
      {
	// if we have already stored a canonical path this is ok so long as
	// this new path is equal or is smaller that the existing one but in
	// that case we ignore it.
	if (p->is_equal (path))
	  return;
	else
	  {
	    rust_assert (p->size () >= path.size ());
	    return;
	  }
      }

    paths[crate].emplace (id, std::move (path));
  }

  bool lookup_canonical_path (CrateNum crate, NodeId id,
			      const Resolver::CanonicalPath **path)
  {
    auto it = paths.find (crate);
    if (it == paths.end ())
      return false;

    auto iy = it->second.find (id);
    if (iy == it->second.end ())
      return false;

    *path = &iy->second;
    return true;
  }

  void insert_lang_item (RustLangItem::ItemType item_type, DefId id)
  {
    auto it = lang_item_mappings.find (item_type);
    rust_assert (it == lang_item_mappings.end ());

    lang_item_mappings[item_type] = id;
  }

  bool lookup_lang_item (RustLangItem::ItemType item_type, DefId *id)
  {
    auto it = lang_item_mappings.find (item_type);
    if (it == lang_item_mappings.end ())
      return false;

    *id = it->second;
    return true;
  }

  void insert_macro_def (AST::MacroRulesDefinition *macro);

  bool lookup_macro_def (NodeId id, AST::MacroRulesDefinition **def);

  void insert_visibility (NodeId id, Privacy::ModuleVisibility visibility);
  bool lookup_visibility (NodeId id, Privacy::ModuleVisibility &def);

  void insert_module_child (NodeId module, NodeId child);
  Optional<std::vector<NodeId> &> lookup_module_children (NodeId module);

private:
  Mappings ();

  CrateNum crateNumItr = 0;
  CrateNum currentCrateNum;

  std::map<CrateNum, HirId> hirIdIter;
  std::map<CrateNum, NodeId> nodeIdIter;
  std::map<CrateNum, LocalDefId> localIdIter;

  std::map<CrateNum, AST::Crate *> astCrateMappings;
  std::map<CrateNum, HIR::Crate *> hirCrateMappings;

  std::map<DefId, HIR::Item *> defIdMappings;
  std::map<CrateNum, std::map<LocalDefId, HIR::Item *>> localDefIdMappings;
  std::map<CrateNum, std::map<HirId, HIR::Module *>> hirModuleMappings;
  std::map<CrateNum, std::map<HirId, HIR::Item *>> hirItemMappings;
  std::map<CrateNum, std::map<HirId, HIR::Type *>> hirTypeMappings;
  std::map<CrateNum, std::map<HirId, HIR::Expr *>> hirExprMappings;
  std::map<CrateNum, std::map<HirId, HIR::Stmt *>> hirStmtMappings;
  std::map<CrateNum, std::map<HirId, HIR::FunctionParam *>> hirParamMappings;
  std::map<CrateNum, std::map<HirId, HIR::StructExprField *>>
    hirStructFieldMappings;
  std::map<CrateNum, std::map<HirId, std::pair<HirId, HIR::ImplItem *>>>
    hirImplItemMappings;
  std::map<CrateNum, std::map<HirId, HIR::SelfParam *>> hirSelfParamMappings;
  std::map<HirId, HIR::ImplBlock *> hirImplItemsToImplMappings;
  std::map<CrateNum, std::map<HirId, HIR::ImplBlock *>> hirImplBlockMappings;
  std::map<CrateNum, std::map<HirId, HIR::TraitItem *>> hirTraitItemMappings;
  std::map<CrateNum, std::map<HirId, HIR::ExternalItem *>>
    hirExternItemMappings;
  std::map<CrateNum, std::map<HirId, const AST::SimplePath *>>
    astSimplePathMappings;
  std::map<CrateNum, std::map<HirId, const AST::SimplePathSegment *>>
    astSimplePathSegmentMappings;
  std::map<CrateNum, std::map<HirId, HIR::PathExprSegment *>>
    hirPathSegMappings;
  std::map<CrateNum, std::map<HirId, HIR::GenericParam *>>
    hirGenericParamMappings;
  std::map<HirId, HIR::Trait *> hirTraitItemsToTraitMappings;
  std::map<CrateNum, std::map<HirId, HIR::Pattern *>> hirPatternMappings;

  // this maps the lang=<item_type> to DefId mappings
  std::map<RustLangItem::ItemType, DefId> lang_item_mappings;

  // canonical paths
  std::map<CrateNum, std::map<NodeId, const Resolver::CanonicalPath>> paths;

  // location info
  std::map<CrateNum, std::map<NodeId, Location>> locations;

  std::map<CrateNum, std::map<NodeId, HirId>> nodeIdToHirMappings;
  std::map<CrateNum, std::map<HirId, NodeId>> hirIdToNodeMappings;

  // all hirid nodes
  std::map<CrateNum, std::set<HirId>> hirNodesWithinCrate;

  // macros
  std::map<NodeId, AST::MacroRulesDefinition *> macroMappings;

  // crate names
  std::map<CrateNum, std::string> crate_names;

  // Low level visibility map for each DefId
  std::map<NodeId, Privacy::ModuleVisibility> visibility_map;

  // Module tree maps

  // Maps each module's node id to a list of its children
  std::map<NodeId, std::vector<NodeId>> module_child_map;
};

} // namespace Analysis
} // namespace Rust

#endif // RUST_HIR_MAP_H

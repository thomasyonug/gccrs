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

#ifndef RUST_HIR_TYPE_CHECK_TOPLEVEL
#define RUST_HIR_TYPE_CHECK_TOPLEVEL

#include "rust-hir-type-check-base.h"
#include "rust-hir-full.h"
#include "rust-hir-type-check-implitem.h"
#include "rust-hir-type-check-type.h"
#include "rust-hir-type-check-expr.h"
#include "rust-hir-type-check-enumitem.h"
#include "rust-tyty.h"

namespace Rust {
namespace Resolver {

class TypeCheckTopLevel : public TypeCheckBase
{
  using Rust::Resolver::TypeCheckBase::visit;

public:
  static void Resolve (HIR::Item *item)
  {
    TypeCheckTopLevel resolver;
    item->accept_vis (resolver);
  }

  void visit (HIR::TypeAlias &alias) override
  {
    TyTy::BaseType *actual_type
      = TypeCheckType::Resolve (alias.get_type_aliased ().get ());

    context->insert_type (alias.get_mappings (), actual_type);

    for (auto &where_clause_item : alias.get_where_clause ().get_items ())
      {
	ResolveWhereClauseItem::Resolve (*where_clause_item.get ());
      }
  }

  void visit (HIR::TupleStruct &struct_decl) override
  {
    std::vector<TyTy::SubstitutionParamMapping> substitutions;
    if (struct_decl.has_generics ())
      {
	for (auto &generic_param : struct_decl.get_generic_params ())
	  {
	    switch (generic_param.get ()->get_kind ())
	      {
	      case HIR::GenericParam::GenericKind::LIFETIME:
		// Skipping Lifetime completely until better handling.
		break;

		case HIR::GenericParam::GenericKind::TYPE: {
		  auto param_type
		    = TypeResolveGenericParam::Resolve (generic_param.get ());
		  context->insert_type (generic_param->get_mappings (),
					param_type);

		  substitutions.push_back (TyTy::SubstitutionParamMapping (
		    static_cast<HIR::TypeParam &> (*generic_param),
		    param_type));
		}
		break;
	      }
	  }
      }

    for (auto &where_clause_item : struct_decl.get_where_clause ().get_items ())
      {
	ResolveWhereClauseItem::Resolve (*where_clause_item.get ());
      }

    std::vector<TyTy::StructFieldType *> fields;
    size_t idx = 0;
    for (auto &field : struct_decl.get_fields ())
      {
	TyTy::BaseType *field_type
	  = TypeCheckType::Resolve (field.get_field_type ().get ());
	TyTy::StructFieldType *ty_field
	  = new TyTy::StructFieldType (field.get_mappings ().get_hirid (),
				       std::to_string (idx), field_type);
	fields.push_back (ty_field);
	context->insert_type (field.get_mappings (),
			      ty_field->get_field_type ());
	idx++;
      }

    // get the path
    const CanonicalPath *canonical_path = nullptr;
    bool ok = mappings->lookup_canonical_path (
      struct_decl.get_mappings ().get_crate_num (),
      struct_decl.get_mappings ().get_nodeid (), &canonical_path);
    rust_assert (ok);
    RustIdent ident{*canonical_path, struct_decl.get_locus ()};

    // its a single variant ADT
    std::vector<TyTy::VariantDef *> variants;
    variants.push_back (
      new TyTy::VariantDef (struct_decl.get_mappings ().get_hirid (),
			    struct_decl.get_identifier (), ident,
			    TyTy::VariantDef::VariantType::TUPLE, nullptr,
			    std::move (fields)));

    // Process #[repr(X)] attribute, if any
    const AST::AttrVec &attrs = struct_decl.get_outer_attrs ();
    TyTy::ADTType::ReprOptions repr
      = parse_repr_options (attrs, struct_decl.get_locus ());

    TyTy::BaseType *type
      = new TyTy::ADTType (struct_decl.get_mappings ().get_hirid (),
			   mappings->get_next_hir_id (),
			   struct_decl.get_identifier (), ident,
			   TyTy::ADTType::ADTKind::TUPLE_STRUCT,
			   std::move (variants), std::move (substitutions),
			   repr);

    context->insert_type (struct_decl.get_mappings (), type);
  }

  void visit (HIR::Module &module) override
  {
    for (auto &item : module.get_items ())
      TypeCheckTopLevel::Resolve (item.get ());
  }

  void visit (HIR::StructStruct &struct_decl) override
  {
    std::vector<TyTy::SubstitutionParamMapping> substitutions;
    if (struct_decl.has_generics ())
      {
	for (auto &generic_param : struct_decl.get_generic_params ())
	  {
	    switch (generic_param.get ()->get_kind ())
	      {
	      case HIR::GenericParam::GenericKind::LIFETIME:
		// Skipping Lifetime completely until better handling.
		break;

		case HIR::GenericParam::GenericKind::TYPE: {
		  auto param_type
		    = TypeResolveGenericParam::Resolve (generic_param.get ());
		  context->insert_type (generic_param->get_mappings (),
					param_type);

		  substitutions.push_back (TyTy::SubstitutionParamMapping (
		    static_cast<HIR::TypeParam &> (*generic_param),
		    param_type));
		}
		break;
	      }
	  }
      }

    for (auto &where_clause_item : struct_decl.get_where_clause ().get_items ())
      {
	ResolveWhereClauseItem::Resolve (*where_clause_item.get ());
      }

    std::vector<TyTy::StructFieldType *> fields;
    for (auto &field : struct_decl.get_fields ())
      {
	TyTy::BaseType *field_type
	  = TypeCheckType::Resolve (field.get_field_type ().get ());
	TyTy::StructFieldType *ty_field
	  = new TyTy::StructFieldType (field.get_mappings ().get_hirid (),
				       field.get_field_name (), field_type);
	fields.push_back (ty_field);
	context->insert_type (field.get_mappings (),
			      ty_field->get_field_type ());
      }

    // get the path
    const CanonicalPath *canonical_path = nullptr;
    bool ok = mappings->lookup_canonical_path (
      struct_decl.get_mappings ().get_crate_num (),
      struct_decl.get_mappings ().get_nodeid (), &canonical_path);
    rust_assert (ok);
    RustIdent ident{*canonical_path, struct_decl.get_locus ()};

    // its a single variant ADT
    std::vector<TyTy::VariantDef *> variants;
    variants.push_back (
      new TyTy::VariantDef (struct_decl.get_mappings ().get_hirid (),
			    struct_decl.get_identifier (), ident,
			    TyTy::VariantDef::VariantType::STRUCT, nullptr,
			    std::move (fields)));

    // Process #[repr(X)] attribute, if any
    const AST::AttrVec &attrs = struct_decl.get_outer_attrs ();
    TyTy::ADTType::ReprOptions repr
      = parse_repr_options (attrs, struct_decl.get_locus ());

    TyTy::BaseType *type
      = new TyTy::ADTType (struct_decl.get_mappings ().get_hirid (),
			   mappings->get_next_hir_id (),
			   struct_decl.get_identifier (), ident,
			   TyTy::ADTType::ADTKind::STRUCT_STRUCT,
			   std::move (variants), std::move (substitutions),
			   repr);

    context->insert_type (struct_decl.get_mappings (), type);
  }

  void visit (HIR::Enum &enum_decl) override
  {
    std::vector<TyTy::SubstitutionParamMapping> substitutions;
    if (enum_decl.has_generics ())
      {
	for (auto &generic_param : enum_decl.get_generic_params ())
	  {
	    switch (generic_param.get ()->get_kind ())
	      {
	      case HIR::GenericParam::GenericKind::LIFETIME:
		// Skipping Lifetime completely until better handling.
		break;

		case HIR::GenericParam::GenericKind::TYPE: {
		  auto param_type
		    = TypeResolveGenericParam::Resolve (generic_param.get ());
		  context->insert_type (generic_param->get_mappings (),
					param_type);

		  substitutions.push_back (TyTy::SubstitutionParamMapping (
		    static_cast<HIR::TypeParam &> (*generic_param),
		    param_type));
		}
		break;
	      }
	  }
      }

    std::vector<TyTy::VariantDef *> variants;
    int64_t discriminant_value = 0;
    for (auto &variant : enum_decl.get_variants ())
      {
	TyTy::VariantDef *field_type
	  = TypeCheckEnumItem::Resolve (variant.get (), discriminant_value);

	discriminant_value++;
	variants.push_back (field_type);
      }

    // get the path
    const CanonicalPath *canonical_path = nullptr;
    bool ok = mappings->lookup_canonical_path (
      enum_decl.get_mappings ().get_crate_num (),
      enum_decl.get_mappings ().get_nodeid (), &canonical_path);
    rust_assert (ok);
    RustIdent ident{*canonical_path, enum_decl.get_locus ()};

    // multi variant ADT
    TyTy::BaseType *type
      = new TyTy::ADTType (enum_decl.get_mappings ().get_hirid (),
			   mappings->get_next_hir_id (),
			   enum_decl.get_identifier (), ident,
			   TyTy::ADTType::ADTKind::ENUM, std::move (variants),
			   std::move (substitutions));

    context->insert_type (enum_decl.get_mappings (), type);
  }

  void visit (HIR::Union &union_decl) override
  {
    std::vector<TyTy::SubstitutionParamMapping> substitutions;
    if (union_decl.has_generics ())
      {
	for (auto &generic_param : union_decl.get_generic_params ())
	  {
	    switch (generic_param.get ()->get_kind ())
	      {
	      case HIR::GenericParam::GenericKind::LIFETIME:
		// Skipping Lifetime completely until better handling.
		break;

		case HIR::GenericParam::GenericKind::TYPE: {
		  auto param_type
		    = TypeResolveGenericParam::Resolve (generic_param.get ());
		  context->insert_type (generic_param->get_mappings (),
					param_type);

		  substitutions.push_back (TyTy::SubstitutionParamMapping (
		    static_cast<HIR::TypeParam &> (*generic_param),
		    param_type));
		}
		break;
	      }
	  }
      }

    for (auto &where_clause_item : union_decl.get_where_clause ().get_items ())
      {
	ResolveWhereClauseItem::Resolve (*where_clause_item.get ());
      }

    std::vector<TyTy::StructFieldType *> fields;
    for (auto &variant : union_decl.get_variants ())
      {
	TyTy::BaseType *variant_type
	  = TypeCheckType::Resolve (variant.get_field_type ().get ());
	TyTy::StructFieldType *ty_variant
	  = new TyTy::StructFieldType (variant.get_mappings ().get_hirid (),
				       variant.get_field_name (), variant_type);
	fields.push_back (ty_variant);
	context->insert_type (variant.get_mappings (),
			      ty_variant->get_field_type ());
      }

    // get the path
    const CanonicalPath *canonical_path = nullptr;
    bool ok = mappings->lookup_canonical_path (
      union_decl.get_mappings ().get_crate_num (),
      union_decl.get_mappings ().get_nodeid (), &canonical_path);
    rust_assert (ok);
    RustIdent ident{*canonical_path, union_decl.get_locus ()};

    // there is only a single variant
    std::vector<TyTy::VariantDef *> variants;
    variants.push_back (
      new TyTy::VariantDef (union_decl.get_mappings ().get_hirid (),
			    union_decl.get_identifier (), ident,
			    TyTy::VariantDef::VariantType::STRUCT, nullptr,
			    std::move (fields)));

    TyTy::BaseType *type
      = new TyTy::ADTType (union_decl.get_mappings ().get_hirid (),
			   mappings->get_next_hir_id (),
			   union_decl.get_identifier (), ident,
			   TyTy::ADTType::ADTKind::UNION, std::move (variants),
			   std::move (substitutions));

    context->insert_type (union_decl.get_mappings (), type);
  }

  void visit (HIR::StaticItem &var) override
  {
    TyTy::BaseType *type = TypeCheckType::Resolve (var.get_type ());
    TyTy::BaseType *expr_type = TypeCheckExpr::Resolve (var.get_expr ());

    context->insert_type (var.get_mappings (), type->unify (expr_type));
  }

  void visit (HIR::ConstantItem &constant) override
  {
    TyTy::BaseType *type = TypeCheckType::Resolve (constant.get_type ());
    TyTy::BaseType *expr_type = TypeCheckExpr::Resolve (constant.get_expr ());

    context->insert_type (constant.get_mappings (), type->unify (expr_type));
  }

  void visit (HIR::Function &function) override
  {
    std::vector<TyTy::SubstitutionParamMapping> substitutions;
    if (function.has_generics ())
      {
	for (auto &generic_param : function.get_generic_params ())
	  {
	    switch (generic_param.get ()->get_kind ())
	      {
	      case HIR::GenericParam::GenericKind::LIFETIME:
		// Skipping Lifetime completely until better handling.
		break;

		case HIR::GenericParam::GenericKind::TYPE: {
		  auto param_type
		    = TypeResolveGenericParam::Resolve (generic_param.get ());
		  context->insert_type (generic_param->get_mappings (),
					param_type);

		  substitutions.push_back (TyTy::SubstitutionParamMapping (
		    static_cast<HIR::TypeParam &> (*generic_param),
		    param_type));
		}
		break;
	      }
	  }
      }

    for (auto &where_clause_item : function.get_where_clause ().get_items ())
      {
	ResolveWhereClauseItem::Resolve (*where_clause_item.get ());
      }

    TyTy::BaseType *ret_type = nullptr;
    if (!function.has_function_return_type ())
      ret_type = TyTy::TupleType::get_unit_type (
	function.get_mappings ().get_hirid ());
    else
      {
	auto resolved
	  = TypeCheckType::Resolve (function.get_return_type ().get ());
	if (resolved->get_kind () == TyTy::TypeKind::ERROR)
	  {
	    rust_error_at (function.get_locus (),
			   "failed to resolve return type");
	    return;
	  }

	ret_type = resolved->clone ();
	ret_type->set_ref (
	  function.get_return_type ()->get_mappings ().get_hirid ());
      }

    std::vector<std::pair<HIR::Pattern *, TyTy::BaseType *> > params;
    for (auto &param : function.get_function_params ())
      {
	// get the name as well required for later on
	auto param_tyty = TypeCheckType::Resolve (param.get_type ());
	params.push_back (
	  std::pair<HIR::Pattern *, TyTy::BaseType *> (param.get_param_name (),
						       param_tyty));

	context->insert_type (param.get_mappings (), param_tyty);
	TypeCheckPattern::Resolve (param.get_param_name (), param_tyty);
      }

    const CanonicalPath *canonical_path = nullptr;
    bool ok = mappings->lookup_canonical_path (
      function.get_mappings ().get_crate_num (),
      function.get_mappings ().get_nodeid (), &canonical_path);
    rust_assert (ok);

    RustIdent ident{*canonical_path, function.get_locus ()};
    auto fnType = new TyTy::FnType (function.get_mappings ().get_hirid (),
				    function.get_mappings ().get_defid (),
				    function.get_function_name (), ident,
				    TyTy::FnType::FNTYPE_DEFAULT_FLAGS,
				    ABI::RUST, std::move (params), ret_type,
				    std::move (substitutions));

    context->insert_type (function.get_mappings (), fnType);
  }

  void visit (HIR::ImplBlock &impl_block) override
  {
    std::vector<TyTy::SubstitutionParamMapping> substitutions;
    if (impl_block.has_generics ())
      {
	for (auto &generic_param : impl_block.get_generic_params ())
	  {
	    switch (generic_param.get ()->get_kind ())
	      {
	      case HIR::GenericParam::GenericKind::LIFETIME:
		// Skipping Lifetime completely until better handling.
		break;

		case HIR::GenericParam::GenericKind::TYPE: {
		  auto param_type
		    = TypeResolveGenericParam::Resolve (generic_param.get ());
		  context->insert_type (generic_param->get_mappings (),
					param_type);

		  substitutions.push_back (TyTy::SubstitutionParamMapping (
		    static_cast<HIR::TypeParam &> (*generic_param),
		    param_type));
		}
		break;
	      }
	  }
      }

    for (auto &where_clause_item : impl_block.get_where_clause ().get_items ())
      {
	ResolveWhereClauseItem::Resolve (*where_clause_item.get ());
      }

    auto self = TypeCheckType::Resolve (impl_block.get_type ().get ());
    if (self->get_kind () == TyTy::TypeKind::ERROR)
      return;

    for (auto &impl_item : impl_block.get_impl_items ())
      TypeCheckTopLevelImplItem::Resolve (impl_item.get (), self,
					  substitutions);
  }

  void visit (HIR::ExternBlock &extern_block) override
  {
    for (auto &item : extern_block.get_extern_items ())
      {
	TypeCheckTopLevelExternItem::Resolve (item.get (), extern_block);
      }
  }

private:
  TypeCheckTopLevel () : TypeCheckBase () {}
};

} // namespace Resolver
} // namespace Rust

#endif // RUST_HIR_TYPE_CHECK_TOPLEVEL

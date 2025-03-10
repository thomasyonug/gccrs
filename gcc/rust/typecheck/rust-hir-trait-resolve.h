// Copyright (C) 2021-2022 Free Software Foundation, Inc.

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

#ifndef RUST_HIR_TRAIT_RESOLVE_H
#define RUST_HIR_TRAIT_RESOLVE_H

#include "rust-hir-type-check-base.h"
#include "rust-hir-full.h"
#include "rust-tyty-visitor.h"
#include "rust-hir-type-check-type.h"
#include "rust-hir-trait-ref.h"
#include "rust-expr.h"

namespace Rust {
namespace Resolver {

class ResolveTraitItemToRef : public TypeCheckBase
{
  using Rust::Resolver::TypeCheckBase::visit;

public:
  static TraitItemReference
  Resolve (HIR::TraitItem &item, TyTy::BaseType *self,
	   std::vector<TyTy::SubstitutionParamMapping> substitutions)
  {
    ResolveTraitItemToRef resolver (self, std::move (substitutions));
    item.accept_vis (resolver);
    return std::move (resolver.resolved);
  }

  void visit (HIR::TraitItemType &type) override;

  void visit (HIR::TraitItemConst &cst) override;

  void visit (HIR::TraitItemFunc &fn) override;

private:
  ResolveTraitItemToRef (
    TyTy::BaseType *self,
    std::vector<TyTy::SubstitutionParamMapping> &&substitutions);

  TraitItemReference resolved;
  TyTy::BaseType *self;
  std::vector<TyTy::SubstitutionParamMapping> substitutions;
};

class TraitResolver : public TypeCheckBase
{
  using Rust::Resolver::TypeCheckBase::visit;

public:
  static TraitReference *Resolve (HIR::TypePath &path);

  static TraitReference *Resolve (HIR::Trait &trait);

  static TraitReference *Lookup (HIR::TypePath &path);

private:
  TraitResolver ();

  TraitReference *resolve_path (HIR::TypePath &path);

  TraitReference *resolve_trait (HIR::Trait *trait_reference);

  TraitReference *lookup_path (HIR::TypePath &path);

  HIR::Trait *resolved_trait_reference;

public:
  void visit (HIR::Trait &trait) override { resolved_trait_reference = &trait; }
};

} // namespace Resolver
} // namespace Rust

#endif // RUST_HIR_TRAIT_RESOLVE_H

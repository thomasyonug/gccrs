// Copyright (C) 2020 Free Software Foundation, Inc.

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

#ifndef RUST_SUBSTITUTION_MAPPER_H
#define RUST_SUBSTITUTION_MAPPER_H

#include "rust-tyty.h"
#include "rust-tyty-visitor.h"

namespace Rust {
namespace Resolver {

class SubstMapper : public TyTy::TyVisitor
{
public:
  static TyTy::BaseType *Resolve (TyTy::BaseType *base,
				  HIR::GenericArgs *generics = nullptr)
  {
    SubstMapper mapper (base->get_ref (), generics);
    base->accept_vis (mapper);
    rust_assert (mapper.resolved != nullptr);
    return mapper.resolved;
  }

  bool have_generic_args () const { return generics != nullptr; }

  void visit (TyTy::FnType &type) override
  {
    auto concrete = have_generic_args () ? type.handle_substitions (
		      type.get_mappings_from_generic_args (*generics))
					 : type.infer_substitions ();
    if (concrete != nullptr)
      resolved = concrete;
  }

  void visit (TyTy::ADTType &type) override
  {
    printf ("Trying to substitute: %s have generic args %s\n",
	    type.as_string ().c_str (),
	    have_generic_args () ? "true" : "false");

    TyTy::ADTType *concrete = nullptr;
    if (!have_generic_args ())
      {
	concrete = type.infer_substitions ();
      }
    else
      {
	TyTy::SubstitutionArgumentMappings mappings
	  = type.get_mappings_from_generic_args (*generics);
	concrete = type.handle_substitions (mappings);
      }

    if (concrete != nullptr)
      resolved = concrete;
  }

  void visit (TyTy::UnitType &) override { gcc_unreachable (); }
  void visit (TyTy::InferType &) override { gcc_unreachable (); }
  void visit (TyTy::TupleType &) override { gcc_unreachable (); }
  void visit (TyTy::FnPtr &) override { gcc_unreachable (); }
  void visit (TyTy::ArrayType &) override { gcc_unreachable (); }
  void visit (TyTy::BoolType &) override { gcc_unreachable (); }
  void visit (TyTy::IntType &) override { gcc_unreachable (); }
  void visit (TyTy::UintType &) override { gcc_unreachable (); }
  void visit (TyTy::FloatType &) override { gcc_unreachable (); }
  void visit (TyTy::USizeType &) override { gcc_unreachable (); }
  void visit (TyTy::ISizeType &) override { gcc_unreachable (); }
  void visit (TyTy::ErrorType &) override { gcc_unreachable (); }
  void visit (TyTy::CharType &) override { gcc_unreachable (); }
  void visit (TyTy::ReferenceType &) override { gcc_unreachable (); }
  void visit (TyTy::ParamType &) override { gcc_unreachable (); }
  void visit (TyTy::StrType &) override { gcc_unreachable (); }

private:
  SubstMapper (HirId ref, HIR::GenericArgs *generics)
    : resolved (new TyTy::ErrorType (ref)), generics (generics)
  {}

  TyTy::BaseType *resolved;
  HIR::GenericArgs *generics;
};

} // namespace Resolver
} // namespace Rust

#endif // RUST_SUBSTITUTION_MAPPER_H

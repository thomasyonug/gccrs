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

#ifndef RUST_AST_MACRO_H
#define RUST_AST_MACRO_H

#include "rust-ast.h"
#include "rust-location.h"

namespace Rust {
namespace AST {

// Decls as definitions moved to rust-ast.h
class MacroItem;

class MacroFragSpec
{
public:
  enum Kind
  {
    BLOCK,
    EXPR,
    IDENT,
    ITEM,
    LIFETIME,
    LITERAL,
    META,
    PAT,
    PATH,
    STMT,
    TT,
    TY,
    VIS,
    INVALID // not really a specifier, but used to mark invalid one passed in
  };

  MacroFragSpec (Kind kind) : kind (kind) {}

  static MacroFragSpec get_frag_spec_from_str (const std::string &str)
  {
    if (str == "block")
      return MacroFragSpec (BLOCK);
    else if (str == "expr")
      return MacroFragSpec (EXPR);
    else if (str == "ident")
      return MacroFragSpec (IDENT);
    else if (str == "item")
      return MacroFragSpec (ITEM);
    else if (str == "lifetime")
      return MacroFragSpec (LIFETIME);
    else if (str == "literal")
      return MacroFragSpec (LITERAL);
    else if (str == "meta")
      return MacroFragSpec (META);
    else if (str == "pat" || str == "pat_param")
      return MacroFragSpec (PAT);
    else if (str == "path")
      return MacroFragSpec (PATH);
    else if (str == "stmt")
      return MacroFragSpec (STMT);
    else if (str == "tt")
      return MacroFragSpec (TT);
    else if (str == "ty")
      return MacroFragSpec (TY);
    else if (str == "vis")
      return MacroFragSpec (VIS);
    else
      {
	// error_at("invalid string '%s' used as fragment specifier",
	// str->c_str()));
	return MacroFragSpec (INVALID);
      }
  }

  Kind get_kind () const { return kind; }
  bool is_error () const { return kind == Kind::INVALID; }

  // Converts a frag spec enum item to a string form.
  std::string as_string () const
  {
    switch (kind)
      {
      case BLOCK:
	return "block";
      case EXPR:
	return "expr";
      case IDENT:
	return "ident";
      case ITEM:
	return "item";
      case LIFETIME:
	return "lifetime";
      case LITERAL:
	return "literal";
      case META:
	return "meta";
      case PAT:
	return "pat";
      case PATH:
	return "path";
      case STMT:
	return "stmt";
      case TT:
	return "tt";
      case TY:
	return "ty";
      case VIS:
	return "vis";
      case INVALID:
	return "INVALID_FRAG_SPEC";
      default:
	return "ERROR_MARK_STRING - unknown frag spec";
      }
  }

  bool has_follow_set_restrictions () const
  {
    switch (kind)
      {
      case EXPR:
      case STMT:
      case PAT:
      case PATH:
      case TY:
      case VIS:
	return true;
      default:
	return false;
      }
  }

  bool has_follow_set_fragment_restrictions () const
  {
    switch (kind)
      {
      case PAT:
      case TY:
      case VIS:
	return true;
      default:
	return false;
      }
  }

private:
  Kind kind;
};

// A macro match that has an identifier and fragment spec
class MacroMatchFragment : public MacroMatch
{
  Identifier ident;
  MacroFragSpec frag_spec;
  Location locus;

public:
  MacroMatchFragment (Identifier ident, MacroFragSpec frag_spec, Location locus)
    : ident (std::move (ident)), frag_spec (frag_spec), locus (locus)
  {}

  // Returns whether macro match fragment is in an error state.
  bool is_error () const
  {
    return frag_spec.get_kind () == MacroFragSpec::INVALID;
  }

  // Creates an error state macro match fragment.
  static MacroMatchFragment create_error (Location locus)
  {
    return MacroMatchFragment (std::string (""),
			       MacroFragSpec (MacroFragSpec::Kind::INVALID),
			       locus);
  }

  std::string as_string () const override;
  Location get_match_locus () const override { return locus; };

  void accept_vis (ASTVisitor &vis) override;

  MacroMatchType get_macro_match_type () const override
  {
    return MacroMatchType::Fragment;
  }

  Identifier get_ident () const { return ident; }
  const MacroFragSpec &get_frag_spec () const { return frag_spec; }

protected:
  /* Use covariance to implement clone function as returning this object rather
   * than base */
  MacroMatchFragment *clone_macro_match_impl () const override
  {
    return new MacroMatchFragment (*this);
  }
};

// A repetition macro match
class MacroMatchRepetition : public MacroMatch
{
public:
  enum MacroRepOp
  {
    NONE,
    ANY,
    ONE_OR_MORE,
    ZERO_OR_ONE,
  };

private:
  std::vector<std::unique_ptr<MacroMatch> > matches;
  MacroRepOp op;

  // bool has_sep;
  typedef Token MacroRepSep;
  // any token except delimiters and repetition operators
  std::unique_ptr<MacroRepSep> sep;
  Location locus;

public:
  // Returns whether macro match repetition has separator token.
  bool has_sep () const { return sep != nullptr; }

  MacroMatchRepetition (std::vector<std::unique_ptr<MacroMatch> > matches,
			MacroRepOp op, std::unique_ptr<MacroRepSep> sep,
			Location locus)
    : matches (std::move (matches)), op (op), sep (std::move (sep)),
      locus (locus)
  {}

  // Copy constructor with clone
  MacroMatchRepetition (MacroMatchRepetition const &other)
    : op (other.op), locus (other.locus)
  {
    // guard to protect from null pointer dereference
    if (other.sep != nullptr)
      sep = other.sep->clone_token ();

    matches.reserve (other.matches.size ());
    for (const auto &e : other.matches)
      matches.push_back (e->clone_macro_match ());
  }

  // Overloaded assignment operator to clone
  MacroMatchRepetition &operator= (MacroMatchRepetition const &other)
  {
    op = other.op;
    locus = other.locus;

    // guard to protect from null pointer dereference
    if (other.sep != nullptr)
      sep = other.sep->clone_token ();
    else
      sep = nullptr;

    matches.reserve (other.matches.size ());
    for (const auto &e : other.matches)
      matches.push_back (e->clone_macro_match ());

    return *this;
  }

  // move constructors
  MacroMatchRepetition (MacroMatchRepetition &&other) = default;
  MacroMatchRepetition &operator= (MacroMatchRepetition &&other) = default;

  std::string as_string () const override;
  Location get_match_locus () const override { return locus; };

  void accept_vis (ASTVisitor &vis) override;

  MacroMatchType get_macro_match_type () const override
  {
    return MacroMatchType::Repetition;
  }

  MacroRepOp get_op () const { return op; }
  const std::unique_ptr<MacroRepSep> &get_sep () const { return sep; }
  std::vector<std::unique_ptr<MacroMatch> > &get_matches () { return matches; }
  const std::vector<std::unique_ptr<MacroMatch> > &get_matches () const
  {
    return matches;
  }

protected:
  /* Use covariance to implement clone function as returning this object rather
   * than base */
  MacroMatchRepetition *clone_macro_match_impl () const override
  {
    return new MacroMatchRepetition (*this);
  }
};

// can't inline due to polymorphism
class MacroMatcher : public MacroMatch
{
  DelimType delim_type;
  std::vector<std::unique_ptr<MacroMatch> > matches;
  Location locus;

  // TODO: think of way to mark invalid that doesn't take up more space
  bool is_invalid;

public:
  MacroMatcher (DelimType delim_type,
		std::vector<std::unique_ptr<MacroMatch> > matches,
		Location locus)
    : delim_type (delim_type), matches (std::move (matches)), locus (locus),
      is_invalid (false)
  {}

  // copy constructor with vector clone
  MacroMatcher (MacroMatcher const &other)
    : delim_type (other.delim_type), locus (other.locus)
  {
    matches.reserve (other.matches.size ());
    for (const auto &e : other.matches)
      matches.push_back (e->clone_macro_match ());
  }

  // overloaded assignment operator with vector clone
  MacroMatcher &operator= (MacroMatcher const &other)
  {
    delim_type = other.delim_type;
    locus = other.locus;

    matches.reserve (other.matches.size ());
    for (const auto &e : other.matches)
      matches.push_back (e->clone_macro_match ());

    return *this;
  }

  // move constructors
  MacroMatcher (MacroMatcher &&other) = default;
  MacroMatcher &operator= (MacroMatcher &&other) = default;

  // Creates an error state macro matcher.
  static MacroMatcher create_error (Location locus)
  {
    return MacroMatcher (true, locus);
  }

  // Returns whether MacroMatcher is in an error state.
  bool is_error () const { return is_invalid; }
  Location get_match_locus () const override { return locus; }

  std::string as_string () const override;

  void accept_vis (ASTVisitor &vis) override;

  MacroMatchType get_macro_match_type () const override
  {
    return MacroMatchType::Matcher;
  }

  DelimType get_delim_type () const { return delim_type; }
  std::vector<std::unique_ptr<MacroMatch> > &get_matches () { return matches; }
  const std::vector<std::unique_ptr<MacroMatch> > &get_matches () const
  {
    return matches;
  }

protected:
  /* Use covariance to implement clone function as returning this object rather
   * than base */
  MacroMatcher *clone_macro_match_impl () const override
  {
    return new MacroMatcher (*this);
  }

  // constructor only used to create error matcher
  MacroMatcher (bool is_invalid, Location locus)
    : delim_type (PARENS), locus (locus), is_invalid (is_invalid)
  {}
};

// TODO: inline?
struct MacroTranscriber
{
private:
  DelimTokenTree token_tree;
  Location locus;

public:
  MacroTranscriber (DelimTokenTree token_tree, Location locus)
    : token_tree (std::move (token_tree)), locus (locus)
  {}

  std::string as_string () const { return token_tree.as_string (); }

  Location get_locus () const { return locus; }

  DelimTokenTree &get_token_tree () { return token_tree; }
};

// A macro rule? Matcher and transcriber pair?
struct MacroRule
{
private:
  MacroMatcher matcher;
  MacroTranscriber transcriber;
  Location locus;

public:
  MacroRule (MacroMatcher matcher, MacroTranscriber transcriber, Location locus)
    : matcher (std::move (matcher)), transcriber (std::move (transcriber)),
      locus (locus)
  {}

  // Returns whether macro rule is in error state.
  bool is_error () const { return matcher.is_error (); }

  // Creates an error state macro rule.
  static MacroRule create_error (Location locus)
  {
    return MacroRule (MacroMatcher::create_error (locus),
		      MacroTranscriber (DelimTokenTree::create_empty (),
					Location ()),
		      locus);
  }

  Location get_locus () const { return locus; }

  std::string as_string () const;

  MacroMatcher &get_matcher () { return matcher; }
  MacroTranscriber &get_transcriber () { return transcriber; }
};

// A macro rules definition item AST node
class MacroRulesDefinition : public MacroItem
{
  std::vector<Attribute> outer_attrs;
  Identifier rule_name;
  // MacroRulesDef rules_def;
  // only curly without required semicolon at end
  DelimType delim_type;
  // MacroRules rules;
  std::vector<MacroRule> rules; // inlined form
  Location locus;

  std::function<ASTFragment (Location, MacroInvocData &)>
    associated_transcriber;
  // Since we can't compare std::functions, we need to use an extra boolean
  bool is_builtin_rule;

  /**
   * Default function to use as an associated transcriber. This function should
   * never be called, hence the gcc_unreachable().
   * If this function is used, then the macro is not builtin and the compiler
   * should make use of the actual rules. If the macro is builtin, then another
   * associated transcriber should be used
   */
  static ASTFragment dummy_builtin (Location, MacroInvocData &)
  {
    gcc_unreachable ();
    return ASTFragment::create_error ();
  }

  /* NOTE: in rustc, macro definitions are considered (and parsed as) a type
   * of macro, whereas here they are considered part of the language itself.
   * I am not aware of the implications of this decision. The rustc spec does
   * mention that using the same parser for macro definitions and invocations
   * is "extremely self-referential and non-intuitive". */

public:
  std::string as_string () const override;

  MacroRulesDefinition (Identifier rule_name, DelimType delim_type,
			std::vector<MacroRule> rules,
			std::vector<Attribute> outer_attrs, Location locus)
    : outer_attrs (std::move (outer_attrs)), rule_name (std::move (rule_name)),
      delim_type (delim_type), rules (std::move (rules)), locus (locus),
      associated_transcriber (dummy_builtin), is_builtin_rule (false)
  {}

  MacroRulesDefinition (Identifier builtin_name, DelimType delim_type,
			std::function<ASTFragment (Location, MacroInvocData &)>
			  associated_transcriber)
    : outer_attrs (std::vector<Attribute> ()), rule_name (builtin_name),
      delim_type (delim_type), rules (std::vector<MacroRule> ()),
      locus (Location ()), associated_transcriber (associated_transcriber),
      is_builtin_rule (true)
  {}

  void accept_vis (ASTVisitor &vis) override;

  // Invalid if rule name is empty, so base stripping on that.
  void mark_for_strip () override { rule_name = ""; }
  bool is_marked_for_strip () const override { return rule_name.empty (); }

  // TODO: this mutable getter seems really dodgy. Think up better way.
  std::vector<Attribute> &get_outer_attrs () { return outer_attrs; }
  const std::vector<Attribute> &get_outer_attrs () const { return outer_attrs; }

  std::vector<MacroRule> &get_macro_rules () { return rules; }
  const std::vector<MacroRule> &get_macro_rules () const { return rules; }

  Location get_locus () const override final { return locus; }

  Identifier get_rule_name () const { return rule_name; }

  std::vector<MacroRule> &get_rules () { return rules; }
  const std::vector<MacroRule> &get_rules () const { return rules; }

  bool is_builtin () const { return is_builtin_rule; }
  const std::function<ASTFragment (Location, MacroInvocData &)> &
  get_builtin_transcriber () const
  {
    rust_assert (is_builtin ());
    return associated_transcriber;
  }
  void set_builtin_transcriber (
    std::function<ASTFragment (Location, MacroInvocData &)> transcriber)
  {
    associated_transcriber = transcriber;
    is_builtin_rule = true;
  }

  Kind get_ast_kind () const override { return Kind::MACRO_RULES_DEFINITION; }

protected:
  /* Use covariance to implement clone function as returning this object rather
   * than base */
  MacroRulesDefinition *clone_item_impl () const override
  {
    return new MacroRulesDefinition (*this);
  }
};

/* AST node of a macro invocation, which is replaced by the macro result at
 * compile time */
class MacroInvocation : public TypeNoBounds,
			public Pattern,
			public MacroItem,
			public TraitItem,
			public TraitImplItem,
			public InherentImplItem,
			public ExternalItem,
			public ExprWithoutBlock
{
  std::vector<Attribute> outer_attrs;
  MacroInvocData invoc_data;
  Location locus;

  // Important for when we actually expand the macro
  bool is_semi_coloned;

  NodeId node_id;

public:
  std::string as_string () const override;

  MacroInvocation (MacroInvocData invoc_data,
		   std::vector<Attribute> outer_attrs, Location locus,
		   bool is_semi_coloned = false)
    : outer_attrs (std::move (outer_attrs)),
      invoc_data (std::move (invoc_data)), locus (locus),
      is_semi_coloned (is_semi_coloned),
      node_id (Analysis::Mappings::get ()->get_next_node_id ())
  {}

  Location get_locus () const override final { return locus; }

  void accept_vis (ASTVisitor &vis) override;

  // Invalid if path is empty, so base stripping on that.
  void mark_for_strip () override { invoc_data.mark_for_strip (); }
  bool is_marked_for_strip () const override
  {
    return invoc_data.is_marked_for_strip ();
  }

  const std::vector<Attribute> &get_outer_attrs () const { return outer_attrs; }
  std::vector<Attribute> &get_outer_attrs () { return outer_attrs; }

  void set_outer_attrs (std::vector<Attribute> new_attrs) override
  {
    outer_attrs = std::move (new_attrs);
  }

  NodeId get_pattern_node_id () const override final
  {
    return ExprWithoutBlock::get_node_id ();
  }

  Kind get_ast_kind () const override { return Kind::MACRO_INVOCATION; }

  NodeId get_macro_node_id () const { return node_id; }

  MacroInvocData &get_invoc_data () { return invoc_data; }

  bool has_semicolon () const { return is_semi_coloned; }

protected:
  /* Use covariance to implement clone function as returning this object rather
   * than base */
  MacroInvocation *clone_pattern_impl () const final override
  {
    return clone_macro_invocation_impl ();
  }

  /* Use covariance to implement clone function as returning this object rather
   * than base */
  MacroInvocation *clone_expr_without_block_impl () const final override
  {
    return clone_macro_invocation_impl ();
  }

  /* Use covariance to implement clone function as returning this object rather
   * than base */
  MacroInvocation *clone_type_no_bounds_impl () const final override
  {
    return clone_macro_invocation_impl ();
  }

  MacroInvocation *clone_external_item_impl () const final override
  {
    return clone_macro_invocation_impl ();
  }

  /*virtual*/ MacroInvocation *clone_macro_invocation_impl () const
  {
    return new MacroInvocation (*this);
  }

  Item *clone_item_impl () const override
  {
    return clone_macro_invocation_impl ();
  }

  bool is_item () const override { return !has_semicolon (); }

  TraitItem *clone_trait_item_impl () const override
  {
    return clone_macro_invocation_impl ();
  };

  TraitImplItem *clone_trait_impl_item_impl () const override
  {
    return clone_macro_invocation_impl ();
  };

  InherentImplItem *clone_inherent_impl_item_impl () const override
  {
    return clone_macro_invocation_impl ();
  }

  ExprWithoutBlock *to_stmt () const override

  {
    auto new_impl = clone_macro_invocation_impl ();
    new_impl->is_semi_coloned = true;

    return new_impl;
  }
};

// more generic meta item path-only form
class MetaItemPath : public MetaItem
{
  SimplePath path;

public:
  MetaItemPath (SimplePath path) : path (std::move (path)) {}

  std::string as_string () const override { return path.as_string (); }

  void accept_vis (ASTVisitor &vis) override;

  // HACK: used to simplify parsing - returns non-empty only in this case
  SimplePath to_path_item () const override
  {
    // this should copy construct - TODO ensure it does
    return path;
  }

  bool check_cfg_predicate (const Session &session) const override;

  Attribute to_attribute () const override;

protected:
  // Use covariance to implement clone function as returning this type
  MetaItemPath *clone_meta_item_inner_impl () const override
  {
    return new MetaItemPath (*this);
  }
};

// more generic meta item sequence form
class MetaItemSeq : public MetaItem
{
  SimplePath path;
  std::vector<std::unique_ptr<MetaItemInner> > seq;

public:
  MetaItemSeq (SimplePath path,
	       std::vector<std::unique_ptr<MetaItemInner> > seq)
    : path (std::move (path)), seq (std::move (seq))
  {}

  // copy constructor with vector clone
  MetaItemSeq (const MetaItemSeq &other) : path (other.path)
  {
    seq.reserve (other.seq.size ());
    for (const auto &e : other.seq)
      seq.push_back (e->clone_meta_item_inner ());
  }

  // overloaded assignment operator with vector clone
  MetaItemSeq &operator= (const MetaItemSeq &other)
  {
    MetaItem::operator= (other);
    path = other.path;

    seq.reserve (other.seq.size ());
    for (const auto &e : other.seq)
      seq.push_back (e->clone_meta_item_inner ());

    return *this;
  }

  // default move constructors
  MetaItemSeq (MetaItemSeq &&other) = default;
  MetaItemSeq &operator= (MetaItemSeq &&other) = default;

  std::string as_string () const override;

  void accept_vis (ASTVisitor &vis) override;

  bool check_cfg_predicate (const Session &session) const override;

  Attribute to_attribute () const override;

protected:
  // Use covariance to implement clone function as returning this type
  MetaItemSeq *clone_meta_item_inner_impl () const override
  {
    return new MetaItemSeq (*this);
  }
};

// Preferred specialisation for single-identifier meta items.
class MetaWord : public MetaItem
{
  Identifier ident;
  Location ident_locus;

public:
  MetaWord (Identifier ident, Location ident_locus)
    : ident (std::move (ident)), ident_locus (ident_locus)
  {}

  std::string as_string () const override { return ident; }

  void accept_vis (ASTVisitor &vis) override;

  bool check_cfg_predicate (const Session &session) const override;

  Attribute to_attribute () const override;

protected:
  // Use covariance to implement clone function as returning this type
  MetaWord *clone_meta_item_inner_impl () const override
  {
    return new MetaWord (*this);
  }
};

// Preferred specialisation for "identifier '=' string literal" meta items.
class MetaNameValueStr : public MetaItem
{
  Identifier ident;
  Location ident_locus;

  // NOTE: str stored without quotes
  std::string str;
  Location str_locus;

public:
  MetaNameValueStr (Identifier ident, Location ident_locus, std::string str,
		    Location str_locus)
    : ident (std::move (ident)), ident_locus (ident_locus),
      str (std::move (str)), str_locus (str_locus)
  {}

  std::string as_string () const override
  {
    return ident + " = \"" + str + "\"";
  }

  void accept_vis (ASTVisitor &vis) override;

  // HACK: used to simplify parsing - creates a copy of this
  std::unique_ptr<MetaNameValueStr> to_meta_name_value_str () const override
  {
    return std::unique_ptr<MetaNameValueStr> (clone_meta_item_inner_impl ());
  }

  bool check_cfg_predicate (const Session &session) const override;

  Attribute to_attribute () const override;

protected:
  // Use covariance to implement clone function as returning this type
  MetaNameValueStr *clone_meta_item_inner_impl () const override
  {
    return new MetaNameValueStr (*this);
  }
};

// doubles up as MetaListIdents - determine via iterating through each path?
// Preferred specialisation for "identifier '(' SimplePath, SimplePath, ... ')'"
class MetaListPaths : public MetaItem
{
  Identifier ident;
  Location ident_locus;
  std::vector<SimplePath> paths;

public:
  MetaListPaths (Identifier ident, Location ident_locus,
		 std::vector<SimplePath> paths)
    : ident (std::move (ident)), ident_locus (ident_locus),
      paths (std::move (paths))
  {}

  std::string as_string () const override;

  void accept_vis (ASTVisitor &vis) override;

  bool check_cfg_predicate (const Session &session) const override;

  Attribute to_attribute () const override;

private:
  bool check_path_exists_in_cfg (const Session &session,
				 const SimplePath &path) const;

protected:
  // Use covariance to implement clone function as returning this type
  MetaListPaths *clone_meta_item_inner_impl () const override
  {
    return new MetaListPaths (*this);
  }
};

// Preferred specialisation for "identifier '(' MetaNameValueStr, ... ')'"
class MetaListNameValueStr : public MetaItem
{
  Identifier ident;
  Location ident_locus;
  std::vector<MetaNameValueStr> strs;

public:
  MetaListNameValueStr (Identifier ident, Location ident_locus,
			std::vector<MetaNameValueStr> strs)
    : ident (std::move (ident)), ident_locus (ident_locus),
      strs (std::move (strs))
  {}

  std::string as_string () const override;

  void accept_vis (ASTVisitor &vis) override;

  bool check_cfg_predicate (const Session &session) const override;

  Attribute to_attribute () const override;

protected:
  // Use covariance to implement clone function as returning this type
  MetaListNameValueStr *clone_meta_item_inner_impl () const override
  {
    return new MetaListNameValueStr (*this);
  }
};

// Object that parses macros from a token stream.
/* TODO: would "AttributeParser" be a better name? MetaItems are only for
 * attributes, I believe */
struct AttributeParser
{
private:
  // TODO: might as well rewrite to use lexer tokens
  std::vector<std::unique_ptr<Token> > token_stream;
  int stream_pos;

public:
  AttributeParser (std::vector<std::unique_ptr<Token> > token_stream,
		   int stream_start_pos = 0)
    : token_stream (std::move (token_stream)), stream_pos (stream_start_pos)
  {}

  ~AttributeParser () = default;

  std::vector<std::unique_ptr<MetaItemInner> > parse_meta_item_seq ();

private:
  // Parses a MetaItemInner.
  std::unique_ptr<MetaItemInner> parse_meta_item_inner ();
  // Returns whether token can end a meta item.
  bool is_end_meta_item_tok (TokenId id) const;
  // Parses a simple path.
  SimplePath parse_simple_path ();
  // Parses a segment of a simple path (but not scope resolution operator).
  SimplePathSegment parse_simple_path_segment ();
  // Parses a MetaItemLitExpr.
  std::unique_ptr<MetaItemLitExpr> parse_meta_item_lit ();
  // Parses a literal.
  Literal parse_literal ();
  // Parses a meta item that begins with a simple path.
  std::unique_ptr<MetaItem> parse_path_meta_item ();

  // TODO: should this be const?
  std::unique_ptr<Token> &peek_token (int i = 0)
  {
    return token_stream[stream_pos + i];
  }

  void skip_token (int i = 0) { stream_pos += 1 + i; }
};
} // namespace AST
} // namespace Rust

/* <https://stackoverflow.com/a/35304501> */
namespace std {
template <> struct hash<Rust::AST::MacroFragSpec::Kind>
{
  size_t operator() (const Rust::AST::MacroFragSpec::Kind &t) const noexcept
  {
    return size_t (t);
  }
};
} // namespace std

#endif

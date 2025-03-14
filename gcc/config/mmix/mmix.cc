/* Definitions of target machine for GNU compiler, for MMIX.
   Copyright (C) 2000-2022 Free Software Foundation, Inc.
   Contributed by Hans-Peter Nilsson (hp@bitrange.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#define IN_TARGET_CODE 1

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "stringpool.h"
#include "attribs.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "optabs.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "diagnostic-core.h"
#include "output.h"
#include "varasm.h"
#include "stor-layout.h"
#include "calls.h"
#include "explow.h"
#include "expr.h"
#include "dwarf2.h"
#include "tm-constrs.h"
#include "builtins.h"

/* This file should be included last.  */
#include "target-def.h"

/* First some local helper definitions.  */
#define MMIX_FIRST_GLOBAL_REGNUM 32

/* We'd need a current_function_has_landing_pad.  It's marked as such when
   a nonlocal_goto_receiver is expanded.  Not just a C++ thing, but
   mostly.  */
#define MMIX_CFUN_HAS_LANDING_PAD (cfun->machine->has_landing_pad != 0)

/* We have no means to tell DWARF 2 about the register stack, so we need
   to store the return address on the stack if an exception can get into
   this function.  We'll have an "initial value" recorded for the
   return-register if we've seen a call instruction emitted.  This note
   will be inaccurate before instructions are emitted, but the only caller
   at that time is looking for modulo from stack-boundary, to which the
   return-address does not contribute, and which is always 0 for MMIX
   anyway.  Beware of calling leaf_function_p here, as it'll abort if
   called within a sequence.  */
#define MMIX_CFUN_NEEDS_SAVED_EH_RETURN_ADDRESS			\
 (flag_exceptions						\
  && has_hard_reg_initial_val (Pmode, MMIX_INCOMING_RETURN_ADDRESS_REGNUM))

#define IS_MMIX_EH_RETURN_DATA_REG(REGNO)	\
 (crtl->calls_eh_return		\
  && (EH_RETURN_DATA_REGNO (0) == REGNO		\
      || EH_RETURN_DATA_REGNO (1) == REGNO	\
      || EH_RETURN_DATA_REGNO (2) == REGNO	\
      || EH_RETURN_DATA_REGNO (3) == REGNO))

/* For the default ABI, we rename registers at output-time to fill the gap
   between the (statically partitioned) saved registers and call-clobbered
   registers.  In effect this makes unused call-saved registers to be used
   as call-clobbered registers.  The benefit comes from keeping the number
   of local registers (value of rL) low, since there's a cost of
   increasing rL and clearing unused (unset) registers with lower numbers.
   Don't translate while outputting the prologue.  */
#define MMIX_OUTPUT_REGNO(N)					\
 (TARGET_ABI_GNU 						\
  || (int) (N) < MMIX_RETURN_VALUE_REGNUM			\
  || (int) (N) > MMIX_LAST_STACK_REGISTER_REGNUM		\
  || cfun == NULL 						\
  || cfun->machine == NULL 					\
  || cfun->machine->in_prologue					\
  ? (N) : ((N) - MMIX_RETURN_VALUE_REGNUM			\
	   + cfun->machine->highest_saved_stack_register + 1))

/* The %d in "POP %d,0".  */
#define MMIX_POP_ARGUMENT()						\
 ((! TARGET_ABI_GNU							\
   && crtl->return_rtx != NULL				\
   && ! cfun->returns_struct)				\
  ? (GET_CODE (crtl->return_rtx) == PARALLEL			\
     ? GET_NUM_ELEM (XVEC (crtl->return_rtx, 0)) : 1)	\
  : 0)

/* The canonical saved comparison operands for non-cc0 machines, set in
   the compare expander.  */
rtx mmix_compare_op0;
rtx mmix_compare_op1;

/* Declarations of locals.  */

/* Intermediate for insn output.  */
static int mmix_output_destination_register;

static void mmix_option_override (void);
static void mmix_asm_output_source_filename (FILE *, const char *);
static void mmix_output_shiftvalue_op_from_str
  (FILE *, const char *, int64_t);
static void mmix_output_shifted_value (FILE *, int64_t);
static void mmix_output_condition (FILE *, const_rtx, int);
static void mmix_output_octa (FILE *, int64_t, int);
static bool mmix_assemble_integer (rtx, unsigned int, int);
static struct machine_function *mmix_init_machine_status (void);
static void mmix_encode_section_info (tree, rtx, int);
static const char *mmix_strip_name_encoding (const char *);
static void mmix_emit_sp_add (HOST_WIDE_INT offset);
static void mmix_target_asm_function_prologue (FILE *);
static void mmix_target_asm_function_end_prologue (FILE *);
static void mmix_target_asm_function_epilogue (FILE *);
static reg_class_t mmix_preferred_reload_class (rtx, reg_class_t);
static reg_class_t mmix_preferred_output_reload_class (rtx, reg_class_t);
static bool mmix_legitimate_address_p (machine_mode, rtx, bool);
static bool mmix_legitimate_constant_p (machine_mode, rtx);
static void mmix_reorg (void);
static void mmix_asm_output_mi_thunk
  (FILE *, tree, HOST_WIDE_INT, HOST_WIDE_INT, tree);
static void mmix_setup_incoming_varargs
  (cumulative_args_t, const function_arg_info &, int *, int);
static void mmix_file_start (void);
static void mmix_file_end (void);
static void mmix_init_libfuncs (void);
static bool mmix_rtx_costs (rtx, machine_mode, int, int, int *, bool);
static int mmix_register_move_cost (machine_mode,
				    reg_class_t, reg_class_t);
static rtx mmix_struct_value_rtx (tree, int);
static machine_mode mmix_promote_function_mode (const_tree,
						     machine_mode,
	                                             int *, const_tree, int);
static void mmix_function_arg_advance (cumulative_args_t,
				       const function_arg_info &);
static rtx mmix_function_incoming_arg (cumulative_args_t,
				       const function_arg_info &);
static rtx mmix_function_arg (cumulative_args_t, const function_arg_info &);
static rtx mmix_function_value (const_tree, const_tree, bool);
static rtx mmix_libcall_value (machine_mode, const_rtx);
static bool mmix_function_value_regno_p (const unsigned int);
static bool mmix_pass_by_reference (cumulative_args_t,
				    const function_arg_info &);
static bool mmix_frame_pointer_required (void);
static void mmix_asm_trampoline_template (FILE *);
static void mmix_trampoline_init (rtx, tree, rtx);
static void mmix_print_operand (FILE *, rtx, int);
static void mmix_print_operand_address (FILE *, machine_mode, rtx);
static bool mmix_print_operand_punct_valid_p (unsigned char);
static void mmix_conditional_register_usage (void);
static HOST_WIDE_INT mmix_static_rtx_alignment (machine_mode);
static HOST_WIDE_INT mmix_constant_alignment (const_tree, HOST_WIDE_INT);
static HOST_WIDE_INT mmix_starting_frame_offset (void);

/* Target structure macros.  Listed by node.  See `Using and Porting GCC'
   for a general description.  */

/* Node: Function Entry */

#undef TARGET_ASM_BYTE_OP
#define TARGET_ASM_BYTE_OP NULL
#undef TARGET_ASM_ALIGNED_HI_OP
#define TARGET_ASM_ALIGNED_HI_OP NULL
#undef TARGET_ASM_ALIGNED_SI_OP
#define TARGET_ASM_ALIGNED_SI_OP NULL
#undef TARGET_ASM_ALIGNED_DI_OP
#define TARGET_ASM_ALIGNED_DI_OP NULL
#undef TARGET_ASM_INTEGER
#define TARGET_ASM_INTEGER mmix_assemble_integer

#undef TARGET_ASM_FUNCTION_PROLOGUE
#define TARGET_ASM_FUNCTION_PROLOGUE mmix_target_asm_function_prologue

#undef TARGET_ASM_FUNCTION_END_PROLOGUE
#define TARGET_ASM_FUNCTION_END_PROLOGUE mmix_target_asm_function_end_prologue

#undef TARGET_ASM_FUNCTION_EPILOGUE
#define TARGET_ASM_FUNCTION_EPILOGUE mmix_target_asm_function_epilogue

#undef TARGET_PRINT_OPERAND
#define TARGET_PRINT_OPERAND mmix_print_operand
#undef TARGET_PRINT_OPERAND_ADDRESS
#define TARGET_PRINT_OPERAND_ADDRESS mmix_print_operand_address
#undef TARGET_PRINT_OPERAND_PUNCT_VALID_P
#define TARGET_PRINT_OPERAND_PUNCT_VALID_P mmix_print_operand_punct_valid_p

#undef TARGET_ENCODE_SECTION_INFO
#define TARGET_ENCODE_SECTION_INFO  mmix_encode_section_info
#undef TARGET_STRIP_NAME_ENCODING
#define TARGET_STRIP_NAME_ENCODING  mmix_strip_name_encoding

#undef TARGET_ASM_OUTPUT_MI_THUNK
#define TARGET_ASM_OUTPUT_MI_THUNK mmix_asm_output_mi_thunk
#undef TARGET_ASM_CAN_OUTPUT_MI_THUNK
#define TARGET_ASM_CAN_OUTPUT_MI_THUNK default_can_output_mi_thunk_no_vcall
#undef TARGET_ASM_FILE_START
#define TARGET_ASM_FILE_START mmix_file_start
#undef TARGET_ASM_FILE_START_FILE_DIRECTIVE
#define TARGET_ASM_FILE_START_FILE_DIRECTIVE true
#undef TARGET_ASM_FILE_END
#define TARGET_ASM_FILE_END mmix_file_end
#undef TARGET_ASM_OUTPUT_SOURCE_FILENAME
#define TARGET_ASM_OUTPUT_SOURCE_FILENAME mmix_asm_output_source_filename

#undef TARGET_ASM_OUTPUT_IDENT
#define TARGET_ASM_OUTPUT_IDENT default_asm_output_ident_directive

#undef TARGET_INIT_LIBFUNCS
#define TARGET_INIT_LIBFUNCS mmix_init_libfuncs

#undef TARGET_CONDITIONAL_REGISTER_USAGE
#define TARGET_CONDITIONAL_REGISTER_USAGE mmix_conditional_register_usage

#undef TARGET_HAVE_SPECULATION_SAFE_VALUE
#define TARGET_HAVE_SPECULATION_SAFE_VALUE speculation_safe_value_not_needed

#undef TARGET_RTX_COSTS
#define TARGET_RTX_COSTS mmix_rtx_costs
#undef TARGET_ADDRESS_COST
#define TARGET_ADDRESS_COST hook_int_rtx_mode_as_bool_0

#undef TARGET_REGISTER_MOVE_COST
#define TARGET_REGISTER_MOVE_COST mmix_register_move_cost

#undef TARGET_MACHINE_DEPENDENT_REORG
#define TARGET_MACHINE_DEPENDENT_REORG mmix_reorg

#undef TARGET_PROMOTE_FUNCTION_MODE
#define TARGET_PROMOTE_FUNCTION_MODE mmix_promote_function_mode

#undef TARGET_FUNCTION_VALUE
#define TARGET_FUNCTION_VALUE mmix_function_value
#undef TARGET_LIBCALL_VALUE
#define TARGET_LIBCALL_VALUE mmix_libcall_value
#undef TARGET_FUNCTION_VALUE_REGNO_P
#define TARGET_FUNCTION_VALUE_REGNO_P mmix_function_value_regno_p

#undef TARGET_FUNCTION_ARG
#define TARGET_FUNCTION_ARG mmix_function_arg
#undef TARGET_FUNCTION_INCOMING_ARG
#define TARGET_FUNCTION_INCOMING_ARG mmix_function_incoming_arg
#undef TARGET_FUNCTION_ARG_ADVANCE
#define TARGET_FUNCTION_ARG_ADVANCE mmix_function_arg_advance
#undef TARGET_STRUCT_VALUE_RTX
#define TARGET_STRUCT_VALUE_RTX mmix_struct_value_rtx
#undef TARGET_SETUP_INCOMING_VARARGS
#define TARGET_SETUP_INCOMING_VARARGS mmix_setup_incoming_varargs
#undef TARGET_PASS_BY_REFERENCE
#define TARGET_PASS_BY_REFERENCE mmix_pass_by_reference
#undef TARGET_CALLEE_COPIES
#define TARGET_CALLEE_COPIES hook_bool_CUMULATIVE_ARGS_arg_info_true

#undef TARGET_PREFERRED_RELOAD_CLASS
#define TARGET_PREFERRED_RELOAD_CLASS mmix_preferred_reload_class
#undef TARGET_PREFERRED_OUTPUT_RELOAD_CLASS
#define TARGET_PREFERRED_OUTPUT_RELOAD_CLASS mmix_preferred_output_reload_class

#undef TARGET_LRA_P
#define TARGET_LRA_P hook_bool_void_false

#undef TARGET_LEGITIMATE_ADDRESS_P
#define TARGET_LEGITIMATE_ADDRESS_P	mmix_legitimate_address_p
#undef TARGET_LEGITIMATE_CONSTANT_P
#define TARGET_LEGITIMATE_CONSTANT_P	mmix_legitimate_constant_p

#undef TARGET_FRAME_POINTER_REQUIRED
#define TARGET_FRAME_POINTER_REQUIRED mmix_frame_pointer_required

#undef TARGET_ASM_TRAMPOLINE_TEMPLATE
#define TARGET_ASM_TRAMPOLINE_TEMPLATE mmix_asm_trampoline_template
#undef TARGET_TRAMPOLINE_INIT
#define TARGET_TRAMPOLINE_INIT mmix_trampoline_init

#undef TARGET_OPTION_OVERRIDE
#define TARGET_OPTION_OVERRIDE mmix_option_override

#undef TARGET_STATIC_RTX_ALIGNMENT
#define TARGET_STATIC_RTX_ALIGNMENT mmix_static_rtx_alignment
#undef TARGET_CONSTANT_ALIGNMENT
#define TARGET_CONSTANT_ALIGNMENT mmix_constant_alignment

#undef TARGET_STARTING_FRAME_OFFSET
#define TARGET_STARTING_FRAME_OFFSET mmix_starting_frame_offset

struct gcc_target targetm = TARGET_INITIALIZER;

/* Functions that are expansions for target macros.
   See Target Macros in `Using and Porting GCC'.  */

/* TARGET_OPTION_OVERRIDE.  */

static void
mmix_option_override (void)
{
  /* Should we err or should we warn?  Hmm.  At least we must neutralize
     it.  For example the wrong kind of case-tables will be generated with
     PIC; we use absolute address items for mmixal compatibility.  FIXME:
     They could be relative if we just elide them to after all pertinent
     labels.  */
  if (flag_pic)
    {
      warning (0, "%<-f%s%> not supported: ignored",
	       (flag_pic > 1) ? "PIC" : "pic");
      flag_pic = 0;
    }

  /* Don't bother with mmixal-compatible syntax if it's likely that a
     certain format of the assembly is expected, like no new-line
     after the .byte (or BYTE) parameter, when scanning debug-info
     output, as happens in many places in the gcc testsuite.  The
     dwarf2 output code (maybe others) takes a shortcut based on the
     presence of certain assembler directives, instead of calling
     assemble_integer.  Not worthwhile editing the test-cases:
     mixed-syntax assembly output already looks too ugly for the
     intent of being readable, and the resulting mix certainly fails
     the intent of being compatible with mmixal.  See
     varasm.cc:default_file_start for this triple.  See also
     mmix_assemble_integer.  */
  if (flag_verbose_asm || flag_debug_asm || flag_dump_rtl_in_asm)
    {
      /* "Reinstate" the defaults from target-def.h that we
	 overrode.  */
      targetm.asm_out.byte_op = "\t.byte\t";
      targetm.asm_out.aligned_op.hi = "\t.short\t";
      targetm.asm_out.aligned_op.si = "\t.long\t";

      /* Note that TARGET_ASM_ALIGNED_DI_OP is default NULL, so
	 there's nothing to "reinstate".  Still, we add the universal
	 default (with "recent" gas) for an address.  */
      targetm.asm_out.aligned_op.di = "\t.dc.a\t";
    }
}

/* INIT_EXPANDERS.  */

void
mmix_init_expanders (void)
{
  init_machine_status = mmix_init_machine_status;
}

/* Set the per-function data.  */

static struct machine_function *
mmix_init_machine_status (void)
{
  return ggc_cleared_alloc<machine_function> ();
}

/* DATA_ABI_ALIGNMENT.
   We have trouble getting the address of stuff that is located at other
   than 32-bit alignments (GETA requirements), so try to give everything
   at least 32-bit alignment.  */

int
mmix_data_alignment (tree type ATTRIBUTE_UNUSED, int basic_align)
{
  if (basic_align < 32)
    return 32;

  return basic_align;
}

/* Implement TARGET_STATIC_RTX_ALIGNMENT.  */

static HOST_WIDE_INT
mmix_static_rtx_alignment (machine_mode mode)
{
  return MAX (GET_MODE_ALIGNMENT (mode), 32);
}

/* Implement tARGET_CONSTANT_ALIGNMENT.  */

static HOST_WIDE_INT
mmix_constant_alignment (const_tree, HOST_WIDE_INT basic_align)
{
  if (basic_align < 32)
    return 32;

  return basic_align;
}

/* LOCAL_ALIGNMENT.  */

unsigned
mmix_local_alignment (tree type ATTRIBUTE_UNUSED, unsigned basic_align)
{
  if (basic_align < 32)
    return 32;

  return basic_align;
}

/* TARGET_CONDITIONAL_REGISTER_USAGE.  */

static void
mmix_conditional_register_usage (void)
{
  int i;

  if (TARGET_ABI_GNU)
    {
      static const int gnu_abi_reg_alloc_order[]
	= MMIX_GNU_ABI_REG_ALLOC_ORDER;

      for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
	reg_alloc_order[i] = gnu_abi_reg_alloc_order[i];

      /* Change the default from the mmixware ABI.  For the GNU ABI,
	 $15..$30 are call-saved just as $0..$14.  There must be one
	 call-clobbered local register for the "hole" that holds the
	 number of saved local registers saved by PUSHJ/PUSHGO during the
	 function call, receiving the return value at return.  So best is
	 to use the highest, $31.  It's already marked call-clobbered for
	 the mmixware ABI.  */
      for (i = 15; i <= 30; i++)
	call_used_regs[i] = 0;

      /* "Unfix" the parameter registers.  */
      for (i = MMIX_RESERVED_GNU_ARG_0_REGNUM;
	   i < MMIX_RESERVED_GNU_ARG_0_REGNUM + MMIX_MAX_ARGS_IN_REGS;
	   i++)
	fixed_regs[i] = 0;
    }

  /* Step over the ":" in special register names.  */
  if (! TARGET_TOPLEVEL_SYMBOLS)
    for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
      if (reg_names[i][0] == ':')
	reg_names[i]++;
}

/* INCOMING_REGNO and OUTGOING_REGNO worker function.
   Those two macros must only be applied to function argument
   registers and the function return value register for the opposite
   use.  FIXME: for their current use in gcc, it'd be better with an
   explicit specific additional FUNCTION_INCOMING_ARG_REGNO_P a'la
   TARGET_FUNCTION_ARG / TARGET_FUNCTION_INCOMING_ARG instead of
   forcing the target to commit to a fixed mapping and for any
   unspecified register use.  Particularly when thinking about the
   return-value, it is better to imagine INCOMING_REGNO and
   OUTGOING_REGNO as named CALLEE_TO_CALLER_REGNO and INNER_REGNO as
   named CALLER_TO_CALLEE_REGNO because the direction.  The "incoming"
   and "outgoing" is from the perspective of the parameter-registers,
   but the same macro is (must be, lacking an alternative like
   suggested above) used to map the return-value-register from the
   same perspective.  To make directions even more confusing, the macro
   MMIX_OUTGOING_RETURN_VALUE_REGNUM holds the number of the register
   in which to return a value, i.e. INCOMING_REGNO for the return-value-
   register as received from a called function; the return-value on the
   way out.  */

int
mmix_opposite_regno (int regno, int incoming)
{
  if (incoming && regno == MMIX_OUTGOING_RETURN_VALUE_REGNUM)
    return MMIX_RETURN_VALUE_REGNUM;

  if (!incoming && regno == MMIX_RETURN_VALUE_REGNUM)
    return MMIX_OUTGOING_RETURN_VALUE_REGNUM;

  if (!mmix_function_arg_regno_p (regno, incoming))
    return regno;

  return
    regno - (incoming
	     ? MMIX_FIRST_INCOMING_ARG_REGNUM - MMIX_FIRST_ARG_REGNUM
	     : MMIX_FIRST_ARG_REGNUM - MMIX_FIRST_INCOMING_ARG_REGNUM);
}

/* LOCAL_REGNO.
   All registers that are part of the register stack and that will be
   saved are local.  */

int
mmix_local_regno (int regno)
{
  return (regno <= MMIX_LAST_STACK_REGISTER_REGNUM
	  && !call_used_or_fixed_reg_p (regno));
}

/* TARGET_PREFERRED_RELOAD_CLASS.
   We need to extend the reload class of REMAINDER_REG and HIMULT_REG.  */

static reg_class_t
mmix_preferred_reload_class (rtx x, reg_class_t rclass)
{
  /* FIXME: Revisit.  */
  return GET_CODE (x) == MOD && GET_MODE (x) == DImode
    ? REMAINDER_REG : rclass;
}

/* TARGET_PREFERRED_OUTPUT_RELOAD_CLASS.
   We need to extend the reload class of REMAINDER_REG and HIMULT_REG.  */

static reg_class_t
mmix_preferred_output_reload_class (rtx x, reg_class_t rclass)
{
  /* FIXME: Revisit.  */
  return GET_CODE (x) == MOD && GET_MODE (x) == DImode
    ? REMAINDER_REG : rclass;
}

/* SECONDARY_RELOAD_CLASS.
   We need to reload regs of REMAINDER_REG and HIMULT_REG elsewhere.  */

enum reg_class
mmix_secondary_reload_class (enum reg_class rclass,
			     machine_mode mode ATTRIBUTE_UNUSED,
			     rtx x ATTRIBUTE_UNUSED,
			     int in_p ATTRIBUTE_UNUSED)
{
  if (rclass == REMAINDER_REG
      || rclass == HIMULT_REG
      || rclass == SYSTEM_REGS)
    return GENERAL_REGS;

  return NO_REGS;
}

/* DYNAMIC_CHAIN_ADDRESS.  */

rtx
mmix_dynamic_chain_address (rtx frame)
{
  /* FIXME: the frame-pointer is stored at offset -8 from the current
     frame-pointer.  Unfortunately, the caller assumes that a
     frame-pointer is present for *all* previous frames.  There should be
     a way to say that that cannot be done, like for RETURN_ADDR_RTX.  */
  return plus_constant (Pmode, frame, -8);
}

/* Implement TARGET_STARTING_FRAME_OFFSET.  */

static HOST_WIDE_INT
mmix_starting_frame_offset (void)
{
  /* The old frame pointer is in the slot below the new one, so
     FIRST_PARM_OFFSET does not need to depend on whether the
     frame-pointer is needed or not.  We have to adjust for the register
     stack pointer being located below the saved frame pointer.
     Similarly, we store the return address on the stack too, for
     exception handling, and always if we save the register stack pointer.  */
  return
    (-8
     + (MMIX_CFUN_HAS_LANDING_PAD
	? -16 : (MMIX_CFUN_NEEDS_SAVED_EH_RETURN_ADDRESS ? -8 : 0)));
}

/* RETURN_ADDR_RTX.  */

rtx
mmix_return_addr_rtx (int count, rtx frame ATTRIBUTE_UNUSED)
{
  return count == 0
    ? (MMIX_CFUN_NEEDS_SAVED_EH_RETURN_ADDRESS
       /* FIXME: Set frame_alias_set on the following.  (Why?)
	  See mmix_initial_elimination_offset for the reason we can't use
	  get_hard_reg_initial_val for both.  Always using a stack slot
	  and not a register would be suboptimal.  */
       ? validize_mem (gen_rtx_MEM (Pmode,
				    plus_constant (Pmode,
						   frame_pointer_rtx, -16)))
       : get_hard_reg_initial_val (Pmode, MMIX_INCOMING_RETURN_ADDRESS_REGNUM))
    : NULL_RTX;
}

/* SETUP_FRAME_ADDRESSES.  */

void
mmix_setup_frame_addresses (void)
{
  /* Nothing needed at the moment.  */
}

/* The difference between the (imaginary) frame pointer and the stack
   pointer.  Used to eliminate the frame pointer.  */

int
mmix_initial_elimination_offset (int fromreg, int toreg)
{
  int regno;
  int fp_sp_offset
    = (get_frame_size () + crtl->outgoing_args_size + 7) & ~7;

  /* There is no actual offset between these two virtual values, but for
     the frame-pointer, we have the old one in the stack position below
     it, so the offset for the frame-pointer to the stack-pointer is one
     octabyte larger.  */
  if (fromreg == MMIX_ARG_POINTER_REGNUM
      && toreg == MMIX_FRAME_POINTER_REGNUM)
    return 0;

  /* The difference is the size of local variables plus the size of
     outgoing function arguments that would normally be passed as
     registers but must be passed on stack because we're out of
     function-argument registers.  Only global saved registers are
     counted; the others go on the register stack.

     The frame-pointer is counted too if it is what is eliminated, as we
     need to balance the offset for it from TARGET_STARTING_FRAME_OFFSET.

     Also add in the slot for the register stack pointer we save if we
     have a landing pad.

     Unfortunately, we can't access $0..$14, from unwinder code easily, so
     store the return address in a frame slot too.  FIXME: Only for
     non-leaf functions.  FIXME: Always with a landing pad, because it's
     hard to know whether we need the other at the time we know we need
     the offset for one (and have to state it).  It's a kludge until we
     can express the register stack in the EH frame info.

     We have to do alignment here; get_frame_size will not return a
     multiple of STACK_BOUNDARY.  FIXME: Add note in manual.  */

  for (regno = MMIX_FIRST_GLOBAL_REGNUM;
       regno <= 255;
       regno++)
    if ((df_regs_ever_live_p (regno) && !call_used_or_fixed_reg_p (regno))
	|| IS_MMIX_EH_RETURN_DATA_REG (regno))
      fp_sp_offset += 8;

  return fp_sp_offset
    + (MMIX_CFUN_HAS_LANDING_PAD
       ? 16 : (MMIX_CFUN_NEEDS_SAVED_EH_RETURN_ADDRESS ? 8 : 0))
    + (fromreg == MMIX_ARG_POINTER_REGNUM ? 0 : 8);
}

static void
mmix_function_arg_advance (cumulative_args_t argsp_v,
			   const function_arg_info &arg)
{
  CUMULATIVE_ARGS *argsp = get_cumulative_args (argsp_v);
  int arg_size = MMIX_FUNCTION_ARG_SIZE (arg.mode, arg.type);

  argsp->regs = ((targetm.calls.must_pass_in_stack (arg)
		  || (arg_size > 8
		      && !TARGET_LIBFUNC
		      && !argsp->lib))
		 ? (MMIX_MAX_ARGS_IN_REGS) + 1
		 : argsp->regs + (7 + arg_size) / 8);
}

/* Helper function for mmix_function_arg and mmix_function_incoming_arg.  */

static rtx
mmix_function_arg_1 (const cumulative_args_t argsp_v,
		     const function_arg_info &arg, bool incoming)
{
  CUMULATIVE_ARGS *argsp = get_cumulative_args (argsp_v);

  /* The mode of the argument will be VOIDmode for the "end_marker".  Make sure
     we don't ever generate a VOIDmode register; later passes will barf on that.
     We may want to use the register number, so return something nominally
     useful.  Thus, for VOIDmode, use DImode, being the natural mode for the
     register.  */
  machine_mode mode = arg.mode == VOIDmode ? DImode : arg.mode;

  /* Last-argument marker.  */
  if (arg.end_marker_p ())
    return (argsp->regs < MMIX_MAX_ARGS_IN_REGS)
      ? gen_rtx_REG (mode,
		     (incoming
		      ? MMIX_FIRST_INCOMING_ARG_REGNUM
		      : MMIX_FIRST_ARG_REGNUM) + argsp->regs)
      : NULL_RTX;

  return (argsp->regs < MMIX_MAX_ARGS_IN_REGS
	  && !targetm.calls.must_pass_in_stack (arg)
	  && (GET_MODE_BITSIZE (mode) <= 64
	      || argsp->lib
	      || TARGET_LIBFUNC))
    ? gen_rtx_REG (mode,
		   (incoming
		    ? MMIX_FIRST_INCOMING_ARG_REGNUM
		    : MMIX_FIRST_ARG_REGNUM)
		   + argsp->regs)
    : NULL_RTX;
}

/* Return an rtx for a function argument to go in a register, and 0 for
   one that must go on stack.  */

static rtx
mmix_function_arg (cumulative_args_t argsp, const function_arg_info &arg)
{
  return mmix_function_arg_1 (argsp, arg, false);
}

static rtx
mmix_function_incoming_arg (cumulative_args_t argsp,
			    const function_arg_info &arg)
{
  return mmix_function_arg_1 (argsp, arg, true);
}

/* Returns nonzero for everything that goes by reference, 0 for
   everything that goes by value.  */

static bool
mmix_pass_by_reference (cumulative_args_t argsp_v,
			const function_arg_info &arg)
{
  CUMULATIVE_ARGS *argsp = get_cumulative_args (argsp_v);

  /* FIXME: Check: I'm not sure the must_pass_in_stack check is
     necessary.  */
  if (targetm.calls.must_pass_in_stack (arg))
    return true;

  if (MMIX_FUNCTION_ARG_SIZE (arg.mode, arg.type) > 8
      && !TARGET_LIBFUNC
      && (!argsp || !argsp->lib))
    return true;

  return false;
}

/* Return nonzero if regno is a register number where a parameter is
   passed, and 0 otherwise.  */

int
mmix_function_arg_regno_p (int regno, int incoming)
{
  int first_arg_regnum
    = incoming ? MMIX_FIRST_INCOMING_ARG_REGNUM : MMIX_FIRST_ARG_REGNUM;

  return regno >= first_arg_regnum
    && regno < first_arg_regnum + MMIX_MAX_ARGS_IN_REGS;
}

/* Implements TARGET_FUNCTION_VALUE.  */

static rtx
mmix_function_value (const_tree valtype,
		     const_tree func ATTRIBUTE_UNUSED,
		     bool outgoing)
{
  machine_mode mode = TYPE_MODE (valtype);
  machine_mode cmode;
  int first_val_regnum = MMIX_OUTGOING_RETURN_VALUE_REGNUM;
  rtx vec[MMIX_MAX_REGS_FOR_VALUE];
  int i;
  int nregs;

  if (!outgoing)
    return gen_rtx_REG (mode, MMIX_RETURN_VALUE_REGNUM);
  
  /* Return values that fit in a register need no special handling.
     There's no register hole when parameters are passed in global
     registers.  */
  if (TARGET_ABI_GNU
      || GET_MODE_BITSIZE (mode) <= BITS_PER_WORD)
    return
      gen_rtx_REG (mode, MMIX_OUTGOING_RETURN_VALUE_REGNUM);

  if (COMPLEX_MODE_P (mode))
    /* A complex type, made up of components.  */
    cmode = TYPE_MODE (TREE_TYPE (valtype));
  else
    {
      /* Of the other larger-than-register modes, we only support
	 scalar mode TImode.  (At least, that's the only one that's
	 been rudimentally tested.)  Make sure we're alerted for
	 unexpected cases.  */
      if (mode != TImode)
	sorry ("support for mode %qs", GET_MODE_NAME (mode));

      /* In any case, we will fill registers to the natural size.  */
      cmode = DImode;
    }

  nregs = ((GET_MODE_BITSIZE (mode) + BITS_PER_WORD - 1) / BITS_PER_WORD);

  /* We need to take care of the effect of the register hole on return
     values of large sizes; the last register will appear as the first
     register, with the rest shifted.  (For complex modes, this is just
     swapped registers.)  */

  if (nregs > MMIX_MAX_REGS_FOR_VALUE)
    internal_error ("too large function value type, needs %d registers,\
 have only %d registers for this", nregs, MMIX_MAX_REGS_FOR_VALUE);

  /* FIXME: Maybe we should handle structure values like this too
     (adjusted for BLKmode), perhaps for both ABI:s.  */
  for (i = 0; i < nregs - 1; i++)
    vec[i]
      = gen_rtx_EXPR_LIST (VOIDmode,
			   gen_rtx_REG (cmode, first_val_regnum + i),
			   GEN_INT ((i + 1) * BITS_PER_UNIT));

  vec[nregs - 1]
    = gen_rtx_EXPR_LIST (VOIDmode,
			 gen_rtx_REG (cmode, first_val_regnum + nregs - 1),
			 const0_rtx);

  return gen_rtx_PARALLEL (mode, gen_rtvec_v (nregs, vec));
}

/* Implements TARGET_LIBCALL_VALUE.  */

static rtx
mmix_libcall_value (machine_mode mode,
		    const_rtx fun ATTRIBUTE_UNUSED)
{
  return gen_rtx_REG (mode, MMIX_RETURN_VALUE_REGNUM);
}

/* Implements TARGET_FUNCTION_VALUE_REGNO_P.  */

static bool
mmix_function_value_regno_p (const unsigned int regno)
{
  return regno == MMIX_RETURN_VALUE_REGNUM;
}

/* EH_RETURN_DATA_REGNO. */

int
mmix_eh_return_data_regno (int n)
{
  if (n >= 0 && n < 4)
    return MMIX_EH_RETURN_DATA_REGNO_START + n;

  return INVALID_REGNUM;
}

/* EH_RETURN_STACKADJ_RTX. */

rtx
mmix_eh_return_stackadj_rtx (void)
{
  return gen_rtx_REG (Pmode, MMIX_EH_RETURN_STACKADJ_REGNUM);
}

/* EH_RETURN_HANDLER_RTX.  */

rtx
mmix_eh_return_handler_rtx (void)
{
  return gen_rtx_REG (Pmode, MMIX_INCOMING_RETURN_ADDRESS_REGNUM);
}

/* ASM_PREFERRED_EH_DATA_FORMAT. */

int
mmix_asm_preferred_eh_data_format (int code ATTRIBUTE_UNUSED,
				   int global ATTRIBUTE_UNUSED)
{
  /* This is the default (was at 2001-07-20).  Revisit when needed.  */
  return DW_EH_PE_absptr;
}

/* Make a note that we've seen the beginning of the prologue.  This
   matters to whether we'll translate register numbers as calculated by
   mmix_reorg.  */

static void
mmix_target_asm_function_prologue (FILE *)
{
  cfun->machine->in_prologue = 1;
}

/* Make a note that we've seen the end of the prologue.  */

static void
mmix_target_asm_function_end_prologue (FILE *stream ATTRIBUTE_UNUSED)
{
  cfun->machine->in_prologue = 0;
}

/* Implement TARGET_MACHINE_DEPENDENT_REORG.  No actual rearrangements
   done here; just virtually by calculating the highest saved stack
   register number used to modify the register numbers at output time.  */

static void
mmix_reorg (void)
{
  int regno;

  /* We put the number of the highest saved register-file register in a
     location convenient for the call-patterns to output.  Note that we
     don't tell dwarf2 about these registers, since it can't restore them
     anyway.  */
  for (regno = MMIX_LAST_STACK_REGISTER_REGNUM;
       regno >= 0;
       regno--)
    if ((df_regs_ever_live_p (regno) && !call_used_or_fixed_reg_p (regno))
	|| (regno == MMIX_FRAME_POINTER_REGNUM && frame_pointer_needed))
      break;

  /* Regardless of whether they're saved (they might be just read), we
     mustn't include registers that carry parameters.  We could scan the
     insns to see whether they're actually used (and indeed do other less
     trivial register usage analysis and transformations), but it seems
     wasteful to optimize for unused parameter registers.  As of
     2002-04-30, df_regs_ever_live_p (n) seems to be set for only-reads too, but
     that might change.  */
  if (!TARGET_ABI_GNU && regno < crtl->args.info.regs - 1)
    {
      regno = crtl->args.info.regs - 1;

      /* We don't want to let this cause us to go over the limit and make
	 incoming parameter registers be misnumbered and treating the last
	 parameter register and incoming return value register call-saved.
	 Stop things at the unmodified scheme.  */
      if (regno > MMIX_RETURN_VALUE_REGNUM - 1)
	regno = MMIX_RETURN_VALUE_REGNUM - 1;
    }

  cfun->machine->highest_saved_stack_register = regno;
}

/* TARGET_ASM_FUNCTION_EPILOGUE.  */

static void
mmix_target_asm_function_epilogue (FILE *stream)
{
  /* Emit an \n for readability of the generated assembly.  */
  fputc ('\n', stream);
}

/* TARGET_ASM_OUTPUT_MI_THUNK.  */

static void
mmix_asm_output_mi_thunk (FILE *stream,
			  tree thunk_fndecl ATTRIBUTE_UNUSED,
			  HOST_WIDE_INT delta,
			  HOST_WIDE_INT vcall_offset ATTRIBUTE_UNUSED,
			  tree func)
{
  /* If you define TARGET_STRUCT_VALUE_RTX that returns 0 (i.e. pass
     location of structure to return as invisible first argument), you
     need to tweak this code too.  */
  const char *regname = reg_names[MMIX_FIRST_INCOMING_ARG_REGNUM];
  const char *fnname = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (thunk_fndecl));

  assemble_start_function (thunk_fndecl, fnname);

  if (delta >= 0 && delta < 65536)
    fprintf (stream, "\tINCL %s,%d\n", regname, (int)delta);
  else if (delta < 0 && delta >= -255)
    fprintf (stream, "\tSUBU %s,%s,%d\n", regname, regname, (int)-delta);
  else
    {
      mmix_output_register_setting (stream, 255, delta, 1);
      fprintf (stream, "\tADDU %s,%s,$255\n", regname, regname);
    }

  fprintf (stream, "\tJMP ");
  assemble_name (stream, XSTR (XEXP (DECL_RTL (func), 0), 0));
  fprintf (stream, "\n");
  assemble_end_function (thunk_fndecl, fnname);
}

/* FUNCTION_PROFILER.  */

void
mmix_function_profiler (FILE *stream ATTRIBUTE_UNUSED,
			int labelno ATTRIBUTE_UNUSED)
{
  sorry ("function_profiler support for MMIX");
}

/* Worker function for TARGET_SETUP_INCOMING_VARARGS.  For the moment,
   let's stick to pushing argument registers on the stack.  Later, we
   can parse all arguments in registers, to improve performance.  */

static void
mmix_setup_incoming_varargs (cumulative_args_t args_so_farp_v,
			     const function_arg_info &arg,
			     int *pretend_sizep,
			     int second_time ATTRIBUTE_UNUSED)
{
  CUMULATIVE_ARGS *args_so_farp = get_cumulative_args (args_so_farp_v);

  /* The last named variable has been handled, but
     args_so_farp has not been advanced for it.  */
  if (args_so_farp->regs + 1 < MMIX_MAX_ARGS_IN_REGS)
    *pretend_sizep = (MMIX_MAX_ARGS_IN_REGS - (args_so_farp->regs + 1)) * 8;

  /* We assume that one argument takes up one register here.  That should
     be true until we start messing with multi-reg parameters.  */
  if ((7 + (MMIX_FUNCTION_ARG_SIZE (arg.mode, arg.type))) / 8 != 1)
    internal_error ("MMIX Internal: Last named vararg would not fit in a register");
}

/* TARGET_ASM_TRAMPOLINE_TEMPLATE.  */

static void
mmix_asm_trampoline_template (FILE *stream)
{
  /* Read a value into the static-chain register and jump somewhere.  The
     static chain is stored at offset 16, and the function address is
     stored at offset 24.  */

  fprintf (stream, "\tGETA $255,1F\n\t");
  fprintf (stream, "LDOU %s,$255,0\n\t", reg_names[MMIX_STATIC_CHAIN_REGNUM]);
  fprintf (stream, "LDOU $255,$255,8\n\t");
  fprintf (stream, "GO $255,$255,0\n");
  fprintf (stream, "1H\tOCTA 0\n\t");
  fprintf (stream, "OCTA 0\n");
}

/* TARGET_TRAMPOLINE_INIT.  */
/* Set the static chain and function pointer field in the trampoline.
   We also SYNCID here to be sure (doesn't matter in the simulator, but
   some day it will).  */

static void
mmix_trampoline_init (rtx m_tramp, tree fndecl, rtx static_chain)
{
  rtx fnaddr = XEXP (DECL_RTL (fndecl), 0);
  rtx mem;

  emit_block_move (m_tramp, assemble_trampoline_template (),
		   GEN_INT (2*UNITS_PER_WORD), BLOCK_OP_NORMAL);

  mem = adjust_address (m_tramp, DImode, 2*UNITS_PER_WORD);
  emit_move_insn (mem, static_chain);
  mem = adjust_address (m_tramp, DImode, 3*UNITS_PER_WORD);
  emit_move_insn (mem, fnaddr);

  mem = adjust_address (m_tramp, DImode, 0);
  emit_insn (gen_sync_icache (mem, GEN_INT (TRAMPOLINE_SIZE - 1)));
}

/* We must exclude constant addresses that have an increment that is not a
   multiple of four bytes because of restrictions of the GETA
   instruction, unless TARGET_BASE_ADDRESSES.  */

int
mmix_constant_address_p (rtx x)
{
  RTX_CODE code = GET_CODE (x);
  int addend = 0;
  /* When using "base addresses", anything constant goes.  */
  int constant_ok = TARGET_BASE_ADDRESSES != 0;

  switch (code)
    {
    case LABEL_REF:
    case SYMBOL_REF:
      return 1;

    case HIGH:
      /* FIXME: Don't know how to dissect these.  Avoid them for now,
	 except we know they're constants.  */
      return constant_ok;

    case CONST_INT:
      addend = INTVAL (x);
      break;

    case CONST_DOUBLE:
      if (GET_MODE (x) != VOIDmode)
	/* Strange that we got here.  FIXME: Check if we do.  */
	return constant_ok;
      addend = CONST_DOUBLE_LOW (x);
      break;

    case CONST:
      /* Note that expressions with arithmetic on forward references don't
	 work in mmixal.  People using gcc assembly code with mmixal might
	 need to move arrays and such to before the point of use.  */
      if (GET_CODE (XEXP (x, 0)) == PLUS)
	{
	  rtx x0 = XEXP (XEXP (x, 0), 0);
	  rtx x1 = XEXP (XEXP (x, 0), 1);

	  if ((GET_CODE (x0) == SYMBOL_REF
	       || GET_CODE (x0) == LABEL_REF)
	      && (GET_CODE (x1) == CONST_INT
		  || (GET_CODE (x1) == CONST_DOUBLE
		      && GET_MODE (x1) == VOIDmode)))
	    addend = mmix_intval (x1);
	  else
	    return constant_ok;
	}
      else
	return constant_ok;
      break;

    default:
      return 0;
    }

  return constant_ok || (addend & 3) == 0;
}

/* Return 1 if the address is OK, otherwise 0.  */

bool
mmix_legitimate_address_p (machine_mode mode ATTRIBUTE_UNUSED,
			   rtx x,
			   bool strict_checking)
{
#define MMIX_REG_OK(X)							\
  ((strict_checking							\
    && (REGNO (X) <= MMIX_LAST_GENERAL_REGISTER				\
	|| (reg_renumber[REGNO (X)] > 0					\
	    && reg_renumber[REGNO (X)] <= MMIX_LAST_GENERAL_REGISTER)))	\
   || (!strict_checking							\
       && (REGNO (X) <= MMIX_LAST_GENERAL_REGISTER			\
	   || REGNO (X) >= FIRST_PSEUDO_REGISTER			\
	   || REGNO (X) == ARG_POINTER_REGNUM)))

  /* We only accept:
     (mem reg)
     (mem (plus reg reg))
     (mem (plus reg 0..255)).
     unless TARGET_BASE_ADDRESSES, in which case we accept all
     (mem constant_address) too.  */


    /* (mem reg) */
  if (REG_P (x) && MMIX_REG_OK (x))
    return 1;

  if (GET_CODE(x) == PLUS)
    {
      rtx x1 = XEXP (x, 0);
      rtx x2 = XEXP (x, 1);

      /* Try swapping the order.  FIXME: Do we need this?  */
      if (! REG_P (x1))
	{
	  rtx tem = x1;
	  x1 = x2;
	  x2 = tem;
	}

      /* (mem (plus (reg?) (?))) */
      if (!REG_P (x1) || !MMIX_REG_OK (x1))
	return TARGET_BASE_ADDRESSES && mmix_constant_address_p (x);

      /* (mem (plus (reg) (reg?))) */
      if (REG_P (x2) && MMIX_REG_OK (x2))
	return 1;

      /* (mem (plus (reg) (0..255?))) */
      if (satisfies_constraint_I (x2))
	return 1;

      return 0;
    }

  return TARGET_BASE_ADDRESSES && mmix_constant_address_p (x);
}

/* Implement TARGET_LEGITIMATE_CONSTANT_P.  */

static bool
mmix_legitimate_constant_p (machine_mode mode ATTRIBUTE_UNUSED, rtx x)
{
  RTX_CODE code = GET_CODE (x);

  /* We must allow any number due to the way the cse passes works; if we
     do not allow any number here, general_operand will fail, and insns
     will fatally fail recognition instead of "softly".  */
  if (code == CONST_INT || code == CONST_DOUBLE)
    return 1;

  return CONSTANT_ADDRESS_P (x);
}

/* SELECT_CC_MODE.  */

machine_mode
mmix_select_cc_mode (RTX_CODE op, rtx x, rtx y ATTRIBUTE_UNUSED)
{
  /* We use CCmode, CC_UNSmode, CC_FPmode, CC_FPEQmode and CC_FUNmode to
     output different compare insns.  Note that we do not check the
     validity of the comparison here.  */

  if (GET_MODE_CLASS (GET_MODE (x)) == MODE_FLOAT)
    {
      if (op == ORDERED || op == UNORDERED || op == UNGE
	  || op == UNGT || op == UNLE || op == UNLT)
	return CC_FUNmode;

      if (op == EQ || op == NE)
	return CC_FPEQmode;

      return CC_FPmode;
    }

  if (op == GTU || op == LTU || op == GEU || op == LEU)
    return CC_UNSmode;

  return CCmode;
}

/* REVERSIBLE_CC_MODE.  */

int
mmix_reversible_cc_mode (machine_mode mode)
{
  /* That is, all integer and the EQ, NE, ORDERED and UNORDERED float
     compares.  */
  return mode != CC_FPmode;
}

/* TARGET_RTX_COSTS.  */

static bool
mmix_rtx_costs (rtx x ATTRIBUTE_UNUSED,
		machine_mode mode ATTRIBUTE_UNUSED,
		int outer_code ATTRIBUTE_UNUSED,
		int opno ATTRIBUTE_UNUSED,
		int *total ATTRIBUTE_UNUSED,
		bool speed ATTRIBUTE_UNUSED)
{
  /* For the time being, this is just a stub and we'll accept the
     generic calculations, until we can do measurements, at least.
     Say we did not modify any calculated costs.  */
  return false;
}

/* TARGET_REGISTER_MOVE_COST.

   The special registers can only move to and from general regs, and we
   need to check that their constraints match, so say 3 for them.  */

static int
mmix_register_move_cost (machine_mode mode ATTRIBUTE_UNUSED,
			 reg_class_t from,
			 reg_class_t to)
{
  return (from == GENERAL_REGS && from == to) ? 2 : 3;
}

/* Note that we don't have a TEXT_SECTION_ASM_OP, because it has to be a
   compile-time constant; it's used in an asm in crtstuff.c, compiled for
   the target.  */

/* DATA_SECTION_ASM_OP.  */

const char *
mmix_data_section_asm_op (void)
{
  return "\t.data ! mmixal:= 8H LOC 9B";
}

static void
mmix_encode_section_info (tree decl, rtx rtl, int first)
{
  /* Test for an external declaration, and do nothing if it is one.  */
  if ((TREE_CODE (decl) == VAR_DECL
       && (DECL_EXTERNAL (decl) || TREE_PUBLIC (decl)))
      || (TREE_CODE (decl) == FUNCTION_DECL && TREE_PUBLIC (decl)))
    ;
  else if (first && DECL_P (decl))
    {
      /* For non-visible declarations, add a "@" prefix, which we skip
	 when the label is output.  If the label does not have this
	 prefix, a ":" is output if -mtoplevel-symbols.

	 Note that this does not work for data that is declared extern and
	 later defined as static.  If there's code in between, that code
	 will refer to the extern declaration, and vice versa.  This just
	 means that when -mtoplevel-symbols is in use, we can just handle
	 well-behaved ISO-compliant code.  */

      const char *str = XSTR (XEXP (rtl, 0), 0);
      int len = strlen (str);
      char *newstr = XALLOCAVEC (char, len + 2);
      newstr[0] = '@';
      strcpy (newstr + 1, str);
      XSTR (XEXP (rtl, 0), 0) = ggc_alloc_string (newstr, len + 1);
    }

  /* Set SYMBOL_REF_FLAG for things that we want to access with GETA.  We
     may need different options to reach for different things with GETA.
     For now, functions and things we know or have been told are constant.  */
  if (TREE_CODE (decl) == FUNCTION_DECL
      || TREE_CONSTANT (decl)
      || (TREE_CODE (decl) == VAR_DECL
	  && TREE_READONLY (decl)
	  && !TREE_SIDE_EFFECTS (decl)
	  && (!DECL_INITIAL (decl)
	      || TREE_CONSTANT (DECL_INITIAL (decl)))))
    SYMBOL_REF_FLAG (XEXP (rtl, 0)) = 1;
}

static const char *
mmix_strip_name_encoding (const char *name)
{
  for (; (*name == '@' || *name == '*'); name++)
    ;

  return name;
}

/* TARGET_ASM_FILE_START.
   We just emit a little comment for the time being.  */

static void
mmix_file_start (void)
{
  default_file_start ();

  fputs ("! mmixal:= 8H LOC Data_Section\n", asm_out_file);

  /* Make sure each file starts with the text section.  */
  switch_to_section (text_section);
}

/* TARGET_ASM_FILE_END.  */

static void
mmix_file_end (void)
{
  /* Make sure each file ends with the data section.  */
  switch_to_section (data_section);
}

/* TARGET_ASM_OUTPUT_SOURCE_FILENAME.  */

static void
mmix_asm_output_source_filename (FILE *stream, const char *name)
{
  fprintf (stream, "# 1 ");
  OUTPUT_QUOTED_STRING (stream, name);
  fprintf (stream, "\n");
}

/* Unfortunately, by default __builtin_ffs is expanded to ffs for
   targets where INT_TYPE_SIZE < BITS_PER_WORD.  That together with
   newlib since 2017-07-04 implementing ffs as __builtin_ffs leads to
   (newlib) ffs recursively calling itself.  But, because of argument
   promotion, and with ffs we're counting from the least bit, the
   libgcc equivalent for ffsl works equally well for int arguments, so
   just use that.  */

static void
mmix_init_libfuncs (void)
{
  set_optab_libfunc (ffs_optab, SImode, "__ffsdi2");
}

/* OUTPUT_QUOTED_STRING.  */

void
mmix_output_quoted_string (FILE *stream, const char *string, int length)
{
  const char * string_end = string + length;
  static const char *const unwanted_chars = "\"[]\\";

  /* Output "any character except newline and double quote character".  We
     play it safe and avoid all control characters too.  We also do not
     want [] as characters, should input be passed through m4 with [] as
     quotes.  Further, we avoid "\", because the GAS port handles it as a
     quoting character.  */
  while (string < string_end)
    {
      if (*string
	  && (unsigned char) *string < 128
	  && !ISCNTRL (*string)
	  && strchr (unwanted_chars, *string) == NULL)
	{
	  fputc ('"', stream);
	  while (*string
		 && (unsigned char) *string < 128
		 && !ISCNTRL (*string)
		 && strchr (unwanted_chars, *string) == NULL
		 && string < string_end)
	    {
	      fputc (*string, stream);
	      string++;
	    }
	  fputc ('"', stream);
	  if (string < string_end)
	    fprintf (stream, ",");
	}
      if (string < string_end)
	{
	  fprintf (stream, "#%x", *string & 255);
	  string++;
	  if (string < string_end)
	    fprintf (stream, ",");
	}
    }
}

/* Target hook for assembling integer objects.  Use mmix_print_operand
   for WYDE and TETRA.  Use mmix_output_octa to output 8-byte
   CONST_DOUBLEs.  */

static bool
mmix_assemble_integer (rtx x, unsigned int size, int aligned_p)
{
  if (aligned_p)
    switch (size)
      {
	/* We handle a limited number of types of operands in here.  But
	   that's ok, because we can punt to generic functions.  We then
	   pretend that aligned data isn't needed, so the usual .<pseudo>
	   syntax is used (which works for aligned data too).  We actually
	   *must* do that, since we (usually) say we don't have simple aligned
	   pseudos, causing this function to be called.  See
	   mmix_option_override for an exception.  We just try and keep as
	   much compatibility as possible with mmixal syntax for normal
	   cases (i.e. without GNU extensions and C only).  */
      case 1:
	if (GET_CODE (x) != CONST_INT)
	  {
	    /* There is no "unaligned byte" op or generic function to
	       which we can punt, so we have to handle this here.  As
	       the expression isn't a plain literal, the generated
	       assembly-code can't be mmixal-equivalent (i.e. "BYTE"
	       won't work) and thus it's ok to emit the default op
	       ".byte". */
	    assemble_integer_with_op ("\t.byte\t", x);
	    return true;
	  }
	fputs ("\tBYTE\t", asm_out_file);
	mmix_print_operand (asm_out_file, x, 'B');
	fputc ('\n', asm_out_file);
	return true;

      case 2:
	if (GET_CODE (x) != CONST_INT)
	  {
	    aligned_p = 0;
	    break;
	  }
	fputs ("\tWYDE\t", asm_out_file);
	mmix_print_operand (asm_out_file, x, 'W');
	fputc ('\n', asm_out_file);
	return true;

      case 4:
	if (GET_CODE (x) != CONST_INT)
	  {
	    aligned_p = 0;
	    break;
	  }
	fputs ("\tTETRA\t", asm_out_file);
	mmix_print_operand (asm_out_file, x, 'L');
	fputc ('\n', asm_out_file);
	return true;

      case 8:
	/* We don't get here anymore for CONST_DOUBLE, because DImode
	   isn't expressed as CONST_DOUBLE, and DFmode is handled
	   elsewhere.  */
	gcc_assert (GET_CODE (x) != CONST_DOUBLE);
	assemble_integer_with_op ("\tOCTA\t", x);
	return true;
      }
  return default_assemble_integer (x, size, aligned_p);
}

/* ASM_OUTPUT_ASCII.  */

void
mmix_asm_output_ascii (FILE *stream, const char *string, int length)
{
  while (length > 0)
    {
      int chunk_size = length > 60 ? 60 : length;
      fprintf (stream, "\tBYTE ");
      mmix_output_quoted_string (stream, string, chunk_size);
      string += chunk_size;
      length -= chunk_size;
      fprintf (stream, "\n");
    }
}

/* ASM_OUTPUT_ALIGNED_COMMON.  */

void
mmix_asm_output_aligned_common (FILE *stream,
				const char *name,
				int size,
				int align)
{
  /* This is mostly the elfos.h one.  There doesn't seem to be a way to
     express this in a mmixal-compatible way.  */
  fprintf (stream, "\t.comm\t");
  assemble_name (stream, name);
  fprintf (stream, ",%u,%u ! mmixal-incompatible COMMON\n",
	   size, align / BITS_PER_UNIT);
}

/* ASM_OUTPUT_ALIGNED_LOCAL.  */

void
mmix_asm_output_aligned_local (FILE *stream,
			       const char *name,
			       int size,
			       int align)
{
  switch_to_section (data_section);

  ASM_OUTPUT_ALIGN (stream, exact_log2 (align/BITS_PER_UNIT));
  assemble_name (stream, name);
  fprintf (stream, "\tLOC @+%d\n", size);
}

/* ASM_OUTPUT_LABEL.  */

void
mmix_asm_output_label (FILE *stream, const char *name)
{
  assemble_name (stream, name);
  fprintf (stream, "\tIS @\n");
}

/* ASM_OUTPUT_INTERNAL_LABEL.  */

void
mmix_asm_output_internal_label (FILE *stream, const char *name)
{
  assemble_name_raw (stream, name);
  fprintf (stream, "\tIS @\n");
}

/* ASM_DECLARE_REGISTER_GLOBAL.  */

void
mmix_asm_declare_register_global (FILE *stream ATTRIBUTE_UNUSED,
				  tree decl ATTRIBUTE_UNUSED,
				  int regno ATTRIBUTE_UNUSED,
				  const char *name ATTRIBUTE_UNUSED)
{
  /* Nothing to do here, but there *will* be, therefore the framework is
     here.  */
}

/* ASM_WEAKEN_LABEL.  */

void
mmix_asm_weaken_label (FILE *stream ATTRIBUTE_UNUSED,
		       const char *name ATTRIBUTE_UNUSED)
{
  fprintf (stream, "\t.weak ");
  assemble_name (stream, name);
  fprintf (stream, " ! mmixal-incompatible\n");
}

/* MAKE_DECL_ONE_ONLY.  */

void
mmix_make_decl_one_only (tree decl)
{
  DECL_WEAK (decl) = 1;
}

/* ASM_OUTPUT_LABELREF.
   Strip GCC's '*' and our own '@'.  No order is assumed.  */

void
mmix_asm_output_labelref (FILE *stream, const char *name)
{
  int is_extern = 1;

  for (; (*name == '@' || *name == '*'); name++)
    if (*name == '@')
      is_extern = 0;

  asm_fprintf (stream, "%s%U%s",
	       is_extern && TARGET_TOPLEVEL_SYMBOLS ? ":" : "",
	       name);
}

/* ASM_OUTPUT_DEF.  */

void
mmix_asm_output_def (FILE *stream, const char *name, const char *value)
{
  assemble_name (stream, name);
  fprintf (stream, "\tIS ");
  assemble_name (stream, value);
  fputc ('\n', stream);
}

/* TARGET_PRINT_OPERAND.  */

static void
mmix_print_operand (FILE *stream, rtx x, int code)
{
  /* When we add support for different codes later, we can, when needed,
     drop through to the main handler with a modified operand.  */
  rtx modified_x = x;
  int regno = x != NULL_RTX && REG_P (x) ? REGNO (x) : 0;

  switch (code)
    {
      /* Unrelated codes are in alphabetic order.  */

    case '+':
      /* For conditional branches, output "P" for a probable branch.  */
      if (TARGET_BRANCH_PREDICT)
	{
	  x = find_reg_note (current_output_insn, REG_BR_PROB, 0);
	  if (x && profile_probability::from_reg_br_prob_note (XINT (x, 0))
	      > profile_probability::even ())
	    putc ('P', stream);
	}
      return;

    case '.':
      /* For the %d in POP %d,0.  */
      fprintf (stream, "%d", MMIX_POP_ARGUMENT ());
      return;

    case '!':
      /* The number of registers we want to save.  This was setup by the
	 prologue.  */
      fprintf (stream, "%d", cfun->machine->highest_saved_stack_register + 1);
      return;

    case 'B':
      if (GET_CODE (x) != CONST_INT)
	fatal_insn ("MMIX Internal: Expected a CONST_INT, not this", x);
      fprintf (stream, "%d", (int) (INTVAL (x) & 0xff));
      return;

    case 'H':
      /* Highpart.  Must be general register, and not the last one, as
	 that one cannot be part of a consecutive register pair.  */
      if (regno > MMIX_LAST_GENERAL_REGISTER - 1)
	internal_error ("MMIX Internal: Bad register: %d", regno);

      /* This is big-endian, so the high-part is the first one.  */
      fprintf (stream, "%s", reg_names[MMIX_OUTPUT_REGNO (regno)]);
      return;

    case 'L':
      /* Lowpart.  Must be CONST_INT or general register, and not the last
	 one, as that one cannot be part of a consecutive register pair.  */
      if (GET_CODE (x) == CONST_INT)
	{
	  fprintf (stream, "#%lx",
		   (unsigned long) (INTVAL (x)
				    & ((unsigned int) 0x7fffffff * 2 + 1)));
	  return;
	}

      if (GET_CODE (x) == SYMBOL_REF)
	{
	  output_addr_const (stream, x);
	  return;
	}

      if (regno > MMIX_LAST_GENERAL_REGISTER - 1)
	internal_error ("MMIX Internal: Bad register: %d", regno);

      /* This is big-endian, so the low-part is + 1.  */
      fprintf (stream, "%s", reg_names[MMIX_OUTPUT_REGNO (regno) + 1]);
      return;

      /* Can't use 'a' because that's a generic modifier for address
	 output.  */
    case 'A':
      mmix_output_shiftvalue_op_from_str (stream, "ANDN",
					  ~(uint64_t)
					  mmix_intval (x));
      return;

    case 'i':
      mmix_output_shiftvalue_op_from_str (stream, "INC",
					  (uint64_t)
					  mmix_intval (x));
      return;

    case 'o':
      mmix_output_shiftvalue_op_from_str (stream, "OR",
					  (uint64_t)
					  mmix_intval (x));
      return;

    case 's':
      mmix_output_shiftvalue_op_from_str (stream, "SET",
					  (uint64_t)
					  mmix_intval (x));
      return;

    case 'd':
    case 'D':
      mmix_output_condition (stream, x, (code == 'D'));
      return;

    case 'e':
      /* Output an extra "e" to make fcmpe, fune.  */
      if (TARGET_FCMP_EPSILON)
	fprintf (stream, "e");
      return;

    case 'm':
      /* Output the number minus 1.  */
      if (GET_CODE (x) != CONST_INT)
	{
	  fatal_insn ("MMIX Internal: Bad value for 'm', not a CONST_INT",
		      x);
	}
      fprintf (stream, "%" PRId64,
	       (int64_t) (mmix_intval (x) - 1));
      return;

    case 'r':
      /* Store the register to output a constant to.  */
      if (! REG_P (x))
	fatal_insn ("MMIX Internal: Expected a register, not this", x);
      mmix_output_destination_register = MMIX_OUTPUT_REGNO (regno);
      return;

    case 'I':
      /* Output the constant.  Note that we use this for floats as well.  */
      if (GET_CODE (x) != CONST_INT
	  && (GET_CODE (x) != CONST_DOUBLE
	      || (GET_MODE (x) != VOIDmode && GET_MODE (x) != DFmode
		  && GET_MODE (x) != SFmode)))
	fatal_insn ("MMIX Internal: Expected a constant, not this", x);
      mmix_output_register_setting (stream,
				    mmix_output_destination_register,
				    mmix_intval (x), 0);
      return;

    case 'U':
      /* An U for unsigned, if TARGET_ZERO_EXTEND.  Ignore the operand.  */
      if (TARGET_ZERO_EXTEND)
	putc ('U', stream);
      return;

    case 'v':
      mmix_output_shifted_value (stream, (int64_t) mmix_intval (x));
      return;

    case 'V':
      mmix_output_shifted_value (stream, (int64_t) ~mmix_intval (x));
      return;

    case 'W':
      if (GET_CODE (x) != CONST_INT)
	fatal_insn ("MMIX Internal: Expected a CONST_INT, not this", x);
      fprintf (stream, "#%x", (int) (INTVAL (x) & 0xffff));
      return;

    case 0:
      /* Nothing to do.  */
      break;

    default:
      /* Presumably there's a missing case above if we get here.  */
      internal_error ("MMIX Internal: Missing %qc case in mmix_print_operand", code);
    }

  switch (GET_CODE (modified_x))
    {
    case REG:
      regno = REGNO (modified_x);
      if (regno >= FIRST_PSEUDO_REGISTER)
	internal_error ("MMIX Internal: Bad register: %d", regno);
      fprintf (stream, "%s", reg_names[MMIX_OUTPUT_REGNO (regno)]);
      return;

    case MEM:
      output_address (GET_MODE (modified_x), XEXP (modified_x, 0));
      return;

    case CONST_INT:
      /* For -2147483648, mmixal complains that the constant does not fit
	 in 4 bytes, so let's output it as hex.  Take care to handle hosts
	 where HOST_WIDE_INT is longer than an int.

	 Print small constants +-255 using decimal.  */

      if (INTVAL (modified_x) > -256 && INTVAL (modified_x) < 256)
	fprintf (stream, "%d", (int) (INTVAL (modified_x)));
      else
	fprintf (stream, "#%x",
		 (int) (INTVAL (modified_x)) & (unsigned int) ~0);
      return;

    case CONST_DOUBLE:
      /* Do somewhat as CONST_INT.  */
      mmix_output_octa (stream, mmix_intval (modified_x), 0);
      return;

    case CONST:
      output_addr_const (stream, modified_x);
      return;

    default:
      /* No need to test for all strange things.  Let output_addr_const do
	 it for us.  */
      if (CONSTANT_P (modified_x)
	  /* Strangely enough, this is not included in CONSTANT_P.
	     FIXME: Ask/check about sanity here.  */
	  || LABEL_P (modified_x))
	{
	  output_addr_const (stream, modified_x);
	  return;
	}

      /* We need the original here.  */
      fatal_insn ("MMIX Internal: Cannot decode this operand", x);
    }
}

/* TARGET_PRINT_OPERAND_PUNCT_VALID_P.  */

static bool
mmix_print_operand_punct_valid_p (unsigned char code)
{
  /* A '+' is used for branch prediction, similar to other ports.  */
  return code == '+'
    /* A '.' is used for the %d in the POP %d,0 return insn.  */
    || code == '.'
    /* A '!' is used for the number of saved registers, like when outputting
       PUSHJ and PUSHGO. */
    || code == '!';
}

/* TARGET_PRINT_OPERAND_ADDRESS.  */

static void
mmix_print_operand_address (FILE *stream, machine_mode /*mode*/, rtx x)
{
  if (REG_P (x))
    {
      /* I find the generated assembly code harder to read without
	 the ",0".  */
      fprintf (stream, "%s,0", reg_names[MMIX_OUTPUT_REGNO (REGNO (x))]);
      return;
    }
  else if (GET_CODE (x) == PLUS)
    {
      rtx x1 = XEXP (x, 0);
      rtx x2 = XEXP (x, 1);

      if (REG_P (x1))
	{
	  fprintf (stream, "%s,", reg_names[MMIX_OUTPUT_REGNO (REGNO (x1))]);

	  if (REG_P (x2))
	    {
	      fprintf (stream, "%s",
		       reg_names[MMIX_OUTPUT_REGNO (REGNO (x2))]);
	      return;
	    }
	  else if (satisfies_constraint_I (x2))
	    {
	      output_addr_const (stream, x2);
	      return;
	    }
	}
    }

  if (TARGET_BASE_ADDRESSES && mmix_legitimate_constant_p (Pmode, x))
    {
      output_addr_const (stream, x);
      return;
    }

  fatal_insn ("MMIX Internal: This is not a recognized address", x);
}

/* ASM_OUTPUT_REG_PUSH.  */

void
mmix_asm_output_reg_push (FILE *stream, int regno)
{
  fprintf (stream, "\tSUBU %s,%s,8\n\tSTOU %s,%s,0\n",
	   reg_names[MMIX_STACK_POINTER_REGNUM],
	   reg_names[MMIX_STACK_POINTER_REGNUM],
	   reg_names[MMIX_OUTPUT_REGNO (regno)],
	   reg_names[MMIX_STACK_POINTER_REGNUM]);
}

/* ASM_OUTPUT_REG_POP.  */

void
mmix_asm_output_reg_pop (FILE *stream, int regno)
{
  fprintf (stream, "\tLDOU %s,%s,0\n\tINCL %s,8\n",
	   reg_names[MMIX_OUTPUT_REGNO (regno)],
	   reg_names[MMIX_STACK_POINTER_REGNUM],
	   reg_names[MMIX_STACK_POINTER_REGNUM]);
}

/* ASM_OUTPUT_ADDR_DIFF_ELT.  */

void
mmix_asm_output_addr_diff_elt (FILE *stream,
			       rtx body ATTRIBUTE_UNUSED,
			       int value,
			       int rel)
{
  fprintf (stream, "\tTETRA L%d-L%d\n", value, rel);
}

/* ASM_OUTPUT_ADDR_VEC_ELT.  */

void
mmix_asm_output_addr_vec_elt (FILE *stream, int value)
{
  fprintf (stream, "\tOCTA L:%d\n", value);
}

/* ASM_OUTPUT_SKIP.  */

void
mmix_asm_output_skip (FILE *stream, int nbytes)
{
  fprintf (stream, "\tLOC @+%d\n", nbytes);
}

/* ASM_OUTPUT_ALIGN.  */

void
mmix_asm_output_align (FILE *stream, int power)
{
  /* We need to record the needed alignment of this section in the object,
     so we have to output an alignment directive.  Use a .p2align (not
     .align) so people will never have to wonder about whether the
     argument is in number of bytes or the log2 thereof.  We do it in
     addition to the LOC directive, so nothing needs tweaking when
     copy-pasting assembly into mmixal.  */
 fprintf (stream, "\t.p2align %d\n", power);
 fprintf (stream, "\tLOC @+(%d-@)&%d\n", 1 << power, (1 << power) - 1);
}

/* DBX_REGISTER_NUMBER.  */

unsigned
mmix_dbx_register_number (unsigned regno)
{
  /* Adjust the register number to the one it will be output as, dammit.
     It'd be nice if we could check the assumption that we're filling a
     gap, but every register between the last saved register and parameter
     registers might be a valid parameter register.  */
  regno = MMIX_OUTPUT_REGNO (regno);

  /* We need to renumber registers to get the number of the return address
     register in the range 0..255.  It is also space-saving if registers
     mentioned in the call-frame information (which uses this function by
     defaulting DWARF_FRAME_REGNUM to DBX_REGISTER_NUMBER) are numbered
     0 .. 63.  So map 224 .. 256+15 -> 0 .. 47 and 0 .. 223 -> 48..223+48.  */
  return regno >= 224 ? (regno - 224) : (regno + 48);
}

/* End of target macro support functions.

   Now the MMIX port's own functions.  First the exported ones.  */

/* Wrapper for get_hard_reg_initial_val since integrate.h isn't included
   from insn-emit.cc.  */

rtx
mmix_get_hard_reg_initial_val (machine_mode mode, int regno)
{
  return get_hard_reg_initial_val (mode, regno);
}

/* Nonzero when the function epilogue is simple enough that a single
   "POP %d,0" should be used even within the function.  */

int
mmix_use_simple_return (void)
{
  int regno;

  int stack_space_to_allocate
    = (crtl->outgoing_args_size
       + crtl->args.pretend_args_size
       + get_frame_size () + 7) & ~7;

  if (!TARGET_USE_RETURN_INSN || !reload_completed)
    return 0;

  for (regno = 255;
       regno >= MMIX_FIRST_GLOBAL_REGNUM;
       regno--)
    /* Note that we assume that the frame-pointer-register is one of these
       registers, in which case we don't count it here.  */
    if ((((regno != MMIX_FRAME_POINTER_REGNUM || !frame_pointer_needed)
	  && df_regs_ever_live_p (regno) && !call_used_or_fixed_reg_p (regno)))
	|| IS_MMIX_EH_RETURN_DATA_REG (regno))
      return 0;

  if (frame_pointer_needed)
    stack_space_to_allocate += 8;

  if (MMIX_CFUN_HAS_LANDING_PAD)
    stack_space_to_allocate += 16;
  else if (MMIX_CFUN_NEEDS_SAVED_EH_RETURN_ADDRESS)
    stack_space_to_allocate += 8;

  return stack_space_to_allocate == 0;
}


/* Expands the function prologue into RTX.  */

void
mmix_expand_prologue (void)
{
  HOST_WIDE_INT locals_size = get_frame_size ();
  int regno;
  HOST_WIDE_INT stack_space_to_allocate
    = (crtl->outgoing_args_size
       + crtl->args.pretend_args_size
       + locals_size + 7) & ~7;
  HOST_WIDE_INT offset = -8;
  HOST_WIDE_INT total_allocated_stack_space = 0;

  /* Add room needed to save global non-register-stack registers.  */
  for (regno = 255;
       regno >= MMIX_FIRST_GLOBAL_REGNUM;
       regno--)
    /* Note that we assume that the frame-pointer-register is one of these
       registers, in which case we don't count it here.  */
    if ((((regno != MMIX_FRAME_POINTER_REGNUM || !frame_pointer_needed)
	  && df_regs_ever_live_p (regno) && !call_used_or_fixed_reg_p (regno)))
	|| IS_MMIX_EH_RETURN_DATA_REG (regno))
      stack_space_to_allocate += 8;

  /* If we do have a frame-pointer, add room for it.  */
  if (frame_pointer_needed)
    stack_space_to_allocate += 8;

  /* If we have a non-local label, we need to be able to unwind to it, so
     store the current register stack pointer.  Also store the return
     address if we do that.  */
  if (MMIX_CFUN_HAS_LANDING_PAD)
    stack_space_to_allocate += 16;
  else if (MMIX_CFUN_NEEDS_SAVED_EH_RETURN_ADDRESS)
    /* If we do have a saved return-address slot, add room for it.  */
    stack_space_to_allocate += 8;

  /* Make sure we don't get an unaligned stack.  */
  if ((stack_space_to_allocate % 8) != 0)
    internal_error ("stack frame not a multiple of 8 bytes: %wd",
		    stack_space_to_allocate);

  if (crtl->args.pretend_args_size)
    {
      int mmix_first_vararg_reg
	= (MMIX_FIRST_INCOMING_ARG_REGNUM
	   + (MMIX_MAX_ARGS_IN_REGS
	      - crtl->args.pretend_args_size / 8));

      for (regno
	     = MMIX_FIRST_INCOMING_ARG_REGNUM + MMIX_MAX_ARGS_IN_REGS - 1;
	   regno >= mmix_first_vararg_reg;
	   regno--)
	{
	  if (offset < 0)
	    {
	      HOST_WIDE_INT stack_chunk
		= stack_space_to_allocate > (256 - 8)
		? (256 - 8) : stack_space_to_allocate;

	      mmix_emit_sp_add (-stack_chunk);
	      total_allocated_stack_space += stack_chunk;

	      offset += stack_chunk;
	      stack_space_to_allocate -= stack_chunk;
	    }

	  /* These registers aren't actually saved (as in "will be
	     restored"), so don't tell DWARF2 they're saved.  */
	  emit_move_insn (gen_rtx_MEM (DImode,
				       plus_constant (Pmode, stack_pointer_rtx,
						      offset)),
			  gen_rtx_REG (DImode, regno));
	  offset -= 8;
	}
    }

  /* Store the frame-pointer.  */

  if (frame_pointer_needed)
    {
      rtx insn;

      if (offset < 0)
	{
	  /* Get 8 less than otherwise, since we need to reach offset + 8.  */
	  HOST_WIDE_INT stack_chunk
	    = stack_space_to_allocate > (256 - 8 - 8)
	    ? (256 - 8 - 8) : stack_space_to_allocate;

	  mmix_emit_sp_add (-stack_chunk);
	  total_allocated_stack_space += stack_chunk;

	  offset += stack_chunk;
	  stack_space_to_allocate -= stack_chunk;
	}

      insn = emit_move_insn (gen_rtx_MEM (DImode,
					  plus_constant (Pmode,
							 stack_pointer_rtx,
							 offset)),
			     hard_frame_pointer_rtx);
      RTX_FRAME_RELATED_P (insn) = 1;
      insn = emit_insn (gen_adddi3 (hard_frame_pointer_rtx,
				    stack_pointer_rtx,
				    GEN_INT (offset + 8)));
      RTX_FRAME_RELATED_P (insn) = 1;
      offset -= 8;
    }

  if (MMIX_CFUN_NEEDS_SAVED_EH_RETURN_ADDRESS)
    {
      rtx tmpreg, retreg;
      rtx insn;

      /* Store the return-address, if one is needed on the stack.  We
	 usually store it in a register when needed, but that doesn't work
	 with -fexceptions.  */

      if (offset < 0)
	{
	  /* Get 8 less than otherwise, since we need to reach offset + 8.  */
	  HOST_WIDE_INT stack_chunk
	    = stack_space_to_allocate > (256 - 8 - 8)
	    ? (256 - 8 - 8) : stack_space_to_allocate;

	  mmix_emit_sp_add (-stack_chunk);
	  total_allocated_stack_space += stack_chunk;

	  offset += stack_chunk;
	  stack_space_to_allocate -= stack_chunk;
	}

      tmpreg = gen_rtx_REG (DImode, 255);
      retreg = gen_rtx_REG (DImode, MMIX_rJ_REGNUM);

      /* Dwarf2 code is confused by the use of a temporary register for
	 storing the return address, so we have to express it as a note,
	 which we attach to the actual store insn.  */
      emit_move_insn (tmpreg, retreg);

      insn = emit_move_insn (gen_rtx_MEM (DImode,
					  plus_constant (Pmode,
							 stack_pointer_rtx,
							 offset)),
			     tmpreg);
      RTX_FRAME_RELATED_P (insn) = 1;
      add_reg_note (insn, REG_FRAME_RELATED_EXPR,
		    gen_rtx_SET (gen_rtx_MEM (DImode,
					      plus_constant (Pmode,
							     stack_pointer_rtx,
							     offset)),
				 retreg));

      offset -= 8;
    }
  else if (MMIX_CFUN_HAS_LANDING_PAD)
    offset -= 8;

  if (MMIX_CFUN_HAS_LANDING_PAD)
    {
      /* Store the register defining the numbering of local registers, so
	 we know how long to unwind the register stack.  */

      if (offset < 0)
	{
	  /* Get 8 less than otherwise, since we need to reach offset + 8.  */
	  HOST_WIDE_INT stack_chunk
	    = stack_space_to_allocate > (256 - 8 - 8)
	    ? (256 - 8 - 8) : stack_space_to_allocate;

	  mmix_emit_sp_add (-stack_chunk);
	  total_allocated_stack_space += stack_chunk;

	  offset += stack_chunk;
	  stack_space_to_allocate -= stack_chunk;
	}

      /* We don't tell dwarf2 about this one; we just have it to unwind
	 the register stack at landing pads.  FIXME: It's a kludge because
	 we can't describe the effect of the PUSHJ and PUSHGO insns on the
	 register stack at the moment.  Best thing would be to handle it
	 like stack-pointer offsets.  Better: some hook into dwarf2out.cc
	 to produce DW_CFA_expression:s that specify the increment of rO,
	 and unwind it at eh_return (preferred) or at the landing pad.
	 Then saves to $0..$G-1 could be specified through that register.  */

      emit_move_insn (gen_rtx_REG (DImode, 255),
		      gen_rtx_REG (DImode,
				   MMIX_rO_REGNUM));
      emit_move_insn (gen_rtx_MEM (DImode,
				   plus_constant (Pmode, stack_pointer_rtx,
						  offset)),
		      gen_rtx_REG (DImode, 255));
      offset -= 8;
    }

  /* After the return-address and the frame-pointer, we have the local
     variables.  They're the ones that may have an "unaligned" size.  */
  offset -= (locals_size + 7) & ~7;

  /* Now store all registers that are global, i.e. not saved by the
     register file machinery.

     It is assumed that the frame-pointer is one of these registers, so it
     is explicitly excluded in the count.  */

  for (regno = 255;
       regno >= MMIX_FIRST_GLOBAL_REGNUM;
       regno--)
    if (((regno != MMIX_FRAME_POINTER_REGNUM || !frame_pointer_needed)
	 && df_regs_ever_live_p (regno) && !call_used_or_fixed_reg_p (regno))
	|| IS_MMIX_EH_RETURN_DATA_REG (regno))
      {
	rtx insn;

	if (offset < 0)
	  {
	    HOST_WIDE_INT stack_chunk
	      = (stack_space_to_allocate > (256 - offset - 8)
		 ? (256 - offset - 8) : stack_space_to_allocate);

	    mmix_emit_sp_add (-stack_chunk);
	    total_allocated_stack_space += stack_chunk;

	    offset += stack_chunk;
	    stack_space_to_allocate -= stack_chunk;
	  }

	insn = emit_move_insn (gen_rtx_MEM (DImode,
					    plus_constant (Pmode,
							   stack_pointer_rtx,
							   offset)),
			       gen_rtx_REG (DImode, regno));
	RTX_FRAME_RELATED_P (insn) = 1;
	offset -= 8;
      }

  /* Finally, allocate room for outgoing args and local vars if room
     wasn't allocated above.  */
  if (stack_space_to_allocate)
    mmix_emit_sp_add (-stack_space_to_allocate);
  total_allocated_stack_space += stack_space_to_allocate;

  /* Let's assume that reporting the usage of the regular stack on its
     own, is more useful than either not supporting -fstack-usage or
     reporting the sum of the usages of the regular stack and the
     register stack.  */
  if (flag_stack_usage_info)
    current_function_static_stack_size = total_allocated_stack_space;
}

/* Expands the function epilogue into RTX.  */

void
mmix_expand_epilogue (void)
{
  HOST_WIDE_INT locals_size = get_frame_size ();
  int regno;
  HOST_WIDE_INT stack_space_to_deallocate
    = (crtl->outgoing_args_size
       + crtl->args.pretend_args_size
       + locals_size + 7) & ~7;

  /* The first address to access is beyond the outgoing_args area.  */
  HOST_WIDE_INT offset = crtl->outgoing_args_size;

  /* Add the space for global non-register-stack registers.
     It is assumed that the frame-pointer register can be one of these
     registers, in which case it is excluded from the count when needed.  */
  for (regno = 255;
       regno >= MMIX_FIRST_GLOBAL_REGNUM;
       regno--)
    if (((regno != MMIX_FRAME_POINTER_REGNUM || !frame_pointer_needed)
	 && df_regs_ever_live_p (regno) && !call_used_or_fixed_reg_p (regno))
	|| IS_MMIX_EH_RETURN_DATA_REG (regno))
      stack_space_to_deallocate += 8;

  /* Add in the space for register stack-pointer.  If so, always add room
     for the saved PC.  */
  if (MMIX_CFUN_HAS_LANDING_PAD)
    stack_space_to_deallocate += 16;
  else if (MMIX_CFUN_NEEDS_SAVED_EH_RETURN_ADDRESS)
    /* If we have a saved return-address slot, add it in.  */
    stack_space_to_deallocate += 8;

  /* Add in the frame-pointer.  */
  if (frame_pointer_needed)
    stack_space_to_deallocate += 8;

  /* Make sure we don't get an unaligned stack.  */
  if ((stack_space_to_deallocate % 8) != 0)
    internal_error ("stack frame not a multiple of octabyte: %wd",
		    stack_space_to_deallocate);

  /* We will add back small offsets to the stack pointer as we go.
     First, we restore all registers that are global, i.e. not saved by
     the register file machinery.  */

  for (regno = MMIX_FIRST_GLOBAL_REGNUM;
       regno <= 255;
       regno++)
    if (((regno != MMIX_FRAME_POINTER_REGNUM || !frame_pointer_needed)
	 && df_regs_ever_live_p (regno) && !call_used_or_fixed_reg_p (regno))
	|| IS_MMIX_EH_RETURN_DATA_REG (regno))
      {
	if (offset > 255)
	  {
	    mmix_emit_sp_add (offset);
	    stack_space_to_deallocate -= offset;
	    offset = 0;
	  }

	emit_move_insn (gen_rtx_REG (DImode, regno),
			gen_rtx_MEM (DImode,
				     plus_constant (Pmode, stack_pointer_rtx,
						    offset)));
	offset += 8;
      }

  /* Here is where the local variables were.  As in the prologue, they
     might be of an unaligned size.  */
  offset += (locals_size + 7) & ~7;

  /* The saved register stack pointer is just below the frame-pointer
     register.  We don't need to restore it "manually"; the POP
     instruction does that.  */
  if (MMIX_CFUN_HAS_LANDING_PAD)
    offset += 16;
  else if (MMIX_CFUN_NEEDS_SAVED_EH_RETURN_ADDRESS)
    /* The return-address slot is just below the frame-pointer register.
       We don't need to restore it because we don't really use it.  */
    offset += 8;

  /* Get back the old frame-pointer-value.  */
  if (frame_pointer_needed)
    {
      if (offset > 255)
	{
	  mmix_emit_sp_add (offset);

	  stack_space_to_deallocate -= offset;
	  offset = 0;
	}

      emit_move_insn (hard_frame_pointer_rtx,
		      gen_rtx_MEM (DImode,
				   plus_constant (Pmode, stack_pointer_rtx,
						  offset)));
      offset += 8;
    }

  /* We do not need to restore pretended incoming args, just add back
     offset to sp.  */
  if (stack_space_to_deallocate != 0)
    mmix_emit_sp_add (stack_space_to_deallocate);

  if (crtl->calls_eh_return)
    /* Adjust the (normal) stack-pointer to that of the receiver.
       FIXME: It would be nice if we could also adjust the register stack
       here, but we need to express it through DWARF 2 too.  */
    emit_insn (gen_adddi3 (stack_pointer_rtx, stack_pointer_rtx,
			   gen_rtx_REG (DImode,
					MMIX_EH_RETURN_STACKADJ_REGNUM)));
}

/* Output an optimal sequence for setting a register to a specific
   constant.  Used in an alternative for const_ints in movdi, and when
   using large stack-frame offsets.

   Use do_begin_end to say if a line-starting TAB and newline before the
   first insn and after the last insn is wanted.  */

void
mmix_output_register_setting (FILE *stream,
			      int regno,
			      int64_t value,
			      int do_begin_end)
{
  if (do_begin_end)
    fprintf (stream, "\t");

  if (insn_const_int_ok_for_constraint (value, CONSTRAINT_K))
    fprintf (stream, "NEGU %s,0,%" PRId64, reg_names[regno], -value);
  else if (mmix_shiftable_wyde_value ((uint64_t) value))
    {
      /* First, the one-insn cases.  */
      mmix_output_shiftvalue_op_from_str (stream, "SET",
					  (uint64_t)
					  value);
      fprintf (stream, " %s,", reg_names[regno]);
      mmix_output_shifted_value (stream, (uint64_t) value);
    }
  else if (mmix_shiftable_wyde_value (-(uint64_t) value))
    {
      /* We do this to get a bit more legible assembly code.  The next
	 alternative is mostly redundant with this.  */

      mmix_output_shiftvalue_op_from_str (stream, "SET",
					  -(uint64_t)
					  value);
      fprintf (stream, " %s,", reg_names[regno]);
      mmix_output_shifted_value (stream, -(uint64_t) value);
      fprintf (stream, "\n\tNEGU %s,0,%s", reg_names[regno],
	       reg_names[regno]);
    }
  else if (mmix_shiftable_wyde_value (~(uint64_t) value))
    {
      /* Slightly more expensive, the two-insn cases.  */

      /* FIXME: We could of course also test if 0..255-N or ~(N | 1..255)
	 is shiftable, or any other one-insn transformation of the value.
	 FIXME: Check first if the value is "shiftable" by two loading
	 with two insns, since it makes more readable assembly code (if
	 anyone else cares).  */

      mmix_output_shiftvalue_op_from_str (stream, "SET",
					  ~(uint64_t)
					  value);
      fprintf (stream, " %s,", reg_names[regno]);
      mmix_output_shifted_value (stream, ~(uint64_t) value);
      fprintf (stream, "\n\tNOR %s,%s,0", reg_names[regno],
	       reg_names[regno]);
    }
  else
    {
      /* The generic case.  2..4 insns.  */
      static const char *const higher_parts[] = {"L", "ML", "MH", "H"};
      const char *op = "SET";
      const char *line_begin = "";
      int insns = 0;
      int i;
      int64_t tmpvalue = value;

      /* Compute the number of insns needed to output this constant.  */
      for (i = 0; i < 4 && tmpvalue != 0; i++)
	{
	  if (tmpvalue & 65535)
	    insns++;
	  tmpvalue >>= 16;
	}
      if (TARGET_BASE_ADDRESSES && insns == 3)
	{
	  /* The number three is based on a static observation on
	     ghostscript-6.52.  Two and four are excluded because there
	     are too many such constants, and each unique constant (maybe
	     offset by 1..255) were used few times compared to other uses,
	     e.g. addresses.

	     We use base-plus-offset addressing to force it into a global
	     register; we just use a "LDA reg,VALUE", which will cause the
	     assembler and linker to DTRT (for constants as well as
	     addresses).  */
	  fprintf (stream, "LDA %s,", reg_names[regno]);
	  mmix_output_octa (stream, value, 0);
	}
      else
	{
	  /* Output pertinent parts of the 4-wyde sequence.
	     Still more to do if we want this to be optimal, but hey...
	     Note that the zero case has been handled above.  */
	  for (i = 0; i < 4 && value != 0; i++)
	    {
	      if (value & 65535)
		{
		  fprintf (stream, "%s%s%s %s,#%x", line_begin, op,
			   higher_parts[i], reg_names[regno],
			   (int) (value & 65535));
		  /* The first one sets the rest of the bits to 0, the next
		     ones add set bits.  */
		  op = "INC";
		  line_begin = "\n\t";
		}

	      value >>= 16;
	    }
	}
    }

  if (do_begin_end)
    fprintf (stream, "\n");
}

/* Return 1 if value is 0..65535*2**(16*N) for N=0..3.
   else return 0.  */

int
mmix_shiftable_wyde_value (uint64_t value)
{
  /* Shift by 16 bits per group, stop when we've found two groups with
     nonzero bits.  */
  int i;
  int has_candidate = 0;

  for (i = 0; i < 4; i++)
    {
      if (value & 65535)
	{
	  if (has_candidate)
	    return 0;
	  else
	    has_candidate = 1;
	}

      value >>= 16;
    }

  return 1;
}

/* X and Y are two things to compare using CODE.  Return the rtx for
   the cc-reg in the proper mode.  */

rtx
mmix_gen_compare_reg (RTX_CODE code, rtx x, rtx y)
{
  machine_mode ccmode = SELECT_CC_MODE (code, x, y);
  return gen_reg_rtx (ccmode);
}

/* Local (static) helper functions.  */

static void
mmix_emit_sp_add (HOST_WIDE_INT offset)
{
  rtx insn;

  if (offset < 0)
    {
      /* Negative stack-pointer adjustments are allocations and appear in
	 the prologue only.  We mark them as frame-related so unwind and
	 debug info is properly emitted for them.  */
      if (offset > -255)
	insn = emit_insn (gen_adddi3 (stack_pointer_rtx,
				      stack_pointer_rtx,
				      GEN_INT (offset)));
      else
	{
	  rtx tmpr = gen_rtx_REG (DImode, 255);
	  RTX_FRAME_RELATED_P (emit_move_insn (tmpr, GEN_INT (offset))) = 1;
	  insn = emit_insn (gen_adddi3 (stack_pointer_rtx,
					stack_pointer_rtx, tmpr));
	}
      RTX_FRAME_RELATED_P (insn) = 1;
    }
  else
    {
      /* Positive adjustments are in the epilogue only.  Don't mark them
	 as "frame-related" for unwind info.  */
      if (insn_const_int_ok_for_constraint (offset, CONSTRAINT_L))
	emit_insn (gen_adddi3 (stack_pointer_rtx,
			       stack_pointer_rtx,
			       GEN_INT (offset)));
      else
	{
	  rtx tmpr = gen_rtx_REG (DImode, 255);
	  emit_move_insn (tmpr, GEN_INT (offset));
	  insn = emit_insn (gen_adddi3 (stack_pointer_rtx,
					stack_pointer_rtx, tmpr));
	}
    }
}

/* Print operator suitable for doing something with a shiftable
   wyde.  The type of operator is passed as an asm output modifier.  */

static void
mmix_output_shiftvalue_op_from_str (FILE *stream,
				    const char *mainop,
				    int64_t value)
{
  static const char *const op_part[] = {"L", "ML", "MH", "H"};
  int i;

  if (! mmix_shiftable_wyde_value (value))
    {
      char s[sizeof ("0xffffffffffffffff")];
      sprintf (s, "%#" PRIx64, value);
      internal_error ("MMIX Internal: %s is not a shiftable int", s);
    }

  for (i = 0; i < 4; i++)
    {
      /* We know we're through when we find one-bits in the low
	 16 bits.  */
      if (value & 0xffff)
	{
	  fprintf (stream, "%s%s", mainop, op_part[i]);
	  return;
	}
      value >>= 16;
    }

  /* No bits set?  Then it must have been zero.  */
  fprintf (stream, "%sL", mainop);
}

/* Print a 64-bit value, optionally prefixed by assembly pseudo.  */

static void
mmix_output_octa (FILE *stream, int64_t value, int do_begin_end)
{
  if (do_begin_end)
    fprintf (stream, "\tOCTA ");

  /* Provide a few alternative output formats depending on the number, to
     improve legibility of assembler output.  */
  if ((value < (int64_t) 0 && value > (int64_t) -10000)
      || (value >= (int64_t) 0 && value <= (int64_t) 16384))
    fprintf (stream, "%d", (int) value);
  else if (value > (int64_t) 0
	   && value < ((int64_t) 1 << 31) * 2)
    fprintf (stream, "#%x", (unsigned int) value);
  else if (sizeof (HOST_WIDE_INT) == sizeof (int64_t))
    /* We need to avoid the not-so-universal "0x" prefix; we need the
       pure hex-digits together with the mmixal "#" hex prefix.  */
    fprintf (stream, "#" HOST_WIDE_INT_PRINT_HEX_PURE,
	     (HOST_WIDE_INT) value);
  else /* Need to avoid the hex output; there's no ...WIDEST...HEX_PURE.  */
    fprintf (stream, "%" PRIu64, value);

  if (do_begin_end)
    fprintf (stream, "\n");
}

/* Print the presumed shiftable wyde argument shifted into place (to
   be output with an operand).  */

static void
mmix_output_shifted_value (FILE *stream, int64_t value)
{
  int i;

  if (! mmix_shiftable_wyde_value (value))
    {
      char s[16+2+1];
      sprintf (s, "%#" PRIx64, value);
      internal_error ("MMIX Internal: %s is not a shiftable int", s);
    }

  for (i = 0; i < 4; i++)
    {
      /* We know we're through when we find one-bits in the low 16 bits.  */
      if (value & 0xffff)
	{
	  fprintf (stream, "#%x", (int) (value & 0xffff));
	  return;
	}

    value >>= 16;
  }

  /* No bits set?  Then it must have been zero.  */
  fprintf (stream, "0");
}

/* Output an MMIX condition name corresponding to an operator
   and operands:
   (comparison_operator [(comparison_operator ...) (const_int 0)])
   which means we have to look at *two* operators.

   The argument "reversed" refers to reversal of the condition (not the
   same as swapping the arguments).  */

static void
mmix_output_condition (FILE *stream, const_rtx x, int reversed)
{
  struct cc_conv
  {
    RTX_CODE cc;

    /* The normal output cc-code.  */
    const char *const normal;

    /* The reversed cc-code, or NULL if invalid.  */
    const char *const reversed;
  };

  struct cc_type_conv
  {
    machine_mode cc_mode;

    /* Terminated with {UNKNOWN, NULL, NULL} */
    const struct cc_conv *const convs;
  };

#undef CCEND
#define CCEND {UNKNOWN, NULL, NULL}

  static const struct cc_conv cc_fun_convs[]
    = {{ORDERED, "Z", "P"},
       {UNORDERED, "P", "Z"},
       CCEND};
  static const struct cc_conv cc_fp_convs[]
    = {{GT, "P", NULL},
       {LT, "N", NULL},
       CCEND};
  static const struct cc_conv cc_fpeq_convs[]
    = {{NE, "Z", "P"},
       {EQ, "P", "Z"},
       CCEND};
  static const struct cc_conv cc_uns_convs[]
    = {{GEU, "NN", "N"},
       {GTU, "P", "NP"},
       {LEU, "NP", "P"},
       {LTU, "N", "NN"},
       CCEND};
  static const struct cc_conv cc_signed_convs[]
    = {{NE, "NZ", "Z"},
       {EQ, "Z", "NZ"},
       {GE, "NN", "N"},
       {GT, "P", "NP"},
       {LE, "NP", "P"},
       {LT, "N", "NN"},
       CCEND};
  static const struct cc_conv cc_di_convs[]
    = {{NE, "NZ", "Z"},
       {EQ, "Z", "NZ"},
       {GE, "NN", "N"},
       {GT, "P", "NP"},
       {LE, "NP", "P"},
       {LT, "N", "NN"},
       {GTU, "NZ", "Z"},
       {LEU, "Z", "NZ"},
       CCEND};
#undef CCEND

  static const struct cc_type_conv cc_convs[]
    = {{E_CC_FUNmode, cc_fun_convs},
       {E_CC_FPmode, cc_fp_convs},
       {E_CC_FPEQmode, cc_fpeq_convs},
       {E_CC_UNSmode, cc_uns_convs},
       {E_CCmode, cc_signed_convs},
       {E_DImode, cc_di_convs}};

  size_t i;
  int j;

  machine_mode mode = GET_MODE (XEXP (x, 0));
  RTX_CODE cc = GET_CODE (x);

  for (i = 0; i < ARRAY_SIZE (cc_convs); i++)
    {
      if (mode == cc_convs[i].cc_mode)
	{
	  for (j = 0; cc_convs[i].convs[j].cc != UNKNOWN; j++)
	    if (cc == cc_convs[i].convs[j].cc)
	      {
		const char *mmix_cc
		  = (reversed ? cc_convs[i].convs[j].reversed
		     : cc_convs[i].convs[j].normal);

		if (mmix_cc == NULL)
		  fatal_insn ("MMIX Internal: Trying to output invalidly\
 reversed condition:", x);

		fprintf (stream, "%s", mmix_cc);
		return;
	      }

	  fatal_insn ("MMIX Internal: What's the CC of this?", x);
	}
    }

  fatal_insn ("MMIX Internal: What is the CC of this?", x);
}

/* Return the bit-value for a const_int or const_double.  */

int64_t
mmix_intval (const_rtx x)
{
  if (GET_CODE (x) == CONST_INT)
    return INTVAL (x);

  /* We make a little song and dance because converting to long long in
     gcc-2.7.2 is broken.  I still want people to be able to use it for
     cross-compilation to MMIX.  */
  if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == VOIDmode)
    return CONST_DOUBLE_HIGH (x);

  if (GET_CODE (x) == CONST_DOUBLE)
    {
      if (GET_MODE (x) == DFmode)
	{
	  long bits[2];

	  REAL_VALUE_TO_TARGET_DOUBLE (*CONST_DOUBLE_REAL_VALUE (x), bits);

	  /* The double cast is necessary to avoid getting the long
	     sign-extended to unsigned long long(!) when they're of
	     different size (usually 32-bit hosts).  */
	  return
	    ((uint64_t) (unsigned long) bits[0]
	     << (uint64_t) 32U)
	    | (uint64_t) (unsigned long) bits[1];
	}
      else if (GET_MODE (x) == SFmode)
	{
	  long bits;
	  REAL_VALUE_TO_TARGET_SINGLE (*CONST_DOUBLE_REAL_VALUE (x), bits);

	  return (unsigned long) bits;
	}
    }

  fatal_insn ("MMIX Internal: This is not a constant:", x);
}

/* Worker function for TARGET_PROMOTE_FUNCTION_MODE.  */

machine_mode
mmix_promote_function_mode (const_tree type ATTRIBUTE_UNUSED,
                            machine_mode mode,
                            int *punsignedp ATTRIBUTE_UNUSED,
                            const_tree fntype ATTRIBUTE_UNUSED,
                            int for_return)
{
  /* Apparently not doing TRT if int < register-size.  FIXME: Perhaps
     FUNCTION_VALUE and LIBCALL_VALUE needs tweaking as some ports say.  */
  if (for_return == 1)
    return mode;

  /* Promotion of modes currently generates slow code, extending before
     operation, so we do it only for arguments.  */
  if (GET_MODE_CLASS (mode) == MODE_INT
      && GET_MODE_SIZE (mode) < 8)
    return DImode;
  else
    return mode;
}
/* Worker function for TARGET_STRUCT_VALUE_RTX.  */

static rtx
mmix_struct_value_rtx (tree fntype ATTRIBUTE_UNUSED,
		       int incoming ATTRIBUTE_UNUSED)
{
  return gen_rtx_REG (Pmode, MMIX_STRUCT_VALUE_REGNUM);
}

/* Worker function for TARGET_FRAME_POINTER_REQUIRED.

   FIXME: Is this requirement built-in?  Anyway, we should try to get rid
   of it; we can deduce the value.  */

bool
mmix_frame_pointer_required (void)
{
  return (cfun->has_nonlocal_label);
}

/*
 * Local variables:
 * eval: (c-set-style "gnu")
 * indent-tabs-mode: t
 * End:
 */

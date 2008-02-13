/*
 * $Id: run.c,v 1.25 2007/11/10 15:21:40 bacon Exp $
 *
 * {License}
 */

#include <ase/awk/awk_i.h>

#ifdef DEBUG_RUN
#include <ase/utl/stdio.h>
#endif

#define CMP_ERROR -99
#define DEF_BUF_CAPA 256
#define STACK_INCREMENT 512

#define IDXBUFSIZE 64

#define STACK_AT(run,n) ((run)->stack[(run)->stack_base+(n)])
#define STACK_NARGS(run) (STACK_AT(run,3))
#define STACK_ARG(run,n) STACK_AT(run,3+1+(n))
#define STACK_LOCAL(run,n) STACK_AT(run,3+(ase_size_t)STACK_NARGS(run)+1+(n))
#define STACK_RETVAL(run) STACK_AT(run,2)
#define STACK_GLOBAL(run,n) ((run)->stack[(n)])
#define STACK_RETVAL_GLOBAL(run) ((run)->stack[(run)->awk->tree.nglobals+2])

enum exit_level_t
{
	EXIT_NONE,
	EXIT_BREAK,
	EXIT_CONTINUE,
	EXIT_FUNCTION,
	EXIT_NEXT,
	EXIT_GLOBAL,
	EXIT_ABORT
};

#define DEFAULT_CONVFMT ASE_T("%.6g")
#define DEFAULT_OFMT ASE_T("%.6g")
#define DEFAULT_OFS ASE_T(" ")
#define DEFAULT_ORS ASE_T("\n")
#define DEFAULT_ORS_CRLF ASE_T("\r\n")
#define DEFAULT_SUBSEP ASE_T("\034")

/* the index of a positional variable should be a positive interger
 * equal to or less than the maximum value of the type by which
 * the index is represented. but it has an extra check against the
 * maximum value of ase_size_t as the reference is represented
 * in a pointer variable of ase_awk_val_ref_t and sizeof(void*) is
 * equal to sizeof(ase_size_t). */
#define IS_VALID_POSIDX(idx) \
	((idx) >= 0 && \
	 (idx) < ASE_TYPE_MAX(ase_long_t) && \
	 (idx) < ASE_TYPE_MAX(ase_size_t))

static int set_global (
	ase_awk_run_t* run, ase_size_t idx, 
	ase_awk_nde_var_t* var, ase_awk_val_t* val);

static int init_run (
	ase_awk_run_t* run, ase_awk_t* awk,
	ase_awk_runios_t* runios, void* custom_data);
static void deinit_run (ase_awk_run_t* run);

static int build_runarg (
	ase_awk_run_t* run, ase_awk_runarg_t* runarg, ase_size_t* nargs);
static void cleanup_globals (ase_awk_run_t* run);
static int set_globals_to_default (ase_awk_run_t* run);

static int run_main (
	ase_awk_run_t* run, const ase_char_t* main, 
	ase_awk_runarg_t* runarg);

static int run_pattern_blocks  (ase_awk_run_t* run);
static int run_pattern_block_chain (
	ase_awk_run_t* run, ase_awk_chain_t* chain);
static int run_pattern_block (
	ase_awk_run_t* run, ase_awk_chain_t* chain, ase_size_t block_no);
static int run_block (ase_awk_run_t* run, ase_awk_nde_blk_t* nde);
static int run_block0 (ase_awk_run_t* run, ase_awk_nde_blk_t* nde);
static int run_statement (ase_awk_run_t* run, ase_awk_nde_t* nde);
static int run_if (ase_awk_run_t* run, ase_awk_nde_if_t* nde);
static int run_while (ase_awk_run_t* run, ase_awk_nde_while_t* nde);
static int run_for (ase_awk_run_t* run, ase_awk_nde_for_t* nde);
static int run_foreach (ase_awk_run_t* run, ase_awk_nde_foreach_t* nde);
static int run_break (ase_awk_run_t* run, ase_awk_nde_break_t* nde);
static int run_continue (ase_awk_run_t* run, ase_awk_nde_continue_t* nde);
static int run_return (ase_awk_run_t* run, ase_awk_nde_return_t* nde);
static int run_exit (ase_awk_run_t* run, ase_awk_nde_exit_t* nde);
static int run_next (ase_awk_run_t* run, ase_awk_nde_next_t* nde);
static int run_nextfile (ase_awk_run_t* run, ase_awk_nde_nextfile_t* nde);
static int run_delete (ase_awk_run_t* run, ase_awk_nde_delete_t* nde);
static int run_reset (ase_awk_run_t* run, ase_awk_nde_reset_t* nde);
static int run_print (ase_awk_run_t* run, ase_awk_nde_print_t* nde);
static int run_printf (ase_awk_run_t* run, ase_awk_nde_print_t* nde);

static int output_formatted (
	ase_awk_run_t* run, int out_type, const ase_char_t* dst, 
	const ase_char_t* fmt, ase_size_t fmt_len, ase_awk_nde_t* args);

static ase_awk_val_t* eval_expression (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_expression0 (ase_awk_run_t* run, ase_awk_nde_t* nde);

static ase_awk_val_t* eval_group (ase_awk_run_t* run, ase_awk_nde_t* nde);

static ase_awk_val_t* eval_assignment (
	ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* do_assignment (
	ase_awk_run_t* run, ase_awk_nde_t* var, ase_awk_val_t* val);
static ase_awk_val_t* do_assignment_scalar (
	ase_awk_run_t* run, ase_awk_nde_var_t* var, ase_awk_val_t* val);
static ase_awk_val_t* do_assignment_map (
	ase_awk_run_t* run, ase_awk_nde_var_t* var, ase_awk_val_t* val);
static ase_awk_val_t* do_assignment_pos (
	ase_awk_run_t* run, ase_awk_nde_pos_t* pos, ase_awk_val_t* val);

static ase_awk_val_t* eval_binary (
	ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_binop_lor (
	ase_awk_run_t* run, ase_awk_nde_t* left, ase_awk_nde_t* right);
static ase_awk_val_t* eval_binop_land (
	ase_awk_run_t* run, ase_awk_nde_t* left, ase_awk_nde_t* right);
static ase_awk_val_t* eval_binop_in (
	ase_awk_run_t* run, ase_awk_nde_t* left, ase_awk_nde_t* right);
static ase_awk_val_t* eval_binop_bor (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_bxor (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_band (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_eq (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_ne (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_gt (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_ge (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_lt (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_le (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_lshift (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_rshift (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_plus (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_minus (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_mul (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_div (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_idiv (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_mod (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_exp (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_concat (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
static ase_awk_val_t* eval_binop_ma (
	ase_awk_run_t* run, ase_awk_nde_t* left, ase_awk_nde_t* right);
static ase_awk_val_t* eval_binop_nm (
	ase_awk_run_t* run, ase_awk_nde_t* left, ase_awk_nde_t* right);
static ase_awk_val_t* eval_binop_match0 (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right,
	ase_size_t lline, ase_size_t rline, int ret);

static ase_awk_val_t* eval_unary (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_incpre (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_incpst (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_cnd (ase_awk_run_t* run, ase_awk_nde_t* nde);

static ase_awk_val_t* eval_afn_intrinsic (
	ase_awk_run_t* run, ase_awk_nde_t* nde, 
	void(*errhandler)(void*), void* eharg);

static ase_awk_val_t* eval_bfn (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_afn (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_call (
	ase_awk_run_t* run, ase_awk_nde_t* nde, 
	const ase_char_t* bfn_arg_spec, ase_awk_afn_t* afn,
	void(*errhandler)(void*), void* eharg);

static int get_reference (
	ase_awk_run_t* run, ase_awk_nde_t* nde, ase_awk_val_t*** ref);
static ase_awk_val_t** get_reference_indexed (
	ase_awk_run_t* run, ase_awk_nde_var_t* nde, ase_awk_val_t** val);

static ase_awk_val_t* eval_int (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_real (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_str (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_rex (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_named (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_global (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_local (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_arg (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_namedidx (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_globalidx (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_localidx (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_argidx (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_pos (ase_awk_run_t* run, ase_awk_nde_t* nde);
static ase_awk_val_t* eval_getline (ase_awk_run_t* run, ase_awk_nde_t* nde);

static int __raw_push (ase_awk_run_t* run, void* val);
#define __raw_pop(run) \
	do { \
		ASE_ASSERT ((run)->stack_top > (run)->stack_base); \
		(run)->stack_top--; \
	} while (0)
static void __raw_pop_times (ase_awk_run_t* run, ase_size_t times);

static int read_record (ase_awk_run_t* run);
static int shorten_record (ase_awk_run_t* run, ase_size_t nflds);

static ase_char_t* idxnde_to_str (
	ase_awk_run_t* run, ase_awk_nde_t* nde, ase_char_t* buf, ase_size_t* len);

typedef ase_awk_val_t* (*binop_func_t) (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right);
typedef ase_awk_val_t* (*eval_expr_t) (ase_awk_run_t* run, ase_awk_nde_t* nde);

ase_size_t ase_awk_getnargs (ase_awk_run_t* run)
{
	return (ase_size_t) STACK_NARGS (run);
}

ase_awk_val_t* ase_awk_getarg (ase_awk_run_t* run, ase_size_t idx)
{
	return STACK_ARG (run, idx);
}

ase_awk_val_t* ase_awk_getglobal (ase_awk_run_t* run, int id)
{
	ASE_ASSERT (id >= 0 && id < (int)ase_awk_tab_getsize(&run->awk->parse.globals));
	return STACK_GLOBAL (run, id);
}

int ase_awk_setglobal (ase_awk_run_t* run, int id, ase_awk_val_t* val)
{
	ASE_ASSERT (id >= 0 && id < (int)ase_awk_tab_getsize(&run->awk->parse.globals));
	return set_global (run, (ase_size_t)id, ASE_NULL, val);
}

static int set_global (
	ase_awk_run_t* run, ase_size_t idx, 
	ase_awk_nde_var_t* var, ase_awk_val_t* val)
{
	ase_awk_val_t* old;
       
	old = STACK_GLOBAL (run, idx);
	if (old->type == ASE_AWK_VAL_MAP)
	{	
		/* once a variable becomes a map,
		 * it cannot be changed to a scalar variable */

		if (var != ASE_NULL)
		{
			/* global variable */
			ase_cstr_t errarg;

			errarg.ptr = var->id.name; 
			errarg.len = var->id.name_len;

			ase_awk_setrunerror (run, 
				ASE_AWK_EMAPTOSCALAR, var->line, &errarg, 1);
		}
		else
		{
			/* ase_awk_setglobal has been called */
			ase_cstr_t errarg;

			errarg.ptr = ase_awk_getglobalname (
				run->awk, idx, &errarg.len);
			ase_awk_setrunerror (run, 
				ASE_AWK_EMAPTOSCALAR, 0, &errarg, 1);
		}

		return -1;
	}

	/* builtin variables except ARGV cannot be assigned a map */
	if (val->type == ASE_AWK_VAL_MAP &&
	    (idx >= ASE_AWK_GLOBAL_ARGC && idx <= ASE_AWK_GLOBAL_SUBSEP) &&
	    idx != ASE_AWK_GLOBAL_ARGV)
	{
		/* TODO: better error code */
		ase_awk_setrunerrnum (run, ASE_AWK_ESCALARTOMAP);
		return -1;
	}

	if (idx == ASE_AWK_GLOBAL_CONVFMT)
	{
		ase_char_t* convfmt_ptr;
		ase_size_t convfmt_len, i;

		convfmt_ptr = ase_awk_valtostr (run, 
			val, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &convfmt_len);
		if (convfmt_ptr == ASE_NULL) return  -1;

		for (i = 0; i < convfmt_len; i++)
		{
			if (convfmt_ptr[i] == ASE_T('\0'))
			{
				ASE_AWK_FREE (run->awk, convfmt_ptr);
				ase_awk_setrunerrnum (run, ASE_AWK_ECONVFMTCHR);
				return -1;
			}
		}

		if (run->global.convfmt.ptr != ASE_NULL)
			ASE_AWK_FREE (run->awk, run->global.convfmt.ptr);
		run->global.convfmt.ptr = convfmt_ptr;
		run->global.convfmt.len = convfmt_len;
	}
	else if (idx == ASE_AWK_GLOBAL_FNR)
	{
		int n;
		ase_long_t lv;
		ase_real_t rv;

		n = ase_awk_valtonum (run, val, &lv, &rv);
		if (n == -1) return -1;
		if (n == 1) lv = (ase_long_t)rv;

		run->global.fnr = lv;
	}
	else if (idx == ASE_AWK_GLOBAL_FS)
	{
		ase_char_t* fs_ptr;
		ase_size_t fs_len;

		if (val->type == ASE_AWK_VAL_STR)
		{
			fs_ptr = ((ase_awk_val_str_t*)val)->buf;
			fs_len = ((ase_awk_val_str_t*)val)->len;
		}
		else
		{
			/* due to the expression evaluation rule, the 
			 * regular expression can not be an assigned value */
			ASE_ASSERT (val->type != ASE_AWK_VAL_REX);

			fs_ptr = ase_awk_valtostr (
				run, val, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &fs_len);
			if (fs_ptr == ASE_NULL) return -1;
		}

		if (fs_len > 1)
		{
			void* rex;

			/* compile the regular expression */
			/* TODO: use safebuild */
			rex = ase_awk_buildrex (
				run->awk, fs_ptr, fs_len, &run->errnum);
			if (rex == ASE_NULL)
			{
				if (val->type != ASE_AWK_VAL_STR) 
					ASE_AWK_FREE (run->awk, fs_ptr);
				return -1;
			}

			if (run->global.fs != ASE_NULL) 
			{
				ase_awk_freerex (run->awk, run->global.fs);
			}
			run->global.fs = rex;
		}

		if (val->type != ASE_AWK_VAL_STR) ASE_AWK_FREE (run->awk, fs_ptr);
	}
	else if (idx == ASE_AWK_GLOBAL_IGNORECASE)
	{
		if ((val->type == ASE_AWK_VAL_INT &&
		     ((ase_awk_val_int_t*)val)->val != 0) ||
		    (val->type == ASE_AWK_VAL_REAL &&
		     ((ase_awk_val_real_t*)val)->val != 0.0) ||
		    (val->type == ASE_AWK_VAL_STR &&
		     ((ase_awk_val_str_t*)val)->len != 0))
		{
			run->global.ignorecase = 1;
		}
		else
		{
			run->global.ignorecase = 0;
		}
	}
	else if (idx == ASE_AWK_GLOBAL_NF)
	{
		int n;
		ase_long_t lv;
		ase_real_t rv;

		n = ase_awk_valtonum (run, val, &lv, &rv);
		if (n == -1) return -1;
		if (n == 1) lv = (ase_long_t)rv;

		if (lv < (ase_long_t)run->inrec.nflds)
		{
			if (shorten_record (run, (ase_size_t)lv) == -1)
			{
				/* adjust the error line */
				if (var != ASE_NULL) run->errlin = var->line;
				return -1;
			}
		}
	}
	else if (idx == ASE_AWK_GLOBAL_NR)
	{
		int n;
		ase_long_t lv;
		ase_real_t rv;

		n = ase_awk_valtonum (run, val, &lv, &rv);
		if (n == -1) return -1;
		if (n == 1) lv = (ase_long_t)rv;

		run->global.nr = lv;
	}
	else if (idx == ASE_AWK_GLOBAL_OFMT)
	{
		ase_char_t* ofmt_ptr;
		ase_size_t ofmt_len, i;

		ofmt_ptr = ase_awk_valtostr (
			run, val, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &ofmt_len);
		if (ofmt_ptr == ASE_NULL) return  -1;

		for (i = 0; i < ofmt_len; i++)
		{
			if (ofmt_ptr[i] == ASE_T('\0'))
			{
				ASE_AWK_FREE (run->awk, ofmt_ptr);
				ase_awk_setrunerrnum (run, ASE_AWK_EOFMTCHR);
				return -1;
			}
		}

		if (run->global.ofmt.ptr != ASE_NULL)
			ASE_AWK_FREE (run->awk, run->global.ofmt.ptr);
		run->global.ofmt.ptr = ofmt_ptr;
		run->global.ofmt.len = ofmt_len;
	}
	else if (idx == ASE_AWK_GLOBAL_OFS)
	{	
		ase_char_t* ofs_ptr;
		ase_size_t ofs_len;

		ofs_ptr = ase_awk_valtostr (
			run, val, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &ofs_len);
		if (ofs_ptr == ASE_NULL) return  -1;

		if (run->global.ofs.ptr != ASE_NULL)
			ASE_AWK_FREE (run->awk, run->global.ofs.ptr);
		run->global.ofs.ptr = ofs_ptr;
		run->global.ofs.len = ofs_len;
	}
	else if (idx == ASE_AWK_GLOBAL_ORS)
	{	
		ase_char_t* ors_ptr;
		ase_size_t ors_len;

		ors_ptr = ase_awk_valtostr (
			run, val, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &ors_len);
		if (ors_ptr == ASE_NULL) return  -1;

		if (run->global.ors.ptr != ASE_NULL)
			ASE_AWK_FREE (run->awk, run->global.ors.ptr);
		run->global.ors.ptr = ors_ptr;
		run->global.ors.len = ors_len;
	}
	else if (idx == ASE_AWK_GLOBAL_RS)
	{
		ase_char_t* rs_ptr;
		ase_size_t rs_len;

		if (val->type == ASE_AWK_VAL_STR)
		{
			rs_ptr = ((ase_awk_val_str_t*)val)->buf;
			rs_len = ((ase_awk_val_str_t*)val)->len;
		}
		else
		{
			/* due to the expression evaluation rule, the 
			 * regular expression can not be an assigned value */
			ASE_ASSERT (val->type != ASE_AWK_VAL_REX);

			rs_ptr = ase_awk_valtostr (
				run, val, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &rs_len);
			if (rs_ptr == ASE_NULL) return -1;
		}

		if (rs_len > 1)
		{
			void* rex;

			/* compile the regular expression */
			/* TODO: use safebuild */
			rex = ase_awk_buildrex (
				run->awk, rs_ptr, rs_len, &run->errnum);
			if (rex == ASE_NULL)
			{
				if (val->type != ASE_AWK_VAL_STR) 
					ASE_AWK_FREE (run->awk, rs_ptr);
				return -1;
			}

			if (run->global.rs != ASE_NULL) 
			{
				ase_awk_freerex (run->awk, run->global.rs);
			}
			run->global.rs = rex;
		}

		if (val->type != ASE_AWK_VAL_STR) ASE_AWK_FREE (run->awk, rs_ptr);
	}
	else if (idx == ASE_AWK_GLOBAL_SUBSEP)
	{
		ase_char_t* subsep_ptr;
		ase_size_t subsep_len;

		subsep_ptr = ase_awk_valtostr (
			run, val, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &subsep_len);
		if (subsep_ptr == ASE_NULL) return  -1;

		if (run->global.subsep.ptr != ASE_NULL)
			ASE_AWK_FREE (run->awk, run->global.subsep.ptr);
		run->global.subsep.ptr = subsep_ptr;
		run->global.subsep.len = subsep_len;
	}

	ase_awk_refdownval (run, old);
	STACK_GLOBAL(run,idx) = val;
	ase_awk_refupval (run, val);

	return 0;
}

void ase_awk_setretval (ase_awk_run_t* run, ase_awk_val_t* val)
{
	ase_awk_refdownval (run, STACK_RETVAL(run));
	STACK_RETVAL(run) = val;
	/* should use the same trick as run_return */
	ase_awk_refupval (run, val); 
}

int ase_awk_setfilename (
	ase_awk_run_t* run, const ase_char_t* name, ase_size_t len)
{
	ase_awk_val_t* tmp;
	int n;

	if (len == 0) tmp = ase_awk_val_zls;
	else
	{
		tmp = ase_awk_makestrval (run, name, len);
		if (tmp == ASE_NULL) return -1;
	}

	ase_awk_refupval (run, tmp);
	n = ase_awk_setglobal (run, ASE_AWK_GLOBAL_FILENAME, tmp);
	ase_awk_refdownval (run, tmp);

	return n;
}

int ase_awk_setofilename (
	ase_awk_run_t* run, const ase_char_t* name, ase_size_t len)
{
	ase_awk_val_t* tmp;
	int n;

	if (run->awk->option & ASE_AWK_NEXTOFILE)
	{
		if (len == 0) tmp = ase_awk_val_zls;
		else
		{
			tmp = ase_awk_makestrval (run, name, len);
			if (tmp == ASE_NULL) return -1;
		}

		ase_awk_refupval (run, tmp);
		n = ase_awk_setglobal (run, ASE_AWK_GLOBAL_OFILENAME, tmp);
		ase_awk_refdownval (run, tmp);
	}
	else n = 0;

	return n;
}

ase_awk_t* ase_awk_getrunawk (ase_awk_run_t* run)
{
	return run->awk;
}

void* ase_awk_getruncustomdata (ase_awk_run_t* run)
{
	return run->custom_data;
}

ase_map_t* ase_awk_getrunnamedvarmap (ase_awk_run_t* awk)
{
	return awk->named;
}

int ase_awk_run (ase_awk_t* awk, 
	const ase_char_t* main,
	ase_awk_runios_t* runios, 
	ase_awk_runcbs_t* runcbs, 
	ase_awk_runarg_t* runarg,
	void* custom_data)
{
	ase_awk_run_t* run;
	int n;

	/* clear the awk error code */
	ase_awk_seterror (awk, ASE_AWK_ENOERR, 0, ASE_NULL, 0);

	/* check if the code has ever been parsed */
	if (awk->tree.nglobals == 0 && 
	    awk->tree.begin == ASE_NULL &&
	    awk->tree.end == ASE_NULL &&
	    awk->tree.chain_size == 0 &&
	    ase_map_getsize(awk->tree.afns) == 0)
	{
		/* if not, deny the run */
		ase_awk_seterror (awk, ASE_AWK_ENOPER, 0, ASE_NULL, 0);
		return -1;
	}
	
	/* allocate the storage for the run object */
	run = (ase_awk_run_t*) ASE_AWK_MALLOC (awk, ASE_SIZEOF(ase_awk_run_t));
	if (run == ASE_NULL)
	{
		/* if it fails, the failure is reported thru 
		 * the awk object */
		ase_awk_seterror (awk, ASE_AWK_ENOMEM, 0, ASE_NULL, 0);
		return -1;
	}

	/* clear the run object space */
	ase_memset (run, 0, ASE_SIZEOF(ase_awk_run_t));

	/* initialize the run object */
	if (init_run (run, awk, runios, custom_data) == -1) 
	{
		ASE_AWK_FREE (awk, run);
		return -1;
	}

	/* clear the run error */
	run->errnum    = ASE_AWK_ENOERR;
	run->errlin    = 0;
	run->errmsg[0] = ASE_T('\0');

	run->cbs = runcbs;

	/* execute the start callback if it exists */
	if (runcbs != ASE_NULL && runcbs->on_start != ASE_NULL) 
	{
		runcbs->on_start (run, runcbs->custom_data);
	}

	/* enter the main run loop */
	n = run_main (run, main, runarg);
	if (n == -1) 
	{
		/* if no callback is specified, awk's error number 
		 * is updated with the run's error number */
		if (runcbs == ASE_NULL)
		{
			awk->errnum = run->errnum;
			awk->errlin = run->errlin;

			ase_strxcpy (
				awk->errmsg, ASE_COUNTOF(awk->errmsg),
				run->errmsg);
		}
		else
		{
			ase_awk_seterrnum (awk, ASE_AWK_ERUNTIME);
		}
	}

	/* the run loop ended. execute the end callback if it exists */
	if (runcbs != ASE_NULL && runcbs->on_end != ASE_NULL) 
	{
		if (n == 0) 
		{
			/* clear error if run is successful just in case */
			ase_awk_setrunerrnum (run, ASE_AWK_ENOERR);
		}

		runcbs->on_end (run, 
			((n == -1)? run->errnum: ASE_AWK_ENOERR), 
			runcbs->custom_data);

		/* when using callbacks, this function always returns 0 
		 * after the start callbacks has been triggered */
		n = 0;
	}

	/* uninitialize the run object */
	deinit_run (run);

	ASE_AWK_FREE (awk, run);
	return n;
}

void ase_awk_stop (ase_awk_run_t* run)
{
	run->exit_level = EXIT_ABORT;
}

ase_bool_t ase_awk_isstop (ase_awk_run_t* run)
{
	return (run->exit_level == EXIT_ABORT || run->awk->stopall);
}

static void free_namedval (void* run, void* val)
{
	ase_awk_refdownval ((ase_awk_run_t*)run, val);
}

static void same_namedval (void* run, void* val)
{
	ase_awk_refdownval_nofree ((ase_awk_run_t*)run, val);
}

static int init_run (
	ase_awk_run_t* run, ase_awk_t* awk,
	ase_awk_runios_t* runios, void* custom_data)
{
	run->awk = awk;
	run->custom_data = custom_data;

	run->stack = ASE_NULL;
	run->stack_top = 0;
	run->stack_base = 0;
	run->stack_limit = 0;

	run->exit_level = EXIT_NONE;

	run->fcache_count = 0;
	/*run->scache32_count = 0;
	run->scache64_count = 0;*/
	run->vmgr.ichunk = ASE_NULL;
	run->vmgr.ifree = ASE_NULL;
	run->vmgr.rchunk = ASE_NULL;
	run->vmgr.rfree = ASE_NULL;

	run->errnum = ASE_AWK_ENOERR;
	run->errlin = 0;
	run->errmsg[0] = ASE_T('\0');

	run->inrec.buf_pos = 0;
	run->inrec.buf_len = 0;
	run->inrec.flds = ASE_NULL;
	run->inrec.nflds = 0;
	run->inrec.maxflds = 0;
	run->inrec.d0 = ase_awk_val_nil;
	if (ase_str_open (
		&run->inrec.line, 
		DEF_BUF_CAPA, &run->awk->prmfns.mmgr) == ASE_NULL)
	{
		ase_awk_seterror (awk, ASE_AWK_ENOMEM, 0, ASE_NULL, 0);
		return -1;
	}

	if (ase_str_open (
		&run->format.out, 256, &run->awk->prmfns.mmgr) == ASE_NULL)
	{
		ase_str_close (&run->inrec.line);
		ase_awk_seterror (awk, ASE_AWK_ENOMEM, 0, ASE_NULL, 0);
		return -1;
	}

	if (ase_str_open (
		&run->format.fmt, 256, &run->awk->prmfns.mmgr) == ASE_NULL)
	{
		ase_str_close (&run->format.out);
		ase_str_close (&run->inrec.line);
		ase_awk_seterror (awk, ASE_AWK_ENOMEM, 0, ASE_NULL, 0);
		return -1;
	}

	run->named = ase_map_open (
		run, 1024, 70, free_namedval, same_namedval, 
		&run->awk->prmfns.mmgr);
	if (run->named == ASE_NULL)
	{
		ase_str_close (&run->format.fmt);
		ase_str_close (&run->format.out);
		ase_str_close (&run->inrec.line);
		ase_awk_seterror (awk, ASE_AWK_ENOMEM, 0, ASE_NULL, 0);
		return -1;
	}

	run->format.tmp.ptr = (ase_char_t*)
		ASE_AWK_MALLOC (run->awk, 4096*ASE_SIZEOF(ase_char_t*));
	if (run->format.tmp.ptr == ASE_NULL)
	{
		ase_map_close (run->named);
		ase_str_close (&run->format.fmt);
		ase_str_close (&run->format.out);
		ase_str_close (&run->inrec.line);
		ase_awk_seterror (awk, ASE_AWK_ENOMEM, 0, ASE_NULL, 0);
		return -1;
	}
	run->format.tmp.len = 4096;
	run->format.tmp.inc = 4096*2;

	if (run->awk->tree.chain_size > 0)
	{
		run->pattern_range_state = (ase_byte_t*) ASE_AWK_MALLOC (
			run->awk, run->awk->tree.chain_size*ASE_SIZEOF(ase_byte_t));
		if (run->pattern_range_state == ASE_NULL)
		{
			ASE_AWK_FREE (run->awk, run->format.tmp.ptr);
			ase_map_close (run->named);
			ase_str_close (&run->format.fmt);
			ase_str_close (&run->format.out);
			ase_str_close (&run->inrec.line);
			ase_awk_seterror (awk, ASE_AWK_ENOMEM, 0, ASE_NULL, 0);
			return -1;
		}

		ase_memset (
			run->pattern_range_state, 0, 
			run->awk->tree.chain_size * ASE_SIZEOF(ase_byte_t));
	}
	else run->pattern_range_state = ASE_NULL;

	if (runios != ASE_NULL)
	{
		run->extio.handler[ASE_AWK_EXTIO_PIPE] = runios->pipe;
		run->extio.handler[ASE_AWK_EXTIO_COPROC] = runios->coproc;
		run->extio.handler[ASE_AWK_EXTIO_FILE] = runios->file;
		run->extio.handler[ASE_AWK_EXTIO_CONSOLE] = runios->console;
		run->extio.custom_data = runios->custom_data;
		run->extio.chain = ASE_NULL;
	}

	run->global.rs = ASE_NULL;
	run->global.fs = ASE_NULL;
	run->global.ignorecase = 0;

	run->depth.max.block = awk->run.depth.max.block;
	run->depth.max.expr = awk->run.depth.max.expr;
	run->depth.cur.block = 0; 
	run->depth.cur.expr = 0;

	return 0;
}

static void deinit_run (ase_awk_run_t* run)
{
	if (run->pattern_range_state != ASE_NULL)
		ASE_AWK_FREE (run->awk, run->pattern_range_state);

	/* close all pending extio's */
	/* TODO: what if this operation fails? */
	ase_awk_clearextio (run);
	ASE_ASSERT (run->extio.chain == ASE_NULL);

	if (run->global.rs != ASE_NULL) 
	{
		ASE_AWK_FREE (run->awk, run->global.rs);
		run->global.rs = ASE_NULL;
	}
	if (run->global.fs != ASE_NULL)
	{
		ASE_AWK_FREE (run->awk, run->global.fs);
		run->global.fs = ASE_NULL;
	}

	if (run->global.convfmt.ptr != ASE_NULL &&
	    run->global.convfmt.ptr != DEFAULT_CONVFMT)
	{
		ASE_AWK_FREE (run->awk, run->global.convfmt.ptr);
		run->global.convfmt.ptr = ASE_NULL;
		run->global.convfmt.len = 0;
	}

	if (run->global.ofmt.ptr != ASE_NULL && 
	    run->global.ofmt.ptr != DEFAULT_OFMT)
	{
		ASE_AWK_FREE (run->awk, run->global.ofmt.ptr);
		run->global.ofmt.ptr = ASE_NULL;
		run->global.ofmt.len = 0;
	}

	if (run->global.ofs.ptr != ASE_NULL && 
	    run->global.ofs.ptr != DEFAULT_OFS)
	{
		ASE_AWK_FREE (run->awk, run->global.ofs.ptr);
		run->global.ofs.ptr = ASE_NULL;
		run->global.ofs.len = 0;
	}

	if (run->global.ors.ptr != ASE_NULL && 
	    run->global.ors.ptr != DEFAULT_ORS &&
	    run->global.ors.ptr != DEFAULT_ORS_CRLF)
	{
		ASE_AWK_FREE (run->awk, run->global.ors.ptr);
		run->global.ors.ptr = ASE_NULL;
		run->global.ors.len = 0;
	}

	if (run->global.subsep.ptr != ASE_NULL && 
	    run->global.subsep.ptr != DEFAULT_SUBSEP)
	{
		ASE_AWK_FREE (run->awk, run->global.subsep.ptr);
		run->global.subsep.ptr = ASE_NULL;
		run->global.subsep.len = 0;
	}

	ASE_AWK_FREE (run->awk, run->format.tmp.ptr);
	run->format.tmp.ptr = ASE_NULL;
	run->format.tmp.len = 0;
	ase_str_close (&run->format.fmt);
	ase_str_close (&run->format.out);

	/* destroy input record. ase_awk_clrrec should be called
	 * before the run stack has been destroyed because it may try
	 * to change the value to ASE_AWK_GLOBAL_NF. */
	ase_awk_clrrec (run, ase_false);  
	if (run->inrec.flds != ASE_NULL) 
	{
		ASE_AWK_FREE (run->awk, run->inrec.flds);
		run->inrec.flds = ASE_NULL;
		run->inrec.maxflds = 0;
	}
	ase_str_close (&run->inrec.line);

	/* destroy run stack */
	if (run->stack != ASE_NULL)
	{
		ASE_ASSERT (run->stack_top == 0);

		ASE_AWK_FREE (run->awk, run->stack);
		run->stack = ASE_NULL;
		run->stack_top = 0;
		run->stack_base = 0;
		run->stack_limit = 0;
	}

	/* destroy named variables */
	ase_map_close (run->named);

	/* destroy values in free list */
	while (run->fcache_count > 0)
	{
		ase_awk_val_ref_t* tmp = run->fcache[--run->fcache_count];
		ase_awk_freeval (run, (ase_awk_val_t*)tmp, ase_false);
	}

	/*while (run->scache32_count > 0)
	{
		ase_awk_val_str_t* tmp = run->scache32[--run->scache32_count];
		ase_awk_freeval (run, (ase_awk_val_t*)tmp, ase_false);
	}

	while (run->scache64_count > 0)
	{
		ase_awk_val_str_t* tmp = run->scache64[--run->scache64_count];
		ase_awk_freeval (run, (ase_awk_val_t*)tmp, ase_false);
	}*/

	while (run->vmgr.ichunk != ASE_NULL)
	{
		ase_awk_val_chunk_t* next = run->vmgr.ichunk->next;
		ASE_AWK_FREE (run->awk, run->vmgr.ichunk);
		run->vmgr.ichunk = next;
	}

	while (run->vmgr.rchunk != ASE_NULL)
	{
		ase_awk_val_chunk_t* next = run->vmgr.rchunk->next;
		ASE_AWK_FREE (run->awk, run->vmgr.rchunk);
		run->vmgr.rchunk = next;
	}
}

static int build_runarg (
	ase_awk_run_t* run, ase_awk_runarg_t* runarg, ase_size_t* nargs)
{
	ase_awk_runarg_t* p;
	ase_size_t argc;
	ase_awk_val_t* v_argc;
	ase_awk_val_t* v_argv;
	ase_awk_val_t* v_tmp;
	ase_char_t key[ASE_SIZEOF(ase_long_t)*8+2];
	ase_size_t key_len;

	v_argv = ase_awk_makemapval (run);
	if (v_argv == ASE_NULL) return -1;

	ase_awk_refupval (run, v_argv);

	if (runarg == ASE_NULL) argc = 0;
	else
	{
		for (argc = 0, p = runarg; p->ptr != ASE_NULL; argc++, p++)
		{
			v_tmp = ase_awk_makestrval (run, p->ptr, p->len);
			if (v_tmp == ASE_NULL)
			{
				ase_awk_refdownval (run, v_argv);
				return -1;
			}

			if (ase_awk_getoption(run->awk) & ASE_AWK_BASEONE)
			{
				key_len = ase_awk_longtostr (argc+1, 
					10, ASE_NULL, key, ASE_COUNTOF(key));
			}
			else
			{
				key_len = ase_awk_longtostr (argc, 
					10, ASE_NULL, key, ASE_COUNTOF(key));
			}
			ASE_ASSERT (key_len != (ase_size_t)-1);

			/* increment reference count of v_tmp in advance as if 
			 * it has successfully been assigned into ARGV. */
			ase_awk_refupval (run, v_tmp);

			if (ase_map_putx (
				((ase_awk_val_map_t*)v_argv)->map,
				key, key_len, v_tmp, ASE_NULL) == -1)
			{
				/* if the assignment operation fails, decrements
				 * the reference of v_tmp to free it */
				ase_awk_refdownval (run, v_tmp);

				/* the values previously assigned into the
				 * map will be freeed when v_argv is freed */
				ase_awk_refdownval (run, v_argv);

				ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
				return -1;
			}
		}
	}

	v_argc = ase_awk_makeintval (run, (ase_long_t)argc);
	if (v_argc == ASE_NULL)
	{
		ase_awk_refdownval (run, v_argv);
		return -1;
	}

	ase_awk_refupval (run, v_argc);

	ASE_ASSERT (
		STACK_GLOBAL(run,ASE_AWK_GLOBAL_ARGC) == ase_awk_val_nil);

	if (ase_awk_setglobal (run, ASE_AWK_GLOBAL_ARGC, v_argc) == -1) 
	{
		ase_awk_refdownval (run, v_argc);
		ase_awk_refdownval (run, v_argv);
		return -1;
	}

	if (ase_awk_setglobal (run, ASE_AWK_GLOBAL_ARGV, v_argv) == -1)
	{
		/* ARGC is assigned nil when ARGV assignment has failed.
		 * However, this requires preconditions, as follows:
		 *  1. build_runarg should be called in a proper place
		 *     as it is not a generic-purpose routine.
		 *  2. ARGC should be nil before build_runarg is called 
		 * If the restoration fails, nothing can salvage it. */
		ase_awk_setglobal (run, ASE_AWK_GLOBAL_ARGC, ase_awk_val_nil);
		ase_awk_refdownval (run, v_argc);
		ase_awk_refdownval (run, v_argv);
		return -1;
	}

	ase_awk_refdownval (run, v_argc);
	ase_awk_refdownval (run, v_argv);

	*nargs = argc;
	return 0;
}

static void cleanup_globals (ase_awk_run_t* run)
{
	ase_size_t nglobals = run->awk->tree.nglobals;
	while (nglobals > 0)
	{
		--nglobals;
		ase_awk_refdownval (run, STACK_GLOBAL(run,nglobals));
		STACK_GLOBAL (run, nglobals) = ase_awk_val_nil;
	}
}

static int update_fnr (ase_awk_run_t* run, ase_long_t fnr, ase_long_t nr)
{
	ase_awk_val_t* tmp1, * tmp2;

	tmp1 = ase_awk_makeintval (run, fnr);
	if (tmp1 == ASE_NULL) return -1;

	ase_awk_refupval (run, tmp1);

	if (nr == fnr) tmp2 = tmp1;
	else
	{
		tmp2 = ase_awk_makeintval (run, nr);
		if (tmp2 == ASE_NULL)
		{
			ase_awk_refdownval (run, tmp1);
			return -1;
		}

		ase_awk_refupval (run, tmp2);
	}


	if (ase_awk_setglobal (run, ASE_AWK_GLOBAL_FNR, tmp1) == -1)
	{
		if (nr != fnr) ase_awk_refdownval (run, tmp2);
		ase_awk_refdownval (run, tmp1);
		return -1;
	}

	if (ase_awk_setglobal (run, ASE_AWK_GLOBAL_NR, tmp2) == -1)
	{
		if (nr != fnr) ase_awk_refdownval (run, tmp2);
		ase_awk_refdownval (run, tmp1);
		return -1;
	}

	if (nr != fnr) ase_awk_refdownval (run, tmp2);
	ase_awk_refdownval (run, tmp1);
	return 0;
}

static int set_globals_to_default (ase_awk_run_t* run)
{
	struct gtab_t
	{
		int idx;
		const ase_char_t* str;
	} gtab[] =
	{
		{ ASE_AWK_GLOBAL_CONVFMT,   DEFAULT_CONVFMT },
		{ ASE_AWK_GLOBAL_FILENAME,  ASE_NULL },
		{ ASE_AWK_GLOBAL_OFILENAME, ASE_NULL },
		{ ASE_AWK_GLOBAL_OFMT,      DEFAULT_OFMT },
		{ ASE_AWK_GLOBAL_OFS,       DEFAULT_OFS },
		{ ASE_AWK_GLOBAL_ORS,       DEFAULT_ORS },
		{ ASE_AWK_GLOBAL_SUBSEP,    DEFAULT_SUBSEP },
	};

	ase_awk_val_t* tmp;
	ase_size_t i, j;

	if (run->awk->option & ASE_AWK_CRLF)
	{
		/* ugly */
		gtab[5].str = DEFAULT_ORS_CRLF;
	}

	for (i = 0; i < ASE_COUNTOF(gtab); i++)
	{
		if (gtab[i].str == ASE_NULL || gtab[i].str[0] == ASE_T('\0'))
		{
			tmp = ase_awk_val_zls;
		}
		else 
		{
			tmp = ase_awk_makestrval0 (run, gtab[i].str);
			if (tmp == ASE_NULL) return -1;
		}
		
		ase_awk_refupval (run, tmp);

		ASE_ASSERT (
			STACK_GLOBAL(run,gtab[i].idx) == ase_awk_val_nil);

		if (ase_awk_setglobal (run, gtab[i].idx, tmp) == -1)
		{
			for (j = 0; j < i; j++)
			{
				ase_awk_setglobal (
					run, gtab[i].idx, ase_awk_val_nil);
			}

			ase_awk_refdownval (run, tmp);
			return -1;
		}

		ase_awk_refdownval (run, tmp);
	}

	return 0;
}

struct capture_retval_data_t
{
	ase_awk_run_t* run;
	ase_awk_val_t* val;	
};

static void capture_retval_on_exit (void* arg)
{
	struct capture_retval_data_t* data;

	data = (struct capture_retval_data_t*)arg;
	data->val = STACK_RETVAL(data->run);
	ase_awk_refupval (data->run, data->val);
}

static int run_main (
	ase_awk_run_t* run, const ase_char_t* main, 
	ase_awk_runarg_t* runarg)
{
	ase_size_t nglobals, nargs, nrunargs, i;
	ase_size_t saved_stack_top;
	ase_awk_val_t* v;
	int n;

	ASE_ASSERT (run->stack_base == 0 && run->stack_top == 0);

	/* secure space for global variables */
	saved_stack_top = run->stack_top;

	nglobals = run->awk->tree.nglobals;

	while (nglobals > 0)
	{
		--nglobals;
		if (__raw_push(run,ase_awk_val_nil) == -1)
		{
			/* restore the stack_top with the saved value
			 * instead of calling __raw_pop as many times as
			 * the successful __raw_push. it is ok because
			 * the values pushed so far are all ase_awk_val_nil */
			run->stack_top = saved_stack_top;
			ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
			return -1;
		}
	}	

	if (ase_awk_setglobal (run, ASE_AWK_GLOBAL_NF, ase_awk_val_zero) == -1)
	{
		/* it can simply restore the top of the stack this way
		 * because the values pused onto the stack so far are
		 * all ase_awk_val_nils  and ase_awk_val_zeros */
		run->stack_top = saved_stack_top;
		return -1;
	}
	
	if (build_runarg (run, runarg, &nrunargs) == -1)
	{
		/* it can simply restore the top of the stack this way
		 * because the values pused onto the stack so far are
		 * all ase_awk_val_nils and ase_awk_val_zeros and 
		 * build_runarg doesn't push other values than them
		 * when it has failed */
		run->stack_top = saved_stack_top;
		return -1;
	}

	run->exit_level = EXIT_NONE;

	n = update_fnr (run, 0, 0);
	if (n == 0) n = set_globals_to_default (run);
	if (n == 0 && main != ASE_NULL)
	{
		/* run the given function */
		struct capture_retval_data_t crdata;
		ase_awk_nde_call_t nde;

		nde.type = ASE_AWK_NDE_AFN;
		nde.line = 0;
		nde.next = ASE_NULL;
		nde.what.afn.name.ptr = (ase_char_t*)main;
		nde.what.afn.name.len = ase_strlen(main);

		nde.args = ASE_NULL;
		nde.nargs = 0;

		if (runarg != ASE_NULL)
		{

			if (!(run->awk->option & ASE_AWK_ARGSTOMAIN)) 
			{
				/* if the option is not set, the arguments 
				 * are not passed to the main function as 
				 * parameters */
				nrunargs = 0;
			}

			/* prepare to pass the arguments to the main function */
			for (i = nrunargs; i > 0; )
			{
				ase_awk_nde_str_t* tmp, * tmp2;

				i--;
				tmp = (ase_awk_nde_str_t*) ASE_AWK_MALLOC (
					run->awk, ASE_SIZEOF(*tmp));
				if (tmp == ASE_NULL)
				{
					tmp = (ase_awk_nde_str_t*)nde.args;
					while (tmp != ASE_NULL)
					{
						tmp2 = (ase_awk_nde_str_t*)tmp->next;
						ASE_AWK_FREE (run->awk, tmp->buf);
						ASE_AWK_FREE (run->awk, tmp);
						tmp = tmp2;
					}
					cleanup_globals (run);
					run->stack_top = saved_stack_top;

					ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
					return -1;
				}

				tmp->type = ASE_AWK_NDE_STR;
				tmp->buf = ase_awk_strxdup (run->awk,
					runarg[i].ptr, runarg[i].len);
				if (tmp->buf == ASE_NULL)
				{
					ASE_AWK_FREE (run->awk, tmp);
					tmp = (ase_awk_nde_str_t*)nde.args;
					while (tmp != ASE_NULL)
					{
						tmp2 = (ase_awk_nde_str_t*)tmp->next;
						ASE_AWK_FREE (run->awk, tmp->buf);
						ASE_AWK_FREE (run->awk, tmp);
						tmp = tmp2;
					} 
					cleanup_globals (run);
					run->stack_top = saved_stack_top;

					ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
					return -1;
				}

				tmp->len = runarg[i].len;
				tmp->next = nde.args;
				nde.args = (ase_awk_nde_t*)tmp;
				nde.nargs++;
			}

			ASE_ASSERT (nrunargs == nde.nargs);
		}

		crdata.run = run;
		crdata.val = ASE_NULL;
		v = eval_afn_intrinsic (run, (ase_awk_nde_t*)&nde, 
			capture_retval_on_exit, &crdata);
		if (v == ASE_NULL) 
		{
			if (crdata.val == ASE_NULL) 
			{
				ASE_ASSERT (run->errnum != ASE_AWK_ENOERR);
				n = -1;
			}
			else 
			{
				if (run->errnum == ASE_AWK_ENOERR)
				{
					if (run->cbs != ASE_NULL && run->cbs->on_return != ASE_NULL)
					{
						run->cbs->on_return (run, crdata.val, run->cbs->custom_data);
					}
				}
				else n = -1;

				ase_awk_refdownval(run, crdata.val);
			}
		}
		else
		{
			ase_awk_refupval (run, v);

			if (run->cbs != ASE_NULL && run->cbs->on_return != ASE_NULL)
			{
				run->cbs->on_return (run, v, run->cbs->custom_data);
			}

			ase_awk_refdownval (run, v);
		}

		if (nde.args != ASE_NULL) ase_awk_clrpt (run->awk, nde.args);
	}
	else if (n == 0)
	{
		ase_awk_nde_t* nde;

		/* no main function is specified. 
		 * run the normal patter blocks including BEGIN and END */
		saved_stack_top = run->stack_top;

		if (__raw_push(run,(void*)run->stack_base) == -1) 
		{
			/* restore the stack top in a cheesy(?) way */
			run->stack_top = saved_stack_top;
			/* pops off global variables in a decent way */	
			cleanup_globals (run);
			__raw_pop_times (run, run->awk->tree.nglobals);

			ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
			return -1;
		}

		if (__raw_push(run,(void*)saved_stack_top) == -1) 
		{
			run->stack_top = saved_stack_top;
			cleanup_globals (run);
			__raw_pop_times (run, run->awk->tree.nglobals);

			ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
			return -1;
		}
	
		/* secure space for a return value */
		if (__raw_push(run,ase_awk_val_nil) == -1)
		{
			run->stack_top = saved_stack_top;
			cleanup_globals (run);
			__raw_pop_times (run, run->awk->tree.nglobals);

			ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
			return -1;
		}
	
		/* secure space for nargs */
		if (__raw_push(run,ase_awk_val_nil) == -1)
		{
			run->stack_top = saved_stack_top;
			cleanup_globals (run);
			__raw_pop_times (run, run->awk->tree.nglobals);

			ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
			return -1;
		}
	
		run->stack_base = saved_stack_top;
	
		/* set nargs to zero */
		nargs = 0;
		STACK_NARGS(run) = (void*)nargs;
	
		/* stack set up properly. ready to exeucte statement blocks */
		for (nde = run->awk->tree.begin; 
		     n == 0 && nde != ASE_NULL && run->exit_level < EXIT_GLOBAL;
		     nde = nde->next)
		{
			ase_awk_nde_blk_t* blk;

			blk = (ase_awk_nde_blk_t*)nde;
			ASE_ASSERT (blk->type == ASE_AWK_NDE_BLK);

			run->active_block = blk;
			run->exit_level = EXIT_NONE;
			if (run_block (run, blk) == -1) n = -1;
		}

		if (n == -1 && run->errnum == ASE_AWK_ENOERR) 
		{
			/* an error is returned with no error number set.
			 * this feature is used by eval_expression to
			 * abort the evaluation when exit() is executed 
			 * during function evaluation */
			n = 0;
			run->errlin = 0;
			run->errmsg[0] = ASE_T('\0');
		}

		if (n == 0 && 
		    (run->awk->tree.chain != ASE_NULL ||
		     run->awk->tree.end != ASE_NULL) && 
		     run->exit_level < EXIT_GLOBAL)
		{
			if (run_pattern_blocks (run) == -1) n = -1;
		}

		if (n == -1 && run->errnum == ASE_AWK_ENOERR)
		{
			/* an error is returned with no error number set.
			 * this feature is used by eval_expression to
			 * abort the evaluation when exit() is executed 
			 * during function evaluation */
			n = 0;
			run->errlin = 0;
			run->errmsg[0] = ASE_T('\0');
		}

		/* the first END block is executed if the program is not
		 * explicitly aborted with ase_awk_stop */
		for (nde = run->awk->tree.end;
		     n == 0 && nde != ASE_NULL && run->exit_level < EXIT_ABORT;
		     nde = nde->next) 
		{
			ase_awk_nde_blk_t* blk;

			blk = (ase_awk_nde_blk_t*)nde;
			ASE_ASSERT (blk->type == ASE_AWK_NDE_BLK);

			run->active_block = blk;
			run->exit_level = EXIT_NONE;
			if (run_block (run, blk) == -1) n = -1;
			else if (run->exit_level >= EXIT_GLOBAL) 
			{
				/* once exit is called inside one of END blocks,
				 * subsequent END blocks must not be executed */
				break;
			}
		}

		if (n == -1 && run->errnum == ASE_AWK_ENOERR)
		{
			/* an error is returned with no error number set.
			 * this feature is used by eval_expression to
			 * abort the evaluation when exit() is executed 
			 * during function evaluation */
			n = 0;
			run->errlin = 0;
			run->errmsg[0] = ASE_T('\0');
		}

		/* restore stack */
		nargs = (ase_size_t)STACK_NARGS(run);
		ASE_ASSERT (nargs == 0);
		for (i = 0; i < nargs; i++)
		{
			ase_awk_refdownval (run, STACK_ARG(run,i));
		}

		v = STACK_RETVAL(run);
		if (n == 0)
		{
			if (run->cbs != ASE_NULL && run->cbs->on_return != ASE_NULL)
			{
				run->cbs->on_return (run, v, run->cbs->custom_data);
			}
		}
		/* end the life of the global return value */
		ase_awk_refdownval (run, v);

		run->stack_top = 
			(ase_size_t)run->stack[run->stack_base+1];
		run->stack_base = 
			(ase_size_t)run->stack[run->stack_base+0];
	}

	/* pops off the global variables */
	nglobals = run->awk->tree.nglobals;
	while (nglobals > 0)
	{
		--nglobals;
		ase_awk_refdownval (run, STACK_GLOBAL(run,nglobals));
		__raw_pop (run);
	}

	/* reset the exit level */
	run->exit_level = EXIT_NONE;

	return n;
}

static int run_pattern_blocks (ase_awk_run_t* run)
{
	int n;

#define ADJUST_ERROR_LINE(run) \
	if (run->awk->tree.chain != ASE_NULL) \
	{ \
		if (run->awk->tree.chain->pattern != ASE_NULL) \
			run->errlin = run->awk->tree.chain->pattern->line; \
		else if (run->awk->tree.chain->action != ASE_NULL) \
			run->errlin = run->awk->tree.chain->action->line; \
	} \
	else if (run->awk->tree.end != ASE_NULL) \
	{ \
		run->errlin = run->awk->tree.end->line; \
	} 

	run->inrec.buf_pos = 0;
	run->inrec.buf_len = 0;
	run->inrec.eof = ase_false;

	/* run each pattern block */
	while (run->exit_level < EXIT_GLOBAL)
	{
		run->exit_level = EXIT_NONE;

		n = read_record (run);
		if (n == -1) 
		{
			ADJUST_ERROR_LINE (run);
			return -1; /* error */
		}
		if (n == 0) break; /* end of input */

		if (update_fnr (run, run->global.fnr+1, run->global.nr+1) == -1) 
		{
			ADJUST_ERROR_LINE (run);
			return -1;
		}

		if (run->awk->tree.chain != ASE_NULL)
		{
			if (run_pattern_block_chain (
				run, run->awk->tree.chain) == -1) return -1;
		}
	}

#undef ADJUST_ERROR_LINE
	return 0;
}

static int run_pattern_block_chain (ase_awk_run_t* run, ase_awk_chain_t* chain)
{
	ase_size_t block_no = 0;

	while (run->exit_level < EXIT_GLOBAL && chain != ASE_NULL)
	{
		if (run->exit_level == EXIT_NEXT)
		{
			run->exit_level = EXIT_NONE;
			break;
		}

		if (run_pattern_block (run, chain, block_no) == -1) return -1;

		chain = chain->next; 
		block_no++;
	}

	return 0;
}

static int run_pattern_block (
	ase_awk_run_t* run, ase_awk_chain_t* chain, ase_size_t block_no)
{
	ase_awk_nde_t* ptn;
	ase_awk_nde_blk_t* blk;

	ptn = chain->pattern;
	blk = (ase_awk_nde_blk_t*)chain->action;

	if (ptn == ASE_NULL)
	{
		/* just execute the block */
		run->active_block = blk;
		if (run_block (run, blk) == -1) return -1;
	}
	else
	{
		if (ptn->next == ASE_NULL)
		{
			/* pattern { ... } */
			ase_awk_val_t* v1;

			v1 = eval_expression (run, ptn);
			if (v1 == ASE_NULL) return -1;

			ase_awk_refupval (run, v1);

			if (ase_awk_valtobool (run, v1))
			{
				run->active_block = blk;
				if (run_block (run, blk) == -1) 
				{
					ase_awk_refdownval (run, v1);
					return -1;
				}
			}

			ase_awk_refdownval (run, v1);
		}
		else
		{
			/* pattern, pattern { ... } */
			ASE_ASSERT (ptn->next->next == ASE_NULL);
			ASE_ASSERT (run->pattern_range_state != ASE_NULL);

			if (run->pattern_range_state[block_no] == 0)
			{
				ase_awk_val_t* v1;

				v1 = eval_expression (run, ptn);
				if (v1 == ASE_NULL) return -1;
				ase_awk_refupval (run, v1);

				if (ase_awk_valtobool (run, v1))
				{
					run->active_block = blk;
					if (run_block (run, blk) == -1) 
					{
						ase_awk_refdownval (run, v1);
						return -1;
					}

					run->pattern_range_state[block_no] = 1;
				}

				ase_awk_refdownval (run, v1);
			}
			else if (run->pattern_range_state[block_no] == 1)
			{
				ase_awk_val_t* v2;

				v2 = eval_expression (run, ptn->next);
				if (v2 == ASE_NULL) return -1;
				ase_awk_refupval (run, v2);

				run->active_block = blk;
				if (run_block (run, blk) == -1) 
				{
					ase_awk_refdownval (run, v2);
					return -1;
				}

				if (ase_awk_valtobool (run, v2)) 
					run->pattern_range_state[block_no] = 0;

				ase_awk_refdownval (run, v2);
			}
		}
	}

	return 0;
}

static int run_block (ase_awk_run_t* run, ase_awk_nde_blk_t* nde)
{
	int n;

	if (run->depth.max.block > 0 &&
	    run->depth.cur.block >= run->depth.max.block)
	{
		ase_awk_setrunerror (
			run, ASE_AWK_EBLKNST, nde->line, ASE_NULL, 0);
		return -1;;
	}

	run->depth.cur.block++;
	n = run_block0 (run, nde);
	run->depth.cur.block--;
	
	return n;
}

static int run_block0 (ase_awk_run_t* run, ase_awk_nde_blk_t* nde)
{
	ase_awk_nde_t* p;
	ase_size_t nlocals;
	ase_size_t saved_stack_top;
	int n = 0;

	if (nde == ASE_NULL)
	{
		/* blockless pattern - execute print $0*/
		ase_awk_refupval (run, run->inrec.d0);

		n = ase_awk_writeextio_str (run, 
			ASE_AWK_OUT_CONSOLE, ASE_T(""),
			ASE_STR_BUF(&run->inrec.line),
			ASE_STR_LEN(&run->inrec.line));
		if (n == -1)
		{
			ase_awk_refdownval (run, run->inrec.d0);

			/* adjust the error line */
			run->errlin = nde->line;
			return -1;
		}

		n = ase_awk_writeextio_str (
			run, ASE_AWK_OUT_CONSOLE, ASE_T(""),
			run->global.ors.ptr, run->global.ors.len);
		if (n == -1)
		{
			ase_awk_refdownval (run, run->inrec.d0);

			/* adjust the error line */
			run->errlin = nde->line;
			return -1;
		}

		ase_awk_refdownval (run, run->inrec.d0);
		return 0;
	}

	ASE_ASSERT (nde->type == ASE_AWK_NDE_BLK);

	p = nde->body;
	nlocals = nde->nlocals;

#ifdef DEBUG_RUN
	ase_dprintf (
		ASE_T("securing space for local variables nlocals = %d\n"), 
		(int)nlocals);
#endif

	saved_stack_top = run->stack_top;

	/* secure space for local variables */
	while (nlocals > 0)
	{
		--nlocals;
		if (__raw_push(run,ase_awk_val_nil) == -1)
		{
			/* restore stack top */
			run->stack_top = saved_stack_top;
			return -1;
		}

		/* refupval is not required for ase_awk_val_nil */
	}

#ifdef DEBUG_RUN
	ase_dprintf (ASE_T("executing block statements\n"));
#endif

	while (p != ASE_NULL && run->exit_level == EXIT_NONE)
	{
		if (run_statement (run, p) == -1) 
		{
			n = -1;
			break;
		}
		p = p->next;
	}

	/* pop off local variables */
#ifdef DEBUG_RUN
	ase_dprintf (ASE_T("popping off local variables\n"));
#endif
	nlocals = nde->nlocals;
	while (nlocals > 0)
	{
		--nlocals;
		ase_awk_refdownval (run, STACK_LOCAL(run,nlocals));
		__raw_pop (run);
	}

	return n;
}

#define ON_STATEMENT(run,nde) \
	if ((run)->awk->stopall) (run)->exit_level = EXIT_ABORT; \
	if ((run)->cbs != ASE_NULL &&  \
	    (run)->cbs->on_statement != ASE_NULL) \
	{ \
		(run)->cbs->on_statement ( \
			run, (nde)->line, (run)->cbs->custom_data); \
	} 

static int run_statement (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ON_STATEMENT (run, nde);

	switch (nde->type) 
	{
		case ASE_AWK_NDE_NULL:
		{
			/* do nothing */
			break;
		}

		case ASE_AWK_NDE_BLK:
		{
			if (run_block (run, 
				(ase_awk_nde_blk_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_IF:
		{
			if (run_if (run, 
				(ase_awk_nde_if_t*)nde) == -1) return -1;	
			break;
		}

		case ASE_AWK_NDE_WHILE:
		case ASE_AWK_NDE_DOWHILE:
		{
			if (run_while (run, 
				(ase_awk_nde_while_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_FOR:
		{
			if (run_for (run, 
				(ase_awk_nde_for_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_FOREACH:
		{
			if (run_foreach (run, 
				(ase_awk_nde_foreach_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_BREAK:
		{
			if (run_break (run, 
				(ase_awk_nde_break_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_CONTINUE:
		{
			if (run_continue (run, 
				(ase_awk_nde_continue_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_RETURN:
		{
			if (run_return (run, 
				(ase_awk_nde_return_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_EXIT:
		{
			if (run_exit (run, 
				(ase_awk_nde_exit_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_NEXT:
		{
			if (run_next (run, 
				(ase_awk_nde_next_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_NEXTFILE:
		{
			if (run_nextfile (run, 
				(ase_awk_nde_nextfile_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_DELETE:
		{
			if (run_delete (run, 
				(ase_awk_nde_delete_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_RESET:
		{
			if (run_reset (run, 
				(ase_awk_nde_reset_t*)nde) == -1) return -1;
			break;
		}


		case ASE_AWK_NDE_PRINT:
		{
			if (run_print (run, 
				(ase_awk_nde_print_t*)nde) == -1) return -1;
			break;
		}

		case ASE_AWK_NDE_PRINTF:
		{
			if (run_printf (run, 
				(ase_awk_nde_print_t*)nde) == -1) return -1;
			break;
		}

		default:
		{
			ase_awk_val_t* v;
			v = eval_expression (run, nde);
			if (v == ASE_NULL) return -1;

			/* destroy the value if not referenced */
			ase_awk_refupval (run, v);
			ase_awk_refdownval (run, v);

			break;
		}
	}

	return 0;
}

static int run_if (ase_awk_run_t* run, ase_awk_nde_if_t* nde)
{
	ase_awk_val_t* test;
	int n = 0;

	/* the test expression for the if statement cannot have 
	 * chained expressions. this should not be allowed by the
	 * parser first of all */
	ASE_ASSERT (nde->test->next == ASE_NULL);

	test = eval_expression (run, nde->test);
	if (test == ASE_NULL) return -1;

	ase_awk_refupval (run, test);
	if (ase_awk_valtobool (run, test))
	{
		n = run_statement (run, nde->then_part);
	}
	else if (nde->else_part != ASE_NULL)
	{
		n = run_statement (run, nde->else_part);
	}

	ase_awk_refdownval (run, test); /* TODO: is this correct?*/
	return n;
}

static int run_while (ase_awk_run_t* run, ase_awk_nde_while_t* nde)
{
	ase_awk_val_t* test;

	if (nde->type == ASE_AWK_NDE_WHILE)
	{
		/* no chained expressions are allowed for the test 
		 * expression of the while statement */
		ASE_ASSERT (nde->test->next == ASE_NULL);

		while (1)
		{
			ON_STATEMENT (run, nde->test);

			test = eval_expression (run, nde->test);
			if (test == ASE_NULL) return -1;

			ase_awk_refupval (run, test);

			if (ase_awk_valtobool (run, test))
			{
				if (run_statement(run,nde->body) == -1)
				{
					ase_awk_refdownval (run, test);
					return -1;
				}
			}
			else
			{
				ase_awk_refdownval (run, test);
				break;
			}

			ase_awk_refdownval (run, test);

			if (run->exit_level == EXIT_BREAK)
			{	
				run->exit_level = EXIT_NONE;
				break;
			}
			else if (run->exit_level == EXIT_CONTINUE)
			{
				run->exit_level = EXIT_NONE;
			}
			else if (run->exit_level != EXIT_NONE) break;

		}
	}
	else if (nde->type == ASE_AWK_NDE_DOWHILE)
	{
		/* no chained expressions are allowed for the test 
		 * expression of the while statement */
		ASE_ASSERT (nde->test->next == ASE_NULL);

		do
		{
			if (run_statement(run,nde->body) == -1) return -1;

			if (run->exit_level == EXIT_BREAK)
			{	
				run->exit_level = EXIT_NONE;
				break;
			}
			else if (run->exit_level == EXIT_CONTINUE)
			{
				run->exit_level = EXIT_NONE;
			}
			else if (run->exit_level != EXIT_NONE) break;

			ON_STATEMENT (run, nde->test);

			test = eval_expression (run, nde->test);
			if (test == ASE_NULL) return -1;

			ase_awk_refupval (run, test);

			if (!ase_awk_valtobool (run, test))
			{
				ase_awk_refdownval (run, test);
				break;
			}

			ase_awk_refdownval (run, test);
		}
		while (1);
	}

	return 0;
}

static int run_for (ase_awk_run_t* run, ase_awk_nde_for_t* nde)
{
	ase_awk_val_t* val;

	if (nde->init != ASE_NULL)
	{
		ASE_ASSERT (nde->init->next == ASE_NULL);

		ON_STATEMENT (run, nde->init);
		val = eval_expression(run,nde->init);
		if (val == ASE_NULL) return -1;

		ase_awk_refupval (run, val);
		ase_awk_refdownval (run, val);
	}

	while (1)
	{
		if (nde->test != ASE_NULL)
		{
			ase_awk_val_t* test;

			/* no chained expressions for the test expression of
			 * the for statement are allowed */
			ASE_ASSERT (nde->test->next == ASE_NULL);

			ON_STATEMENT (run, nde->test);
			test = eval_expression (run, nde->test);
			if (test == ASE_NULL) return -1;

			ase_awk_refupval (run, test);
			if (ase_awk_valtobool (run, test))
			{
				if (run_statement(run,nde->body) == -1)
				{
					ase_awk_refdownval (run, test);
					return -1;
				}
			}
			else
			{
				ase_awk_refdownval (run, test);
				break;
			}

			ase_awk_refdownval (run, test);
		}	
		else
		{
			if (run_statement(run,nde->body) == -1) return -1;
		}

		if (run->exit_level == EXIT_BREAK)
		{	
			run->exit_level = EXIT_NONE;
			break;
		}
		else if (run->exit_level == EXIT_CONTINUE)
		{
			run->exit_level = EXIT_NONE;
		}
		else if (run->exit_level != EXIT_NONE) break;

		if (nde->incr != ASE_NULL)
		{
			ASE_ASSERT (nde->incr->next == ASE_NULL);

			ON_STATEMENT (run, nde->incr);
			val = eval_expression (run, nde->incr);
			if (val == ASE_NULL) return -1;

			ase_awk_refupval (run, val);
			ase_awk_refdownval (run, val);
		}
	}

	return 0;
}

struct __foreach_walker_t
{
	ase_awk_run_t* run;
	ase_awk_nde_t* var;
	ase_awk_nde_t* body;
};

static int __walk_foreach (ase_pair_t* pair, void* arg)
{
	struct __foreach_walker_t* w = (struct __foreach_walker_t*)arg;
	ase_awk_val_t* str;

	str = (ase_awk_val_t*) ase_awk_makestrval (
		w->run, ASE_PAIR_KEYPTR(pair), ASE_PAIR_KEYLEN(pair));
	if (str == ASE_NULL) 
	{
		/* adjust the error line */
		w->run->errlin = w->var->line;
		return -1;
	}

	ase_awk_refupval (w->run, str);
	if (do_assignment (w->run, w->var, str) == ASE_NULL)
	{
		ase_awk_refdownval (w->run, str);
		return -1;
	}

	if (run_statement (w->run, w->body) == -1)
	{
		ase_awk_refdownval (w->run, str);
		return -1;
	}
	
	ase_awk_refdownval (w->run, str);
	return 0;
}

static int run_foreach (ase_awk_run_t* run, ase_awk_nde_foreach_t* nde)
{
	int n;
	ase_awk_nde_exp_t* test;
	ase_awk_val_t* rv;
	ase_map_t* map;
	struct __foreach_walker_t walker;

	test = (ase_awk_nde_exp_t*)nde->test;
	ASE_ASSERT (
		test->type == ASE_AWK_NDE_EXP_BIN && 
		test->opcode == ASE_AWK_BINOP_IN);

	/* chained expressions should not be allowed 
	 * by the parser first of all */
	ASE_ASSERT (test->right->next == ASE_NULL); 

	rv = eval_expression (run, test->right);
	if (rv == ASE_NULL) return -1;

	ase_awk_refupval (run, rv);
	if (rv->type != ASE_AWK_VAL_MAP)
	{
		ase_awk_refdownval (run, rv);

		ase_awk_setrunerror (
			run, ASE_AWK_ENOTMAPIN, test->right->line, ASE_NULL, 0);
		return -1;
	}
	map = ((ase_awk_val_map_t*)rv)->map;

	walker.run = run;
	walker.var = test->left;
	walker.body = nde->body;
	n = ase_map_walk (map, __walk_foreach, &walker);

	ase_awk_refdownval (run, rv);
	return n;
}

static int run_break (ase_awk_run_t* run, ase_awk_nde_break_t* nde)
{
	run->exit_level = EXIT_BREAK;
	return 0;
}

static int run_continue (ase_awk_run_t* run, ase_awk_nde_continue_t* nde)
{
	run->exit_level = EXIT_CONTINUE;
	return 0;
}

static int run_return (ase_awk_run_t* run, ase_awk_nde_return_t* nde)
{
	if (nde->val != ASE_NULL)
	{
		ase_awk_val_t* val;

		/* chained expressions should not be allowed 
		 * by the parser first of all */
		ASE_ASSERT (nde->val->next == ASE_NULL); 

		val = eval_expression (run, nde->val);
		if (val == ASE_NULL) return -1;

		if ((run->awk->option & ASE_AWK_MAPTOVAR) == 0)
		{
			if (val->type == ASE_AWK_VAL_MAP)
			{
				/* cannot return a map */
				ase_awk_refupval (run, val);
				ase_awk_refdownval (run, val);

				ase_awk_setrunerror (
					run, ASE_AWK_EMAPNOTALLOWED, 
					nde->line, ASE_NULL, 0);
				return -1;
			}
		}

		ase_awk_refdownval (run, STACK_RETVAL(run));
		STACK_RETVAL(run) = val;
		ase_awk_refupval (run, val); /* see eval_call for the trick */
	}
	
	run->exit_level = EXIT_FUNCTION;
	return 0;
}

static int run_exit (ase_awk_run_t* run, ase_awk_nde_exit_t* nde)
{
	if (nde->val != ASE_NULL)
	{
		ase_awk_val_t* val;

		/* chained expressions should not be allowed 
		 * by the parser first of all */
		ASE_ASSERT (nde->val->next == ASE_NULL); 

		val = eval_expression (run, nde->val);
		if (val == ASE_NULL) return -1;

		ase_awk_refdownval (run, STACK_RETVAL_GLOBAL(run));
		STACK_RETVAL_GLOBAL(run) = val; /* global return value */

		ase_awk_refupval (run, val);
	}

	run->exit_level = EXIT_GLOBAL;
	return 0;
}

static int run_next (ase_awk_run_t* run, ase_awk_nde_next_t* nde)
{
	/* the parser checks if next has been called in the begin/end
	 * block or whereever inappropriate. so the runtime doesn't 
	 * check that explicitly */
	if  (run->active_block == (ase_awk_nde_blk_t*)run->awk->tree.begin)
	{
		ase_awk_setrunerror (
			run, ASE_AWK_ERNEXTBEG, nde->line, ASE_NULL, 0);
		return -1;
	}
	else if (run->active_block == (ase_awk_nde_blk_t*)run->awk->tree.end)
	{
		ase_awk_setrunerror (
			run, ASE_AWK_ERNEXTEND, nde->line, ASE_NULL, 0);
		return -1;
	}

	run->exit_level = EXIT_NEXT;
	return 0;
}

static int run_nextinfile (ase_awk_run_t* run, ase_awk_nde_nextfile_t* nde)
{
	int n;

	/* normal nextfile statement */
	if  (run->active_block == (ase_awk_nde_blk_t*)run->awk->tree.begin)
	{
		ase_awk_setrunerror (
			run, ASE_AWK_ERNEXTFBEG, nde->line, ASE_NULL, 0);
		return -1;
	}
	else if (run->active_block == (ase_awk_nde_blk_t*)run->awk->tree.end)
	{
		ase_awk_setrunerror (
			run, ASE_AWK_ERNEXTFEND, nde->line, ASE_NULL, 0);
		return -1;
	}

	n = ase_awk_nextextio_read (run, ASE_AWK_IN_CONSOLE, ASE_T(""));
	if (n == -1)
	{
		/* adjust the error line */
		run->errlin = nde->line;
		return -1;
	}

	if (n == 0)
	{
		/* no more input console */
		run->exit_level = EXIT_GLOBAL;
		return 0;
	}

	/* FNR resets to 0, NR remains the same */
	if (update_fnr (run, 0, run->global.nr) == -1) 
	{
		run->errlin = nde->line;
		return -1;
	}

	run->exit_level = EXIT_NEXT;
	return 0;

}

static int run_nextoutfile (ase_awk_run_t* run, ase_awk_nde_nextfile_t* nde)
{
	int n;

	/* nextofile can be called from BEGIN and END block unlike nextfile */

	n = ase_awk_nextextio_write (run, ASE_AWK_OUT_CONSOLE, ASE_T(""));
	if (n == -1)
	{
		/* adjust the error line */
		run->errlin = nde->line;
		return -1;
	}

	if (n == 0)
	{
		/* should it terminate the program there is no more 
		 * output console? no. there will just be no more console 
		 * output */
		/*run->exit_level = EXIT_GLOBAL;*/
		return 0;
	}

	return 0;
}

static int run_nextfile (ase_awk_run_t* run, ase_awk_nde_nextfile_t* nde)
{
	return (nde->out)? 
		run_nextoutfile (run, nde): 
		run_nextinfile (run, nde);
}

static int run_delete (ase_awk_run_t* run, ase_awk_nde_delete_t* nde)
{
	ase_awk_nde_var_t* var;

	var = (ase_awk_nde_var_t*) nde->var;

	if (var->type == ASE_AWK_NDE_NAMED ||
	    var->type == ASE_AWK_NDE_NAMEDIDX)
	{
		ase_pair_t* pair;

		ASE_ASSERTX (
			(var->type == ASE_AWK_NDE_NAMED && var->idx == ASE_NULL) ||
			(var->type == ASE_AWK_NDE_NAMEDIDX && var->idx != ASE_NULL),
			"if a named variable has an index part and a named indexed variable doesn't have an index part, the program is definitely wrong");

		pair = ase_map_get (
			run->named, var->id.name, var->id.name_len);
		if (pair == ASE_NULL)
		{
			ase_awk_val_t* tmp;

			/* value not set for the named variable. 
			 * create a map and assign it to the variable */

			tmp = ase_awk_makemapval (run);
			if (tmp == ASE_NULL) 
			{
				/* adjust error line */
				run->errlin = nde->line;
				return -1;
			}

			pair = ase_map_put (run->named, 
				var->id.name, var->id.name_len, tmp);
			if (pair == ASE_NULL)
			{
				ase_awk_refupval (run, tmp);
				ase_awk_refdownval (run, tmp);

				ase_awk_setrunerror (
					run, ASE_AWK_ENOMEM, var->line,
					ASE_NULL, 0);
				return -1;
			}

			/* as this is the assignment, it needs to update
			 * the reference count of the target value. */
			ase_awk_refupval (run, tmp);
		}
		else
		{
			ase_awk_val_t* val;
			ase_map_t* map;

			val = (ase_awk_val_t*)pair->val;
			ASE_ASSERT (val != ASE_NULL);

			if (val->type != ASE_AWK_VAL_MAP)
			{
				ase_cstr_t errarg;

				errarg.ptr = var->id.name; 
				errarg.len = var->id.name_len;

				ase_awk_setrunerror (
					run, ASE_AWK_ENOTDEL, var->line,
					&errarg, 1);
				return -1;
			}

			map = ((ase_awk_val_map_t*)val)->map;
			if (var->type == ASE_AWK_NDE_NAMEDIDX)
			{
				ase_char_t* key;
				ase_size_t keylen;
				ase_awk_val_t* idx;
				ase_char_t buf[IDXBUFSIZE];

				ASE_ASSERT (var->idx != ASE_NULL);

				idx = eval_expression (run, var->idx);
				if (idx == ASE_NULL) return -1;

				ase_awk_refupval (run, idx);

				/* try with a fixed-size buffer */
				keylen = ASE_COUNTOF(buf);
				key = ase_awk_valtostr (
					run, idx, ASE_AWK_VALTOSTR_FIXED, 
					(ase_str_t*)buf, &keylen);
				if (key == ASE_NULL)
				{
					/* if it doesn't work, switch to dynamic mode */
					key = ase_awk_valtostr (
						run, idx, ASE_AWK_VALTOSTR_CLEAR,
						ASE_NULL, &keylen);
				}

				ase_awk_refdownval (run, idx);

				if (key == ASE_NULL) 
				{
					/* change the error line */
					run->errlin = var->line;
					return -1;
				}

				ase_map_remove (map, key, keylen);
				if (key != buf) ASE_AWK_FREE (run->awk, key);
			}
			else
			{
				ase_map_clear (map);
			}
		}
	}
	else if (var->type == ASE_AWK_NDE_GLOBAL ||
	         var->type == ASE_AWK_NDE_LOCAL ||
	         var->type == ASE_AWK_NDE_ARG ||
	         var->type == ASE_AWK_NDE_GLOBALIDX ||
	         var->type == ASE_AWK_NDE_LOCALIDX ||
	         var->type == ASE_AWK_NDE_ARGIDX)
	{
		ase_awk_val_t* val;

		if (var->type == ASE_AWK_NDE_GLOBAL ||
		    var->type == ASE_AWK_NDE_GLOBALIDX)
			val = STACK_GLOBAL (run,var->id.idxa);
		else if (var->type == ASE_AWK_NDE_LOCAL ||
		         var->type == ASE_AWK_NDE_LOCALIDX)
			val = STACK_LOCAL (run,var->id.idxa);
		else val = STACK_ARG (run,var->id.idxa);

		ASE_ASSERT (val != ASE_NULL);

		if (val->type == ASE_AWK_VAL_NIL)
		{
			ase_awk_val_t* tmp;

			/* value not set.
			 * create a map and assign it to the variable */

			tmp = ase_awk_makemapval (run);
			if (tmp == ASE_NULL) 
			{
				/* adjust error line */
				run->errlin = nde->line;
				return -1;
			}

			/* no need to reduce the reference count of
			 * the previous value because it was nil. */
			if (var->type == ASE_AWK_NDE_GLOBAL ||
			    var->type == ASE_AWK_NDE_GLOBALIDX)
			{
				if (ase_awk_setglobal (
					run, (int)var->id.idxa, tmp) == -1)
				{
					ase_awk_refupval (run, tmp);
					ase_awk_refdownval (run, tmp);

					run->errlin = var->line;
					return -1;
				}
			}
			else if (var->type == ASE_AWK_NDE_LOCAL ||
			         var->type == ASE_AWK_NDE_LOCALIDX)
			{
				STACK_LOCAL(run,var->id.idxa) = tmp;
				ase_awk_refupval (run, tmp);
			}
			else 
			{
				STACK_ARG(run,var->id.idxa) = tmp;
				ase_awk_refupval (run, tmp);
			}
		}
		else
		{
			ase_map_t* map;

			if (val->type != ASE_AWK_VAL_MAP)
			{
				ase_cstr_t errarg;

				errarg.ptr = var->id.name; 
				errarg.len = var->id.name_len;

				ase_awk_setrunerror (
					run, ASE_AWK_ENOTDEL, var->line, 
					&errarg, 1);
				return -1;
			}

			map = ((ase_awk_val_map_t*)val)->map;
			if (var->type == ASE_AWK_NDE_GLOBALIDX ||
			    var->type == ASE_AWK_NDE_LOCALIDX ||
			    var->type == ASE_AWK_NDE_ARGIDX)
			{
				ase_char_t* key;
				ase_size_t keylen;
				ase_awk_val_t* idx;
				ase_char_t buf[IDXBUFSIZE];

				ASE_ASSERT (var->idx != ASE_NULL);

				idx = eval_expression (run, var->idx);
				if (idx == ASE_NULL) return -1;

				ase_awk_refupval (run, idx);

				/* try with a fixed-size buffer */
				keylen = ASE_COUNTOF(buf);
				key = ase_awk_valtostr (
					run, idx, ASE_AWK_VALTOSTR_FIXED, 
					(ase_str_t*)buf, &keylen);
				if (key == ASE_NULL)
				{
					/* if it doesn't work, switch to dynamic mode */
					key = ase_awk_valtostr (
						run, idx, ASE_AWK_VALTOSTR_CLEAR,
						ASE_NULL, &keylen);
				}

				ase_awk_refdownval (run, idx);

				if (key == ASE_NULL)
				{
					run->errlin = var->line;
					return -1;
				}
				ase_map_remove (map, key, keylen);
				if (key != buf) ASE_AWK_FREE (run->awk, key);
			}
			else
			{
				ase_map_clear (map);
			}
		}
	}
	else
	{
		ASE_ASSERTX (
			!"should never happen - wrong target for delete",
			"the delete statement cannot be called with other nodes than the variables such as a named variable, a named indexed variable, etc");

		ase_awk_setrunerror (
			run, ASE_AWK_ERDELETE, var->line, ASE_NULL, 0);
		return -1;
	}

	return 0;
}

static int run_reset (ase_awk_run_t* run, ase_awk_nde_reset_t* nde)
{
	ase_awk_nde_var_t* var;

	var = (ase_awk_nde_var_t*) nde->var;

	if (var->type == ASE_AWK_NDE_NAMED)
	{
		ASE_ASSERTX (
			var->type == ASE_AWK_NDE_NAMED && var->idx == ASE_NULL,
			"if a named variable has an index part, something is definitely wrong");

		/* a named variable can be reset if removed from a internal map 
		   to manage it */
		ase_map_remove (run->named, var->id.name, var->id.name_len);
	}
	else if (var->type == ASE_AWK_NDE_GLOBAL ||
	         var->type == ASE_AWK_NDE_LOCAL ||
	         var->type == ASE_AWK_NDE_ARG)
	{
		ase_awk_val_t* val;

		if (var->type == ASE_AWK_NDE_GLOBAL)
			val = STACK_GLOBAL(run,var->id.idxa);
		else if (var->type == ASE_AWK_NDE_LOCAL)
			val = STACK_LOCAL(run,var->id.idxa);
		else val = STACK_ARG(run,var->id.idxa);

		ASE_ASSERT (val != ASE_NULL);

		if (val->type != ASE_AWK_VAL_NIL)
		{
			ase_awk_refdownval (run, val);
			if (var->type == ASE_AWK_NDE_GLOBAL)
				STACK_GLOBAL(run,var->id.idxa) = ase_awk_val_nil;
			else if (var->type == ASE_AWK_NDE_LOCAL)
				STACK_LOCAL(run,var->id.idxa) = ase_awk_val_nil;
			else
				STACK_ARG(run,var->id.idxa) = ase_awk_val_nil;
		}
	}
	else
	{
		ASE_ASSERTX (
			!"should never happen - wrong target for reset",
			"the reset statement can only be called with plain variables");

		ase_awk_setrunerror (
			run, ASE_AWK_ERRESET, var->line, ASE_NULL, 0);
		return -1;
	}

	return 0;
}

static int run_print (ase_awk_run_t* run, ase_awk_nde_print_t* nde)
{
	ase_char_t* out = ASE_NULL;
	const ase_char_t* dst;
	ase_awk_val_t* v;
	int n;

	ASE_ASSERT (
		(nde->out_type == ASE_AWK_OUT_PIPE && nde->out != ASE_NULL) ||
		(nde->out_type == ASE_AWK_OUT_COPROC && nde->out != ASE_NULL) ||
		(nde->out_type == ASE_AWK_OUT_FILE && nde->out != ASE_NULL) ||
		(nde->out_type == ASE_AWK_OUT_FILE_APPEND && nde->out != ASE_NULL) ||
		(nde->out_type == ASE_AWK_OUT_CONSOLE && nde->out == ASE_NULL));

	/* check if destination has been specified. */
	if (nde->out != ASE_NULL)
	{
		ase_size_t len;

		/* if so, resolve the destination name */
		v = eval_expression (run, nde->out);
		if (v == ASE_NULL) return -1;

		ase_awk_refupval (run, v);
		out = ase_awk_valtostr (
			run, v, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &len);
		if (out == ASE_NULL) 
		{
			ase_awk_refdownval (run, v);

			/* change the error line */
			run->errlin = nde->line;
			return -1;
		}
		ase_awk_refdownval (run, v);

		if (len <= 0) 
		{
			/* the destination name is empty */
			ASE_AWK_FREE (run->awk, out);
			ase_awk_setrunerror (
				run, ASE_AWK_EIONMEM, nde->line, ASE_NULL, 0);
			return -1;
		}

		/* it needs to check if the destination name contains
		 * any invalid characters to the underlying system */
		while (len > 0)
		{
			if (out[--len] == ASE_T('\0'))
			{
				/* if so, it skips writing */
				ASE_AWK_FREE (run->awk, out);
				ase_awk_setrunerror (
					run, ASE_AWK_EIONMNL, nde->line,
					ASE_NULL, 0);
				return -1;
			}
		}
	}

	/* transforms the destination to suit the usage with extio */
	dst = (out == ASE_NULL)? ASE_T(""): out;

	/* check if print is followed by any arguments */
	if (nde->args == ASE_NULL)
	{
		/* if it doesn't have any arguments, print the entire 
		 * input record */
		n = ase_awk_writeextio_str (
			run, nde->out_type, dst,
			ASE_STR_BUF(&run->inrec.line),
			ASE_STR_LEN(&run->inrec.line));
		if (n <= -1 /*&& run->errnum != ASE_AWK_EIOIMPL*/)
		{
			if (out != ASE_NULL) 
				ASE_AWK_FREE (run->awk, out);

			/* adjust the error line */
			run->errlin = nde->line;
			return -1;
		}
	}
	else
	{
		/* if it has any arguments, print the arguments separated by
		 * the value OFS */
		ase_awk_nde_t* head, * np;

		if (nde->args->type == ASE_AWK_NDE_GRP)
		{
			/* parenthesized print */
			ASE_ASSERT (nde->args->next == ASE_NULL);
			head = ((ase_awk_nde_grp_t*)nde->args)->body;
		}
		else head = nde->args;

		for (np = head; np != ASE_NULL; np = np->next)
		{
			if (np != head)
			{
				n = ase_awk_writeextio_str (
					run, nde->out_type, dst, 
					run->global.ofs.ptr, 
					run->global.ofs.len);
				if (n <= -1 /*&& run->errnum != ASE_AWK_EIOIMPL*/) 
				{
					if (out != ASE_NULL)
						ASE_AWK_FREE (run->awk, out);

					/* adjust the error line */
					run->errlin = nde->line;
					return -1;
				}
			}

			v = eval_expression (run, np);
			if (v == ASE_NULL) 
			{
				if (out != ASE_NULL)
					ASE_AWK_FREE (run->awk, out);
				return -1;
			}
			ase_awk_refupval (run, v);

			n = ase_awk_writeextio_val (run, nde->out_type, dst, v);
			if (n <= -1 /*&& run->errnum != ASE_AWK_EIOIMPL*/) 
			{
				if (out != ASE_NULL) 
					ASE_AWK_FREE (run->awk, out);

				ase_awk_refdownval (run, v);
				/* adjust the error line */
				run->errlin = nde->line;
				return -1;
			}

			ase_awk_refdownval (run, v);
		}
	}

	/* print the value ORS to terminate the operation */
	n = ase_awk_writeextio_str (
		run, nde->out_type, dst, 
		run->global.ors.ptr, run->global.ors.len);
	if (n <= -1 /*&& run->errnum != ASE_AWK_EIOIMPL*/)
	{
		if (out != ASE_NULL) ASE_AWK_FREE (run->awk, out);

		/* change the error line */
		run->errlin = nde->line;
		return -1;
	}

	if (out != ASE_NULL) ASE_AWK_FREE (run->awk, out);

/*skip_write:*/
	return 0;
}

static int run_printf (ase_awk_run_t* run, ase_awk_nde_print_t* nde)
{
	ase_char_t* out = ASE_NULL;
	const ase_char_t* dst;
	ase_awk_val_t* v;
	ase_awk_nde_t* head;
	int n;

	ASE_ASSERT (
		(nde->out_type == ASE_AWK_OUT_PIPE && nde->out != ASE_NULL) ||
		(nde->out_type == ASE_AWK_OUT_COPROC && nde->out != ASE_NULL) ||
		(nde->out_type == ASE_AWK_OUT_FILE && nde->out != ASE_NULL) ||
		(nde->out_type == ASE_AWK_OUT_FILE_APPEND && nde->out != ASE_NULL) ||
		(nde->out_type == ASE_AWK_OUT_CONSOLE && nde->out == ASE_NULL));

	if (nde->out != ASE_NULL)
	{
		ase_size_t len;

		v = eval_expression (run, nde->out);
		if (v == ASE_NULL) return -1;

		ase_awk_refupval (run, v);
		out = ase_awk_valtostr (
			run, v, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &len);
		if (out == ASE_NULL) 
		{
			ase_awk_refdownval (run, v);

			/* change the error line */
			run->errlin = nde->line;
			return -1;
		}
		ase_awk_refdownval (run, v);

		if (len <= 0) 
		{
			/* the output destination name is empty. */
			ASE_AWK_FREE (run->awk, out);
			ase_awk_setrunerror (
				run, ASE_AWK_EIONMEM, nde->line, ASE_NULL, 0);
			return -1;
		}

		while (len > 0)
		{
			if (out[--len] == ASE_T('\0'))
			{
				/* the output destination name contains a null 
				 * character. */
				ASE_AWK_FREE (run->awk, out);
				ase_awk_setrunerror (
					run, ASE_AWK_EIONMNL, nde->line,
					ASE_NULL, 0);
				return -1;
			}
		}
	}

	dst = (out == ASE_NULL)? ASE_T(""): out;

	ASE_ASSERTX (nde->args != ASE_NULL, 
		"a valid printf statement should have at least one argument. the parser must ensure this.");

	if (nde->args->type == ASE_AWK_NDE_GRP)
	{
		/* parenthesized print */
		ASE_ASSERT (nde->args->next == ASE_NULL);
		head = ((ase_awk_nde_grp_t*)nde->args)->body;
	}
	else head = nde->args;

	ASE_ASSERTX (head != ASE_NULL,
		"a valid printf statement should have at least one argument. the parser must ensure this.");

	v = eval_expression (run, head);
	if (v == ASE_NULL) 
	{
		if (out != ASE_NULL) ASE_AWK_FREE (run->awk, out);
		return -1;
	}

	ase_awk_refupval (run, v);
	if (v->type != ASE_AWK_VAL_STR)
	{
		/* the remaining arguments are ignored as the format cannot 
		 * contain any % characters */
		n = ase_awk_writeextio_val (run, nde->out_type, dst, v);
		if (n <= -1 /*&& run->errnum != ASE_AWK_EIOIMPL*/)
		{
			if (out != ASE_NULL) ASE_AWK_FREE (run->awk, out);
			ase_awk_refdownval (run, v);

			/* change the error line */
			run->errlin = nde->line;
			return -1;
		}
	}
	else
	{
		/* perform the formatted output */
		if (output_formatted (
			run, nde->out_type, dst,
			((ase_awk_val_str_t*)v)->buf,
			((ase_awk_val_str_t*)v)->len,
			head->next) == -1)
		{
			if (out != ASE_NULL) ASE_AWK_FREE (run->awk, out);
			ase_awk_refdownval (run, v);

			/* adjust the error line */
			run->errlin = nde->line;
			return -1;
		}
	}
	ase_awk_refdownval (run, v);

	if (out != ASE_NULL) ASE_AWK_FREE (run->awk, out);

/*skip_write:*/
	return 0;
}

static int output_formatted (
	ase_awk_run_t* run, int out_type, const ase_char_t* dst, 
	const ase_char_t* fmt, ase_size_t fmt_len, ase_awk_nde_t* args)
{
	ase_char_t* ptr;
	ase_size_t len;
	int n;

	ptr = ase_awk_format (run, 
		ASE_NULL, ASE_NULL, fmt, fmt_len, 0, args, &len);
	if (ptr == ASE_NULL) return -1;

	n = ase_awk_writeextio_str (run, out_type, dst, ptr, len);
	if (n <= -1 /*&& run->errnum != ASE_AWK_EIOIMPL*/) return -1;

	return 0;
}

static ase_awk_val_t* eval_expression (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_val_t* v;
	int n, errnum;

#if 0
	if (run->exit_level >= EXIT_GLOBAL) 
	{
		/* returns ASE_NULL as if an error occurred but
		 * clears the error number. run_main will 
		 * detect this condition and treat it as a 
		 * non-error condition.*/
		run->errnum = ASE_AWK_ENOERR;
		return ASE_NULL;
	}
#endif

	v = eval_expression0 (run, nde);
	if (v == ASE_NULL) return ASE_NULL;

	if (v->type == ASE_AWK_VAL_REX)
	{
		ase_awk_refupval (run, v);

		if (run->inrec.d0->type == ASE_AWK_VAL_NIL)
		{
			/* the record has never been read. 
			 * probably, this functions has been triggered
			 * by the statements in the BEGIN block */
			n = ase_awk_isemptyrex (
				run->awk, ((ase_awk_val_rex_t*)v)->code)? 1: 0;
		}
		else
		{
			ASE_ASSERTX (
				run->inrec.d0->type == ASE_AWK_VAL_STR,
				"the internal value representing $0 should always be of the string type once it has been set/updated. it is nil initially.");

			n = ase_awk_matchrex (
				((ase_awk_run_t*)run)->awk, 
				((ase_awk_val_rex_t*)v)->code,
				((((ase_awk_run_t*)run)->global.ignorecase)? ASE_AWK_REX_IGNORECASE: 0),
				((ase_awk_val_str_t*)run->inrec.d0)->buf,
				((ase_awk_val_str_t*)run->inrec.d0)->len,
				ASE_NULL, ASE_NULL, &errnum);
	
			if (n == -1) 
			{
				ase_awk_refdownval (run, v);

				/* matchrex should never set the error number
				 * whose message contains a formatting 
				 * character. otherwise, the following way of
				 * setting the error information may not work */
				ase_awk_setrunerror (
					run, errnum, nde->line, ASE_NULL, 0);
				return ASE_NULL;
			}
		}

		ase_awk_refdownval (run, v);

		v = ase_awk_makeintval (run, (n != 0));
		if (v == ASE_NULL) 
		{
			/* adjust error line */
			run->errlin = nde->line;
			return ASE_NULL;
		}
	}

	return v;
}

static ase_awk_val_t* eval_expression0 (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	static eval_expr_t eval_func[] =
	{
		/* the order of functions here should match the order
		 * of node types declared in tree.h */
		eval_group,
		eval_assignment,
		eval_binary,
		eval_unary,
		eval_incpre,
		eval_incpst,
		eval_cnd,
		eval_bfn,
		eval_afn,
		eval_int,
		eval_real,
		eval_str,
		eval_rex,
		eval_named,
		eval_global,
		eval_local,
		eval_arg,
		eval_namedidx,
		eval_globalidx,
		eval_localidx,
		eval_argidx,
		eval_pos,
		eval_getline
	};

	ase_awk_val_t* v;


	ASE_ASSERT (nde->type >= ASE_AWK_NDE_GRP &&
		(nde->type - ASE_AWK_NDE_GRP) < ASE_COUNTOF(eval_func));

	v = eval_func[nde->type-ASE_AWK_NDE_GRP] (run, nde);

	if (v != ASE_NULL && run->exit_level >= EXIT_GLOBAL)
	{
		ase_awk_refupval (run, v);	
		ase_awk_refdownval (run, v);

		/* returns ASE_NULL as if an error occurred but
		 * clears the error number. run_main will 
		 * detect this condition and treat it as a 
		 * non-error condition.*/
		run->errnum = ASE_AWK_ENOERR;
		return ASE_NULL;
	}

	return v;
}

static ase_awk_val_t* eval_group (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	/* eval_binop_in evaluates the ASE_AWK_NDE_GRP specially.
	 * so this function should never be reached. */
	ASE_ASSERT (!"should never happen - NDE_GRP only for in");
	ase_awk_setrunerror (run, ASE_AWK_EINTERN, nde->line, ASE_NULL, 0);
	return ASE_NULL;
}

static ase_awk_val_t* eval_assignment (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_val_t* val, * ret;
	ase_awk_nde_ass_t* ass = (ase_awk_nde_ass_t*)nde;

	ASE_ASSERT (ass->left != ASE_NULL);
	ASE_ASSERT (ass->right != ASE_NULL);

	ASE_ASSERT (ass->right->next == ASE_NULL);
	val = eval_expression (run, ass->right);
	if (val == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, val);

	if (ass->opcode != ASE_AWK_ASSOP_NONE)
	{
		ase_awk_val_t* val2, * tmp;
		static binop_func_t binop_func[] =
		{
			ASE_NULL, /* ASE_AWK_ASSOP_NONE */
			eval_binop_plus,
			eval_binop_minus,
			eval_binop_mul,
			eval_binop_div,
			eval_binop_idiv,
			eval_binop_mod,
			eval_binop_exp,
			eval_binop_rshift,
			eval_binop_lshift,
			eval_binop_band,
			eval_binop_bxor,
			eval_binop_bor
		};

		ASE_ASSERT (ass->left->next == ASE_NULL);
		val2 = eval_expression (run, ass->left);
		if (val2 == ASE_NULL)
		{
			ase_awk_refdownval (run, val);
			return ASE_NULL;
		}

		ase_awk_refupval (run, val2);

		ASE_ASSERT (ass->opcode >= 0);
		ASE_ASSERT (ass->opcode < ASE_COUNTOF(binop_func));
		ASE_ASSERT (binop_func[ass->opcode] != ASE_NULL);

		tmp = binop_func[ass->opcode] (run, val2, val);
		if (tmp == ASE_NULL)
		{
			ase_awk_refdownval (run, val2);
			ase_awk_refdownval (run, val);
			return ASE_NULL;
		}

		ase_awk_refdownval (run, val2);
		ase_awk_refdownval (run, val);

		val = tmp;
		ase_awk_refupval (run, val);
	}

	ret = do_assignment (run, ass->left, val);
	ase_awk_refdownval (run, val);

	return ret;
}

static ase_awk_val_t* do_assignment (
	ase_awk_run_t* run, ase_awk_nde_t* var, ase_awk_val_t* val)
{
	ase_awk_val_t* ret;
	int errnum;

	if (var->type == ASE_AWK_NDE_NAMED ||
	    var->type == ASE_AWK_NDE_GLOBAL ||
	    var->type == ASE_AWK_NDE_LOCAL ||
	    var->type == ASE_AWK_NDE_ARG) 
	{
		if ((run->awk->option & ASE_AWK_MAPTOVAR) == 0)
		{
			if (val->type == ASE_AWK_VAL_MAP)
			{
				errnum = ASE_AWK_ENOTASS;
				goto exit_on_error;
			}
		}

		ret = do_assignment_scalar (run, (ase_awk_nde_var_t*)var, val);
	}
	else if (var->type == ASE_AWK_NDE_NAMEDIDX ||
	         var->type == ASE_AWK_NDE_GLOBALIDX ||
	         var->type == ASE_AWK_NDE_LOCALIDX ||
	         var->type == ASE_AWK_NDE_ARGIDX) 
	{
		if (val->type == ASE_AWK_VAL_MAP)
		{
			errnum = ASE_AWK_ENOTASS;
			goto exit_on_error;
		}

		ret = do_assignment_map (run, (ase_awk_nde_var_t*)var, val);
	}
	else if (var->type == ASE_AWK_NDE_POS)
	{
		if (val->type == ASE_AWK_VAL_MAP)
		{
			errnum = ASE_AWK_ENOTASS;
			goto exit_on_error;
		}
	

		ret = do_assignment_pos (run, (ase_awk_nde_pos_t*)var, val);
	}
	else
	{
		ASE_ASSERT (!"should never happen - invalid variable type");
		errnum = ASE_AWK_EINTERN;
		goto exit_on_error;
	}

	return ret;

exit_on_error:
	ase_awk_setrunerror (run, errnum, var->line, ASE_NULL, 0);
	return ASE_NULL;
}

static ase_awk_val_t* do_assignment_scalar (
	ase_awk_run_t* run, ase_awk_nde_var_t* var, ase_awk_val_t* val)
{
	ASE_ASSERT (
		(var->type == ASE_AWK_NDE_NAMED ||
		 var->type == ASE_AWK_NDE_GLOBAL ||
		 var->type == ASE_AWK_NDE_LOCAL ||
		 var->type == ASE_AWK_NDE_ARG) && var->idx == ASE_NULL);

	ASE_ASSERT (
		(run->awk->option & ASE_AWK_MAPTOVAR) ||
		val->type != ASE_AWK_VAL_MAP);

	if (var->type == ASE_AWK_NDE_NAMED) 
	{
		ase_pair_t* pair;
		int n;

		pair = ase_map_get (
			run->named, var->id.name, var->id.name_len);
		if (pair != ASE_NULL && 
		    ((ase_awk_val_t*)pair->val)->type == ASE_AWK_VAL_MAP)
		{
			/* once a variable becomes a map,
			 * it cannot be changed to a scalar variable */
			ase_cstr_t errarg;

			errarg.ptr = var->id.name; 
			errarg.len = var->id.name_len;

			ase_awk_setrunerror (run,
				ASE_AWK_EMAPTOSCALAR, var->line, &errarg, 1);
			return ASE_NULL;
		}

		n = ase_map_putx (run->named, 
			var->id.name, var->id.name_len, val, ASE_NULL);
		if (n < 0) 
		{
			ase_awk_setrunerror (
				run, ASE_AWK_ENOMEM, var->line, ASE_NULL, 0);
			return ASE_NULL;
		}

		ase_awk_refupval (run, val);
	}
	else if (var->type == ASE_AWK_NDE_GLOBAL) 
	{
		if (set_global (run, var->id.idxa, var, val) == -1) 
		{
			/* adjust error line */
			run->errlin = var->line;
			return ASE_NULL;
		}
	}
	else if (var->type == ASE_AWK_NDE_LOCAL) 
	{
		ase_awk_val_t* old = STACK_LOCAL(run,var->id.idxa);
		if (old->type == ASE_AWK_VAL_MAP)
		{	
			/* once the variable becomes a map,
			 * it cannot be changed to a scalar variable */
			ase_cstr_t errarg;

			errarg.ptr = var->id.name; 
			errarg.len = var->id.name_len;

			ase_awk_setrunerror (run,
				ASE_AWK_EMAPTOSCALAR, var->line, &errarg, 1);
			return ASE_NULL;
		}

		ase_awk_refdownval (run, old);
		STACK_LOCAL(run,var->id.idxa) = val;
		ase_awk_refupval (run, val);
	}
	else /* if (var->type == ASE_AWK_NDE_ARG) */
	{
		ase_awk_val_t* old = STACK_ARG(run,var->id.idxa);
		if (old->type == ASE_AWK_VAL_MAP)
		{	
			/* once the variable becomes a map,
			 * it cannot be changed to a scalar variable */
			ase_cstr_t errarg;

			errarg.ptr = var->id.name; 
			errarg.len = var->id.name_len;

			ase_awk_setrunerror (run,
				ASE_AWK_EMAPTOSCALAR, var->line, &errarg, 1);
			return ASE_NULL;
		}

		ase_awk_refdownval (run, old);
		STACK_ARG(run,var->id.idxa) = val;
		ase_awk_refupval (run, val);
	}

	return val;
}

static ase_awk_val_t* do_assignment_map (
	ase_awk_run_t* run, ase_awk_nde_var_t* var, ase_awk_val_t* val)
{
	ase_awk_val_map_t* map;
	ase_char_t* str;
	ase_size_t len;
	ase_char_t idxbuf[IDXBUFSIZE];
	int n;

	ASE_ASSERT (
		(var->type == ASE_AWK_NDE_NAMEDIDX ||
		 var->type == ASE_AWK_NDE_GLOBALIDX ||
		 var->type == ASE_AWK_NDE_LOCALIDX ||
		 var->type == ASE_AWK_NDE_ARGIDX) && var->idx != ASE_NULL);
	ASE_ASSERT (val->type != ASE_AWK_VAL_MAP);

	if (var->type == ASE_AWK_NDE_NAMEDIDX)
	{
		ase_pair_t* pair;
		pair = ase_map_get (
			run->named, var->id.name, var->id.name_len);
		map = (pair == ASE_NULL)? 
			(ase_awk_val_map_t*)ase_awk_val_nil: 
			(ase_awk_val_map_t*)pair->val;
	}
	else
	{
		map = (var->type == ASE_AWK_NDE_GLOBALIDX)? 
		      	(ase_awk_val_map_t*)STACK_GLOBAL(run,var->id.idxa):
		      (var->type == ASE_AWK_NDE_LOCALIDX)? 
		      	(ase_awk_val_map_t*)STACK_LOCAL(run,var->id.idxa):
		      	(ase_awk_val_map_t*)STACK_ARG(run,var->id.idxa);
	}

	if (map->type == ASE_AWK_VAL_NIL)
	{
		/* the map is not initialized yet */
		ase_awk_val_t* tmp;

		tmp = ase_awk_makemapval (run);
		if (tmp == ASE_NULL) 
		{
			/* adjust error line */
			run->errlin = var->line;
			return ASE_NULL;
		}

		if (var->type == ASE_AWK_NDE_NAMEDIDX)
		{
			/* doesn't have to decrease the reference count 
			 * of the previous value here as it is done by 
			 * ase_map_put */
			if (ase_map_put (run->named, 
				var->id.name, var->id.name_len, tmp) == ASE_NULL)
			{
				ase_awk_refupval (run, tmp);
				ase_awk_refdownval (run, tmp);

				ase_awk_setrunerror (
					run, ASE_AWK_ENOMEM, var->line, ASE_NULL, 0);
				return ASE_NULL;
			}

			ase_awk_refupval (run, tmp);
		}
		else if (var->type == ASE_AWK_NDE_GLOBALIDX)
		{
			ase_awk_refupval (run, tmp);
			if (ase_awk_setglobal (run, (int)var->id.idxa, tmp) == -1)
			{
				ase_awk_refdownval (run, tmp);

				/* change error line */
				run->errlin = var->line;
				return ASE_NULL;
			}
			ase_awk_refdownval (run, tmp);
		}
		else if (var->type == ASE_AWK_NDE_LOCALIDX)
		{
			ase_awk_refdownval (run, (ase_awk_val_t*)map);
			STACK_LOCAL(run,var->id.idxa) = tmp;
			ase_awk_refupval (run, tmp);
		}
		else /* if (var->type == ASE_AWK_NDE_ARGIDX) */
		{
			ase_awk_refdownval (run, (ase_awk_val_t*)map);
			STACK_ARG(run,var->id.idxa) = tmp;
			ase_awk_refupval (run, tmp);
		}

		map = (ase_awk_val_map_t*) tmp;
	}
	else if (map->type != ASE_AWK_VAL_MAP)
	{
		/* variable assigned is not a map */
		ase_awk_setrunerror (
			run, ASE_AWK_ENOTIDX, var->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	len = ASE_COUNTOF(idxbuf);
	str = idxnde_to_str (run, var->idx, idxbuf, &len);
	if (str == ASE_NULL) 
	{
		str = idxnde_to_str (run, var->idx, ASE_NULL, &len);
		if (str == ASE_NULL) return ASE_NULL;
	}

#ifdef DEBUG_RUN
	ase_dprintf (ASE_T("**** index str=>%s, map->ref=%d, map->type=%d\n"), 
		str, (int)map->ref, (int)map->type);
#endif

	n = ase_map_putx (map->map, str, len, val, ASE_NULL);
	if (n < 0)
	{
		if (str != idxbuf) ASE_AWK_FREE (run->awk, str);
		ase_awk_setrunerror (run, ASE_AWK_ENOMEM, var->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	if (str != idxbuf) ASE_AWK_FREE (run->awk, str);
	ase_awk_refupval (run, val);
	return val;
}

static ase_awk_val_t* do_assignment_pos (
	ase_awk_run_t* run, ase_awk_nde_pos_t* pos, ase_awk_val_t* val)
{
	ase_awk_val_t* v;
	ase_long_t lv;
	ase_real_t rv;
	ase_char_t* str;
	ase_size_t len;
	int n;

	v = eval_expression (run, pos->val);
	if (v == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, v);
	n = ase_awk_valtonum (run, v, &lv, &rv);
	ase_awk_refdownval (run, v);

	if (n == -1) 
	{
		ase_awk_setrunerror (
			run, ASE_AWK_EPOSIDX, pos->line, ASE_NULL, 0);
		return ASE_NULL;
	}
	if (n == 1) lv = (ase_long_t)rv;
	if (!IS_VALID_POSIDX(lv)) 
	{
		ase_awk_setrunerror (
			run, ASE_AWK_EPOSIDX, pos->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	if (val->type == ASE_AWK_VAL_STR)
	{
		str = ((ase_awk_val_str_t*)val)->buf;
		len = ((ase_awk_val_str_t*)val)->len;
	}
	else
	{
		str = ase_awk_valtostr (
			run, val, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &len);
		if (str == ASE_NULL)
		{
			/* change error line */
			run->errlin = pos->line;
			return ASE_NULL;
		}
	}
	
	n = ase_awk_setrec (run, (ase_size_t)lv, str, len);

	if (val->type != ASE_AWK_VAL_STR) ASE_AWK_FREE (run->awk, str);

	if (n == -1) return ASE_NULL;
	return (lv == 0)? run->inrec.d0: run->inrec.flds[lv-1].val;
}

static ase_awk_val_t* eval_binary (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	static binop_func_t binop_func[] =
	{
		/* the order of the functions should be inline with
		 * the operator declaration in run.h */

		ASE_NULL, /* eval_binop_lor */
		ASE_NULL, /* eval_binop_land */
		ASE_NULL, /* eval_binop_in */

		eval_binop_bor,
		eval_binop_bxor,
		eval_binop_band,

		eval_binop_eq,
		eval_binop_ne,
		eval_binop_gt,
		eval_binop_ge,
		eval_binop_lt,
		eval_binop_le,

		eval_binop_lshift,
		eval_binop_rshift,
		
		eval_binop_plus,
		eval_binop_minus,
		eval_binop_mul,
		eval_binop_div,
		eval_binop_idiv,
		eval_binop_mod,
		eval_binop_exp,

		eval_binop_concat,
		ASE_NULL, /* eval_binop_ma */
		ASE_NULL  /* eval_binop_nm */
	};

	ase_awk_nde_exp_t* exp = (ase_awk_nde_exp_t*)nde;
	ase_awk_val_t* left, * right, * res;

	ASE_ASSERT (exp->type == ASE_AWK_NDE_EXP_BIN);

	if (exp->opcode == ASE_AWK_BINOP_LAND)
	{
		res = eval_binop_land (run, exp->left, exp->right);
	}
	else if (exp->opcode == ASE_AWK_BINOP_LOR)
	{
		res = eval_binop_lor (run, exp->left, exp->right);
	}
	else if (exp->opcode == ASE_AWK_BINOP_IN)
	{
		/* treat the in operator specially */
		res = eval_binop_in (run, exp->left, exp->right);
	}
	else if (exp->opcode == ASE_AWK_BINOP_NM)
	{
		res = eval_binop_nm (run, exp->left, exp->right);
	}
	else if (exp->opcode == ASE_AWK_BINOP_MA)
	{
		res = eval_binop_ma (run, exp->left, exp->right);
	}
	else
	{
		ASE_ASSERT (exp->left->next == ASE_NULL);
		left = eval_expression (run, exp->left);
		if (left == ASE_NULL) return ASE_NULL;

		ase_awk_refupval (run, left);

		ASE_ASSERT (exp->right->next == ASE_NULL);
		right = eval_expression (run, exp->right);
		if (right == ASE_NULL) 
		{
			ase_awk_refdownval (run, left);
			return ASE_NULL;
		}

		ase_awk_refupval (run, right);

		ASE_ASSERT (exp->opcode >= 0 && 
			exp->opcode < ASE_COUNTOF(binop_func));
		ASE_ASSERT (binop_func[exp->opcode] != ASE_NULL);

		res = binop_func[exp->opcode] (run, left, right);
		if (res == ASE_NULL)
		{
			/* change the error line */
			run->errlin = nde->line;
		}

		ase_awk_refdownval (run, left);
		ase_awk_refdownval (run, right);
	}

	return res;
}

static ase_awk_val_t* eval_binop_lor (
	ase_awk_run_t* run, ase_awk_nde_t* left, ase_awk_nde_t* right)
{
	/*
	ase_awk_val_t* res = ASE_NULL;

	res = ase_awk_makeintval (run, 
		ase_awk_valtobool(run left) || ase_awk_valtobool(run,right));
	if (res == ASE_NULL)
	{
		run->errlin = left->line;
		return ASE_NULL;
	}

	return res;
	*/

	/* short-circuit evaluation required special treatment */
	ase_awk_val_t* lv, * rv, * res;

	ASE_ASSERT (left->next == ASE_NULL);
	lv = eval_expression (run, left);
	if (lv == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, lv);
	if (ase_awk_valtobool (run, lv)) 
	{
		res = ase_awk_val_one;
	}
	else
	{
		ASE_ASSERT (right->next == ASE_NULL);
		rv = eval_expression (run, right);
		if (rv == ASE_NULL)
		{
			ase_awk_refdownval (run, lv);
			return ASE_NULL;
		}
		ase_awk_refupval (run, rv);

		res = ase_awk_valtobool(run,rv)? ase_awk_val_one: ase_awk_val_zero;
		ase_awk_refdownval (run, rv);
	}

	ase_awk_refdownval (run, lv);

	return res;
}

static ase_awk_val_t* eval_binop_land (
	ase_awk_run_t* run, ase_awk_nde_t* left, ase_awk_nde_t* right)
{
	/*
	ase_awk_val_t* res = ASE_NULL;

	res = ase_awk_makeintval (run, 
		ase_awk_valtobool(run,left) && ase_awk_valtobool(run,right));
	if (res == ASE_NULL) 
	{
		run->errlin = left->line;
		return ASE_NULL;
	}

	return res;
	*/

	/* short-circuit evaluation required special treatment */
	ase_awk_val_t* lv, * rv, * res;

	ASE_ASSERT (left->next == ASE_NULL);
	lv = eval_expression (run, left);
	if (lv == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, lv);
	if (!ase_awk_valtobool (run, lv)) 
	{
		res = ase_awk_val_zero;
	}
	else
	{
		ASE_ASSERT (right->next == ASE_NULL);
		rv = eval_expression (run, right);
		if (rv == ASE_NULL)
		{
			ase_awk_refdownval (run, lv);
			return ASE_NULL;
		}
		ase_awk_refupval (run, rv);

		res = ase_awk_valtobool(run,rv)? ase_awk_val_one: ase_awk_val_zero;
		ase_awk_refdownval (run, rv);
	}

	ase_awk_refdownval (run, lv);

	return res;
}

static ase_awk_val_t* eval_binop_in (
	ase_awk_run_t* run, ase_awk_nde_t* left, ase_awk_nde_t* right)
{
	ase_awk_val_t* rv;
	ase_char_t* str;
	ase_size_t len;
	ase_char_t idxbuf[IDXBUFSIZE];

	if (right->type != ASE_AWK_NDE_GLOBAL &&
	    right->type != ASE_AWK_NDE_LOCAL &&
	    right->type != ASE_AWK_NDE_ARG &&
	    right->type != ASE_AWK_NDE_NAMED)
	{
		/* the compiler should have handled this case */
		ASE_ASSERT (
			!"should never happen - in needs a plain variable");

		ase_awk_setrunerror (
			run, ASE_AWK_EINTERN, right->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	/* evaluate the left-hand side of the operator */
	len = ASE_COUNTOF(idxbuf);
	str = (left->type == ASE_AWK_NDE_GRP)?
		idxnde_to_str (run, ((ase_awk_nde_grp_t*)left)->body, idxbuf, &len):
		idxnde_to_str (run, left, idxbuf, &len);
	if (str == ASE_NULL)
	{
		str = (left->type == ASE_AWK_NDE_GRP)?
			idxnde_to_str (run, ((ase_awk_nde_grp_t*)left)->body, ASE_NULL, &len):
			idxnde_to_str (run, left, ASE_NULL, &len);
		if (str == ASE_NULL) return ASE_NULL;
	}

	/* evaluate the right-hand side of the operator */
	ASE_ASSERT (right->next == ASE_NULL);
	rv = eval_expression (run, right);
	if (rv == ASE_NULL) 
	{
		if (str != idxbuf) ASE_AWK_FREE (run->awk, str);
		return ASE_NULL;
	}

	ase_awk_refupval (run, rv);

	if (rv->type == ASE_AWK_VAL_NIL)
	{
		if (str != idxbuf) ASE_AWK_FREE (run->awk, str);
		ase_awk_refdownval (run, rv);
		return ase_awk_val_zero;
	}
	else if (rv->type == ASE_AWK_VAL_MAP)
	{
		ase_awk_val_t* res;
		ase_map_t* map;

		map = ((ase_awk_val_map_t*)rv)->map;
		res = (ase_map_get (map, str, len) == ASE_NULL)? 
			ase_awk_val_zero: ase_awk_val_one;

		if (str != idxbuf) ASE_AWK_FREE (run->awk, str);
		ase_awk_refdownval (run, rv);
		return res;
	}

	/* need a map */
	if (str != idxbuf) ASE_AWK_FREE (run->awk, str);
	ase_awk_refdownval (run, rv);

	ase_awk_setrunerror (run, ASE_AWK_ENOTMAPNILIN, right->line, ASE_NULL, 0);
	return ASE_NULL;
}

static ase_awk_val_t* eval_binop_bor (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1) 
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	ASE_ASSERT (n3 >= 0 && n3 <= 3);
	return (n3 == 0)? ase_awk_makeintval(run,(ase_long_t)l1|(ase_long_t)l2):
	       (n3 == 1)? ase_awk_makeintval(run,(ase_long_t)r1|(ase_long_t)l2):
	       (n3 == 2)? ase_awk_makeintval(run,(ase_long_t)l1|(ase_long_t)r2):
	                  ase_awk_makeintval(run,(ase_long_t)r1|(ase_long_t)r2);
}

static ase_awk_val_t* eval_binop_bxor (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1)
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	ASE_ASSERT (n3 >= 0 && n3 <= 3);
	return (n3 == 0)? ase_awk_makeintval(run,(ase_long_t)l1^(ase_long_t)l2):
	       (n3 == 1)? ase_awk_makeintval(run,(ase_long_t)r1^(ase_long_t)l2):
	       (n3 == 2)? ase_awk_makeintval(run,(ase_long_t)l1^(ase_long_t)r2):
	                  ase_awk_makeintval(run,(ase_long_t)r1^(ase_long_t)r2);
}

static ase_awk_val_t* eval_binop_band (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1) 
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	ASE_ASSERT (n3 >= 0 && n3 <= 3);
	return (n3 == 0)? ase_awk_makeintval(run,(ase_long_t)l1&(ase_long_t)l2):
	       (n3 == 1)? ase_awk_makeintval(run,(ase_long_t)r1&(ase_long_t)l2):
	       (n3 == 2)? ase_awk_makeintval(run,(ase_long_t)l1&(ase_long_t)r2):
	                  ase_awk_makeintval(run,(ase_long_t)r1&(ase_long_t)r2);
}

static int __cmp_nil_nil (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	return 0;
}

static int __cmp_nil_int (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	if (((ase_awk_val_int_t*)right)->val < 0) return 1;
	if (((ase_awk_val_int_t*)right)->val > 0) return -1;
	return 0;
}

static int __cmp_nil_real (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	if (((ase_awk_val_real_t*)right)->val < 0) return 1;
	if (((ase_awk_val_real_t*)right)->val > 0) return -1;
	return 0;
}

static int __cmp_nil_str (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	return (((ase_awk_val_str_t*)right)->len == 0)? 0: -1;
}

static int __cmp_int_nil (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	if (((ase_awk_val_int_t*)left)->val > 0) return 1;
	if (((ase_awk_val_int_t*)left)->val < 0) return -1;
	return 0;
}

static int __cmp_int_int (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	if (((ase_awk_val_int_t*)left)->val > 
	    ((ase_awk_val_int_t*)right)->val) return 1;
	if (((ase_awk_val_int_t*)left)->val < 
	    ((ase_awk_val_int_t*)right)->val) return -1;
	return 0;
}

static int __cmp_int_real (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	if (((ase_awk_val_int_t*)left)->val > 
	    ((ase_awk_val_real_t*)right)->val) return 1;
	if (((ase_awk_val_int_t*)left)->val < 
	    ((ase_awk_val_real_t*)right)->val) return -1;
	return 0;
}

static int __cmp_int_str (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	ase_char_t* str;
	ase_size_t len;
	ase_long_t r;
	ase_real_t rr;
	int n;

	r = ase_awk_strxtolong (run->awk, 
		((ase_awk_val_str_t*)right)->buf,
		((ase_awk_val_str_t*)right)->len, 0, (const ase_char_t**)&str);
	if (str == ((ase_awk_val_str_t*)right)->buf + 
		   ((ase_awk_val_str_t*)right)->len)
	{
		if (((ase_awk_val_int_t*)left)->val > r) return 1;
		if (((ase_awk_val_int_t*)left)->val < r) return -1;
		return 0;
	}
/* TODO: should i do this???  conversion to real and comparision... */
	else if (*str == ASE_T('.') || *str == ASE_T('E') || *str == ASE_T('e'))
	{
		rr = ase_awk_strxtoreal (run->awk,
			((ase_awk_val_str_t*)right)->buf,
			((ase_awk_val_str_t*)right)->len, 
			(const ase_char_t**)&str);
		if (str == ((ase_awk_val_str_t*)right)->buf + 
			   ((ase_awk_val_str_t*)right)->len)
		{
			if (((ase_awk_val_int_t*)left)->val > rr) return 1;
			if (((ase_awk_val_int_t*)left)->val < rr) return -1;
			return 0;
		}
	}

	str = ase_awk_valtostr (
		run, left, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &len);
	if (str == ASE_NULL) return CMP_ERROR;

	if (run->global.ignorecase)
	{
		n = ase_strxncasecmp (
			str, len,
			((ase_awk_val_str_t*)right)->buf, 
			((ase_awk_val_str_t*)right)->len,
			&run->awk->prmfns.ccls);
	}
	else
	{
		n = ase_strxncmp (
			str, len,
			((ase_awk_val_str_t*)right)->buf, 
			((ase_awk_val_str_t*)right)->len);
	}

	ASE_AWK_FREE (run->awk, str);
	return n;
}

static int __cmp_real_nil (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	if (((ase_awk_val_real_t*)left)->val > 0) return 1;
	if (((ase_awk_val_real_t*)left)->val < 0) return -1;
	return 0;
}

static int __cmp_real_int (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	if (((ase_awk_val_real_t*)left)->val > 
	    ((ase_awk_val_int_t*)right)->val) return 1;
	if (((ase_awk_val_real_t*)left)->val < 
	    ((ase_awk_val_int_t*)right)->val) return -1;
	return 0;
}

static int __cmp_real_real (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	if (((ase_awk_val_real_t*)left)->val > 
	    ((ase_awk_val_real_t*)right)->val) return 1;
	if (((ase_awk_val_real_t*)left)->val < 
	    ((ase_awk_val_real_t*)right)->val) return -1;
	return 0;
}

static int __cmp_real_str (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	ase_char_t* str;
	ase_size_t len;
	ase_real_t rr;
	int n;

	rr = ase_awk_strxtoreal (run->awk,
		((ase_awk_val_str_t*)right)->buf,
		((ase_awk_val_str_t*)right)->len, 
		(const ase_char_t**)&str);
	if (str == ((ase_awk_val_str_t*)right)->buf + 
		   ((ase_awk_val_str_t*)right)->len)
	{
		if (((ase_awk_val_real_t*)left)->val > rr) return 1;
		if (((ase_awk_val_real_t*)left)->val < rr) return -1;
		return 0;
	}

	str = ase_awk_valtostr (
		run, left, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &len);
	if (str == ASE_NULL) return CMP_ERROR;

	if (run->global.ignorecase)
	{
		n = ase_strxncasecmp (
			str, len,
			((ase_awk_val_str_t*)right)->buf, 
			((ase_awk_val_str_t*)right)->len,
			&run->awk->prmfns.ccls);
	}
	else
	{
		n = ase_strxncmp (
			str, len,
			((ase_awk_val_str_t*)right)->buf, 
			((ase_awk_val_str_t*)right)->len);
	}

	ASE_AWK_FREE (run->awk, str);
	return n;
}

static int __cmp_str_nil (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	return (((ase_awk_val_str_t*)left)->len == 0)? 0: 1;
}

static int __cmp_str_int (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	return -__cmp_int_str (run, right, left);
}

static int __cmp_str_real (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	return -__cmp_real_str (run, right, left);
}

static int __cmp_str_str (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n;
	ase_awk_val_str_t* ls, * rs;

	ls = (ase_awk_val_str_t*)left;
	rs = (ase_awk_val_str_t*)right;

	if (run->global.ignorecase)
	{
		n = ase_strxncasecmp (
			ls->buf, ls->len, rs->buf, rs->len, 
			&run->awk->prmfns.ccls);
	}
	else
	{
		n = ase_strxncmp (
			ls->buf, ls->len, rs->buf, rs->len);
	}

	return n;
}

static int __cmp_val (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	typedef int (*cmp_val_t) (ase_awk_run_t*, ase_awk_val_t*, ase_awk_val_t*);

	static cmp_val_t func[] =
	{
		/* this table must be synchronized with 
		 * the ASE_AWK_VAL_XXX values in val.h */
		__cmp_nil_nil,  __cmp_nil_int,  __cmp_nil_real,  __cmp_nil_str,
		__cmp_int_nil,  __cmp_int_int,  __cmp_int_real,  __cmp_int_str,
		__cmp_real_nil, __cmp_real_int, __cmp_real_real, __cmp_real_str,
		__cmp_str_nil,  __cmp_str_int,  __cmp_str_real,  __cmp_str_str,
	};

	if (left->type == ASE_AWK_VAL_MAP || right->type == ASE_AWK_VAL_MAP)
	{
		/* a map can't be compared againt other values */
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return CMP_ERROR; 
	}

	ASE_ASSERT (
		left->type >= ASE_AWK_VAL_NIL &&
		left->type <= ASE_AWK_VAL_STR);
	ASE_ASSERT (
		right->type >= ASE_AWK_VAL_NIL &&
		right->type <= ASE_AWK_VAL_STR);

	return func[left->type*4+right->type] (run, left, right);
}

static ase_awk_val_t* eval_binop_eq (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n = __cmp_val (run, left, right);
	if (n == CMP_ERROR) return ASE_NULL;
	return (n == 0)? ase_awk_val_one: ase_awk_val_zero;
}

static ase_awk_val_t* eval_binop_ne (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n = __cmp_val (run, left, right);
	if (n == CMP_ERROR) return ASE_NULL;
	return (n != 0)? ase_awk_val_one: ase_awk_val_zero;
}

static ase_awk_val_t* eval_binop_gt (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n = __cmp_val (run, left, right);
	if (n == CMP_ERROR) return ASE_NULL;
	return (n > 0)? ase_awk_val_one: ase_awk_val_zero;
}

static ase_awk_val_t* eval_binop_ge (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n = __cmp_val (run, left, right);
	if (n == CMP_ERROR) return ASE_NULL;
	return (n >= 0)? ase_awk_val_one: ase_awk_val_zero;
}

static ase_awk_val_t* eval_binop_lt (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n = __cmp_val (run, left, right);
	if (n == CMP_ERROR) return ASE_NULL;
	return (n < 0)? ase_awk_val_one: ase_awk_val_zero;
}

static ase_awk_val_t* eval_binop_le (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n = __cmp_val (run, left, right);
	if (n == CMP_ERROR) return ASE_NULL;
	return (n <= 0)? ase_awk_val_one: ase_awk_val_zero;
}

static ase_awk_val_t* eval_binop_lshift (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1)
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	if (n3 == 0)
	{
		return ase_awk_makeintval (run, (ase_long_t)l1<<(ase_long_t)l2);
	}
	else
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}
}

static ase_awk_val_t* eval_binop_rshift (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1)
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	if (n3 == 0)
	{
		return ase_awk_makeintval (
			run, (ase_long_t)l1>>(ase_long_t)l2);
	}
	else
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}
}

static ase_awk_val_t* eval_binop_plus (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1)
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}
	/*
	n1  n2    n3
	0   0   = 0
	1   0   = 1
	0   1   = 2
	1   1   = 3
	*/
	n3 = n1 + (n2 << 1);
	ASE_ASSERT (n3 >= 0 && n3 <= 3);

	return (n3 == 0)? ase_awk_makeintval(run,(ase_long_t)l1+(ase_long_t)l2):
	       (n3 == 1)? ase_awk_makerealval(run,(ase_real_t)r1+(ase_real_t)l2):
	       (n3 == 2)? ase_awk_makerealval(run,(ase_real_t)l1+(ase_real_t)r2):
	                  ase_awk_makerealval(run,(ase_real_t)r1+(ase_real_t)r2);
}

static ase_awk_val_t* eval_binop_minus (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1)
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	ASE_ASSERT (n3 >= 0 && n3 <= 3);
	return (n3 == 0)? ase_awk_makeintval(run,(ase_long_t)l1-(ase_long_t)l2):
	       (n3 == 1)? ase_awk_makerealval(run,(ase_real_t)r1-(ase_real_t)l2):
	       (n3 == 2)? ase_awk_makerealval(run,(ase_real_t)l1-(ase_real_t)r2):
	                  ase_awk_makerealval(run,(ase_real_t)r1-(ase_real_t)r2);
}

static ase_awk_val_t* eval_binop_mul (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1)
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	ASE_ASSERT (n3 >= 0 && n3 <= 3);
	return (n3 == 0)? ase_awk_makeintval(run,(ase_long_t)l1*(ase_long_t)l2):
	       (n3 == 1)? ase_awk_makerealval(run,(ase_real_t)r1*(ase_real_t)l2):
	       (n3 == 2)? ase_awk_makerealval(run,(ase_real_t)l1*(ase_real_t)r2):
	                  ase_awk_makerealval(run,(ase_real_t)r1*(ase_real_t)r2);
}

static ase_awk_val_t* eval_binop_div (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;
	ase_awk_val_t* res;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1) 
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	if (n3 == 0)
	{
		if  (l2 == 0) 
		{
			ase_awk_setrunerrnum (run, ASE_AWK_EDIVBY0);
			return ASE_NULL;
		}

		if (((ase_long_t)l1 % (ase_long_t)l2) == 0)
		{
			res = ase_awk_makeintval (
				run, (ase_long_t)l1 / (ase_long_t)l2);
		}
		else
		{
			res = ase_awk_makerealval (
				run, (ase_real_t)l1 / (ase_real_t)l2);
		}
	}
	else if (n3 == 1)
	{
		res = ase_awk_makerealval (
			run, (ase_real_t)r1 / (ase_real_t)l2);
	}
	else if (n3 == 2)
	{
		res = ase_awk_makerealval (
			run, (ase_real_t)l1 / (ase_real_t)r2);
	}
	else
	{
		ASE_ASSERT (n3 == 3);
		res = ase_awk_makerealval (
			run, (ase_real_t)r1 / (ase_real_t)r2);
	}

	return res;
}

static ase_awk_val_t* eval_binop_idiv (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2, quo;
	ase_awk_val_t* res;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1) 
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	if (n3 == 0)
	{
		if (l2 == 0) 
		{
			ase_awk_setrunerrnum (run, ASE_AWK_EDIVBY0);
			return ASE_NULL;
		}
		res = ase_awk_makeintval (
			run, (ase_long_t)l1 / (ase_long_t)l2);
	}
	else if (n3 == 1)
	{
		quo = (ase_real_t)r1 / (ase_real_t)l2;
		res = ase_awk_makeintval (run, (ase_long_t)quo);
	}
	else if (n3 == 2)
	{
		quo = (ase_real_t)l1 / (ase_real_t)r2;
		res = ase_awk_makeintval (run, (ase_long_t)quo);
	}
	else
	{
		ASE_ASSERT (n3 == 3);

		quo = (ase_real_t)r1 / (ase_real_t)r2;
		res = ase_awk_makeintval (run, (ase_long_t)quo);
	}

	return res;
}

static ase_awk_val_t* eval_binop_mod (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;
	ase_awk_val_t* res;

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1)
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	if (n3 == 0)
	{
		if  (l2 == 0) 
		{
			ase_awk_setrunerrnum (run, ASE_AWK_EDIVBY0);
			return ASE_NULL;
		}
		res = ase_awk_makeintval (
			run, (ase_long_t)l1 % (ase_long_t)l2);
	}
	else 
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	return res;
}

static ase_awk_val_t* eval_binop_exp (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	int n1, n2, n3;
	ase_long_t l1, l2;
	ase_real_t r1, r2;
	ase_awk_val_t* res;

	ASE_ASSERTX (run->awk->prmfns.misc.pow != ASE_NULL,
		"the pow function should be provided when the awk object is created to make the exponentiation work properly.");

	n1 = ase_awk_valtonum (run, left, &l1, &r1);
	n2 = ase_awk_valtonum (run, right, &l2, &r2);

	if (n1 == -1 || n2 == -1) 
	{
		ase_awk_setrunerrnum (run, ASE_AWK_EOPERAND);
		return ASE_NULL;
	}

	n3 = n1 + (n2 << 1);
	if (n3 == 0)
	{
		/* left - int, right - int */
		if (l2 >= 0)
		{
			ase_long_t v = 1;
			while (l2-- > 0) v *= l1;
			res = ase_awk_makeintval (run, v);
		}
		else if (l1 == 0)
		{
			ase_awk_setrunerrnum (run, ASE_AWK_EDIVBY0);
			return ASE_NULL;
		}
		else
		{
			ase_real_t v = 1.0;
			l2 *= -1;
			while (l2-- > 0) v /= l1;
			res = ase_awk_makerealval (run, v);
		}
	}
	else if (n3 == 1)
	{
		/* left - real, right - int */
		if (l2 >= 0)
		{
			ase_real_t v = 1.0;
			while (l2-- > 0) v *= r1;
			res = ase_awk_makerealval (run, v);
		}
		else if (r1 == 0.0)
		{
			ase_awk_setrunerrnum (run, ASE_AWK_EDIVBY0);
			return ASE_NULL;
		}
		else
		{
			ase_real_t v = 1.0;
			l2 *= -1;
			while (l2-- > 0) v /= r1;
			res = ase_awk_makerealval (run, v);
		}
	}
	else if (n3 == 2)
	{
		/* left - int, right - real */
		res = ase_awk_makerealval (run, 
			run->awk->prmfns.misc.pow (
				run->awk->prmfns.misc.custom_data, 
				(ase_real_t)l1,(ase_real_t)r2));
	}
	else
	{
		/* left - real, right - real */
		ASE_ASSERT (n3 == 3);
		res = ase_awk_makerealval (run,
			run->awk->prmfns.misc.pow(
				run->awk->prmfns.misc.custom_data, 
				(ase_real_t)r1,(ase_real_t)r2));
	}

	return res;
}

static ase_awk_val_t* eval_binop_concat (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right)
{
	ase_char_t* strl, * strr;
	ase_size_t strl_len, strr_len;
	ase_awk_val_t* res;

	strl = ase_awk_valtostr (
		run, left, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &strl_len);
	if (strl == ASE_NULL) return ASE_NULL;

	strr = ase_awk_valtostr (
		run, right, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &strr_len);
	if (strr == ASE_NULL) 
	{
		ASE_AWK_FREE (run->awk, strl);
		return ASE_NULL;
	}

	res = ase_awk_makestrval2 (run, strl, strl_len, strr, strr_len);

	ASE_AWK_FREE (run->awk, strl);
	ASE_AWK_FREE (run->awk, strr);

	return res;
}

static ase_awk_val_t* eval_binop_ma (
	ase_awk_run_t* run, ase_awk_nde_t* left, ase_awk_nde_t* right)
{
	ase_awk_val_t* lv, * rv, * res;

	ASE_ASSERT (left->next == ASE_NULL);
	ASE_ASSERT (right->next == ASE_NULL);

	lv = eval_expression (run, left);
	if (lv == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, lv);

	rv = eval_expression0 (run, right);
	if (rv == ASE_NULL)
	{
		ase_awk_refdownval (run, lv);
		return ASE_NULL;
	}

	ase_awk_refupval (run, rv);

	res = eval_binop_match0 (run, lv, rv, left->line, right->line, 1);

	ase_awk_refdownval (run, rv);
	ase_awk_refdownval (run, lv);

	return res;
}

static ase_awk_val_t* eval_binop_nm (
	ase_awk_run_t* run, ase_awk_nde_t* left, ase_awk_nde_t* right)
{
	ase_awk_val_t* lv, * rv, * res;

	ASE_ASSERT (left->next == ASE_NULL);
	ASE_ASSERT (right->next == ASE_NULL);

	lv = eval_expression (run, left);
	if (lv == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, lv);

	rv = eval_expression0 (run, right);
	if (rv == ASE_NULL)
	{
		ase_awk_refdownval (run, lv);
		return ASE_NULL;
	}

	ase_awk_refupval (run, rv);

	res = eval_binop_match0 (
		run, lv, rv, left->line, right->line, 0);

	ase_awk_refdownval (run, rv);
	ase_awk_refdownval (run, lv);

	return res;
}

static ase_awk_val_t* eval_binop_match0 (
	ase_awk_run_t* run, ase_awk_val_t* left, ase_awk_val_t* right, 
	ase_size_t lline, ase_size_t rline, int ret)
{
	ase_awk_val_t* res;
	int n, errnum;
	ase_char_t* str;
	ase_size_t len;
	void* rex_code;

	if (right->type == ASE_AWK_VAL_REX)
	{
		rex_code = ((ase_awk_val_rex_t*)right)->code;
	}
	else if (right->type == ASE_AWK_VAL_STR)
	{
		rex_code = ase_awk_buildrex ( 
			run->awk,
			((ase_awk_val_str_t*)right)->buf,
			((ase_awk_val_str_t*)right)->len, &errnum);
		if (rex_code == ASE_NULL)
		{
			ase_awk_setrunerror (run, errnum, rline, ASE_NULL, 0);
			return ASE_NULL;
		}
	}
	else
	{
		str = ase_awk_valtostr (
			run, right, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &len);
		if (str == ASE_NULL) return ASE_NULL;

		rex_code = ase_awk_buildrex (run->awk, str, len, &errnum);
		if (rex_code == ASE_NULL)
		{
			ASE_AWK_FREE (run->awk, str);

			ase_awk_setrunerror (run, errnum, rline, ASE_NULL, 0);
			return ASE_NULL;
		}

		ASE_AWK_FREE (run->awk, str);
	}

	if (left->type == ASE_AWK_VAL_STR)
	{
		n = ase_awk_matchrex (
			run->awk, rex_code,
			((run->global.ignorecase)? ASE_AWK_REX_IGNORECASE: 0),
			((ase_awk_val_str_t*)left)->buf,
			((ase_awk_val_str_t*)left)->len,
			ASE_NULL, ASE_NULL, &errnum);
		if (n == -1) 
		{
			if (right->type != ASE_AWK_VAL_REX) 
				ASE_AWK_FREE (run->awk, rex_code);

			ase_awk_setrunerror (run, errnum, lline, ASE_NULL, 0);
			return ASE_NULL;
		}

		res = ase_awk_makeintval (run, (n == ret));
		if (res == ASE_NULL) 
		{
			if (right->type != ASE_AWK_VAL_REX) 
				ASE_AWK_FREE (run->awk, rex_code);

			/* adjust error line */
			run->errlin = lline;
			return ASE_NULL;
		}
	}
	else
	{
		str = ase_awk_valtostr (
			run, left, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &len);
		if (str == ASE_NULL) 
		{
			if (right->type != ASE_AWK_VAL_REX) 
				ASE_AWK_FREE (run->awk, rex_code);
			return ASE_NULL;
		}

		n = ase_awk_matchrex (
			run->awk, rex_code, 
			((run->global.ignorecase)? ASE_AWK_REX_IGNORECASE: 0),
			str, len, ASE_NULL, ASE_NULL, &errnum);
		if (n == -1) 
		{
			ASE_AWK_FREE (run->awk, str);
			if (right->type != ASE_AWK_VAL_REX) 
				ASE_AWK_FREE (run->awk, rex_code);

			ase_awk_setrunerror (run, errnum, lline, ASE_NULL, 0);
			return ASE_NULL;
		}

		res = ase_awk_makeintval (run, (n == ret));
		if (res == ASE_NULL) 
		{
			ASE_AWK_FREE (run->awk, str);
			if (right->type != ASE_AWK_VAL_REX) 
				ASE_AWK_FREE (run->awk, rex_code);

			/* adjust error line */
			run->errlin = lline;
			return ASE_NULL;
		}

		ASE_AWK_FREE (run->awk, str);
	}

	if (right->type != ASE_AWK_VAL_REX) ASE_AWK_FREE (run->awk, rex_code);
	return res;
}

static ase_awk_val_t* eval_unary (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_val_t* left, * res = ASE_NULL;
	ase_awk_nde_exp_t* exp = (ase_awk_nde_exp_t*)nde;
	int n;
	ase_long_t l;
	ase_real_t r;

	ASE_ASSERT (
		exp->type == ASE_AWK_NDE_EXP_UNR);
	ASE_ASSERT (
		exp->left != ASE_NULL && exp->right == ASE_NULL);
	ASE_ASSERT (
		exp->opcode == ASE_AWK_UNROP_PLUS ||
		exp->opcode == ASE_AWK_UNROP_MINUS ||
		exp->opcode == ASE_AWK_UNROP_LNOT ||
		exp->opcode == ASE_AWK_UNROP_BNOT);

	ASE_ASSERT (exp->left->next == ASE_NULL);
	left = eval_expression (run, exp->left);
	if (left == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, left);

	if (exp->opcode == ASE_AWK_UNROP_MINUS)
	{
		n = ase_awk_valtonum (run, left, &l, &r);
		if (n == -1) goto exit_func;

		res = (n == 0)? ase_awk_makeintval (run, -l):
		                ase_awk_makerealval (run, -r);
	}
	else if (exp->opcode == ASE_AWK_UNROP_LNOT)
	{
		if (left->type == ASE_AWK_VAL_STR)
		{
			res = ase_awk_makeintval (
				run, !(((ase_awk_val_str_t*)left)->len > 0));
		}
		else
		{
			n = ase_awk_valtonum (run, left, &l, &r);
			if (n == -1) goto exit_func;

			res = (n == 0)? ase_awk_makeintval (run, !l):
			                ase_awk_makerealval (run, !r);
		}
	}
	else if (exp->opcode == ASE_AWK_UNROP_BNOT)
	{
		n = ase_awk_valtonum (run, left, &l, &r);
		if (n == -1) goto exit_func;

		if (n == 1) l = (ase_long_t)r;
		res = ase_awk_makeintval (run, ~l);
	}
	else if (exp->opcode == ASE_AWK_UNROP_PLUS) 
	{
		n = ase_awk_valtonum (run, left, &l, &r);
		if (n == -1) goto exit_func;

		res = (n == 0)? ase_awk_makeintval (run, l):
		                ase_awk_makerealval (run, r);
	}

exit_func:
	ase_awk_refdownval (run, left);
	if (res == ASE_NULL) run->errlin = nde->line;
	return res;
}

static ase_awk_val_t* eval_incpre (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_val_t* left, * res;
	ase_awk_nde_exp_t* exp = (ase_awk_nde_exp_t*)nde;

	ASE_ASSERT (exp->type == ASE_AWK_NDE_EXP_INCPRE);
	ASE_ASSERT (exp->left != ASE_NULL && exp->right == ASE_NULL);

	/* this way of checking if the l-value is assignable is
	 * ugly as it is dependent of the values defined in tree.h.
	 * but let's keep going this way for the time being. */
	if (exp->left->type < ASE_AWK_NDE_NAMED ||
	    exp->left->type > ASE_AWK_NDE_ARGIDX)
	{
		ase_awk_setrunerror (
			run, ASE_AWK_EOPERAND, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	ASE_ASSERT (exp->left->next == ASE_NULL);
	left = eval_expression (run, exp->left);
	if (left == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, left);

	if (exp->opcode == ASE_AWK_INCOP_PLUS) 
	{
		if (left->type == ASE_AWK_VAL_INT)
		{
			ase_long_t r = ((ase_awk_val_int_t*)left)->val;
			res = ase_awk_makeintval (run, r + 1);
			if (res == ASE_NULL) 
			{
				ase_awk_refdownval (run, left);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}
		else if (left->type == ASE_AWK_VAL_REAL)
		{
			ase_real_t r = ((ase_awk_val_real_t*)left)->val;
			res = ase_awk_makerealval (run, r + 1.0);
			if (res == ASE_NULL) 
			{
				ase_awk_refdownval (run, left);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}
		else
		{
			ase_long_t v1;
			ase_real_t v2;
			int n;

			n = ase_awk_valtonum (run, left, &v1, &v2);
			if (n == -1)
			{
				ase_awk_refdownval (run, left);
				/*
				ase_awk_setrunerror (
					run, ASE_AWK_EOPERAND, nde->line,
					ASE_NULL, 0);
				*/
				run->errlin = nde->line;
				return ASE_NULL;
			}

			if (n == 0) 
			{
				res = ase_awk_makeintval (run, v1 + 1);
			}
			else /* if (n == 1) */
			{
				ASE_ASSERT (n == 1);
				res = ase_awk_makerealval (run, v2 + 1.0);
			}

			if (res == ASE_NULL) 
			{
				ase_awk_refdownval (run, left);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}
	}
	else if (exp->opcode == ASE_AWK_INCOP_MINUS)
	{
		if (left->type == ASE_AWK_VAL_INT)
		{
			ase_long_t r = ((ase_awk_val_int_t*)left)->val;
			res = ase_awk_makeintval (run, r - 1);
			if (res == ASE_NULL) 
			{
				ase_awk_refdownval (run, left);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}
		else if (left->type == ASE_AWK_VAL_REAL)
		{
			ase_real_t r = ((ase_awk_val_real_t*)left)->val;
			res = ase_awk_makerealval (run, r - 1.0);
			if (res == ASE_NULL) 
			{
				ase_awk_refdownval (run, left);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}
		else
		{
			ase_long_t v1;
			ase_real_t v2;
			int n;

			n = ase_awk_valtonum (run, left, &v1, &v2);
			if (n == -1)
			{
				ase_awk_refdownval (run, left);
				/*
				ase_awk_setrunerror (
					run, ASE_AWK_EOPERAND, nde->line, 
					ASE_NULL, 0);
				*/
				run->errlin = nde->line;
				return ASE_NULL;
			}

			if (n == 0) 
			{
				res = ase_awk_makeintval (run, v1 - 1);
			}
			else /* if (n == 1) */
			{
				ASE_ASSERT (n == 1);
				res = ase_awk_makerealval (run, v2 - 1.0);
			}

			if (res == ASE_NULL) 
			{
				ase_awk_refdownval (run, left);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}
	}
	else
	{
		ASE_ASSERT (
			!"should never happen - invalid opcode");
		ase_awk_refdownval (run, left);

		ase_awk_setrunerror (
			run, ASE_AWK_EINTERN, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	if (do_assignment (run, exp->left, res) == ASE_NULL)
	{
		ase_awk_refdownval (run, left);
		return ASE_NULL;
	}

	ase_awk_refdownval (run, left);
	return res;
}

static ase_awk_val_t* eval_incpst (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_val_t* left, * res, * res2;
	ase_awk_nde_exp_t* exp = (ase_awk_nde_exp_t*)nde;

	ASE_ASSERT (exp->type == ASE_AWK_NDE_EXP_INCPST);
	ASE_ASSERT (exp->left != ASE_NULL && exp->right == ASE_NULL);

	/* this way of checking if the l-value is assignable is
	 * ugly as it is dependent of the values defined in tree.h.
	 * but let's keep going this way for the time being. */
	if (exp->left->type < ASE_AWK_NDE_NAMED ||
	    exp->left->type > ASE_AWK_NDE_ARGIDX)
	{
		ase_awk_setrunerror (
			run, ASE_AWK_EOPERAND, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	ASE_ASSERT (exp->left->next == ASE_NULL);
	left = eval_expression (run, exp->left);
	if (left == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, left);

	if (exp->opcode == ASE_AWK_INCOP_PLUS) 
	{
		if (left->type == ASE_AWK_VAL_INT)
		{
			ase_long_t r = ((ase_awk_val_int_t*)left)->val;
			res = ase_awk_makeintval (run, r);
			if (res == ASE_NULL) 
			{
				ase_awk_refdownval (run, left);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}

			res2 = ase_awk_makeintval (run, r + 1);
			if (res2 == ASE_NULL)
			{
				ase_awk_refdownval (run, left);
				ase_awk_freeval (run, res, ase_true);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}
		else if (left->type == ASE_AWK_VAL_REAL)
		{
			ase_real_t r = ((ase_awk_val_real_t*)left)->val;
			res = ase_awk_makerealval (run, r);
			if (res == ASE_NULL) 
			{
				ase_awk_refdownval (run, left);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}

			res2 = ase_awk_makerealval (run, r + 1.0);
			if (res2 == ASE_NULL)
			{
				ase_awk_refdownval (run, left);
				ase_awk_freeval (run, res, ase_true);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}
		else
		{
			ase_long_t v1;
			ase_real_t v2;
			int n;

			n = ase_awk_valtonum (run, left, &v1, &v2);
			if (n == -1)
			{
				ase_awk_refdownval (run, left);
				/*
				ase_awk_setrunerror (
					run, ASE_AWK_EOPERAND, nde->line,
					ASE_NULL, 0);
				*/
				run->errlin = nde->line;
				return ASE_NULL;
			}

			if (n == 0) 
			{
				res = ase_awk_makeintval (run, v1);
				if (res == ASE_NULL)
				{
					ase_awk_refdownval (run, left);
					/* adjust error line */
					run->errlin = nde->line;
					return ASE_NULL;
				}

				res2 = ase_awk_makeintval (run, v1 + 1);
				if (res2 == ASE_NULL)
				{
					ase_awk_refdownval (run, left);
					ase_awk_freeval (run, res, ase_true);
					/* adjust error line */
					run->errlin = nde->line;
					return ASE_NULL;
				}
			}
			else /* if (n == 1) */
			{
				ASE_ASSERT (n == 1);
				res = ase_awk_makerealval (run, v2);
				if (res == ASE_NULL)
				{
					ase_awk_refdownval (run, left);
					/* adjust error line */
					run->errlin = nde->line;
					return ASE_NULL;
				}

				res2 = ase_awk_makerealval (run, v2 + 1.0);
				if (res2 == ASE_NULL)
				{
					ase_awk_refdownval (run, left);
					ase_awk_freeval (run, res, ase_true);
					/* adjust error line */
					run->errlin = nde->line;
					return ASE_NULL;
				}
			}
		}
	}
	else if (exp->opcode == ASE_AWK_INCOP_MINUS)
	{
		if (left->type == ASE_AWK_VAL_INT)
		{
			ase_long_t r = ((ase_awk_val_int_t*)left)->val;
			res = ase_awk_makeintval (run, r);
			if (res == ASE_NULL) 
			{
				ase_awk_refdownval (run, left);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}

			res2 = ase_awk_makeintval (run, r - 1);
			if (res2 == ASE_NULL)
			{
				ase_awk_refdownval (run, left);
				ase_awk_freeval (run, res, ase_true);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}
		else if (left->type == ASE_AWK_VAL_REAL)
		{
			ase_real_t r = ((ase_awk_val_real_t*)left)->val;
			res = ase_awk_makerealval (run, r);
			if (res == ASE_NULL) 
			{
				ase_awk_refdownval (run, left);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}

			res2 = ase_awk_makerealval (run, r - 1.0);
			if (res2 == ASE_NULL)
			{
				ase_awk_refdownval (run, left);
				ase_awk_freeval (run, res, ase_true);
				/* adjust error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}
		else
		{
			ase_long_t v1;
			ase_real_t v2;
			int n;

			n = ase_awk_valtonum (run, left, &v1, &v2);
			if (n == -1)
			{
				ase_awk_refdownval (run, left);
				/*
				ase_awk_setrunerror (
					run, ASE_AWK_EOPERAND, nde->line, 
					ASE_NULL, 0);
				*/
				run->errlin = nde->line;
				return ASE_NULL;
			}

			if (n == 0) 
			{
				res = ase_awk_makeintval (run, v1);
				if (res == ASE_NULL)
				{
					ase_awk_refdownval (run, left);
					/* adjust error line */
					run->errlin = nde->line;
					return ASE_NULL;
				}

				res2 = ase_awk_makeintval (run, v1 - 1);
				if (res2 == ASE_NULL)
				{
					ase_awk_refdownval (run, left);
					ase_awk_freeval (run, res, ase_true);
					/* adjust error line */
					run->errlin = nde->line;
					return ASE_NULL;
				}
			}
			else /* if (n == 1) */
			{
				ASE_ASSERT (n == 1);
				res = ase_awk_makerealval (run, v2);
				if (res == ASE_NULL)
				{
					ase_awk_refdownval (run, left);
					/* adjust error line */
					run->errlin = nde->line;
					return ASE_NULL;
				}

				res2 = ase_awk_makerealval (run, v2 - 1.0);
				if (res2 == ASE_NULL)
				{
					ase_awk_refdownval (run, left);
					ase_awk_freeval (run, res, ase_true);
					/* adjust error line */
					run->errlin = nde->line;
					return ASE_NULL;
				}
			}
		}
	}
	else
	{
		ASE_ASSERT (
			!"should never happen - invalid opcode");
		ase_awk_refdownval (run, left);

		ase_awk_setrunerror (
			run, ASE_AWK_EINTERN, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	if (do_assignment (run, exp->left, res2) == ASE_NULL)
	{
		ase_awk_refdownval (run, left);
		return ASE_NULL;
	}

	ase_awk_refdownval (run, left);
	return res;
}

static ase_awk_val_t* eval_cnd (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_val_t* tv, * v;
	ase_awk_nde_cnd_t* cnd = (ase_awk_nde_cnd_t*)nde;

	ASE_ASSERT (cnd->test->next == ASE_NULL);

	tv = eval_expression (run, cnd->test);
	if (tv == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, tv);

	ASE_ASSERT (
		cnd->left->next == ASE_NULL && 
		cnd->right->next == ASE_NULL);
	v = (ase_awk_valtobool (run, tv))?
		eval_expression (run, cnd->left):
		eval_expression (run, cnd->right);

	ase_awk_refdownval (run, tv);
	return v;
}

static ase_awk_val_t* eval_bfn (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_nde_call_t* call = (ase_awk_nde_call_t*)nde;

	/* intrinsic function */
	if (call->nargs < call->what.bfn.arg.min)
	{
		ase_awk_setrunerror (
			run, ASE_AWK_EARGTF, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	if (call->nargs > call->what.bfn.arg.max)
	{
		ase_awk_setrunerror (
			run, ASE_AWK_EARGTM, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	return eval_call (
		run, nde, call->what.bfn.arg.spec, 
		ASE_NULL, ASE_NULL, ASE_NULL);
}

static ase_awk_val_t* eval_afn_intrinsic (
	ase_awk_run_t* run, ase_awk_nde_t* nde, void(*errhandler)(void*), void* eharg)
{
	ase_awk_nde_call_t* call = (ase_awk_nde_call_t*)nde;
	ase_awk_afn_t* afn;
	ase_pair_t* pair;

	pair = ase_map_get (run->awk->tree.afns, 
		call->what.afn.name.ptr, call->what.afn.name.len);
	if (pair == ASE_NULL) 
	{
		ase_cstr_t errarg;

		errarg.ptr = call->what.afn.name.ptr;
		errarg.len = call->what.afn.name.len,

		ase_awk_setrunerror (run, 
			ASE_AWK_EFNNONE, nde->line, &errarg, 1);
		return ASE_NULL;
	}

	afn = (ase_awk_afn_t*)pair->val;
	ASE_ASSERT (afn != ASE_NULL);

	if (call->nargs > afn->nargs)
	{
		/* TODO: is this correct? what if i want to 
		 *       allow arbitarary numbers of arguments? */
		ase_awk_setrunerror (
			run, ASE_AWK_EARGTM, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	return eval_call (run, nde, ASE_NULL, afn, errhandler, eharg);
}

static ase_awk_val_t* eval_afn (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	return eval_afn_intrinsic (run, nde, ASE_NULL, ASE_NULL);
}

/* run->stack_base has not been set for this  
 * stack frame. so the STACK_ARG macro cannot be used as in 
 * ase_awk_refdownval (run, STACK_ARG(run,nargs));*/ 

#define UNWIND_RUN_STACK(run,nargs) \
	do { \
		while ((nargs) > 0) \
		{ \
			--(nargs); \
			ase_awk_refdownval ((run), \
				(run)->stack[(run)->stack_top-1]); \
			__raw_pop (run); \
		} \
		__raw_pop (run); /* nargs */ \
		__raw_pop (run); /* return */ \
		__raw_pop (run); /* prev stack top */ \
		__raw_pop (run); /* prev stack back */ \
	} while (0)

static ase_awk_val_t* eval_call (
	ase_awk_run_t* run, ase_awk_nde_t* nde, 
	const ase_char_t* bfn_arg_spec, ase_awk_afn_t* afn, 
	void(*errhandler)(void*), void* eharg)
{
	ase_awk_nde_call_t* call = (ase_awk_nde_call_t*)nde;
	ase_size_t saved_stack_top;
	ase_size_t nargs, i;
	ase_awk_nde_t* p;
	ase_awk_val_t* v;
	int n;

	/* 
	 * ---------------------
	 *  localn               <- stack top
	 * ---------------------
	 *  ....
	 * ---------------------
	 *  local0               local variables are pushed by run_block
	 * =====================
	 *  argn                     
	 * ---------------------
	 *  ....
	 * ---------------------
	 *  arg1
	 * ---------------------
	 *  arg0 
	 * ---------------------
	 *  nargs 
	 * ---------------------
	 *  return value
	 * ---------------------
	 *  previous stack top
	 * ---------------------
	 *  previous stack base  <- stack base
	 * =====================
	 *  0 (nargs)            <- stack top
	 * ---------------------
	 *  return value
	 * ---------------------
	 *  previous stack top
	 * ---------------------
	 *  previous stack base  <- stack base
	 * =====================
	 *  globaln
	 * ---------------------
	 *  ....
	 * ---------------------
	 *  global0
	 * ---------------------
	 */

	ASE_ASSERT (
		ASE_SIZEOF(void*) >= ASE_SIZEOF(run->stack_top));
	ASE_ASSERT (
		ASE_SIZEOF(void*) >= ASE_SIZEOF(run->stack_base));

	saved_stack_top = run->stack_top;

#ifdef DEBUG_RUN
	ase_dprintf (ASE_T("setting up function stack frame top=%ld base=%ld\n"), 
		(long)run->stack_top, (long)run->stack_base);
#endif
	if (__raw_push(run,(void*)run->stack_base) == -1) 
	{
		ase_awk_setrunerror (
			run, ASE_AWK_ENOMEM, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	if (__raw_push(run,(void*)saved_stack_top) == -1) 
	{
		__raw_pop (run);
		ase_awk_setrunerror (
			run, ASE_AWK_ENOMEM, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	/* secure space for a return value. */
	if (__raw_push(run,ase_awk_val_nil) == -1)
	{
		__raw_pop (run);
		__raw_pop (run);
		ase_awk_setrunerror (
			run, ASE_AWK_ENOMEM, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	/* secure space for nargs */
	if (__raw_push(run,ase_awk_val_nil) == -1)
	{
		__raw_pop (run);
		__raw_pop (run);
		__raw_pop (run);
		ase_awk_setrunerror (
			run, ASE_AWK_ENOMEM, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	for (p = call->args, nargs = 0; p != ASE_NULL; p = p->next, nargs++)
	{
		ASE_ASSERT (
			bfn_arg_spec == ASE_NULL ||
			(bfn_arg_spec != ASE_NULL && 
			 ase_strlen(bfn_arg_spec) > nargs));

		if (bfn_arg_spec != ASE_NULL && 
		    bfn_arg_spec[nargs] == ASE_T('r'))
		{
			ase_awk_val_t** ref;
			      
			if (get_reference (run, p, &ref) == -1)
			{
				UNWIND_RUN_STACK (run, nargs);
				return ASE_NULL;
			}

			/* p->type-ASE_AWK_NDE_NAMED assumes that the
			 * derived value matches ASE_AWK_VAL_REF_XXX */
			v = ase_awk_makerefval (
				run, p->type-ASE_AWK_NDE_NAMED, ref);
		}
		else if (bfn_arg_spec != ASE_NULL && 
		         bfn_arg_spec[nargs] == ASE_T('x'))
		{
			/* a regular expression is passed to 
			 * the function as it is */
			v = eval_expression0 (run, p);
		}
		else
		{
			v = eval_expression (run, p);
		}
		if (v == ASE_NULL)
		{
			UNWIND_RUN_STACK (run, nargs);
			return ASE_NULL;
		}

		if (__raw_push(run,v) == -1) 
		{
			/* ugly - v needs to be freed if it doesn't have
			 * any reference. but its reference has not been 
			 * updated yet as it is carried out after the 
			 * successful stack push. so it adds up a reference 
			 * and dereferences it */
			ase_awk_refupval (run, v);
			ase_awk_refdownval (run, v);

			UNWIND_RUN_STACK (run, nargs);
			ase_awk_setrunerror (
				run, ASE_AWK_ENOMEM, nde->line, ASE_NULL, 0);
			return ASE_NULL;
		}

		ase_awk_refupval (run, v);
		/*nargs++; p = p->next;*/
	}

	ASE_ASSERT (nargs == call->nargs);

	if (afn != ASE_NULL)
	{
		/* extra step for normal awk functions */

		while (nargs < afn->nargs)
		{
			/* push as many nils as the number of missing actual arguments */
			if (__raw_push(run,ase_awk_val_nil) == -1)
			{
				UNWIND_RUN_STACK (run, nargs);
				ase_awk_setrunerror (
					run, ASE_AWK_ENOMEM, nde->line,
					ASE_NULL, 0);
				return ASE_NULL;
			}

			nargs++;
		}
	}

	run->stack_base = saved_stack_top;
	STACK_NARGS(run) = (void*)nargs;
	
#ifdef DEBUG_RUN
	ase_dprintf (ASE_T("running function body\n"));
#endif

	if (afn != ASE_NULL)
	{
		/* normal awk function */
		ASE_ASSERT (afn->body->type == ASE_AWK_NDE_BLK);
		n = run_block(run,(ase_awk_nde_blk_t*)afn->body);
	}
	else
	{
		n = 0;

		/* intrinsic function */
		ASE_ASSERT (
			call->nargs >= call->what.bfn.arg.min &&
			call->nargs <= call->what.bfn.arg.max);

		if (call->what.bfn.handler != ASE_NULL)
		{
			run->errnum = ASE_AWK_ENOERR;

			/* NOTE: oname is used when the handler is invoked.
			 *       name might be differnt from oname if 
			 *       ase_awk_setword has been used */
			n = call->what.bfn.handler (
				run,
				call->what.bfn.oname.ptr, 
				call->what.bfn.oname.len);

			if (n <= -1)
			{
				if (run->errnum == ASE_AWK_ENOERR)
				{
					/* the handler has not set the error.
					 * fix it */ 
					ase_awk_setrunerror (
						run, ASE_AWK_EBFNIMPL, 
						nde->line, ASE_NULL, 0);
				}
				else
				{
					/* adjust the error line */
					run->errlin = nde->line;	
				}

				/* correct the return code just in case */
				if (n < -1) n = -1;
			}
		}
	}

	/* refdown args in the run.stack */
	nargs = (ase_size_t)STACK_NARGS(run);
#ifdef DEBUG_RUN
	ase_dprintf (ASE_T("block run complete nargs = %d\n"), (int)nargs); 
#endif
	for (i = 0; i < nargs; i++)
	{
		ase_awk_refdownval (run, STACK_ARG(run,i));
	}

#ifdef DEBUG_RUN
	ase_dprintf (ASE_T("got return value\n"));
#endif

	v = STACK_RETVAL(run);
	if (n == -1)
	{
		if (errhandler != ASE_NULL) 
		{
			/* capture_retval_on_exit takes advantage of 
			 * this handler to retrieve the return value
			 * when exit() is used to terminate the program. */
			errhandler (eharg);
		}

		/* if the earlier operations failed and this function
		 * has to return a error, the return value is just
		 * destroyed and replaced by nil */
		ase_awk_refdownval (run, v);
		STACK_RETVAL(run) = ase_awk_val_nil;
	}
	else
	{	
		/* this trick has been mentioned in run_return.
		 * adjust the reference count of the return value.
		 * the value must not be freed even if the reference count
		 * reached zero because its reference has been incremented 
		 * in run_return or directly by ase_awk_setretval
		 * regardless of its reference count. */
		ase_awk_refdownval_nofree (run, v);
	}

	run->stack_top =  (ase_size_t)run->stack[run->stack_base+1];
	run->stack_base = (ase_size_t)run->stack[run->stack_base+0];

	if (run->exit_level == EXIT_FUNCTION) run->exit_level = EXIT_NONE;

#ifdef DEBUG_RUN
	ase_dprintf (ASE_T("returning from function top=%ld, base=%ld\n"),
		(long)run->stack_top, (long)run->stack_base); 
#endif
	return (n == -1)? ASE_NULL: v;
}

static int get_reference (
	ase_awk_run_t* run, ase_awk_nde_t* nde, ase_awk_val_t*** ref)
{
	ase_awk_nde_var_t* tgt = (ase_awk_nde_var_t*)nde;
	ase_awk_val_t** tmp;

	/* refer to eval_indexed for application of a similar concept */

	if (nde->type == ASE_AWK_NDE_NAMED)
	{
		ase_pair_t* pair;

		pair = ase_map_get (
			run->named, tgt->id.name, tgt->id.name_len);
		if (pair == ASE_NULL)
		{
			/* it is bad that the named variable has to be
			 * created in the function named "__get_refernce".
			 * would there be any better ways to avoid this? */
			pair = ase_map_put (
				run->named, tgt->id.name,
				tgt->id.name_len, ase_awk_val_nil);
			if (pair == ASE_NULL) 
			{
				ase_awk_setrunerror (
					run, ASE_AWK_ENOMEM, nde->line,
					ASE_NULL, 0);
				return -1;
			}
		}

		*ref = (ase_awk_val_t**)&pair->val;
		return 0;
	}

	if (nde->type == ASE_AWK_NDE_GLOBAL)
	{
		*ref = (ase_awk_val_t**)&STACK_GLOBAL(run,tgt->id.idxa);
		return 0;
	}

	if (nde->type == ASE_AWK_NDE_LOCAL)
	{
		*ref = (ase_awk_val_t**)&STACK_LOCAL(run,tgt->id.idxa);
		return 0;
	}

	if (nde->type == ASE_AWK_NDE_ARG)
	{
		*ref = (ase_awk_val_t**)&STACK_ARG(run,tgt->id.idxa);
		return 0;
	}

	if (nde->type == ASE_AWK_NDE_NAMEDIDX)
	{
		ase_pair_t* pair;

		pair = ase_map_get (
			run->named, tgt->id.name, tgt->id.name_len);
		if (pair == ASE_NULL)
		{
			pair = ase_map_put (
				run->named, tgt->id.name,
				tgt->id.name_len, ase_awk_val_nil);
			if (pair == ASE_NULL) 
			{
				ase_awk_setrunerror (
					run, ASE_AWK_ENOMEM, nde->line,
					ASE_NULL, 0);
				return -1;
			}
		}

		tmp = get_reference_indexed (
			run, tgt, (ase_awk_val_t**)&pair->val);
		if (tmp == ASE_NULL) return -1;
		*ref = tmp;
	}

	if (nde->type == ASE_AWK_NDE_GLOBALIDX)
	{
		tmp = get_reference_indexed (run, tgt, 
			(ase_awk_val_t**)&STACK_GLOBAL(run,tgt->id.idxa));
		if (tmp == ASE_NULL) return -1;
		*ref = tmp;
	}

	if (nde->type == ASE_AWK_NDE_LOCALIDX)
	{
		tmp = get_reference_indexed (run, tgt, 
			(ase_awk_val_t**)&STACK_LOCAL(run,tgt->id.idxa));
		if (tmp == ASE_NULL) return -1;
		*ref = tmp;
	}

	if (nde->type == ASE_AWK_NDE_ARGIDX)
	{
		tmp = get_reference_indexed (run, tgt, 
			(ase_awk_val_t**)&STACK_ARG(run,tgt->id.idxa));
		if (tmp == ASE_NULL) return -1;
		*ref = tmp;
	}

	if (nde->type == ASE_AWK_NDE_POS)
	{
		int n;
		ase_long_t lv;
		ase_real_t rv;
		ase_awk_val_t* v;

		/* the position number is returned for the positional 
		 * variable unlike other reference types. */
		v = eval_expression (run, ((ase_awk_nde_pos_t*)nde)->val);
		if (v == ASE_NULL) return -1;

		ase_awk_refupval (run, v);
		n = ase_awk_valtonum (run, v, &lv, &rv);
		ase_awk_refdownval (run, v);

		if (n == -1) 
		{
			ase_awk_setrunerror (
				run, ASE_AWK_EPOSIDX, nde->line, ASE_NULL, 0);
			return -1;
		}
		if (n == 1) lv = (ase_long_t)rv;
		if (!IS_VALID_POSIDX(lv)) 
		{
			ase_awk_setrunerror (
				run, ASE_AWK_EPOSIDX, nde->line, ASE_NULL, 0);
			return -1;
		}

		*ref = (ase_awk_val_t**)((ase_size_t)lv);
		return 0;
	}

	ase_awk_setrunerror (run, ASE_AWK_ENOTREF, nde->line, ASE_NULL, 0);
	return -1;
}

static ase_awk_val_t** get_reference_indexed (
	ase_awk_run_t* run, ase_awk_nde_var_t* nde, ase_awk_val_t** val)
{
	ase_pair_t* pair;
	ase_char_t* str;
	ase_size_t len;
	ase_char_t idxbuf[IDXBUFSIZE];

	ASE_ASSERT (val != ASE_NULL);

	if ((*val)->type == ASE_AWK_VAL_NIL)
	{
		ase_awk_val_t* tmp;

		tmp = ase_awk_makemapval (run);
		if (tmp == ASE_NULL)
		{
			run->errlin = nde->line;
			return ASE_NULL;
		}

		ase_awk_refdownval (run, *val);
		*val = tmp;
		ase_awk_refupval (run, (ase_awk_val_t*)*val);
	}
	else if ((*val)->type != ASE_AWK_VAL_MAP) 
	{
		ase_awk_setrunerror (
			run, ASE_AWK_ENOTMAP, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	ASE_ASSERT (nde->idx != ASE_NULL);

	len = ASE_COUNTOF(idxbuf);
	str = idxnde_to_str (run, nde->idx, idxbuf, &len);
	if (str == ASE_NULL)
	{
		str = idxnde_to_str (run, nde->idx, ASE_NULL, &len);
		if (str == ASE_NULL) return ASE_NULL;
	}

	pair = ase_map_get ((*(ase_awk_val_map_t**)val)->map, str, len);
	if (pair == ASE_NULL)
	{
		pair = ase_map_put (
			(*(ase_awk_val_map_t**)val)->map, 
			str, len, ase_awk_val_nil);
		if (pair == ASE_NULL)
		{
			if (str != idxbuf) ASE_AWK_FREE (run->awk, str);
			ase_awk_setrunerror (
				run, ASE_AWK_ENOMEM, nde->line, ASE_NULL, 0);
			return ASE_NULL;
		}

		ase_awk_refupval (run, pair->val);
	}

	if (str != idxbuf) ASE_AWK_FREE (run->awk, str);
	return (ase_awk_val_t**)&pair->val;
}

static ase_awk_val_t* eval_int (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_val_t* val;

	val = ase_awk_makeintval (run, ((ase_awk_nde_int_t*)nde)->val);
	if (val == ASE_NULL)
	{
		run->errlin = nde->line;
		return ASE_NULL;
	}
	((ase_awk_val_int_t*)val)->nde = (ase_awk_nde_int_t*)nde; 

	return val;
}

static ase_awk_val_t* eval_real (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_val_t* val;

	val = ase_awk_makerealval (run, ((ase_awk_nde_real_t*)nde)->val);
	if (val == ASE_NULL) 
	{
		run->errlin = nde->line;
		return ASE_NULL;
	}
	((ase_awk_val_real_t*)val)->nde = (ase_awk_nde_real_t*)nde;

	return val;
}

static ase_awk_val_t* eval_str (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_val_t* val;

	val = ase_awk_makestrval (run,
		((ase_awk_nde_str_t*)nde)->buf,
		((ase_awk_nde_str_t*)nde)->len);
	if (val == ASE_NULL)
	{
		run->errlin = nde->line;
		return ASE_NULL;
	}

	return val;
}

static ase_awk_val_t* eval_rex (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_val_t* val;

	val = ase_awk_makerexval (run,
		((ase_awk_nde_rex_t*)nde)->buf,
		((ase_awk_nde_rex_t*)nde)->len,
		((ase_awk_nde_rex_t*)nde)->code);
	if (val == ASE_NULL) 
	{
		run->errlin = nde->line;
		return ASE_NULL;
	}

	return val;
}

static ase_awk_val_t* eval_named (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_pair_t* pair;
		       
	pair = ase_map_get (run->named, 
		((ase_awk_nde_var_t*)nde)->id.name, 
		((ase_awk_nde_var_t*)nde)->id.name_len);

	return (pair == ASE_NULL)? ase_awk_val_nil: pair->val;
}

static ase_awk_val_t* eval_global (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	return STACK_GLOBAL(run,((ase_awk_nde_var_t*)nde)->id.idxa);
}

static ase_awk_val_t* eval_local (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	return STACK_LOCAL(run,((ase_awk_nde_var_t*)nde)->id.idxa);
}

static ase_awk_val_t* eval_arg (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	return STACK_ARG(run,((ase_awk_nde_var_t*)nde)->id.idxa);
}

static ase_awk_val_t* eval_indexed (
	ase_awk_run_t* run, ase_awk_nde_var_t* nde, ase_awk_val_t** val)
{
	ase_pair_t* pair;
	ase_char_t* str;
	ase_size_t len;
	ase_char_t idxbuf[IDXBUFSIZE];

	ASE_ASSERT (val != ASE_NULL);

	if ((*val)->type == ASE_AWK_VAL_NIL)
	{
		ase_awk_val_t* tmp;

		tmp = ase_awk_makemapval (run);
		if (tmp == ASE_NULL)
		{
			run->errlin = nde->line;
			return ASE_NULL;
		}

		ase_awk_refdownval (run, *val);
		*val = tmp;
		ase_awk_refupval (run, (ase_awk_val_t*)*val);
	}
	else if ((*val)->type != ASE_AWK_VAL_MAP) 
	{
		ase_awk_setrunerror (
			run, ASE_AWK_ENOTMAP, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	ASE_ASSERT (nde->idx != ASE_NULL);

	len = ASE_COUNTOF(idxbuf);
	str = idxnde_to_str (run, nde->idx, idxbuf, &len);
	if (str == ASE_NULL) 
	{
		str = idxnde_to_str (run, nde->idx, ASE_NULL, &len);
		if (str == ASE_NULL) return ASE_NULL;
	}

	pair = ase_map_get ((*(ase_awk_val_map_t**)val)->map, str, len);
	if (str != idxbuf) ASE_AWK_FREE (run->awk, str);

	return (pair == ASE_NULL)? ase_awk_val_nil: (ase_awk_val_t*)pair->val;
}

static ase_awk_val_t* eval_namedidx (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_nde_var_t* tgt = (ase_awk_nde_var_t*)nde;
	ase_pair_t* pair;

	pair = ase_map_get (run->named, tgt->id.name, tgt->id.name_len);
	if (pair == ASE_NULL)
	{
		pair = ase_map_put (run->named, 
			tgt->id.name, tgt->id.name_len, ase_awk_val_nil);
		if (pair == ASE_NULL) 
		{
			ase_awk_setrunerror (
				run, ASE_AWK_ENOMEM, nde->line, ASE_NULL, 0);
			return ASE_NULL;
		}

		ase_awk_refupval (run, pair->val); 
	}

	return eval_indexed (run, tgt, (ase_awk_val_t**)&pair->val);
}

static ase_awk_val_t* eval_globalidx (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	return eval_indexed (run, (ase_awk_nde_var_t*)nde, 
		(ase_awk_val_t**)&STACK_GLOBAL(run,((ase_awk_nde_var_t*)nde)->id.idxa));
}

static ase_awk_val_t* eval_localidx (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	return eval_indexed (run, (ase_awk_nde_var_t*)nde, 
		(ase_awk_val_t**)&STACK_LOCAL(run,((ase_awk_nde_var_t*)nde)->id.idxa));
}

static ase_awk_val_t* eval_argidx (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	return eval_indexed (run, (ase_awk_nde_var_t*)nde,
		(ase_awk_val_t**)&STACK_ARG(run,((ase_awk_nde_var_t*)nde)->id.idxa));
}

static ase_awk_val_t* eval_pos (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_nde_pos_t* pos = (ase_awk_nde_pos_t*)nde;
	ase_awk_val_t* v;
	ase_long_t lv;
	ase_real_t rv;
	int n;

	v = eval_expression (run, pos->val);
	if (v == ASE_NULL) return ASE_NULL;

	ase_awk_refupval (run, v);
	n = ase_awk_valtonum (run, v, &lv, &rv);
	ase_awk_refdownval (run, v);
	if (n == -1) 
	{
		ase_awk_setrunerror (
			run, ASE_AWK_EPOSIDX, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}
	if (n == 1) lv = (ase_long_t)rv;

	if (lv < 0)
	{
		ase_awk_setrunerror (
			run, ASE_AWK_EPOSIDX, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}
	if (lv == 0) v = run->inrec.d0;
	else if (lv > 0 && lv <= (ase_long_t)run->inrec.nflds) 
		v = run->inrec.flds[lv-1].val;
	else v = ase_awk_val_zls; /*ase_awk_val_nil;*/

	return v;
}

static ase_awk_val_t* eval_getline (ase_awk_run_t* run, ase_awk_nde_t* nde)
{
	ase_awk_nde_getline_t* p;
	ase_awk_val_t* v, * res;
	ase_char_t* in = ASE_NULL;
	const ase_char_t* dst;
	ase_str_t buf;
	int n;

	p = (ase_awk_nde_getline_t*)nde;

	ASE_ASSERT (
		(p->in_type == ASE_AWK_IN_PIPE && p->in != ASE_NULL) ||
		(p->in_type == ASE_AWK_IN_COPROC && p->in != ASE_NULL) ||
		(p->in_type == ASE_AWK_IN_FILE && p->in != ASE_NULL) ||
		(p->in_type == ASE_AWK_IN_CONSOLE && p->in == ASE_NULL));

	if (p->in != ASE_NULL)
	{
		ase_size_t len;

		v = eval_expression (run, p->in);
		if (v == ASE_NULL) return ASE_NULL;

		/* TODO: distinction between v->type == ASE_AWK_VAL_STR 
		 *       and v->type != ASE_AWK_VAL_STR
		 *       if you use the buffer the v directly when
		 *       v->type == ASE_AWK_VAL_STR, ase_awk_refdownval(v)
		 *       should not be called immediately below */
		ase_awk_refupval (run, v);
		in = ase_awk_valtostr (
			run, v, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &len);
		if (in == ASE_NULL) 
		{
			ase_awk_refdownval (run, v);
			return ASE_NULL;
		}
		ase_awk_refdownval (run, v);

		if (len <= 0) 
		{
			/* the input source name is empty.
			 * make getline return -1 */
			ASE_AWK_FREE (run->awk, in);
			n = -1;
			goto skip_read;
		}

		while (len > 0)
		{
			if (in[--len] == ASE_T('\0'))
			{
				/* the input source name contains a null 
				 * character. make getline return -1 */
				ASE_AWK_FREE (run->awk, in);
				n = -1;
				goto skip_read;
			}
		}
	}

	dst = (in == ASE_NULL)? ASE_T(""): in;

	/* TODO: optimize the line buffer management */
	if (ase_str_open (&buf, DEF_BUF_CAPA, &run->awk->prmfns.mmgr) == ASE_NULL)
	{
		if (in != ASE_NULL) ASE_AWK_FREE (run->awk, in);
		ase_awk_setrunerror (
			run, ASE_AWK_ENOMEM, nde->line, ASE_NULL, 0);
		return ASE_NULL;
	}

	n = ase_awk_readextio (run, p->in_type, dst, &buf);
	if (in != ASE_NULL) ASE_AWK_FREE (run->awk, in);

	if (n <= -1) 
	{
		/* make getline return -1 */
		n = -1;
	}

	if (n > 0)
	{
		if (p->var == ASE_NULL)
		{
			/* set $0 with the input value */
			if (ase_awk_setrec (run, 0,
				ASE_STR_BUF(&buf),
				ASE_STR_LEN(&buf)) == -1)
			{
				ase_str_close (&buf);
				return ASE_NULL;
			}

			ase_str_close (&buf);
		}
		else
		{
			ase_awk_val_t* v;

			v = ase_awk_makestrval (run, 
				ASE_STR_BUF(&buf), ASE_STR_LEN(&buf));
			ase_str_close (&buf);
			if (v == ASE_NULL)
			{
				run->errlin = nde->line;
				return ASE_NULL;
			}

			ase_awk_refupval (run, v);
			if (do_assignment(run, p->var, v) == ASE_NULL)
			{
				ase_awk_refdownval (run, v);
				return ASE_NULL;
			}
			ase_awk_refdownval (run, v);
		}
	}
	else
	{
		ase_str_close (&buf);
	}
	
skip_read:
	res = ase_awk_makeintval (run, n);
	if (res == ASE_NULL) run->errlin = nde->line;
	return res;
}

static int __raw_push (ase_awk_run_t* run, void* val)
{
	if (run->stack_top >= run->stack_limit)
	{
		void** tmp;
		ase_size_t n;
	       
		n = run->stack_limit + STACK_INCREMENT;

		if (run->awk->prmfns.mmgr.realloc != ASE_NULL)
		{
			tmp = (void**) ASE_AWK_REALLOC (
				run->awk, run->stack, n * ASE_SIZEOF(void*)); 
			if (tmp == ASE_NULL) return -1;
		}
		else
		{
			tmp = (void**) ASE_AWK_MALLOC (
				run->awk, n * ASE_SIZEOF(void*));
			if (tmp == ASE_NULL) return -1;
			if (run->stack != ASE_NULL)
			{
				ase_memcpy (
					tmp, run->stack, 
					run->stack_limit * ASE_SIZEOF(void*)); 
				ASE_AWK_FREE (run->awk, run->stack);
			}
		}
		run->stack = tmp;
		run->stack_limit = n;
	}

	run->stack[run->stack_top++] = val;
	return 0;
}

static void __raw_pop_times (ase_awk_run_t* run, ase_size_t times)
{
	while (times > 0)
	{
		--times;
		__raw_pop (run);
	}
}

static int read_record (ase_awk_run_t* run)
{
	ase_ssize_t n;

	if (ase_awk_clrrec (run, ase_false) == -1) return -1;

	n = ase_awk_readextio (
		run, ASE_AWK_IN_CONSOLE, ASE_T(""), &run->inrec.line);
	if (n <= -1) 
	{
		ase_awk_clrrec (run, ase_false);
		return -1;
	}

#ifdef DEBUG_RUN
	ase_dprintf (ASE_T("record len = %d str=[%.*s]\n"), 
			(int)ASE_STR_LEN(&run->inrec.line),
			(int)ASE_STR_LEN(&run->inrec.line),
			ASE_STR_BUF(&run->inrec.line));
#endif
	if (n == 0) 
	{
		ASE_ASSERT (ASE_STR_LEN(&run->inrec.line) == 0);
		return 0;
	}

	if (ase_awk_setrec (run, 0, 
		ASE_STR_BUF(&run->inrec.line), 
		ASE_STR_LEN(&run->inrec.line)) == -1) return -1;

	return 1;
}

static int shorten_record (ase_awk_run_t* run, ase_size_t nflds)
{
	ase_awk_val_t* v;
	ase_char_t* ofs_free = ASE_NULL, * ofs;
	ase_size_t ofs_len, i;
	ase_str_t tmp;

	ASE_ASSERT (nflds <= run->inrec.nflds);

	if (nflds > 1)
	{
		v = STACK_GLOBAL(run, ASE_AWK_GLOBAL_OFS);
		ase_awk_refupval (run, v);

		if (v->type == ASE_AWK_VAL_NIL)
		{
			/* OFS not set */
			ofs = ASE_T(" ");
			ofs_len = 1;
		}
		else if (v->type == ASE_AWK_VAL_STR)
		{
			ofs = ((ase_awk_val_str_t*)v)->buf;
			ofs_len = ((ase_awk_val_str_t*)v)->len;
		}
		else
		{
			ofs = ase_awk_valtostr (
				run, v, ASE_AWK_VALTOSTR_CLEAR, 
				ASE_NULL, &ofs_len);
			if (ofs == ASE_NULL) return -1;

			ofs_free = ofs;
		}
	}

	if (ase_str_open (
		&tmp, ASE_STR_LEN(&run->inrec.line), 
		&run->awk->prmfns.mmgr) == ASE_NULL)
	{
		ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
		return -1;
	}

	for (i = 0; i < nflds; i++)
	{
		if (i > 0 && ase_str_ncat(&tmp,ofs,ofs_len) == (ase_size_t)-1)
		{
			if (ofs_free != ASE_NULL) 
				ASE_AWK_FREE (run->awk, ofs_free);
			if (nflds > 1) ase_awk_refdownval (run, v);
			ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
			return -1;
		}

		if (ase_str_ncat (&tmp, 
			run->inrec.flds[i].ptr, 
			run->inrec.flds[i].len) == (ase_size_t)-1)
		{
			if (ofs_free != ASE_NULL) 
				ASE_AWK_FREE (run->awk, ofs_free);
			if (nflds > 1) ase_awk_refdownval (run, v);

			ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
			return -1;
		}
	}

	if (ofs_free != ASE_NULL) ASE_AWK_FREE (run->awk, ofs_free);
	if (nflds > 1) ase_awk_refdownval (run, v);

	v = (ase_awk_val_t*) ase_awk_makestrval (
		run, ASE_STR_BUF(&tmp), ASE_STR_LEN(&tmp));
	if (v == ASE_NULL) return -1;

	ase_awk_refdownval (run, run->inrec.d0);
	run->inrec.d0 = v;
	ase_awk_refupval (run, run->inrec.d0);

	ase_str_swap (&tmp, &run->inrec.line);
	ase_str_close (&tmp);

	for (i = nflds; i < run->inrec.nflds; i++)
	{
		ase_awk_refdownval (run, run->inrec.flds[i].val);
	}

	run->inrec.nflds = nflds;
	return 0;
}

static ase_char_t* idxnde_to_str (
	ase_awk_run_t* run, ase_awk_nde_t* nde, ase_char_t* buf, ase_size_t* len)
{
	ase_char_t* str;
	ase_awk_val_t* idx;

	ASE_ASSERT (nde != ASE_NULL);

	if (nde->next == ASE_NULL)
	{
		/* single node index */
		idx = eval_expression (run, nde);
		if (idx == ASE_NULL) return ASE_NULL;

		ase_awk_refupval (run, idx);

		str = ASE_NULL;

		if (buf != ASE_NULL)
		{
			/* try with a fixed-size buffer */
			str = ase_awk_valtostr (
				run, idx, ASE_AWK_VALTOSTR_FIXED, (ase_str_t*)buf, len);
		}

		if (str == ASE_NULL)
		{
			/* if it doen't work, switch to the dynamic mode */
			str = ase_awk_valtostr (
				run, idx, ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, len);

			if (str == ASE_NULL) 
			{
				ase_awk_refdownval (run, idx);
				/* change error line */
				run->errlin = nde->line;
				return ASE_NULL;
			}
		}

		ase_awk_refdownval (run, idx);
	}
	else
	{
		/* multidimensional index */
		ase_str_t idxstr;

		if (ase_str_open (
			&idxstr, DEF_BUF_CAPA, 
			&run->awk->prmfns.mmgr) == ASE_NULL) 
		{
			ase_awk_setrunerror (
				run, ASE_AWK_ENOMEM, nde->line, ASE_NULL, 0);
			return ASE_NULL;
		}

		while (nde != ASE_NULL)
		{
			idx = eval_expression (run, nde);
			if (idx == ASE_NULL) 
			{
				ase_str_close (&idxstr);
				return ASE_NULL;
			}

			ase_awk_refupval (run, idx);

			if (ASE_STR_LEN(&idxstr) > 0 &&
			    ase_str_ncat (&idxstr, 
			    	run->global.subsep.ptr, 
			    	run->global.subsep.len) == (ase_size_t)-1)
			{
				ase_awk_refdownval (run, idx);
				ase_str_close (&idxstr);

				ase_awk_setrunerror (
					run, ASE_AWK_ENOMEM, nde->line, 
					ASE_NULL, 0);

				return ASE_NULL;
			}

			if (ase_awk_valtostr (
				run, idx, 0, &idxstr, ASE_NULL) == ASE_NULL)
			{
				ase_awk_refdownval (run, idx);
				ase_str_close (&idxstr);
				return ASE_NULL;
			}

			ase_awk_refdownval (run, idx);
			nde = nde->next;
		}

		str = ASE_STR_BUF(&idxstr);
		*len = ASE_STR_LEN(&idxstr);
		ase_str_forfeit (&idxstr);
	}

	return str;
}

ase_char_t* ase_awk_format (
	ase_awk_run_t* run, ase_str_t* out, ase_str_t* fbu,
	const ase_char_t* fmt, ase_size_t fmt_len, 
	ase_size_t nargs_on_stack, ase_awk_nde_t* args, ase_size_t* len)
{
	ase_size_t i, j;
	ase_size_t stack_arg_idx = 1;
	ase_awk_val_t* val;

#define OUT_CHAR(c) \
	do { \
		if (ase_str_ccat (out, (c)) == -1) \
		{ \
			ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM); \
			return ASE_NULL; \
		} \
	} while (0)

#define FMT_CHAR(c) \
	do { \
		if (ase_str_ccat (fbu, (c)) == -1) \
		{ \
			ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM); \
			return ASE_NULL; \
		} \
	} while (0)

#define GROW(buf) \
	do { \
		if ((buf)->ptr != ASE_NULL) \
		{ \
			ASE_AWK_FREE (run->awk, (buf)->ptr); \
			(buf)->ptr = ASE_NULL; \
		} \
		(buf)->len += (buf)->inc; \
		(buf)->ptr = (ase_char_t*)ASE_AWK_MALLOC ( \
			run->awk, (buf)->len * ASE_SIZEOF(ase_char_t)); \
		if ((buf)->ptr == ASE_NULL) (buf)->len = 0; \
	} while (0) 

	ASE_ASSERTX (run->format.tmp.ptr != ASE_NULL,
		"run->format.tmp.ptr should have been assigned a pointer to a block of memory before this function has been called");

	if (nargs_on_stack == (ase_size_t)-1) 
	{
		val = (ase_awk_val_t*)args;
		nargs_on_stack = 2;
	}
	else 
	{
		val = ASE_NULL;
	}

	if (out == ASE_NULL) out = &run->format.out;
	if (fbu == ASE_NULL) fbu = &run->format.fmt;

	ase_str_clear (out);
	ase_str_clear (fbu);

	for (i = 0; i < fmt_len; i++)
	{
		ase_long_t width = -1, prec = -1;
		ase_bool_t minus = ase_false;

		if (ASE_STR_LEN(fbu) == 0)
		{
			if (fmt[i] == ASE_T('%')) FMT_CHAR (fmt[i]);
			else OUT_CHAR (fmt[i]);
			continue;
		}

		while (i < fmt_len &&
		       (fmt[i] == ASE_T(' ') || fmt[i] == ASE_T('#') ||
		        fmt[i] == ASE_T('0') || fmt[i] == ASE_T('+') ||
		        fmt[i] == ASE_T('-')))
		{
			if (fmt[i] == ASE_T('-')) minus = ase_true;
			FMT_CHAR (fmt[i]); i++;
		}

		if (i < fmt_len && fmt[i] == ASE_T('*'))
		{
			ase_awk_val_t* v;
			ase_real_t r;
			ase_char_t* p;
			int n;

			if (args == ASE_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
					return ASE_NULL;
				}
				v = ase_awk_getarg (run, stack_arg_idx);
			}
			else
			{
				if (val != ASE_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
						return ASE_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (run, args);
					if (v == ASE_NULL) return ASE_NULL;
				}
			}

			ase_awk_refupval (run, v);
			n = ase_awk_valtonum (run, v, &width, &r);
			ase_awk_refdownval (run, v);

			if (n == -1) return ASE_NULL; 
			if (n == 1) width = (ase_long_t)r;

			do
			{
				n = run->awk->prmfns.misc.sprintf (
					run->awk->prmfns.misc.custom_data,
					run->format.tmp.ptr, 
					run->format.tmp.len,
				#if ASE_SIZEOF_LONG_LONG > 0
					ASE_T("%lld"), (long long)width
				#elif ASE_SIZEOF___INT64 > 0
					ASE_T("%I64d"), (__int64)width
				#elif ASE_SIZEOF_LONG > 0
					ASE_T("%ld"), (long)width
				#elif ASE_SIZEOF_INT > 0
					ASE_T("%d"), (int)width
				#else
					#error unsupported size	
				#endif
					);
				if (n == -1)
				{
					GROW (&run->format.tmp);
					if (run->format.tmp.ptr == ASE_NULL)
					{
						ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
						return ASE_NULL;
					}

					continue;
				}

				break;
			}
			while (1);

			p = run->format.tmp.ptr;
			while (*p != ASE_T('\0'))
			{
				FMT_CHAR (*p);
				p++;
			}

			if (args == ASE_NULL || val != ASE_NULL) stack_arg_idx++;
			else args = args->next;
			i++;
		}
		else
		{
			if (i < fmt_len && ASE_AWK_ISDIGIT(run->awk, fmt[i]))
			{
				width = 0;
				do
				{
					width = width * 10 + fmt[i] - ASE_T('0');
					FMT_CHAR (fmt[i]); i++;
				}
				while (i < fmt_len && ASE_AWK_ISDIGIT(run->awk, fmt[i]));
			}
		}

		if (i < fmt_len && fmt[i] == ASE_T('.'))
		{
			prec = 0;
			FMT_CHAR (fmt[i]); i++;
		}

		if (i < fmt_len && fmt[i] == ASE_T('*'))
		{
			ase_awk_val_t* v;
			ase_real_t r;
			ase_char_t* p;
			int n;

			if (args == ASE_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
					return ASE_NULL;
				}
				v = ase_awk_getarg (run, stack_arg_idx);
			}
			else
			{
				if (val != ASE_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
						return ASE_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (run, args);
					if (v == ASE_NULL) return ASE_NULL;
				}
			}

			ase_awk_refupval (run, v);
			n = ase_awk_valtonum (run, v, &prec, &r);
			ase_awk_refdownval (run, v);

			if (n == -1) return ASE_NULL; 
			if (n == 1) prec = (ase_long_t)r;

			do
			{
				n = run->awk->prmfns.misc.sprintf (
					run->awk->prmfns.misc.custom_data,
					run->format.tmp.ptr, 
					run->format.tmp.len,
				#if ASE_SIZEOF_LONG_LONG > 0
					ASE_T("%lld"), (long long)prec
				#elif ASE_SIZEOF___INT64 > 0
					ASE_T("%I64d"), (__int64)prec
				#elif ASE_SIZEOF_LONG > 0
					ASE_T("%ld"), (long)prec
				#elif ASE_SIZEOF_INT > 0
					ASE_T("%d"), (int)prec
				#endif
					);
				if (n == -1)
				{
					GROW (&run->format.tmp);
					if (run->format.tmp.ptr == ASE_NULL)
					{
						ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
						return ASE_NULL;
					}

					continue;
				}

				break;
			}
			while (1);

			p = run->format.tmp.ptr;
			while (*p != ASE_T('\0'))
			{
				FMT_CHAR (*p);
				p++;
			}

			if (args == ASE_NULL || val != ASE_NULL) stack_arg_idx++;
			else args = args->next;
			i++;
		}
		else
		{
			if (i < fmt_len && ASE_AWK_ISDIGIT(run->awk, fmt[i]))
			{
				prec = 0;
				do
				{
					prec = prec * 10 + fmt[i] - ASE_T('0');
					FMT_CHAR (fmt[i]); i++;
				}
				while (i < fmt_len && ASE_AWK_ISDIGIT(run->awk, fmt[i]));
			}
		}

		if (i >= fmt_len) break;

		if (fmt[i] == ASE_T('d') || fmt[i] == ASE_T('i') || 
		    fmt[i] == ASE_T('x') || fmt[i] == ASE_T('X') ||
		    fmt[i] == ASE_T('o'))
		{
			ase_awk_val_t* v;
			ase_long_t l;
			ase_real_t r;
			ase_char_t* p;
			int n;

		#if ASE_SIZEOF_LONG_LONG > 0
			FMT_CHAR (ASE_T('l'));
			FMT_CHAR (ASE_T('l'));
			FMT_CHAR (fmt[i]);
		#elif ASE_SIZEOF___INT64 > 0
			FMT_CHAR (ASE_T('I'));
			FMT_CHAR (ASE_T('6'));
			FMT_CHAR (ASE_T('4'));
			FMT_CHAR (fmt[i]);
		#elif ASE_SIZEOF_LONG > 0
			FMT_CHAR (ASE_T('l'));
			FMT_CHAR (fmt[i]);
		#elif ASE_SIZEOF_INT > 0
			FMT_CHAR (fmt[i]);
		#else
			#error unsupported integer size
		#endif	

			if (args == ASE_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
					return ASE_NULL;
				}
				v = ase_awk_getarg (run, stack_arg_idx);
			}
			else
			{
				if (val != ASE_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
						return ASE_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (run, args);
					if (v == ASE_NULL) return ASE_NULL;
				}
			}

			ase_awk_refupval (run, v);
			n = ase_awk_valtonum (run, v, &l, &r);
			ase_awk_refdownval (run, v);

			if (n == -1) return ASE_NULL; 
			if (n == 1) l = (ase_long_t)r;

			do
			{
				n = run->awk->prmfns.misc.sprintf (
					run->awk->prmfns.misc.custom_data,
					run->format.tmp.ptr, 
					run->format.tmp.len,
					ASE_STR_BUF(fbu),
				#if ASE_SIZEOF_LONG_LONG > 0
					(long long)l
				#elif ASE_SIZEOF___INT64 > 0
					(__int64)l
				#elif ASE_SIZEOF_LONG > 0
					(long)l
				#elif ASE_SIZEOF_INT > 0
					(int)l
				#endif
					);
					
				if (n == -1)
				{
					GROW (&run->format.tmp);
					if (run->format.tmp.ptr == ASE_NULL)
					{
						ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
						return ASE_NULL;
					}

					continue;
				}

				break;
			}
			while (1);

			p = run->format.tmp.ptr;
			while (*p != ASE_T('\0'))
			{
				OUT_CHAR (*p);
				p++;
			}
		}
		else if (fmt[i] == ASE_T('e') || fmt[i] == ASE_T('E') ||
		         fmt[i] == ASE_T('g') || fmt[i] == ASE_T('G') ||
		         fmt[i] == ASE_T('f'))
		{
			ase_awk_val_t* v;
			ase_long_t l;
			ase_real_t r;
			ase_char_t* p;
			int n;

			FMT_CHAR (ASE_T('L'));
			FMT_CHAR (fmt[i]);

			if (args == ASE_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
					return ASE_NULL;
				}
				v = ase_awk_getarg (run, stack_arg_idx);
			}
			else
			{
				if (val != ASE_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
						return ASE_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (run, args);
					if (v == ASE_NULL) return ASE_NULL;
				}
			}

			ase_awk_refupval (run, v);
			n = ase_awk_valtonum (run, v, &l, &r);
			ase_awk_refdownval (run, v);

			if (n == -1) return ASE_NULL;
			if (n == 0) r = (ase_real_t)l;

			do
			{
				n = run->awk->prmfns.misc.sprintf (
					run->awk->prmfns.misc.custom_data,
					run->format.tmp.ptr, 
					run->format.tmp.len,
					ASE_STR_BUF(fbu),
					(long double)r);
					
				if (n == -1)
				{
					GROW (&run->format.tmp);
					if (run->format.tmp.ptr == ASE_NULL)
					{
						ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
						return ASE_NULL;
					}

					continue;
				}

				break;
			}
			while (1);

			p = run->format.tmp.ptr;
			while (*p != ASE_T('\0'))
			{
				OUT_CHAR (*p);
				p++;
			}
		}
		else if (fmt[i] == ASE_T('c')) 
		{
			ase_char_t ch;
			ase_size_t ch_len;
			ase_awk_val_t* v;

			if (args == ASE_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
					return ASE_NULL;
				}
				v = ase_awk_getarg (run, stack_arg_idx);
			}
			else
			{
				if (val != ASE_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
						return ASE_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (run, args);
					if (v == ASE_NULL) return ASE_NULL;
				}
			}

			ase_awk_refupval (run, v);
			if (v->type == ASE_AWK_VAL_NIL)
			{
				ch = ASE_T('\0');
				ch_len = 0;
			}
			else if (v->type == ASE_AWK_VAL_INT)
			{
				ch = (ase_char_t)((ase_awk_val_int_t*)v)->val;
				ch_len = 1;
			}
			else if (v->type == ASE_AWK_VAL_REAL)
			{
				ch = (ase_char_t)((ase_awk_val_real_t*)v)->val;
				ch_len = 1;
			}
			else if (v->type == ASE_AWK_VAL_STR)
			{
				ch_len = ((ase_awk_val_str_t*)v)->len;
				if (ch_len > 0) 
				{
					ch = ((ase_awk_val_str_t*)v)->buf[0];
					ch_len = 1;
				}
				else ch = ASE_T('\0');
			}
			else
			{
				ase_awk_refdownval (run, v);
				ase_awk_setrunerrnum (run, ASE_AWK_EVALTYPE);
				return ASE_NULL;
			}

			if (prec == -1 || prec == 0 || prec > (ase_long_t)ch_len) prec = (ase_long_t)ch_len;
			if (prec > width) width = prec;

			if (!minus)
			{
				while (width > prec) 
				{
					if (ase_str_ccat (out, ASE_T(' ')) == -1) 
					{ 
						ase_awk_refdownval (run, v);
						ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
						return ASE_NULL; 
					} 
					width--;
				}
			}

			if (prec > 0)
			{
				if (ase_str_ccat (out, ch) == -1) 
				{ 
					ase_awk_refdownval (run, v);
					ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
					return ASE_NULL; 
				} 
			}

			if (minus)
			{
				while (width > prec) 
				{
					if (ase_str_ccat (out, ASE_T(' ')) == -1) 
					{ 
						ase_awk_refdownval (run, v);
						ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
						return ASE_NULL; 
					} 
					width--;
				}
			}

			ase_awk_refdownval (run, v);
		}
		else if (fmt[i] == ASE_T('s')) 
		{
			ase_char_t* str, * str_free = ASE_NULL;
			ase_size_t str_len;
			ase_long_t k;
			ase_awk_val_t* v;

			if (args == ASE_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
					return ASE_NULL;
				}
				v = ase_awk_getarg (run, stack_arg_idx);
			}
			else
			{
				if (val != ASE_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						ase_awk_setrunerrnum (run, ASE_AWK_EFMTARG);
						return ASE_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (run, args);
					if (v == ASE_NULL) return ASE_NULL;
				}
			}

			ase_awk_refupval (run, v);
			if (v->type == ASE_AWK_VAL_NIL)
			{
				str = ASE_T("");
				str_len = 0;
			}
			else if (v->type == ASE_AWK_VAL_STR)
			{
				str = ((ase_awk_val_str_t*)v)->buf;
				str_len = ((ase_awk_val_str_t*)v)->len;
			}
			else
			{
				if (v == val)
				{
					ase_awk_refdownval (run, v);
					ase_awk_setrunerrnum (run, ASE_AWK_EFMTCNV);
					return ASE_NULL;
				}
	
				str = ase_awk_valtostr (run, v, 
					ASE_AWK_VALTOSTR_CLEAR, ASE_NULL, &str_len);
				if (str == ASE_NULL)
				{
					ase_awk_refdownval (run, v);
					return ASE_NULL;
				}

				str_free = str;
			}

			if (prec == -1 || prec > (ase_long_t)str_len ) prec = (ase_long_t)str_len;
			if (prec > width) width = prec;

			if (!minus)
			{
				while (width > prec) 
				{
					if (ase_str_ccat (out, ASE_T(' ')) == -1) 
					{ 
						if (str_free != ASE_NULL) 
							ASE_AWK_FREE (run->awk, str_free);
						ase_awk_refdownval (run, v);
						ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
						return ASE_NULL; 
					} 
					width--;
				}
			}

			for (k = 0; k < prec; k++) 
			{
				if (ase_str_ccat (out, str[k]) == -1) 
				{ 
					if (str_free != ASE_NULL) 
						ASE_AWK_FREE (run->awk, str_free);
					ase_awk_refdownval (run, v);
					ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
					return ASE_NULL; 
				} 
			}

			if (str_free != ASE_NULL) ASE_AWK_FREE (run->awk, str_free);

			if (minus)
			{
				while (width > prec) 
				{
					if (ase_str_ccat (out, ASE_T(' ')) == -1) 
					{ 
						ase_awk_refdownval (run, v);
						ase_awk_setrunerrnum (run, ASE_AWK_ENOMEM);
						return ASE_NULL; 
					} 
					width--;
				}
			}

			ase_awk_refdownval (run, v);
		}
		else /*if (fmt[i] == ASE_T('%'))*/
		{
			for (j = 0; j < ASE_STR_LEN(fbu); j++)
				OUT_CHAR (ASE_STR_CHAR(fbu,j));
			OUT_CHAR (fmt[i]);
		}

		if (args == ASE_NULL || val != ASE_NULL) stack_arg_idx++;
		else args = args->next;
		ase_str_clear (fbu);
	}

	/* flush uncompleted formatting sequence */
	for (j = 0; j < ASE_STR_LEN(fbu); j++)
		OUT_CHAR (ASE_STR_CHAR(fbu,j));

	*len = ASE_STR_LEN(out);
	return ASE_STR_BUF(out);
}

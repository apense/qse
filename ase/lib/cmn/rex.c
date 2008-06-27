/*
 * $Id: rex.c 223 2008-06-26 06:44:41Z baconevi $
 *
 * {License}
 */

#include <ase/cmn/rex.h>
#include <ase/cmn/mem.h>

#ifdef DEBUG_REX
#include <ase/utl/stdio.h>
#endif

enum
{
	CT_EOF,
	CT_SPECIAL,
	CT_NORMAL
};

enum
{
	LEVEL_TOP,
	LEVEL_CHARSET,
	LEVEL_RANGE
};

enum
{
	CMD_BOL,
	CMD_EOL,
	CMD_ANY_CHAR,
	CMD_ORD_CHAR,
	CMD_CHARSET,
	CMD_GROUP
};

enum
{
	CHARSET_ONE,
	CHARSET_RANGE,
	CHARSET_CLASS
};

enum
{
	CHARSET_CLASS_PUNCT,
	CHARSET_CLASS_SPACE,
	CHARSET_CLASS_DIGIT,
	CHARSET_CLASS_ALNUM
};

#define DEF_CODE_CAPA 512
#define BOUND_MIN 0
#define BOUND_MAX (ASE_TYPE_MAX(ase_size_t))

typedef struct builder_t builder_t;
typedef struct matcher_t matcher_t;
typedef struct match_t match_t;

typedef struct code_t code_t;
typedef struct rhdr_t rhdr_t;
typedef struct bhdr_t bhdr_t;
typedef struct cshdr_t cshdr_t;

struct builder_t
{
	ase_mmgr_t* mmgr;

	struct
	{
		const ase_char_t* ptr;
		const ase_char_t* end;
		const ase_char_t* curp;
		struct
		{
			int type;
			ase_char_t value;
			ase_bool_t escaped;
		} curc;
	} ptn;

	struct
	{
		ase_byte_t* buf;
		ase_size_t  size;
		ase_size_t  capa;
	} code;	

	struct
	{
		ase_size_t max;
		ase_size_t cur;
	} depth;

	int errnum;
};

struct matcher_t
{
	ase_mmgr_t* mmgr;
	ase_ccls_t* ccls;

	struct
	{
		struct
		{
			const ase_char_t* ptr;
			const ase_char_t* end;
		} str;
	} match;

	struct
	{
		ase_size_t max;
		ase_size_t cur;
	} depth;

	int ignorecase;
	int errnum;
};

struct match_t
{
	const ase_char_t* match_ptr;

	ase_bool_t matched;
	ase_size_t match_len;

	const ase_byte_t* branch;
	const ase_byte_t* branch_end;
};

#include <ase/pack1.h>

ASE_BEGIN_PACKED_STRUCT (code_t)
	/*ase_byte_t cmd;*/
	short cmd;
	short negate; /* only for CMD_CHARSET */
	ase_size_t lbound;
	ase_size_t ubound;
ASE_END_PACKED_STRUCT ()

/* compiled regular expression header */
ASE_BEGIN_PACKED_STRUCT (rhdr_t)
	ase_size_t nb;  /* number of branches */
	ase_size_t el;  /* expression length in bytes */
ASE_END_PACKED_STRUCT ()

/* branch header */
ASE_BEGIN_PACKED_STRUCT (bhdr_t)
	ase_size_t na;  /* number of atoms */
	ase_size_t bl;  /* branch length in bytes */
ASE_END_PACKED_STRUCT ()

/* character set header */
ASE_BEGIN_PACKED_STRUCT (cshdr_t)
	ase_size_t csc; /* count */
	ase_size_t csl; /* length */
ASE_END_PACKED_STRUCT ()

#include <ase/unpack.h>

typedef const ase_byte_t* (*atom_matcher_t) (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);

#define NCHARS_REMAINING(rex) ((rex)->ptn.end - (rex)->ptn.curp)
	
#define NEXT_CHAR(rex,level) \
	do { if (next_char(rex,level) == -1) return -1; } while (0)

#define ADD_CODE(rex,data,len) \
	do { if (add_code(rex,data,len) == -1) return -1; } while (0)

static int build_pattern (builder_t* rex);
static int build_pattern0 (builder_t* rex);
static int build_branch (builder_t* rex);
static int build_atom (builder_t* rex);
static int build_charset (builder_t* rex, code_t* cmd);
static int build_occurrences (builder_t* rex, code_t* cmd);
static int build_cclass (builder_t* rex, ase_char_t* cc);
static int build_range (builder_t* rex, code_t* cmd);
static int next_char (builder_t* rex, int level);
static int add_code (builder_t* rex, void* data, ase_size_t len);

static ase_bool_t __begin_with (
	const ase_char_t* str, ase_size_t len, const ase_char_t* what);

static const ase_byte_t* match_pattern (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);
static const ase_byte_t* match_branch (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);
static const ase_byte_t* match_branch_body (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);
static const ase_byte_t* match_branch_body0 (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);
static const ase_byte_t* match_atom (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);
static const ase_byte_t* match_bol (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);
static const ase_byte_t* match_eol (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);
static const ase_byte_t* match_any_char (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);
static const ase_byte_t* match_ord_char (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);
static const ase_byte_t* match_charset (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);
static const ase_byte_t* match_group (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat);

static const ase_byte_t* match_occurrences (
	matcher_t* matcher, ase_size_t si, const ase_byte_t* p,
	ase_size_t lbound, ase_size_t ubound, match_t* mat);

static ase_bool_t __test_charset (
	matcher_t* matcher, const ase_byte_t* p, ase_size_t csc, ase_char_t c);

static ase_bool_t cc_isalnum (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_isalpha (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_isblank (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_iscntrl (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_isdigit (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_isgraph (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_islower (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_isprint (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_ispunct (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_isspace (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_isupper (ase_ccls_t* ccls, ase_char_t c);
static ase_bool_t cc_isxdigit (ase_ccls_t* ccls, ase_char_t c);

#if 0
XXX
static const ase_byte_t* __print_pattern (ase_awk_t* awk, const ase_byte_t* p);
static const ase_byte_t* __print_branch (ase_awk_t* awk, const ase_byte_t* p);
static const ase_byte_t* __print_atom (ase_awk_t* awk, const ase_byte_t* p);
#endif

struct __char_class_t
{
	const ase_char_t* name;
	ase_size_t name_len;
	ase_bool_t (*func) (ase_ccls_t* ccls, ase_char_t c);
}; 

static struct __char_class_t __char_class[] =
{
	{ ASE_T("alnum"),  5, cc_isalnum },
	{ ASE_T("alpha"),  5, cc_isalpha },
	{ ASE_T("blank"),  5, cc_isblank },
	{ ASE_T("cntrl"),  5, cc_iscntrl },
	{ ASE_T("digit"),  5, cc_isdigit },
	{ ASE_T("graph"),  5, cc_isgraph },
	{ ASE_T("lower"),  5, cc_islower },
	{ ASE_T("print"),  5, cc_isprint },
	{ ASE_T("punct"),  5, cc_ispunct },
	{ ASE_T("space"),  5, cc_isspace },
	{ ASE_T("upper"),  5, cc_isupper },
	{ ASE_T("xdigit"), 6, cc_isxdigit },

	/*
	{ ASE_T("arabic"),   6, cc_isarabic },
	{ ASE_T("chinese"),  7, cc_ischinese },
	{ ASE_T("english"),  7, cc_isenglish },
	{ ASE_T("japanese"), 8, cc_isjapanese },
	{ ASE_T("korean"),   6, cc_iskorean }, 
	{ ASE_T("thai"),     4, cc_isthai }, 
	*/

	{ ASE_NULL,        0, ASE_NULL }
};

void* ase_buildrex (
	ase_mmgr_t* mmgr, ase_size_t depth, 
	const ase_char_t* ptn, ase_size_t len, int* errnum)
{
	builder_t builder;

	builder.mmgr = mmgr;
	builder.code.capa = DEF_CODE_CAPA;
	builder.code.size = 0;
	builder.code.buf = (ase_byte_t*) 
		ASE_MALLOC (builder.mmgr, builder.code.capa);
	if (builder.code.buf == ASE_NULL) 
	{
		*errnum = ASE_REX_ENOMEM;
		return ASE_NULL;
	}

	builder.ptn.ptr = ptn;
	builder.ptn.end = builder.ptn.ptr + len;
	builder.ptn.curp = builder.ptn.ptr;

	builder.ptn.curc.type = CT_EOF;
	builder.ptn.curc.value = ASE_T('\0');
	builder.ptn.curc.escaped = ASE_FALSE;

	builder.depth.max = depth;
	builder.depth.cur = 0;

	if (next_char (&builder, LEVEL_TOP) == -1) 
	{
		if (errnum != ASE_NULL) *errnum = builder.errnum;
		ASE_FREE (builder.mmgr, builder.code.buf);
		return ASE_NULL;
	}

	if (build_pattern (&builder) == -1) 
	{
		if (errnum != ASE_NULL) *errnum = builder.errnum;
		ASE_FREE (builder.mmgr, builder.code.buf);
		return ASE_NULL;
	}

	if (builder.ptn.curc.type != CT_EOF)
	{
		if (errnum != ASE_NULL) 
		{
			if (builder.ptn.curc.type ==  CT_SPECIAL &&
			    builder.ptn.curc.value == ASE_T(')'))
			{
				*errnum = ASE_REX_EUNBALPAR;
			}
			else
			{
				*errnum = ASE_REX_EGARBAGE;
			}
		}

		ASE_FREE (builder.mmgr, builder.code.buf);
		return ASE_NULL;
	}

	return builder.code.buf;
}

int ase_matchrex (
	ase_mmgr_t* mmgr, ase_ccls_t* ccls, ase_size_t depth,
	void* code, int option,
	const ase_char_t* str, ase_size_t len, 
	const ase_char_t** match_ptr, ase_size_t* match_len, int* errnum)
{
	matcher_t matcher;
	match_t mat;
	ase_size_t offset = 0;
	/*const ase_char_t* match_ptr_zero = ASE_NULL;*/

	matcher.mmgr = mmgr;
	matcher.ccls = ccls;

	/* store the source string */
	matcher.match.str.ptr = str;
	matcher.match.str.end = str + len;

	matcher.depth.max = depth;
	matcher.depth.cur = 0;
	matcher.ignorecase = (option & ASE_REX_IGNORECASE)? 1: 0;

	mat.matched = ASE_FALSE;
	/* TODO: should it allow an offset here??? */
	mat.match_ptr = str + offset;

	/*while (mat.match_ptr < matcher.match.str.end)*/
	while (mat.match_ptr <= matcher.match.str.end)
	{
		if (match_pattern (&matcher, code, &mat) == ASE_NULL) 
		{
			if (errnum != ASE_NULL) *errnum = matcher.errnum;
			return -1;
		}

		if (mat.matched)
		{
			/*
			if (mat.match_len == 0)
			{
				if (match_ptr_zero == ASE_NULL)
					match_ptr_zero = mat.match_ptr;
				mat.match_ptr++;
				continue;
			}
			*/

			if (match_ptr != ASE_NULL) *match_ptr = mat.match_ptr;
			if (match_len != ASE_NULL) *match_len = mat.match_len;

			/*match_ptr_zero = ASE_NULL;*/
			break;
		}

		mat.match_ptr++;
	}

	/*
	if (match_ptr_zero != ASE_NULL) 
	{
		if (match_ptr != ASE_NULL) *match_ptr = match_ptr_zero;
		if (match_len != ASE_NULL) *match_len = 0;
		return 1;
	}
	*/

	return (mat.matched)? 1: 0;
}

void ase_freerex (ase_mmgr_t* mmgr, void* code)
{
	ASE_ASSERT (code != ASE_NULL);
	ASE_FREE (mmgr, code);
}

ase_bool_t ase_isemptyrex (void* code)
{
	rhdr_t* rhdr = (rhdr_t*) code;
	ASE_ASSERT (rhdr != ASE_NULL);

	/* an empty regular expression look like:
	 *  | expression                     | 
	 *  | header         | branch        |
	 *  |                | branch header |
	 *  | NB(1) | EL(16) | NA(1) | BL(8) | */
	return (rhdr->nb == 1 && 
	        rhdr->el == ASE_SIZEOF(ase_size_t)*4)? ASE_TRUE: ASE_FALSE;
}

static int build_pattern (builder_t* builder)
{
	int n;

	if (builder->depth.max > 0 && builder->depth.cur >= builder->depth.max)
	{
		builder->errnum = ASE_REX_ERECUR;
		return -1;
	}

	builder->depth.cur++;
	n = build_pattern0 (builder);
	builder->depth.cur--;

	return n;
}

static int build_pattern0 (builder_t* builder)
{
	ase_size_t zero = 0;
	ase_size_t old_size;
	ase_size_t pos_nb;
	rhdr_t* rhdr;
	int n;

	old_size = builder->code.size;

	/* secure space for header and set the header fields to zero */
	pos_nb = builder->code.size;
	ADD_CODE (builder, &zero, ASE_SIZEOF(zero));
	ADD_CODE (builder, &zero, ASE_SIZEOF(zero));

	/* handle the first branch */
	n = build_branch (builder);
	if (n == -1) return -1;
	if (n == 0) 
	{
		/* if the pattern is empty, the control reaches here */
		return 0;
	}

	rhdr = (rhdr_t*)&builder->code.buf[pos_nb];
	rhdr->nb++;

	/* handle subsequent branches if any */
	while (builder->ptn.curc.type == CT_SPECIAL && 
	       builder->ptn.curc.value == ASE_T('|'))
	{
		NEXT_CHAR (builder, LEVEL_TOP);

		n = build_branch(builder);
		if (n == -1) return -1;
		if (n == 0) 
		{
			/* if the pattern ends with a vertical bar(|),
			 * this block can be reached. however, such a 
			 * pattern is highly discouraged */
			break;
		}

		rhdr = (rhdr_t*)&builder->code.buf[pos_nb];
		rhdr->nb++;
	}

	rhdr = (rhdr_t*)&builder->code.buf[pos_nb];
	rhdr->el = builder->code.size - old_size;

	return 1;
}

static int build_branch (builder_t* builder)
{
	int n;
	ase_size_t zero = 0;
	ase_size_t old_size;
	ase_size_t pos_na;
	code_t* cmd;
	bhdr_t* bhdr;

	old_size = builder->code.size;

	pos_na = builder->code.size;
	ADD_CODE (builder, &zero, ASE_SIZEOF(zero));
	ADD_CODE (builder, &zero, ASE_SIZEOF(zero));

	while (1)
	{
		cmd = (code_t*)&builder->code.buf[builder->code.size];

		n = build_atom (builder);
		if (n == -1) 
		{
			builder->code.size = old_size;
			return -1;
		}

		if (n == 0) break; /* no atom */

		n = build_occurrences (builder, cmd);
		if (n == -1)
		{
			builder->code.size = old_size;
			return -1;
		}

		/* n == 0  no bound character. just continue */
		/* n == 1  bound has been applied by build_occurrences */

		bhdr = (bhdr_t*)&builder->code.buf[pos_na];
		bhdr->na++;
	}

	bhdr = (bhdr_t*)&builder->code.buf[pos_na];
	bhdr->bl = builder->code.size - old_size;

	return (builder->code.size == old_size)? 0: 1;
}

static int build_atom (builder_t* builder)
{
	int n;
	code_t tmp;

	if (builder->ptn.curc.type == CT_EOF) return 0;

	if (builder->ptn.curc.type == CT_SPECIAL)
	{
		if (builder->ptn.curc.value == ASE_T('('))
		{
			tmp.cmd = CMD_GROUP;
			tmp.negate = 0;
			tmp.lbound = 1;
			tmp.ubound = 1;
			ADD_CODE (builder, &tmp, ASE_SIZEOF(tmp));

			NEXT_CHAR (builder, LEVEL_TOP);

			n = build_pattern (builder);
			if (n == -1) return -1;

			if (builder->ptn.curc.type != CT_SPECIAL || 
			    builder->ptn.curc.value != ASE_T(')')) 
			{
				builder->errnum = ASE_REX_ERPAREN;
				return -1;
			}
		}
		else if (builder->ptn.curc.value == ASE_T('^'))
		{
			tmp.cmd = CMD_BOL;
			tmp.negate = 0;
			tmp.lbound = 1;
			tmp.ubound = 1;
			ADD_CODE (builder, &tmp, ASE_SIZEOF(tmp));
		}
		else if (builder->ptn.curc.value == ASE_T('$'))
		{
			tmp.cmd = CMD_EOL;
			tmp.negate = 0;
			tmp.lbound = 1;
			tmp.ubound = 1;
			ADD_CODE (builder, &tmp, ASE_SIZEOF(tmp));
		}
		else if (builder->ptn.curc.value == ASE_T('.'))
		{
			tmp.cmd = CMD_ANY_CHAR;
			tmp.negate = 0;
			tmp.lbound = 1;
			tmp.ubound = 1;
			ADD_CODE (builder, &tmp, ASE_SIZEOF(tmp));
		}
		else if (builder->ptn.curc.value == ASE_T('['))
		{
			code_t* cmd;

			cmd = (code_t*)&builder->code.buf[builder->code.size];

			tmp.cmd = CMD_CHARSET;
			tmp.negate = 0;
			tmp.lbound = 1;
			tmp.ubound = 1;
			ADD_CODE (builder, &tmp, ASE_SIZEOF(tmp));

			NEXT_CHAR (builder, LEVEL_CHARSET);

			n = build_charset (builder, cmd);
			if (n == -1) return -1;

			ASE_ASSERT (n != 0);

			if (builder->ptn.curc.type != CT_SPECIAL ||
			    builder->ptn.curc.value != ASE_T(']'))
			{
				builder->errnum = ASE_REX_ERBRACKET;
				return -1;
			}

		}
		else return 0;

		NEXT_CHAR (builder, LEVEL_TOP);
		return 1;
	}
	else 
	{
		ASE_ASSERT (builder->ptn.curc.type == CT_NORMAL);

		tmp.cmd = CMD_ORD_CHAR;
		tmp.negate = 0;
		tmp.lbound = 1;
		tmp.ubound = 1;
		ADD_CODE (builder, &tmp, ASE_SIZEOF(tmp));

		ADD_CODE (builder, 
			&builder->ptn.curc.value, 
			ASE_SIZEOF(builder->ptn.curc.value));
		NEXT_CHAR (builder, LEVEL_TOP);

		return 1;
	}
}

static int build_charset (builder_t* builder, code_t* cmd)
{
	ase_size_t zero = 0;
	ase_size_t old_size;
	ase_size_t pos_csc;
	cshdr_t* cshdr;

	old_size = builder->code.size;

	pos_csc = builder->code.size;
	ADD_CODE (builder, &zero, ASE_SIZEOF(zero));
	ADD_CODE (builder, &zero, ASE_SIZEOF(zero));

	if (builder->ptn.curc.type == CT_NORMAL &&
	    builder->ptn.curc.value == ASE_T('^')) 
	{
		cmd->negate = 1;
		NEXT_CHAR (builder, LEVEL_CHARSET);
	}

	while (builder->ptn.curc.type == CT_NORMAL)
	{
		ase_char_t c0, c1, c2;
		int cc = 0;

		c1 = builder->ptn.curc.value;
		NEXT_CHAR(builder, LEVEL_CHARSET);

		if (c1 == ASE_T('[') &&
		    builder->ptn.curc.type == CT_NORMAL &&
		    builder->ptn.curc.value == ASE_T(':'))
		{
			if (build_cclass (builder, &c1) == -1) return -1;
			cc = cc | 1;
		}

		c2 = c1;
		if (builder->ptn.curc.type == CT_NORMAL &&
		    builder->ptn.curc.value == ASE_T('-') && 
		    builder->ptn.curc.escaped == ASE_FALSE)
		{
			NEXT_CHAR (builder, LEVEL_CHARSET);

			if (builder->ptn.curc.type == CT_NORMAL)
			{
				c2 = builder->ptn.curc.value;
				NEXT_CHAR (builder, LEVEL_CHARSET);

				if (c2 == ASE_T('[') &&
				    builder->ptn.curc.type == CT_NORMAL &&
				    builder->ptn.curc.value == ASE_T(':'))
				{
					if (build_cclass (builder, &c2) == -1)
					{
						return -1;
					}

					cc = cc | 2;
				}
			}	
			else cc = cc | 4;
		}


		if (cc == 0 || cc == 4)
		{
			if (c1 == c2)
			{
				c0 = CHARSET_ONE;
				ADD_CODE (builder, &c0, ASE_SIZEOF(c0));
				ADD_CODE (builder, &c1, ASE_SIZEOF(c1));
			}
			else
			{
				c0 = CHARSET_RANGE;
				ADD_CODE (builder, &c0, ASE_SIZEOF(c0));
				ADD_CODE (builder, &c1, ASE_SIZEOF(c1));
				ADD_CODE (builder, &c2, ASE_SIZEOF(c2));
			}
		}
		else if (cc == 1)
		{
			c0 = CHARSET_CLASS;
			ADD_CODE (builder, &c0, ASE_SIZEOF(c0));
			ADD_CODE (builder, &c1, ASE_SIZEOF(c1));
		}
		else
		{
			/* invalid range */
		#ifdef DEBUG_REX
			ase_dprintf (
				ASE_T("build_charset: invalid character set range\n"));
		#endif
			builder->errnum = ASE_REX_ECRANGE;
			return -1;
		}

		cshdr = (cshdr_t*)&builder->code.buf[pos_csc];
		cshdr->csc++;
	}

	cshdr = (cshdr_t*)&builder->code.buf[pos_csc];
	cshdr->csl = builder->code.size - old_size;

	return 1;
}

static int build_cclass (builder_t* builder, ase_char_t* cc)
{
	const struct __char_class_t* ccp = __char_class;
	ase_size_t len = builder->ptn.end - builder->ptn.curp;

	while (ccp->name != ASE_NULL)
	{
		if (__begin_with (builder->ptn.curp, len, ccp->name)) break;
		ccp++;
	}

	if (ccp->name == ASE_NULL)
	{
		/* wrong class name */
	#ifdef DEBUG_REX
		ase_dprintf (ASE_T("build_cclass: wrong class name\n"));
	#endif
		builder->errnum = ASE_REX_ECCLASS;
		return -1;
	}

	builder->ptn.curp += ccp->name_len;

	NEXT_CHAR (builder, LEVEL_CHARSET);
	if (builder->ptn.curc.type != CT_NORMAL ||
	    builder->ptn.curc.value != ASE_T(':'))
	{
	#ifdef DEBUG_REX
		ase_dprintf (ASE_T("build_cclass: a colon(:) expected\n"));
	#endif
		builder->errnum = ASE_REX_ECOLON;
		return -1;
	}

	NEXT_CHAR (builder, LEVEL_CHARSET); 
	
	/* ] happens to be the charset ender ] */
	if (builder->ptn.curc.type != CT_SPECIAL ||
	    builder->ptn.curc.value != ASE_T(']'))
	{
	#ifdef DEBUG_REX
		ase_dprintf (ASE_T("build_cclass: ] expected\n"));
	#endif
		builder->errnum = ASE_REX_ERBRACKET;	
		return -1;
	}

	NEXT_CHAR (builder, LEVEL_CHARSET);

	*cc = (ase_char_t)(ccp - __char_class);
	return 1;
}

static int build_occurrences (builder_t* builder, code_t* cmd)
{
	if (builder->ptn.curc.type != CT_SPECIAL) return 0;

	switch (builder->ptn.curc.value)
	{
		case ASE_T('+'):
		{
			cmd->lbound = 1;
			cmd->ubound = BOUND_MAX;
			NEXT_CHAR(builder, LEVEL_TOP);
			return 1;
		}

		case ASE_T('*'):
		{
			cmd->lbound = 0;
			cmd->ubound = BOUND_MAX;
			NEXT_CHAR(builder, LEVEL_TOP);
			return 1;
		}

		case ASE_T('?'):
		{
			cmd->lbound = 0;
			cmd->ubound = 1;
			NEXT_CHAR(builder, LEVEL_TOP);
			return 1;
		}

		case ASE_T('{'):
		{
			NEXT_CHAR (builder, LEVEL_RANGE);

			if (build_range(builder, cmd) == -1) return -1;

			if (builder->ptn.curc.type != CT_SPECIAL || 
			    builder->ptn.curc.value != ASE_T('}')) 
			{
				builder->errnum = ASE_REX_ERBRACE;
				return -1;
			}

			NEXT_CHAR (builder, LEVEL_TOP);
			return 1;
		}
	}

	return 0;
}

static int build_range (builder_t* builder, code_t* cmd)
{
	ase_size_t bound;

/* TODO: should allow white spaces in the range???
what if it is not in the raight format? convert it to ordinary characters?? */
	bound = 0;
	while (builder->ptn.curc.type == CT_NORMAL &&
	       (builder->ptn.curc.value >= ASE_T('0') && 
	        builder->ptn.curc.value <= ASE_T('9')))
	{
		bound = bound * 10 + builder->ptn.curc.value - ASE_T('0');
		NEXT_CHAR (builder, LEVEL_RANGE);
	}

	cmd->lbound = bound;

	if (builder->ptn.curc.type == CT_SPECIAL &&
	    builder->ptn.curc.value == ASE_T(',')) 
	{
		NEXT_CHAR (builder, LEVEL_RANGE);

		bound = 0;
		while (builder->ptn.curc.type == CT_NORMAL &&
		       (builder->ptn.curc.value >= ASE_T('0') && 
		        builder->ptn.curc.value <= ASE_T('9')))
		{
			bound = bound * 10 + builder->ptn.curc.value - ASE_T('0');
			NEXT_CHAR (builder, LEVEL_RANGE);
		}

		cmd->ubound = bound;
	}
	else cmd->ubound = BOUND_MAX;

	if (cmd->lbound > cmd->ubound)
	{
		/* invalid occurrences range */
		builder->errnum = ASE_REX_EBRANGE;
		return -1;
	}

	return 0;
}

#define CHECK_END(builder) \
	do { \
		if (builder->ptn.curp >= builder->ptn.end) \
		{ \
			builder->errnum = ASE_REX_EEND; \
			return -1; \
		} \
	} while(0)

#define IS_HEX(c) \
	((c >= ASE_T('0') && c <= ASE_T('9')) || \
	 (c >= ASE_T('A') && c <= ASE_T('F')) || \
	 (c >= ASE_T('a') && c <= ASE_T('f')))

#define HEX_TO_NUM(c) \
	((c >= ASE_T('0') && c <= ASE_T('9'))? c-ASE_T('0'):  \
	 (c >= ASE_T('A') && c <= ASE_T('F'))? c-ASE_T('A')+10: \
	                                       c-ASE_T('a')+10)

static int next_char (builder_t* builder, int level)
{
	if (builder->ptn.curp >= builder->ptn.end)
	{
		builder->ptn.curc.type = CT_EOF;
		builder->ptn.curc.value = ASE_T('\0');
		builder->ptn.curc.escaped = ASE_FALSE;
		return 0;
	}

	builder->ptn.curc.type = CT_NORMAL;
	builder->ptn.curc.value = *builder->ptn.curp++;
	builder->ptn.curc.escaped = ASE_FALSE;

	if (builder->ptn.curc.value == ASE_T('\\'))
	{	       
		ase_char_t c;

		CHECK_END (builder);
		c = *builder->ptn.curp++;

		if (c == ASE_T('n')) c = ASE_T('\n');
		else if (c == ASE_T('r')) c = ASE_T('\r');
		else if (c == ASE_T('t')) c = ASE_T('\t');
		else if (c == ASE_T('f')) c = ASE_T('\f');
		else if (c == ASE_T('b')) c = ASE_T('\b');
		else if (c == ASE_T('v')) c = ASE_T('\v');
		else if (c == ASE_T('a')) c = ASE_T('\a');
		else if (c >= ASE_T('0') && c <= ASE_T('7')) 
		{
			ase_char_t cx;

			c = c - ASE_T('0');

			CHECK_END (builder);
			cx = *builder->ptn.curp++;
			if (cx >= ASE_T('0') && cx <= ASE_T('7'))
			{
				c = c * 8 + cx - ASE_T('0');

				CHECK_END (builder);
				cx = *builder->ptn.curp++;
				if (cx >= ASE_T('0') && cx <= ASE_T('7'))
				{
					c = c * 8 + cx - ASE_T('0');
				}
			}
		}
		else if (c == ASE_T('x')) 
		{
			ase_char_t cx;

			CHECK_END (builder);
			cx = *builder->ptn.curp++;
			if (IS_HEX(cx))
			{
				c = HEX_TO_NUM(cx);

				CHECK_END (builder);
				cx = *builder->ptn.curp++;
				if (IS_HEX(cx))
				{
					c = c * 16 + HEX_TO_NUM(cx);
				}
			}
		}
	#ifdef ASE_CHAR_IS_WCHAR
		else if (c == ASE_T('u') && ASE_SIZEOF(ase_char_t) >= 2) 
		{
			ase_char_t cx;

			CHECK_END (builder);
			cx = *builder->ptn.curp++;
			if (IS_HEX(cx))
			{
				ase_size_t i;

				c = HEX_TO_NUM(cx);

				for (i = 0; i < 3; i++)
				{
					CHECK_END (builder);
					cx = *builder->ptn.curp++;

					if (!IS_HEX(cx)) break;
					c = c * 16 + HEX_TO_NUM(cx);
				}
			}
		}
		else if (c == ASE_T('U') && ASE_SIZEOF(ase_char_t) >= 4) 
		{
			ase_char_t cx;

			CHECK_END (builder);
			cx = *builder->ptn.curp++;
			if (IS_HEX(cx))
			{
				ase_size_t i;

				c = HEX_TO_NUM(cx);

				for (i = 0; i < 7; i++)
				{
					CHECK_END (builder);
					cx = *builder->ptn.curp++;

					if (!IS_HEX(cx)) break;
					c = c * 16 + HEX_TO_NUM(cx);
				}
			}
		}
	#endif

		builder->ptn.curc.value = c;
		builder->ptn.curc.escaped = ASE_TRUE;

		return 0;
	}
	else
	{
		if (level == LEVEL_TOP)
		{
			if (builder->ptn.curc.value == ASE_T('[') ||
			    builder->ptn.curc.value == ASE_T('|') ||
			    builder->ptn.curc.value == ASE_T('^') ||
			    builder->ptn.curc.value == ASE_T('$') ||
			    builder->ptn.curc.value == ASE_T('{') ||
			    builder->ptn.curc.value == ASE_T('+') ||
			    builder->ptn.curc.value == ASE_T('?') ||
			    builder->ptn.curc.value == ASE_T('*') ||
			    builder->ptn.curc.value == ASE_T('.') ||
			    builder->ptn.curc.value == ASE_T('(') ||
			    builder->ptn.curc.value == ASE_T(')')) 
			{
				builder->ptn.curc.type = CT_SPECIAL;
			}
		}
		else if (level == LEVEL_CHARSET)
		{
			if (builder->ptn.curc.value == ASE_T(']')) 
			{
				builder->ptn.curc.type = CT_SPECIAL;
			}
		}
		else if (level == LEVEL_RANGE)
		{
			if (builder->ptn.curc.value == ASE_T(',') ||
			    builder->ptn.curc.value == ASE_T('}')) 
			{
				builder->ptn.curc.type = CT_SPECIAL;
			}
		}
	}

	return 0;
}

static int add_code (builder_t* builder, void* data, ase_size_t len)
{
	if (len > builder->code.capa - builder->code.size)
	{
		ase_size_t capa = builder->code.capa * 2;
		ase_byte_t* tmp;
		
		if (capa == 0) capa = DEF_CODE_CAPA;
		while (len > capa - builder->code.size) { capa = capa * 2; }

		if (builder->mmgr->realloc != ASE_NULL)
		{
			tmp = (ase_byte_t*) ASE_REALLOC (
				builder->mmgr, builder->code.buf, capa);
			if (tmp == ASE_NULL)
			{
				builder->errnum = ASE_REX_ENOMEM;
				return -1;
			}
		}
		else
		{
			tmp = (ase_byte_t*) ASE_MALLOC (builder->mmgr, capa);
			if (tmp == ASE_NULL)
			{
				builder->errnum = ASE_REX_ENOMEM;
				return -1;
			}

			if (builder->code.buf != ASE_NULL)
			{
				ase_memcpy (tmp, builder->code.buf, builder->code.capa);
				ASE_FREE (builder->mmgr, builder->code.buf);
			}
		}

		builder->code.buf = tmp;
		builder->code.capa = capa;
	}

	ase_memcpy (&builder->code.buf[builder->code.size], data, len);
	builder->code.size += len;

	return 0;
}

static ase_bool_t __begin_with (
	const ase_char_t* str, ase_size_t len, const ase_char_t* what)
{
	const ase_char_t* end = str + len;

	while (str < end)
	{
		if (*what == ASE_T('\0')) return ASE_TRUE;
		if (*what != *str) return ASE_FALSE;

		str++; what++;
	}

	if (*what == ASE_T('\0')) return ASE_TRUE;
	return ASE_FALSE;
}

static const ase_byte_t* match_pattern (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	match_t mat2;
	ase_size_t i;
	const ase_byte_t* p;
	rhdr_t* rhdr;

	p = base;
	rhdr = (rhdr_t*) p; p += ASE_SIZEOF(*rhdr);

#ifdef DEBUG_REX
	ase_dprintf (
		ASE_T("match_pattern: NB = %u, EL = %u\n"), 
		(unsigned int)rhdr->nb, (unsigned int)rhdr->el);
#endif

	mat->matched = ASE_FALSE;
	mat->match_len = 0;

	for (i = 0; i < rhdr->nb; i++)
	{
		mat2.match_ptr = mat->match_ptr;

		p = match_branch (matcher, p, &mat2);
		if (p == ASE_NULL) return ASE_NULL;

		if (mat2.matched)
		{
			mat->matched = ASE_TRUE;
			mat->match_len = mat2.match_len;
			break;
		}
	}

	return base + rhdr->el;
}

static const ase_byte_t* match_branch (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	/* branch body  base+sizeof(NA)+sizeof(BL)-----+
	 * BL base+sizeof(NA) ----------+              |
	 * base NA ------+              |              |
	 *               |              |              |
	 *               |NA(ase_size_t)|BL(ase_size_t)|ATOMS.........|
	 */
	mat->branch = base;
	mat->branch_end = base + ((bhdr_t*)base)->bl;

	return match_branch_body (
		matcher, (const ase_byte_t*)((bhdr_t*)base+1), mat);
}

static const ase_byte_t* match_branch_body (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	const ase_byte_t* n;

	if (matcher->depth.max > 0 && matcher->depth.cur >= matcher->depth.max)
	{
		matcher->errnum = ASE_REX_ERECUR;
		return ASE_NULL;
	}

	matcher->depth.cur++;
	n = match_branch_body0 (matcher, base, mat);
	matcher->depth.cur--;

	return n;
}

static const ase_byte_t* match_branch_body0 (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	const ase_byte_t* p;
/*	match_t mat2;*/
	ase_size_t match_len = 0;

	mat->matched = ASE_FALSE;
	mat->match_len = 0;

/* TODO: is mat2 necessary here ? */
/*
	mat2.match_ptr = mat->match_ptr;
	mat2.branch = mat->branch;
	mat2.branch_end = mat->branch_end;
*/

	p = base;

	while (p < mat->branch_end)
	{
		p = match_atom (matcher, p, mat);
		if (p == ASE_NULL) return ASE_NULL;

		if (!mat->matched) break;

		mat->match_ptr = &mat->match_ptr[mat->match_len];
		match_len += mat->match_len;
#if 0
		p = match_atom (matcher, p, &mat2);
		if (p == ASE_NULL) return ASE_NULL;

		if (!mat2.matched) 
		{
			mat->matched = ASE_FALSE;
			break; /* stop matching */
		}

		mat->matched = ASE_TRUE;
		mat->match_len += mat2.match_len;

		mat2.match_ptr = &mat2.match_ptr[mat2.match_len];
#endif
	}

	if (mat->matched) mat->match_len = match_len;
	return mat->branch_end;
}

static const ase_byte_t* match_atom (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	static atom_matcher_t matchers[] =
	{
		match_bol,
		match_eol,
		match_any_char,
		match_ord_char,
		match_charset,
		match_group
	};
       
	ASE_ASSERT (
		((code_t*)base)->cmd >= 0 && 
		((code_t*)base)->cmd < ASE_COUNTOF(matchers));

	return matchers[((code_t*)base)->cmd] (matcher, base, mat);
}

static const ase_byte_t* match_bol (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	const ase_byte_t* p = base;
	const code_t* cp;

	cp = (const code_t*)p; p += ASE_SIZEOF(*cp);
	ASE_ASSERT (cp->cmd == CMD_BOL);

	mat->matched = (mat->match_ptr == matcher->match.str.ptr ||
	               (cp->lbound == cp->ubound && cp->lbound == 0));
	mat->match_len = 0;

	return p;
}

static const ase_byte_t* match_eol (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	const ase_byte_t* p = base;
	const code_t* cp;

	cp = (const code_t*)p; p += ASE_SIZEOF(*cp);
	ASE_ASSERT (cp->cmd == CMD_EOL);

	mat->matched = (mat->match_ptr == matcher->match.str.end ||
	               (cp->lbound == cp->ubound && cp->lbound == 0));
	mat->match_len = 0;

	return p;
}

static const ase_byte_t* match_any_char (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	const ase_byte_t* p = base;
	const code_t* cp;
	ase_size_t si = 0, lbound, ubound;

	cp = (const code_t*)p; p += ASE_SIZEOF(*cp);
	ASE_ASSERT (cp->cmd == CMD_ANY_CHAR);

	lbound = cp->lbound;
	ubound = cp->ubound;

	mat->matched = ASE_FALSE;
	mat->match_len = 0;

	/* merge the same consecutive codes */
	while (p < mat->branch_end &&
	       cp->cmd == ((const code_t*)p)->cmd)
	{
		lbound += ((const code_t*)p)->lbound;
		ubound += ((const code_t*)p)->ubound;

		p += ASE_SIZEOF(*cp);
	}

#ifdef DEBUG_REX
	ase_dprintf (
		ASE_T("match_any_char: lbound = %u, ubound = %u\n"), 
		(unsigned int)lbound, (unsigned int)ubound);
#endif

	/* find the longest match */
	while (si < ubound)
	{
		if (&mat->match_ptr[si] >= matcher->match.str.end) break;
		si++;
	}

#ifdef DEBUG_REX
	ase_dprintf (
		ASE_T("match_any_char: max si = %u\n"), (unsigned)si);
#endif

	if (si >= lbound && si <= ubound)
	{
		p = match_occurrences (matcher, si, p, lbound, ubound, mat);
	}

	return p;
}

static const ase_byte_t* match_ord_char (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	const ase_byte_t* p = base;
	const code_t* cp;
	ase_size_t si = 0, lbound, ubound;
	ase_char_t cc;

	cp = (const code_t*)p; p += ASE_SIZEOF(*cp);
	ASE_ASSERT (cp->cmd == CMD_ORD_CHAR);

	lbound = cp->lbound; 
	ubound = cp->ubound;

	cc = *(ase_char_t*)p; p += ASE_SIZEOF(cc);
	if (matcher->ignorecase) cc = ASE_TOUPPER(matcher->ccls, cc);

	/* merge the same consecutive codes 
	 * for example, a{1,10}a{0,10} is shortened to a{1,20} */
	if (matcher->ignorecase) 
	{
		while (p < mat->branch_end &&
		       cp->cmd == ((const code_t*)p)->cmd)
		{
			if (ASE_TOUPPER (matcher->ccls, *(ase_char_t*)(p+ASE_SIZEOF(*cp))) != cc) break;

			lbound += ((const code_t*)p)->lbound;
			ubound += ((const code_t*)p)->ubound;

			p += ASE_SIZEOF(*cp) + ASE_SIZEOF(cc);
		}
	}
	else
	{
		while (p < mat->branch_end &&
		       cp->cmd == ((const code_t*)p)->cmd)
		{
			if (*(ase_char_t*)(p+ASE_SIZEOF(*cp)) != cc) break;

			lbound += ((const code_t*)p)->lbound;
			ubound += ((const code_t*)p)->ubound;

			p += ASE_SIZEOF(*cp) + ASE_SIZEOF(cc);
		}
	}
	
#ifdef DEBUG_REX
	ase_dprintf (
		ASE_T("match_ord_char: cc = %c, lbound = %u, ubound = %u\n"), 
		cc, (unsigned int)lbound, (unsigned int)ubound);
#endif

	mat->matched = ASE_FALSE;
	mat->match_len = 0;

	/* find the longest match */
	if (matcher->ignorecase) 
	{
		while (si < ubound)
		{
			if (&mat->match_ptr[si] >= matcher->match.str.end) break;
#ifdef DEBUG_REX
			ase_dprintf (
				ASE_T("match_ord_char: <ignorecase> %c %c\n"),
				cc, mat->match_ptr[si]);
#endif
			if (cc != ASE_TOUPPER (matcher->ccls, mat->match_ptr[si])) break;
			si++;
		}
	}
	else
	{
		while (si < ubound)
		{
			if (&mat->match_ptr[si] >= matcher->match.str.end) break;
#ifdef DEBUG_REX
			ase_dprintf (
				ASE_T("match_ord_char: %c %c\n"), 
				cc, mat->match_ptr[si]);
#endif
			if (cc != mat->match_ptr[si]) break;
			si++;
		}
	}

#ifdef DEBUG_REX
	ase_dprintf (
		ASE_T("match_ord_char: max occurrences=%u, lbound=%u, ubound=%u\n"), 
		(unsigned)si, (unsigned)lbound, (unsigned)ubound);
#endif

	if (si >= lbound && si <= ubound)
	{
		p = match_occurrences (matcher, si, p, lbound, ubound, mat);
	}

	return p;
}

static const ase_byte_t* match_charset (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	const ase_byte_t* p = base;
	ase_size_t si = 0;
	ase_bool_t n;
	ase_char_t c;

	code_t* cp;
	cshdr_t* cshdr;

	cp = (code_t*)p; p += ASE_SIZEOF(*cp);
	ASE_ASSERT (cp->cmd == CMD_CHARSET);

	cshdr = (cshdr_t*)p; p += ASE_SIZEOF(*cshdr);

#ifdef DEBUG_REX
	ase_dprintf (
		ASE_T("match_charset: lbound = %u, ubound = %u\n"), 
		(unsigned int)cp->lbound, (unsigned int)cp->ubound);
#endif

	mat->matched = ASE_FALSE;
	mat->match_len = 0;

	while (si < cp->ubound)
	{
		if (&mat->match_ptr[si] >= matcher->match.str.end) break;

		c = mat->match_ptr[si];
		if (matcher->ignorecase) c = ASE_TOUPPER(matcher->ccls, c);

		n = __test_charset (matcher, p, cshdr->csc, c);
		if (cp->negate) n = !n;
		if (!n) break;

		si++;
	}

	p = p + cshdr->csl - ASE_SIZEOF(*cshdr);

#ifdef DEBUG_REX
	ase_dprintf (
		ASE_T("match_charset: max occurrences=%u, lbound=%u, ubound=%u\n"), 
		(unsigned)si, (unsigned)cp->lbound, (unsigned)cp->ubound);
#endif

	if (si >= cp->lbound && si <= cp->ubound)
	{
		p = match_occurrences (matcher, si, p, cp->lbound, cp->ubound, mat);
	}

	return p;
}

static const ase_byte_t* match_group (
	matcher_t* matcher, const ase_byte_t* base, match_t* mat)
{
	const ase_byte_t* p = base;
	const code_t* cp;
	match_t mat2;
	ase_size_t si = 0, grp_len_static[16], * grp_len;

	cp = (const code_t*)p; p += ASE_SIZEOF(*cp);
	ASE_ASSERT (cp->cmd == CMD_GROUP);

	mat->matched = ASE_FALSE;
	mat->match_len = 0;
	
	/* 
	 * A grouped pattern, unlike other atoms, can match one or more 
	 * characters. When it is requested with a variable occurrences, 
	 * the number of characters that have matched at each occurrence 
	 * needs to be remembered for the backtracking purpose.
	 *
	 * An array "grp_len" is used to store the accumulated number of 
	 * characters. grp_len[0] is set to zero always for convenience.
	 * grp_len[1] holds the number of characters that have matched
	 * at the first occurrence, grp_len[2] at the second occurrence, 
	 * and so on.
	 *
	 * Look at the following example
	 *
	 *   pattern: (abc){1,3}x   string: abcabcabcxyz
	 *
	 *  grp_len[3] => 9 -----------+
	 *  grp_len[2] => 6 --------+  |
	 *  grp_len[1] => 3 -----+  |  |
	 *  grp_len[0] => 0 --+  |  |  |
	 *                    |  |  |  |
	 *                     abcabcabcxyz
	 */

	if (cp->ubound < ASE_COUNTOF(grp_len_static))
	{
		grp_len = grp_len_static;
	}
	else 
	{
		grp_len = (ase_size_t*) ASE_MALLOC (
			matcher->mmgr, ASE_SIZEOF(ase_size_t) * cp->ubound);
		if (grp_len == ASE_NULL)
		{
			matcher->errnum = ASE_REX_ENOMEM;
			return ASE_NULL;
		}
	}

	grp_len[si] = 0;

	mat2.match_ptr = mat->match_ptr;
	while (si < cp->ubound)
	{
		if (mat2.match_ptr >= matcher->match.str.end) break;

		if (match_pattern (matcher, p, &mat2) == ASE_NULL) 
		{
			if (grp_len != grp_len_static) 
				ASE_FREE (matcher->mmgr, grp_len);
			return ASE_NULL;
		}
		if (!mat2.matched) break;

		grp_len[si+1] = grp_len[si] + mat2.match_len;

		mat2.match_ptr += mat2.match_len;
		mat2.match_len = 0;
		mat2.matched = ASE_FALSE;

		si++;
	}

	/* increment p by the length of the subpattern */
	p += *(ase_size_t*)(p+ASE_SIZEOF(ase_size_t));

	/* check the occurrences */
	if (si >= cp->lbound && si <= cp->ubound)
	{
		if (cp->lbound == cp->ubound || p >= mat->branch_end)
		{
			mat->matched = ASE_TRUE;
			mat->match_len = grp_len[si];
		}
		else 
		{
			ASE_ASSERT (cp->ubound > cp->lbound);

			do
			{
				const ase_byte_t* tmp;
	
				mat2.match_ptr = &mat->match_ptr[grp_len[si]];
				mat2.branch = mat->branch;
				mat2.branch_end = mat->branch_end;
	
			#ifdef DEBUG_REX
				ase_dprintf (
					ASE_T("match_group: GROUP si=%d [%s]\n"),
					(unsigned)si, mat->match_ptr);
			#endif
				tmp = match_branch_body (matcher, p, &mat2);
				if (tmp == ASE_NULL)
				{
					if (grp_len != grp_len_static) 
						ASE_FREE (matcher->mmgr, grp_len);
					return ASE_NULL;
				}

				if (mat2.matched)
				{
					mat->matched = ASE_TRUE;
					mat->match_len = grp_len[si] + mat2.match_len;
					p = tmp;
					break;
				}

				if (si <= cp->lbound) break;
				si--;
			} 
			while (1);
		}

	}

	if (grp_len != grp_len_static) ASE_FREE (matcher->mmgr, grp_len);
	return p;
}

static const ase_byte_t* match_occurrences (
	matcher_t* matcher, ase_size_t si, const ase_byte_t* p,
	ase_size_t lbound, ase_size_t ubound, match_t* mat)
{
	ASE_ASSERT (si >= lbound && si <= ubound);
	/* the match has been found */

	if (lbound == ubound || p >= mat->branch_end)
	{
		/* if the match for fixed occurrences was 
		 * requested or no atoms remain unchecked in 
		 * the branch, the match is returned. */
		mat->matched = ASE_TRUE;
		mat->match_len = si;
	}
	else 
	{
		/* Otherwise, it checks if the remaining atoms 
		 * match the rest of the string 
		 * 
		 * Let's say the caller of this function was processing
		 * the first period character in the following example.
		 *
		 *     pattern: .{1,3}xx   string: xxxyy
		 * 
		 * It scans up to the third "x" in the string. si is set 
		 * to 3 and p points to the first "x" in the pattern. 
		 * It doesn't change mat.match_ptr so mat.match_ptr remains
		 * the same.
		 *
		 *     si = 3    p -----+    mat.match_ptr ---+
		 *                      |                     |
		 *                .{1,3}xx                    xxxyy
		 *                     
		 *  When the code reaches here, the string pointed at by
		 *  &mat.match_ptr[si] is tried to match against the remaining
		 *  pattern pointed at p.
		 *  
		 *     &mat.match_ptr[si] ---+
		 *                           |
		 *                        xxxyy
		 *
		 * If a match is found, the match and the previous match are
		 * merged and returned.
		 *
		 * If not, si is decremented by one and the match is performed
		 * from the string pointed at by &mat.match_ptr[si].
		 *
		 *     &mat.match_ptr[si] --+
		 *                          |
		 *                        xxxyy
		 *
		 * This process is repeated until a match is found or si 
		 * becomes less than lbound. (si never becomes less than
		 * lbound in the implementation below, though)
		 */

		ASE_ASSERT (ubound > lbound);

		do
		{
			match_t mat2;
			const ase_byte_t* tmp;

			mat2.match_ptr = &mat->match_ptr[si];
			mat2.branch = mat->branch;
			mat2.branch_end = mat->branch_end;

		#ifdef DEBUG_REX
			ase_dprintf (
				ASE_T("__match occurrences: si=%u [%s]\n"), 
				(unsigned)si, mat->match_ptr);
		#endif
			tmp = match_branch_body (matcher, p, &mat2);

			if (mat2.matched)
			{
				mat->matched = ASE_TRUE;
				mat->match_len = si + mat2.match_len;
				p = tmp;
				break;
			}

			if (si <= lbound) break;
			si--;
		} 
		while (1);
	}

	return p;
}

static ase_bool_t __test_charset (
	matcher_t* matcher, const ase_byte_t* p, ase_size_t csc, ase_char_t c)
{
	ase_size_t i;

	for (i = 0; i < csc; i++)
	{
		ase_char_t c0, c1, c2;

		c0 = *(const ase_char_t*)p;
		p += ASE_SIZEOF(c0);
		if (c0 == CHARSET_ONE)
		{
			c1 = *(const ase_char_t*)p;
			if (matcher->ignorecase) 
				c1 = ASE_TOUPPER(matcher->ccls, c1);
		#ifdef DEBUG_REX
			ase_dprintf (
				ASE_T("match_charset: <one> %c %c\n"), c, c1);
		#endif
			if (c == c1) return ASE_TRUE;
		}
		else if (c0 == CHARSET_RANGE)
		{
			c1 = *(const ase_char_t*)p;
			p += ASE_SIZEOF(c1);
			c2 = *(const ase_char_t*)p;

			if (matcher->ignorecase) 
			{
				c1 = ASE_TOUPPER(matcher->ccls, c1);
				c2 = ASE_TOUPPER(matcher->ccls, c2);
			}
		#ifdef DEBUG_REX
			ase_dprintf (
				ASE_T("match_charset: <range> %c %c-%c\n"), c, c1, c2);
		#endif
			if (c >= c1 && c <= c2) return ASE_TRUE;
		}
		else if (c0 == CHARSET_CLASS)
		{
			c1 = *(const ase_char_t*)p;
		#ifdef DEBUG_REX
			ase_dprintf (
				ASE_T("match_charset: <class> %c %s\n"), 
				c, __char_class[c1].name);
		#endif
			if (__char_class[c1].func (
				matcher->ccls, c)) return ASE_TRUE;
		}
		else
		{
			ASE_ASSERT (!"should never happen - invalid charset code");
			break;
		}

		p += ASE_SIZEOF(c1);
	}

	return ASE_FALSE;
}

static ase_bool_t cc_isalnum (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISALNUM (ccls, c);
}

static ase_bool_t cc_isalpha (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISALPHA (ccls, c);
}

static ase_bool_t cc_isblank (ase_ccls_t* ccls, ase_char_t c)
{
	return c == ASE_T(' ') || c == ASE_T('\t');
}

static ase_bool_t cc_iscntrl (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISCNTRL (ccls, c);
}

static ase_bool_t cc_isdigit (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISDIGIT (ccls, c);
}

static ase_bool_t cc_isgraph (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISGRAPH (ccls, c);
}

static ase_bool_t cc_islower (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISLOWER (ccls, c);
}

static ase_bool_t cc_isprint (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISPRINT (ccls, c);
}

static ase_bool_t cc_ispunct (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISPUNCT (ccls, c);
}

static ase_bool_t cc_isspace (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISSPACE (ccls, c);
}

static ase_bool_t cc_isupper (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISUPPER (ccls, c);
}

static ase_bool_t cc_isxdigit (ase_ccls_t* ccls, ase_char_t c)
{
	return ASE_ISXDIGIT (ccls, c);
}

#if 0
#define DPRINTF awk->prmfns.misc.dprintf
#define DCUSTOM awk->prmfns.misc.custom_data

void ase_awk_dprintrex (ase_awk_t* awk, void* rex)
{
	__print_pattern (awk, rex);
	DPRINTF (DCUSTOM, awk->prmfns.misc.custom_data, ASE_T("\n"));
}

static const ase_byte_t* __print_pattern (ase_awk_t* awk, const ase_byte_t* p)
{
	ase_size_t i;
	rhdr_t* rhdr;

	rhdr = (rhdr_t*)p; p += ASE_SIZEOF(*rhdr);

	for (i = 0; i < rhdr->nb; i++)
	{
		if (i != 0) DPRINTF (DCUSTOM, ASE_T("|"));
		p = __print_branch (awk, p);
	}

	return p;
}

static const ase_byte_t* __print_branch (ase_awk_t* awk, const ase_byte_t* p)
{
	ase_size_t i;
	bhdr_t* bhdr;
       
	bhdr = (bhdr_t*)p; p += ASE_SIZEOF(*bhdr);

	for (i = 0; i < bhdr->na; i++)
	{
		p = __print_atom (awk, p);
	}

	return p;
}

static const ase_byte_t* __print_atom (ase_awk_t* awk, const ase_byte_t* p)
{
	const code_t* cp = (const code_t*)p;

	if (cp->cmd == CMD_BOL)
	{
		DPRINTF (DCUSTOM, ASE_T("^"));
		p += ASE_SIZEOF(*cp);
	}
	else if (cp->cmd == CMD_EOL)
	{
		DPRINTF (DCUSTOM, ASE_T("$"));
		p += ASE_SIZEOF(*cp);
	}
	else if (cp->cmd == CMD_ANY_CHAR) 
	{
		DPRINTF (DCUSTOM, ASE_T("."));
		p += ASE_SIZEOF(*cp);
	}
	else if (cp->cmd == CMD_ORD_CHAR) 
	{
		p += ASE_SIZEOF(*cp);
		DPRINTF (DCUSTOM, ASE_T("%c"), *(ase_char_t*)p);
		p += ASE_SIZEOF(ase_char_t);
	}
	else if (cp->cmd == CMD_CHARSET)
	{
		ase_size_t i;
		cshdr_t* cshdr;

		p += ASE_SIZEOF(*cp);
		DPRINTF (DCUSTOM, ASE_T("["));
		if (cp->negate) DPRINTF (DCUSTOM, ASE_T("^"));

		cshdr = (cshdr_t*)p; p += ASE_SIZEOF(*cshdr);

		for (i = 0; i < cshdr->csc; i++)
		{
			ase_char_t c0, c1, c2;

			c0 = *(ase_char_t*)p;
			p += ASE_SIZEOF(c0);

			if (c0 == CHARSET_ONE)
			{
				c1 = *(ase_char_t*)p;
				DPRINTF (DCUSTOM, ASE_T("%c"), c1);
			}
			else if (c0 == CHARSET_RANGE)
			{
				c1 = *(ase_char_t*)p;
				p += ASE_SIZEOF(c1);
				c2 = *(ase_char_t*)p;
				DPRINTF (DCUSTOM, ASE_T("%c-%c"), c1, c2);
			}
			else if (c0 == CHARSET_CLASS)
			{
				c1 = *(ase_char_t*)p;
				DPRINTF (DCUSTOM, ASE_T("[:%s:]"), __char_class[c1].name);
			}
			else
			{
				DPRINTF (DCUSTOM, ASE_T("should never happen - invalid charset code\n"));
			}

			p += ASE_SIZEOF(c1);
		}

		DPRINTF (DCUSTOM, ASE_T("]"));
	}
	else if (cp->cmd == CMD_GROUP)
	{
		p += ASE_SIZEOF(*cp);
		DPRINTF (DCUSTOM, ASE_T("("));
		p = __print_pattern (awk, p);
		DPRINTF (DCUSTOM, ASE_T(")"));
	}
	else 
	{
		DPRINTF (DCUSTOM, ASE_T("should never happen - invalid atom code\n"));
	}

	if (cp->lbound == 0 && cp->ubound == BOUND_MAX)
		DPRINTF (DCUSTOM, ASE_T("*"));
	else if (cp->lbound == 1 && cp->ubound == BOUND_MAX)
		DPRINTF (DCUSTOM, ASE_T("+"));
	else if (cp->lbound == 0 && cp->ubound == 1)
		DPRINTF (DCUSTOM, ASE_T("?"));
	else if (cp->lbound != 1 || cp->ubound != 1)
	{
		DPRINTF (DCUSTOM, ASE_T("{%lu,%lu}"), 
			(unsigned long)cp->lbound, (unsigned long)cp->ubound);
	}

	return p;
}

#endif

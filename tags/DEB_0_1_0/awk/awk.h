/* 
 * $Id: awk.h,v 1.187 2007-02-03 10:47:40 bacon Exp $
 *
 * {License}
 */

#ifndef _ASE_AWK_AWK_H_
#define _ASE_AWK_AWK_H_

#include <ase/types.h>
#include <ase/macros.h>

typedef struct ase_awk_t ase_awk_t;
typedef struct ase_awk_run_t ase_awk_run_t;
typedef struct ase_awk_val_t ase_awk_val_t;
typedef struct ase_awk_extio_t ase_awk_extio_t;

typedef struct ase_awk_prmfns_t ase_awk_prmfns_t;
typedef struct ase_awk_srcios_t ase_awk_srcios_t;
typedef struct ase_awk_runios_t ase_awk_runios_t;
typedef struct ase_awk_runcbs_t ase_awk_runcbs_t;
typedef struct ase_awk_runarg_t ase_awk_runarg_t;

typedef void* (*ase_awk_malloc_t)  (ase_size_t n, void* custom_data); 
typedef void* (*ase_awk_realloc_t) (void* ptr, ase_size_t n, void* custom_data);
typedef void  (*ase_awk_free_t)    (void* ptr, void* custom_data); 
typedef void* (*ase_awk_memcpy_t)  (void* dst, const void* src, ase_size_t n);
typedef void* (*ase_awk_memset_t)  (void* dst, int val, ase_size_t n);

typedef ase_bool_t (*ase_awk_isctype_t) (ase_cint_t c);
typedef ase_cint_t (*ase_awk_toctype_t) (ase_cint_t c);
typedef ase_real_t (*ase_awk_pow_t) (ase_real_t x, ase_real_t y);

typedef int (*ase_awk_sprintf_t) (
	ase_char_t* buf, ase_size_t size, const ase_char_t* fmt, ...);
typedef void (*ase_awk_aprintf_t) (const ase_char_t* fmt, ...); 
typedef void (*ase_awk_dprintf_t) (const ase_char_t* fmt, ...); 
typedef void (*ase_awk_abort_t)   (void* custom_data);
typedef void (*ase_awk_lock_t)    (void* custom_data);

typedef ase_ssize_t (*ase_awk_io_t) (
	int cmd, void* arg, ase_char_t* data, ase_size_t count);

struct ase_awk_extio_t 
{
	ase_awk_run_t* run; /* [IN] */
	int type;           /* [IN] console, file, coproc, pipe */
	int mode;           /* [IN] read, write, etc */
	ase_char_t* name;   /* [IN] */
	void* custom_data;  /* [IN] */
	void* handle;       /* [OUT] */

	/* input */
	struct
	{
		ase_char_t buf[2048];
		ase_size_t pos;
		ase_size_t len;
		ase_bool_t eof;
		ase_bool_t eos;
	} in;

	/* output */
	struct
	{
		ase_bool_t eof;
		ase_bool_t eos;
	} out;

	ase_awk_extio_t* next;
};

struct ase_awk_prmfns_t
{
	/* memory allocation/deallocation */
	ase_awk_malloc_t  malloc;      /* required */
	ase_awk_realloc_t realloc;     /* optional */
	ase_awk_free_t    free;        /* required */
	ase_awk_memcpy_t  memcpy;      /* optional */
	ase_awk_memset_t  memset;      /* optional */

	/* character classes */
	ase_awk_isctype_t is_upper;    /* required */
	ase_awk_isctype_t is_lower;    /* required */
	ase_awk_isctype_t is_alpha;    /* required */
	ase_awk_isctype_t is_digit;    /* required */
	ase_awk_isctype_t is_xdigit;   /* required */
	ase_awk_isctype_t is_alnum;    /* required */
	ase_awk_isctype_t is_space;    /* required */
	ase_awk_isctype_t is_print;    /* required */
	ase_awk_isctype_t is_graph;    /* required */
	ase_awk_isctype_t is_cntrl;    /* required */
	ase_awk_isctype_t is_punct;    /* required */
	ase_awk_toctype_t to_upper;    /* required */
	ase_awk_toctype_t to_lower;    /* required */

	/* utilities */
	ase_awk_pow_t     pow;         /* required */
	ase_awk_sprintf_t sprintf;     /* required */
	ase_awk_aprintf_t aprintf;     /* required in the debug mode */
	ase_awk_dprintf_t dprintf;     /* required in the debug mode */
	ase_awk_abort_t   abort;       /* required in the debug mode */

	/* thread lock */
	ase_awk_lock_t    lock;        /* required if multi-threaded */
	ase_awk_lock_t    unlock;      /* required if multi-threaded */

	/* user-defined data passed to selected system functions */
	void*             custom_data; /* optional */
};

struct ase_awk_srcios_t
{
	ase_awk_io_t in;
	ase_awk_io_t out;
	void* custom_data;
};

struct ase_awk_runios_t
{
	ase_awk_io_t pipe;
	ase_awk_io_t coproc;
	ase_awk_io_t file;
	ase_awk_io_t console;
	void* custom_data;
};

struct ase_awk_runcbs_t
{
	void (*on_start) (
		ase_awk_t* awk, ase_awk_run_t* run, void* custom_data);
	void (*on_end) (
		ase_awk_t* awk, ase_awk_run_t* run, int errnum, void* custom_data);
	void* custom_data;
};

struct ase_awk_runarg_t
{
	const ase_char_t* ptr;
	ase_size_t len;
};

/* io function commands */
enum 
{
	ASE_AWK_IO_OPEN   = 0,
	ASE_AWK_IO_CLOSE  = 1,
	ASE_AWK_IO_READ   = 2,
	ASE_AWK_IO_WRITE  = 3,
	ASE_AWK_IO_FLUSH  = 4,
	ASE_AWK_IO_NEXT   = 5  
};

/* various options */
enum 
{ 
	/* allow undeclared variables */
	ASE_AWK_IMPLICIT    = (1 << 0),

	/* allow explicit variable declarations */
	ASE_AWK_EXPLICIT    = (1 << 1), 

	/* a function name should not coincide to be a variable name */
	ASE_AWK_UNIQUEFN    = (1 << 2),

	/* allow variable shading */
	ASE_AWK_SHADING     = (1 << 3), 

	/* support shift operators */
	ASE_AWK_SHIFT       = (1 << 4), 

	/* enable the idiv operator (double slashes) */
	ASE_AWK_IDIV        = (1 << 5), 

	/* support string concatenation in tokenization.
	 * this option can change the behavior of a certain construct.
	 * getline < "abc" ".def" is treated as if it is getline < "abc.def" 
	 * when this option is on. If this option is off, the same expression
	 * is treated as if it is (getline < "abc") ".def". */
	ASE_AWK_STRCONCAT   = (1 << 6), 

	/* support getline and print */
	ASE_AWK_EXTIO       = (1 << 7), 

	/* support co-process */
	ASE_AWK_COPROC      = (1 << 8),

	/* support blockless patterns */
	ASE_AWK_BLOCKLESS   = (1 << 9), 

	/* use 1 as the start index for string operations */
	ASE_AWK_STRBASEONE  = (1 << 10),

	/* strip off leading and trailing spaces when splitting a record
	 * into fields with a regular expression.
	 *
	 * Consider the following program.
	 *  BEGIN { FS="[:[:space:]]+"; } 
	 *  { 
	 *  	print "NF=" NF; 
	 *  	for (i = 0; i < NF; i++) print i " [" $(i+1) "]";
	 *  }
	 *
	 * The program splits " a b c " into [a], [b], [c] when this
	 * option is on while into [], [a], [b], [c], [] when it is off.
	 */
	ASE_AWK_STRIPSPACES = (1 << 11),

	/* enable the nextoutfile keyword */
	ASE_AWK_NEXTOFILE   = (1 << 12),

	/* cr + lf by default */
	ASE_AWK_CRLF        = (1 << 13)
};

/* error code */
enum 
{
	ASE_AWK_ENOERR,         /* no error */

	ASE_AWK_EINVAL,         /* invalid parameter */
	ASE_AWK_ENOMEM,         /* out of memory */
	ASE_AWK_ENOSUP,         /* not supported */
	ASE_AWK_ENOPER,         /* operation not allowed */
	ASE_AWK_ENODEV,         /* no such device */
	ASE_AWK_ENOSPC,         /* no space left on device */
	ASE_AWK_ENOENT,         /* no such file, directory, or data */
	ASE_AWK_EMFILE,         /* too many open files */
	ASE_AWK_EMLINK,         /* too many links */
	ASE_AWK_EAGAIN,         /* resource temporarily unavailable */
	ASE_AWK_EEXIST,         /* file or data exists */
	ASE_AWK_EFTBIG,         /* file or data too big */
	ASE_AWK_ETBUSY,         /* system too busy */
	ASE_AWK_EISDIR,         /* is a directory */
	ASE_AWK_EIOERR,         /* i/o error */

	ASE_AWK_EINTERN,        /* internal error */
	ASE_AWK_ERUNTIME,       /* run-time error */
	ASE_AWK_ERUNNING,       /* there are running instances */
	ASE_AWK_ERECUR,         /* recursion too deep */
	ASE_AWK_ESYSFNS,        /* system functions not proper */

	ASE_AWK_ESINOP,
	ASE_AWK_ESINCL,
	ASE_AWK_ESINRD, 

	ASE_AWK_ESOUTOP,
	ASE_AWK_ESOUTCL,
	ASE_AWK_ESOUTWR,

	ASE_AWK_ECINOP,
	ASE_AWK_ECINCL,
	ASE_AWK_ECINNX,
	ASE_AWK_ECINDT, 

	ASE_AWK_ECOUTOP,
	ASE_AWK_ECOUTCL,
	ASE_AWK_ECOUTNX,
	ASE_AWK_ECOUTDT,

	ASE_AWK_ELXCHR,         /* lexer came accross an wrong character */
	ASE_AWK_ELXUNG,         /* lexer failed to unget a character */

	ASE_AWK_EENDSRC,        /* unexpected end of source */
	ASE_AWK_EENDCMT,        /* unexpected end of a comment */
	ASE_AWK_EENDSTR,        /* unexpected end of a string */
	ASE_AWK_EENDREX,        /* unexpected end of a regular expression */
	ASE_AWK_ELBRACE,        /* left brace expected */
	ASE_AWK_ELPAREN,        /* left parenthesis expected */
	ASE_AWK_ERPAREN,        /* right parenthesis expected */
	ASE_AWK_ERBRACK,        /* right bracket expected */
	ASE_AWK_ECOMMA,         /* comma expected */
	ASE_AWK_ESCOLON,        /* semicolon expected */
	ASE_AWK_ECOLON,         /* colon expected */
	ASE_AWK_EIN,            /* keyword 'in' is expected */
	ASE_AWK_ENOTVAR,        /* not a variable name after 'in' */
	ASE_AWK_EEXPRES,        /* expression expected */

	ASE_AWK_EWHILE,         /* keyword 'while' is expected */
	ASE_AWK_EASSIGN,        /* assignment statement expected */
	ASE_AWK_EIDENT,         /* identifier expected */
	ASE_AWK_EBLKBEG,        /* BEGIN requires an action block */
	ASE_AWK_EBLKEND,        /* END requires an action block */
	ASE_AWK_EDUPBEG,        /* duplicate BEGIN */
	ASE_AWK_EDUPEND,        /* duplicate END */
	ASE_AWK_EBFNRED,        /* builtin function redefined */
	ASE_AWK_EAFNRED,        /* function redefined */
	ASE_AWK_EGBLRED,        /* global variable redefined */
	ASE_AWK_EPARRED,        /* parameter redefined */
	ASE_AWK_EDUPPAR,        /* duplicate parameter name */
	ASE_AWK_EDUPGBL,        /* duplicate global variable name */
	ASE_AWK_EDUPLCL,        /* duplicate local variable name */
	ASE_AWK_EUNDEF,         /* undefined identifier */
	ASE_AWK_ELVALUE,        /* l-value required */
	ASE_AWK_EGBLTM,         /* too many global variables */
	ASE_AWK_ELCLTM,         /* too many local variables */
	ASE_AWK_EPARTM,         /* too many parameters */
	ASE_AWK_EBREAK,         /* break outside a loop */
	ASE_AWK_ECONTINUE,      /* continue outside a loop */
	ASE_AWK_ENEXT,          /* next illegal in BEGIN or END block */
	ASE_AWK_ENEXTFILE,      /* nextfile illegal in BEGIN or END block */
	ASE_AWK_EGETLINE,       /* getline expected */
	ASE_AWK_EPRINTFARG,     /* printf must have one or more arguments */

	/* run time error */
	ASE_AWK_EDIVBY0,           /* divide by zero */
	ASE_AWK_EOPERAND,          /* invalid operand */
	ASE_AWK_EPOSIDX,           /* wrong position index */
	ASE_AWK_EARGTF,            /* too few arguments */
	ASE_AWK_EARGTM,            /* too many arguments */
	ASE_AWK_EFNNONE,           /* no such function */
	ASE_AWK_ENOTIDX,           /* variable not indexable */
	ASE_AWK_ENOTDEL,           /* variable not deletable */
	ASE_AWK_ENOTMAP,           /* value not a map */
	ASE_AWK_ENOTREF,           /* value not referenceable */
	ASE_AWK_ENOTASS,           /* value not assignable */
	ASE_AWK_EIDXVALASSMAP,     /* indexed value cannot be assigned a map */
	ASE_AWK_EPOSVALASSMAP,     /* a positional cannot be assigned a map */
	ASE_AWK_EMAPTOSCALAR,      /* cannot change a map to a scalar value */
	ASE_AWK_ESCALARTOMAP,      /* cannot change a scalar value to a map */
	ASE_AWK_EMAPNOTALLOWED,    /* a map is not allowed */
	ASE_AWK_EVALTYPE,          /* wrong value type */
	ASE_AWK_ENEXTCALL,         /* next called from BEGIN or END */
	ASE_AWK_ENEXTFILECALL,     /* nextfile called from BEGIN or END */
	ASE_AWK_EBFNUSER,          /* wrong builtin function implementation */
	ASE_AWK_EBFNIMPL,          /* builtin function handler failed */
	ASE_AWK_EIOUSER,           /* wrong user io handler implementation */
	ASE_AWK_EIONONE,           /* no such io name found */
	ASE_AWK_EIOIMPL,           /* i/o callback returned an error */
	ASE_AWK_EIONAME,           /* invalid i/o name */
	ASE_AWK_EFMTARG,           /* arguments to format string not sufficient */
	ASE_AWK_EFMTCNV,           /* recursion detected in format conversion */
	ASE_AWK_ECONVFMTCHAR,      /* an invalid character found in CONVFMT */
	ASE_AWK_EOFMTCHAR,         /* an invalid character found in OFMT */

	/* regular expression error */
	ASE_AWK_EREXRPAREN,       /* a right parenthesis is expected */
	ASE_AWK_EREXRBRACKET,     /* a right bracket is expected */
	ASE_AWK_EREXRBRACE,       /* a right brace is expected */
	ASE_AWK_EREXCOLON,        /* a colon is expected */
	ASE_AWK_EREXCRANGE,       /* invalid character range */
	ASE_AWK_EREXCCLASS,       /* invalid character class */
	ASE_AWK_EREXBRANGE,       /* invalid boundary range */
	ASE_AWK_EREXEND,          /* unexpected end of the pattern */
	ASE_AWK_EREXGARBAGE       /* garbage after the pattern */
};

/* depth types */
enum ase_awk_depth_t
{
	ASE_AWK_DEPTH_BLOCK_PARSE = (1 << 0),
	ASE_AWK_DEPTH_BLOCK_RUN   = (1 << 1),
	ASE_AWK_DEPTH_EXPR_PARSE  = (1 << 2),
	ASE_AWK_DEPTH_EXPR_RUN    = (1 << 3),
	ASE_AWK_DEPTH_REX_BUILD   = (1 << 4),
	ASE_AWK_DEPTH_REX_MATCH   = (1 << 5)
};

/* extio types */
enum ase_awk_extio_type_t
{
	/* extio types available */
	ASE_AWK_EXTIO_PIPE,
	ASE_AWK_EXTIO_COPROC,
	ASE_AWK_EXTIO_FILE,
	ASE_AWK_EXTIO_CONSOLE,

	/* reserved for internal use only */
	ASE_AWK_EXTIO_NUM
};

enum
{
	ASE_AWK_EXTIO_PIPE_READ      = 0,
	ASE_AWK_EXTIO_PIPE_WRITE     = 1,

	/*
	ASE_AWK_EXTIO_COPROC_READ    = 0,
	ASE_AWK_EXTIO_COPROC_WRITE   = 1,
	ASE_AWK_EXTIO_COPROC_RDWR    = 2,
	*/

	ASE_AWK_EXTIO_FILE_READ      = 0,
	ASE_AWK_EXTIO_FILE_WRITE     = 1,
	ASE_AWK_EXTIO_FILE_APPEND    = 2,

	ASE_AWK_EXTIO_CONSOLE_READ   = 0,
	ASE_AWK_EXTIO_CONSOLE_WRITE  = 1
};

/* assertion statement */
#ifdef NDEBUG
	#define ASE_AWK_ASSERT(awk,expr) ((void)0)
	#define ASE_AWK_ASSERTX(awk,expr,desc) ((void)0)
#else
	#define ASE_AWK_ASSERT(awk,expr) (void)((expr) || \
		(ase_awk_assertfail (awk, ASE_T(#expr), ASE_NULL, ASE_T(__FILE__), __LINE__), 0))
	#define ASE_AWK_ASSERTX(awk,expr,desc) (void)((expr) || \
		(ase_awk_assertfail (awk, ASE_T(#expr), ASE_T(desc), ASE_T(__FILE__), __LINE__), 0))
#endif

#ifdef __cplusplus
extern "C" {
#endif

ase_awk_t* ase_awk_open (
	const ase_awk_prmfns_t* prmfns, void* custom_data, int* errnum);
int ase_awk_close (ase_awk_t* awk);
int ase_awk_clear (ase_awk_t* awk);

void* ase_awk_getcustomdata (ase_awk_t* awk);
int ase_awk_geterrnum (ase_awk_t* awk);
ase_size_t ase_awk_geterrlin (ase_awk_t* awk);
const ase_char_t* ase_awk_geterrmsg (ase_awk_t* awk);

void ase_awk_geterror (
	ase_awk_t* awk, int* errnum, 
	ase_size_t* errlin, const ase_char_t** errmsg);

void ase_awk_seterrnum (ase_awk_t* awk, int errnum);

void ase_awk_seterror (
	ase_awk_t* run, int errnum, 
	ase_size_t errlin, const ase_char_t* errmsg);

int ase_awk_getoption (ase_awk_t* awk);
void ase_awk_setoption (ase_awk_t* awk, int opt);

ase_size_t ase_awk_getmaxdepth (ase_awk_t* awk, int type);
void ase_awk_setmaxdepth (ase_awk_t* awk, int types, ase_size_t depth);

int ase_awk_parse (ase_awk_t* awk, ase_awk_srcios_t* srcios);

/*
 * ase_awk_run return 0 on success and -1 on failure, generally speaking.
 *  A runtime context is required for it to start running the program.
 *  Once the runtime context is created, the program starts to run.
 *  The context creation failure is reported by the return value -1 of
 *  this function. however, the runtime error after the context creation
 *  is reported differently depending on the use of the callback.
 *  When no callback is specified (i.e. runcbs is ASE_NULL), ase_awk_run
 *  returns -1 on an error and awk->errnum is set accordingly.
 *  However, if a callback is specified (i.e. runcbs is not ASE_NULL),
 *  ase_awk_run returns 0 on both success and failure. Instead, the 
 *  on_end handler of the callback is triggered with the relevant 
 *  error number. The third parameter to on_end denotes this error number.
 */
int ase_awk_run (
	ase_awk_t* awk, const ase_char_t* main,
	ase_awk_runios_t* runios, ase_awk_runcbs_t* runcbs, 
	ase_awk_runarg_t* runarg, void* custom_data);

int ase_awk_stop (ase_awk_t* awk, ase_awk_run_t* run);
void ase_awk_stopall (ase_awk_t* awk);

/* functions to access internal stack structure */
ase_size_t ase_awk_getnargs (ase_awk_run_t* run);
ase_awk_val_t* ase_awk_getarg (ase_awk_run_t* run, ase_size_t idx);
ase_awk_val_t* ase_awk_getglobal (ase_awk_run_t* run, ase_size_t idx);
int ase_awk_setglobal (ase_awk_run_t* run, ase_size_t idx, ase_awk_val_t* val);
void ase_awk_setretval (ase_awk_run_t* run, ase_awk_val_t* val);

int ase_awk_setfilename (
	ase_awk_run_t* run, const ase_char_t* name, ase_size_t len);
int ase_awk_setofilename (
	ase_awk_run_t* run, const ase_char_t* name, ase_size_t len);

ase_awk_t* ase_awk_getrunawk (ase_awk_run_t* awk);
void* ase_awk_getruncustomdata (ase_awk_run_t* awk);

/* functions to manipulate the run-time error */
int ase_awk_getrunerrnum (ase_awk_run_t* run);
ase_size_t ase_awk_getrunerrlin (ase_awk_run_t* run);
const ase_char_t* ase_awk_getrunerrmsg (ase_awk_run_t* run);
void ase_awk_setrunerrnum (ase_awk_run_t* run, int errnum);

void ase_awk_getrunerror (
	ase_awk_run_t* run, int* errnum, 
	ase_size_t* errlin, const ase_char_t** errmsg);
void ase_awk_setrunerror (
	ase_awk_run_t* run, int errnum, 
	ase_size_t errlin, const ase_char_t* msg);

/* functions to manipulate built-in functions */
void* ase_awk_addbfn (
	ase_awk_t* awk, const ase_char_t* name, ase_size_t name_len, 
	int when_valid, ase_size_t min_args, ase_size_t max_args, 
	const ase_char_t* arg_spec, 
	int (*handler)(ase_awk_run_t*,const ase_char_t*,ase_size_t));

int ase_awk_delbfn (
	ase_awk_t* awk, const ase_char_t* name, ase_size_t name_len);

void ase_awk_clrbfn (ase_awk_t* awk);

/* record and field functions */
int ase_awk_clrrec (ase_awk_run_t* run, ase_bool_t skip_inrec_line);
int ase_awk_setrec (ase_awk_run_t* run, ase_size_t idx, const ase_char_t* str, ase_size_t len);

/* utility functions exported by awk.h */
void* ase_awk_malloc (ase_awk_t* awk, ase_size_t size);
void ase_awk_free (ase_awk_t* awk, void* ptr);

ase_long_t ase_awk_strxtolong (
	ase_awk_t* awk, const ase_char_t* str, ase_size_t len,
	int base, const ase_char_t** endptr);
ase_real_t ase_awk_strxtoreal (
	ase_awk_t* awk, const ase_char_t* str, ase_size_t len, 
	const ase_char_t** endptr);

ase_size_t ase_awk_longtostr (
	ase_long_t value, int radix, const ase_char_t* prefix,
	ase_char_t* buf, ase_size_t size);

/* string functions exported by awk.h */
ase_char_t* ase_awk_strdup (
	ase_awk_t* awk, const ase_char_t* str);
ase_char_t* ase_awk_strxdup (
	ase_awk_t* awk, const ase_char_t* str, ase_size_t len);
ase_char_t* ase_awk_strxdup2 (
	ase_awk_t* awk,
	const ase_char_t* str1, ase_size_t len1,
	const ase_char_t* str2, ase_size_t len2);

ase_size_t ase_awk_strlen (const ase_char_t* str);
ase_size_t ase_awk_strcpy (ase_char_t* buf, const ase_char_t* str);
ase_size_t ase_awk_strxcpy (
	ase_char_t* buf, ase_size_t bsz, const ase_char_t* str);
ase_size_t ase_awk_strncpy (
	ase_char_t* buf, const ase_char_t* str, ase_size_t len);
int ase_awk_strcmp (const ase_char_t* s1, const ase_char_t* s2);

int ase_awk_strxncmp (
	const ase_char_t* s1, ase_size_t len1, 
	const ase_char_t* s2, ase_size_t len2);

int ase_awk_strxncasecmp (
	ase_awk_t* awk,
	const ase_char_t* s1, ase_size_t len1, 
	const ase_char_t* s2, ase_size_t len2);

ase_char_t* ase_awk_strxnstr (
	const ase_char_t* str, ase_size_t strsz, 
	const ase_char_t* sub, ase_size_t subsz);

/* abort function for assertion. use ASE_AWK_ASSERT instead */
int ase_awk_assertfail (ase_awk_t* awk, 
	const ase_char_t* expr, const ase_char_t* desc, 
	const ase_char_t* file, int line);

/* utility functions to convert an error number ot a string */
const ase_char_t* ase_awk_geterrstr (int errnum);

#ifdef __cplusplus
}
#endif

#endif
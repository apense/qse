/*
 * $Id: parser.c 118 2008-03-03 11:21:33Z baconevi $
 */

#include <ase/stx/parser.h>
#include <ase/stx/object.h>
#include <ase/stx/class.h>
#include <ase/stx/method.h>
#include <ase/stx/symbol.h>
#include <ase/stx/bytecode.h>
#include <ase/stx/dict.h>
#include <ase/stx/misc.h>

static int __parse_method (
	ase_stx_parser_t* parser, 
	ase_word_t method_class, void* input);
static int __finish_method (ase_stx_parser_t* parser);

static int __parse_message_pattern (ase_stx_parser_t* parser);
static int __parse_unary_pattern (ase_stx_parser_t* parser);
static int __parse_binary_pattern (ase_stx_parser_t* parser);
static int __parse_keyword_pattern (ase_stx_parser_t* parser);

static int __parse_temporaries (ase_stx_parser_t* parser);
static int __parse_primitive (ase_stx_parser_t* parser);
static int __parse_statements (ase_stx_parser_t* parser);
static int __parse_block_statements (ase_stx_parser_t* parser);
static int __parse_statement (ase_stx_parser_t* parser);
static int __parse_expression (ase_stx_parser_t* parser);

static int __parse_assignment (
	ase_stx_parser_t* parser, const ase_char_t* target);
static int __parse_basic_expression (
	ase_stx_parser_t* parser, const ase_char_t* ident);
static int __parse_primary (
	ase_stx_parser_t* parser, const ase_char_t* ident, ase_bool_t* is_super);
static int __parse_primary_ident (
	ase_stx_parser_t* parser, const ase_char_t* ident, ase_bool_t* is_super);

static int __parse_block_constructor (ase_stx_parser_t* parser);
static int __parse_message_continuation (
	ase_stx_parser_t* parser, ase_bool_t is_super);
static int __parse_keyword_message (
	ase_stx_parser_t* parser, ase_bool_t is_super);
static int __parse_binary_message (
	ase_stx_parser_t* parser, ase_bool_t is_super);
static int __parse_unary_message (
	ase_stx_parser_t* parser, ase_bool_t is_super);

static int __get_token (ase_stx_parser_t* parser);
static int __get_ident (ase_stx_parser_t* parser);
static int __get_numlit (ase_stx_parser_t* parser, ase_bool_t negated);
static int __get_charlit (ase_stx_parser_t* parser);
static int __get_strlit (ase_stx_parser_t* parser);
static int __get_binary (ase_stx_parser_t* parser);
static int __skip_spaces (ase_stx_parser_t* parser);
static int __skip_comment (ase_stx_parser_t* parser);
static int __get_char (ase_stx_parser_t* parser);
static int __unget_char (ase_stx_parser_t* parser, ase_cint_t c);
static int __open_input (ase_stx_parser_t* parser, void* input);
static int __close_input (ase_stx_parser_t* parser);

ase_stx_parser_t* ase_stx_parser_open (ase_stx_parser_t* parser, ase_stx_t* stx)
{
	if (parser == ASE_NULL) {
		parser = (ase_stx_parser_t*)
			ase_malloc (ase_sizeof(ase_stx_parser_t));		
		if (parser == ASE_NULL) return ASE_NULL;
		parser->__dynamic = ase_true;
	}
	else parser->__dynamic = ase_false;

	if (ase_stx_name_open (&parser->method_name, 0) == ASE_NULL) {
		if (parser->__dynamic) ase_free (parser);
		return ASE_NULL;
	}

	if (ase_stx_token_open (&parser->token, 0) == ASE_NULL) {
		ase_stx_name_close (&parser->method_name);
		if (parser->__dynamic) ase_free (parser);
		return ASE_NULL;
	}

	if (ase_arr_open (
		&parser->bytecode, 256, 
		ase_sizeof(ase_byte_t), ASE_NULL) == ASE_NULL) {
		ase_stx_name_close (&parser->method_name);
		ase_stx_token_close (&parser->token);
		if (parser->__dynamic) ase_free (parser);
		return ASE_NULL;
	}

	parser->stx = stx;
	parser->error_code = ASE_STX_PARSER_ERROR_NONE;

	parser->temporary_count = 0;
	parser->argument_count = 0;
	parser->literal_count = 0;

	parser->curc = ASE_T_EOF;
	parser->ungotc_count = 0;

	parser->input_owner = ASE_NULL;
	parser->input_func = ASE_NULL;
	return parser;
}

void ase_stx_parser_close (ase_stx_parser_t* parser)
{
	while (parser->temporary_count > 0) {
		ase_free (parser->temporaries[--parser->temporary_count]);
	}
	parser->argument_count = 0;

	ase_arr_close (&parser->bytecode);
	ase_stx_name_close (&parser->method_name);
	ase_stx_token_close (&parser->token);

	if (parser->__dynamic) ase_free (parser);
}

#define GET_CHAR(parser) \
	do { if (__get_char(parser) == -1) return -1; } while (0)
#define UNGET_CHAR(parser,c) \
	do { if (__unget_char(parser,c) == -1) return -1; } while (0)
#define GET_TOKEN(parser) \
	do { if (__get_token(parser) == -1) return -1; } while (0)
#define ADD_TOKEN_CHAR(parser,c) \
	do {  \
		if (ase_stx_token_addc (&(parser)->token, c) == -1) { \
			(parser)->error_code = ASE_STX_PARSER_ERROR_MEMORY; \
			return -1; \
		} \
	} while (0)
	
const ase_char_t* ase_stx_parser_error_string (ase_stx_parser_t* parser)
{
	static const ase_char_t* msg[] =
	{
		ASE_T("no error"),

		ASE_T("input fucntion not ready"),
		ASE_T("input function error"),
		ASE_T("out of memory"),

		ASE_T("invalid character"),
		ASE_T("incomplete character literal"),
		ASE_T("incomplete string literal"),
		ASE_T("incomplete literal"),

		ASE_T("message selector"),
		ASE_T("invalid argument name"),
		ASE_T("too many arguments"),

		ASE_T("invalid primitive type"),
		ASE_T("primitive number expected"),
		ASE_T("primitive number out of range"),
		ASE_T("primitive not closed"),

		ASE_T("temporary list not closed"),
		ASE_T("too many temporaries"),
		ASE_T("cannot redefine pseudo variable"),
		ASE_T("invalid primary/expression-start"),

		ASE_T("no period at end of statement"),
		ASE_T("no closing parenthesis"),
		ASE_T("block argument name missing"),
		ASE_T("block argument list not closed"),
		ASE_T("block not closed"),

		ASE_T("undeclared name"),
		ASE_T("too many literals")
	};

	if (parser->error_code >= 0 && 
	    parser->error_code < ase_countof(msg)) return msg[parser->error_code];

	return ASE_T("unknown error");
}

static INLINE ase_bool_t __is_pseudo_variable (const ase_stx_token_t* token)
{
	return token->type == ASE_STX_TOKEN_IDENT &&
		(ase_strcmp(token->name.buffer, ASE_T("self")) == 0 ||
		 ase_strcmp(token->name.buffer, ASE_T("super")) == 0 ||
		 ase_strcmp(token->name.buffer, ASE_T("nil")) == 0 ||
		 ase_strcmp(token->name.buffer, ASE_T("true")) == 0 ||
		 ase_strcmp(token->name.buffer, ASE_T("false")) == 0);
}

static INLINE ase_bool_t __is_vbar_token (const ase_stx_token_t* token)
{
	return 
		token->type == ASE_STX_TOKEN_BINARY &&
		token->name.size == 1 &&
		token->name.buffer[0] == ASE_T('|');
}

static INLINE ase_bool_t __is_primitive_opener (const ase_stx_token_t* token)
{
	return 
		token->type == ASE_STX_TOKEN_BINARY &&
		token->name.size == 1 &&
		token->name.buffer[0] == ASE_T('<');
}

static INLINE ase_bool_t __is_primitive_closer (const ase_stx_token_t* token)
{
	return 
		token->type == ASE_STX_TOKEN_BINARY &&
		token->name.size == 1 &&
		token->name.buffer[0] == ASE_T('>');
}

static INLINE ase_bool_t __is_binary_char (ase_cint_t c)
{
	/*
	 * binaryCharacter ::=
	 * 	'!' | '%' | '&' | '*' | '+' | ',' | 
	 * 	'/' | '<' | '=' | '>' | '?' | '@' | 
	 * 	'\' | '~' | '|' | '-'
	 */

	return
		c == ASE_T('!') || c == ASE_T('%') ||
		c == ASE_T('&') || c == ASE_T('*') ||
		c == ASE_T('+') || c == ASE_T(',') ||
		c == ASE_T('/') || c == ASE_T('<') ||
		c == ASE_T('=') || c == ASE_T('>') ||
		c == ASE_T('?') || c == ASE_T('@') ||
		c == ASE_T('\\') || c == ASE_T('|') ||
		c == ASE_T('~') || c == ASE_T('-');
}

static INLINE ase_bool_t __is_closing_char (ase_cint_t c)
{
	return 
		c == ASE_T('.') || c == ASE_T(']') ||
		c == ASE_T(')') || c == ASE_T(';') ||
		c == ASE_T('\"') || c == ASE_T('\'');
}

#define EMIT_CODE_TEST(parser,high,low) \
	do { if (__emit_code_test(parser,high,low) == -1) return -1; } while (0)

#define EMIT_CODE(parser,code) \
	do { if (__emit_code(parser,code) == -1) return -1; } while(0)

#define EMIT_PUSH_RECEIVER_VARIABLE(parser,pos) \
	do {  \
		if (__emit_stack_positional ( \
			parser, PUSH_RECEIVER_VARIABLE, pos) == -1) return -1; \
	} while (0)

#define EMIT_PUSH_TEMPORARY_LOCATION(parser,pos) \
	do {  \
		if (__emit_stack_positional ( \
			parser, PUSH_TEMPORARY_LOCATION, pos) == -1) return -1; \
	} while (0)

#define EMIT_PUSH_LITERAL_CONSTANT(parser,pos) \
	do { \
		if (__emit_stack_positional ( \
			parser, PUSH_LITERAL_CONSTANT, pos) == -1) return -1; \
	} while (0)


#define EMIT_PUSH_LITERAL_VARIABLE(parser,pos) \
	do { \
		if (__emit_stack_positional ( \
			parser, PUSH_LITERAL_VARIABLE, pos) == -1) return -1; \
	} while (0)

#define EMIT_STORE_RECEIVER_VARIABLE(parser,pos) \
	do { \
		if (__emit_stack_positional ( \
			parser, STORE_RECEIVER_VARIABLE, pos) == -1) return -1; \
	} while (0)	

#define EMIT_STORE_TEMPORARY_LOCATION(parser,pos) \
	do { \
		if (__emit_stack_positional ( \
			parser, STORE_TEMPORARY_LOCATION, pos) == -1) return -1; \
	} while (0)


#define EMIT_POP_STACK_TOP(parser) EMIT_CODE(parser, POP_STACK_TOP)
#define EMIT_DUPLICATE_STACK_TOP(parser) EMIT_CODE(parser, DUPLICATE_STACK_TOP)
#define EMIT_PUSH_ACTIVE_CONTEXT(parser) EMIT_CODE(parser, PUSH_ACTIVE_CONTEXT)
#define EMIT_RETURN_FROM_MESSAGE(parser) EMIT_CODE(parser, RETURN_FROM_MESSAGE)
#define EMIT_RETURN_FROM_BLOCK(parser) EMIT_CODE(parser, RETURN_FROM_BLOCK)

#define EMIT_SEND_TO_SELF(parser,nargs,selector) \
	do { \
		if (__emit_send_to_self(parser,nargs,selector) == -1) return -1; \
	} while (0)

#define EMIT_SEND_TO_SUPER(parser,nargs,selector) \
	do { \
		if (__emit_send_to_super(parser,nargs,selector) == -1) return -1; \
	} while (0)

#define EMIT_DO_PRIMITIVE(parser,no) \
	do { if (__emit_do_primitive(parser,no) == -1) return -1; } while(0)

static INLINE int __emit_code_test (
	ase_stx_parser_t* parser, const ase_char_t* high, const ase_char_t* low)
{
	ase_printf (ASE_T("CODE: %s %s\n"), high, low);
	return 0;
}

static INLINE int __emit_code (ase_stx_parser_t* parser, ase_byte_t code)
{
	if (ase_arr_adddatum(&parser->bytecode, &code) == ASE_NULL) {
		parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
		return -1;
	}

	return 0;
}

static INLINE int __emit_stack_positional (
	ase_stx_parser_t* parser, int opcode, int pos)
{
	ase_assert (pos >= 0x0 && pos <= 0xFF);

	if (pos <= 0x0F) {
		EMIT_CODE (parser, (opcode & 0xF0) | (pos & 0x0F));
	}
	else {
		EMIT_CODE (parser, (opcode >> 4) & 0x6F);
		EMIT_CODE (parser, pos & 0xFF);
	}

	return 0;
}

static INLINE int __emit_send_to_self (
	ase_stx_parser_t* parser, int nargs, int selector)
{
	ase_assert (nargs >= 0x00 && nargs <= 0xFF);
	ase_assert (selector >= 0x00 && selector <= 0xFF);

	if (nargs <= 0x08 && selector <= 0x1F) {
		EMIT_CODE (parser, SEND_TO_SELF);
		EMIT_CODE (parser, (nargs << 5) | selector);
	}
	else {
		EMIT_CODE (parser, SEND_TO_SELF_EXTENDED);
		EMIT_CODE (parser, nargs);
		EMIT_CODE (parser, selector);
	}

	return 0;
}

static INLINE int __emit_send_to_super (
	ase_stx_parser_t* parser, int nargs, int selector)
{
	ase_assert (nargs >= 0x00 && nargs <= 0xFF);
	ase_assert (selector >= 0x00 && selector <= 0xFF);

	if (nargs <= 0x08 && selector <= 0x1F) {
		EMIT_CODE (parser, SEND_TO_SUPER);
		EMIT_CODE (parser, (nargs << 5) | selector);
	}
	else {
		EMIT_CODE (parser, SEND_TO_SUPER_EXTENDED);
		EMIT_CODE (parser, nargs);
		EMIT_CODE (parser, selector);
	}

	return 0;
}

static INLINE int __emit_do_primitive (ase_stx_parser_t* parser, int no)
{
	ase_assert (no >= 0x0 && no <= 0xFFF);

	EMIT_CODE (parser, DO_PRIMITIVE | ((no >> 8) & 0x0F));
	EMIT_CODE (parser, no & 0xFF);

	return 0;
}

static int __add_literal (ase_stx_parser_t* parser, ase_word_t literal)
{
	ase_word_t i;

	for (i = 0; i < parser->literal_count; i++) {
		/* 
		 * it would remove redundancy of symbols and small integers. 
		 * more complex redundacy check may be done somewhere else 
		 * like in __add_string_literal.
		 */
		if (parser->literals[i] == literal) return i;
	}

	if (parser->literal_count >= ase_countof(parser->literals)) {
		parser->error_code = ASE_STX_PARSER_ERROR_TOO_MANY_LITERALS;
		return -1;
	}

	parser->literals[parser->literal_count++] = literal;
	return parser->literal_count - 1;
}

static int __add_character_literal (ase_stx_parser_t* parser, ase_char_t ch)
{
	ase_word_t i, c, literal;
	ase_stx_t* stx = parser->stx;

	for (i = 0; i < parser->literal_count; i++) {
		c = ASE_STX_IS_SMALLINT(parser->literals[i])? 
			stx->class_smallinteger: ASE_STX_CLASS (stx, parser->literals[i]);
		if (c != stx->class_character) continue;

		if (ch == ASE_STX_CHAR_AT(stx,parser->literals[i],0)) return i;
	}

	literal = ase_stx_instantiate (
		stx, stx->class_character, &ch, ASE_NULL, 0);
	return __add_literal (parser, literal);
}

static int __add_string_literal (
	ase_stx_parser_t* parser, const ase_char_t* str, ase_word_t size)
{
	ase_word_t i, c, literal;
	ase_stx_t* stx = parser->stx;

	for (i = 0; i < parser->literal_count; i++) {
		c = ASE_STX_IS_SMALLINT(parser->literals[i])? 
			stx->class_smallinteger: ASE_STX_CLASS (stx, parser->literals[i]);
		if (c != stx->class_string) continue;

		if (ase_strxncmp (str, size, 
			ASE_STX_DATA(stx,parser->literals[i]), 
			ASE_STX_SIZE(stx,parser->literals[i])) == 0) return i;
	}

	literal = ase_stx_instantiate (
		stx, stx->class_string, ASE_NULL, str, size);
	return __add_literal (parser, literal);
}

static int __add_symbol_literal (
	ase_stx_parser_t* parser, const ase_char_t* str, ase_word_t size)
{
	ase_stx_t* stx = parser->stx;
	return __add_literal (parser, ase_stx_new_symbolx(stx, str, size));
}

int ase_stx_parser_parse_method (
	ase_stx_parser_t* parser, ase_word_t method_class, void* input)
{
	int n;

	if (parser->input_func == ASE_NULL) { 
		parser->error_code = ASE_STX_PARSER_ERROR_INPUT_FUNC;
		return -1;
	}

	parser->method_class = method_class;
	if (__open_input(parser, input) == -1) return -1;
	n = __parse_method (parser, method_class, input);
	if (__close_input(parser) == -1) return -1;

	return n;
}

static int __parse_method (
	ase_stx_parser_t* parser, ase_word_t method_class, void* input)
{
	/*
	 * <method definition> ::= 
	 * 	<message pattern> [<temporaries>] [<primitive>] [<statements>]
	 */

	GET_CHAR (parser);
	GET_TOKEN (parser);

	ase_stx_name_clear (&parser->method_name);
	ase_arr_clear (&parser->bytecode);

	while (parser->temporary_count > 0) {
		ase_free (parser->temporaries[--parser->temporary_count]);
	}
	parser->argument_count = 0;
	parser->literal_count = 0;

	if (__parse_message_pattern(parser) == -1) return -1;
	if (__parse_temporaries(parser) == -1) return -1;
	if (__parse_primitive(parser) == -1) return -1;
	if (__parse_statements(parser) == -1) return -1;
	if (__finish_method (parser) == -1) return -1;

	return 0;
}

static int __finish_method (ase_stx_parser_t* parser)
{
	ase_stx_t* stx = parser->stx;
	ase_stx_class_t* class_obj;
	ase_stx_method_t* method_obj;
	ase_word_t method, selector;

	ase_assert (parser->bytecode.size != 0);

	class_obj = (ase_stx_class_t*)
		ASE_STX_OBJECT(stx, parser->method_class);

	if (class_obj->methods == stx->nil) {
		/* TODO: reconfigure method dictionary size */
		class_obj->methods = ase_stx_instantiate (
			stx, stx->class_system_dictionary, 
			ASE_NULL, ASE_NULL, 64);
	}
	ase_assert (class_obj->methods != stx->nil);

	selector = ase_stx_new_symbolx (
		stx, parser->method_name.buffer, parser->method_name.size);

	method = ase_stx_instantiate(stx, stx->class_method, 
		ASE_NULL, parser->literals, parser->literal_count);
	method_obj = (ase_stx_method_t*)ASE_STX_OBJECT(stx, method);

	/* TODO: text saving must be optional */
	/*method_obj->text = ase_stx_instantiate (
		stx, stx->class_string, ASE_NULL, 
		parser->text, ase_strlen(parser->text));
	*/
	method_obj->selector = selector;
	method_obj->bytecodes = ase_stx_instantiate (
		stx, stx->class_bytearray, ASE_NULL, 
		parser->bytecode.buf, parser->bytecode.size);

	/* TODO: better way to store argument count & temporary count */
	method_obj->tmpcount = 
		ASE_STX_TO_SMALLINT(parser->temporary_count - parser->argument_count);
	method_obj->argcount = ASE_STX_TO_SMALLINT(parser->argument_count);

	ase_stx_dict_put (stx, class_obj->methods, selector, method);
	return 0;
}

static int __parse_message_pattern (ase_stx_parser_t* parser)
{
	/* 
	 * <message pattern> ::= 
	 * 	<unary pattern> | <binary pattern> | <keyword pattern>
	 * <unary pattern> ::= unarySelector
	 * <binary pattern> ::= binarySelector <method argument>
	 * <keyword pattern> ::= (keyword  <method argument>)+
	 */
	int n;

	if (parser->token.type == ASE_STX_TOKEN_IDENT) { 
		n = __parse_unary_pattern (parser);
	}
	else if (parser->token.type == ASE_STX_TOKEN_BINARY) { 
		n = __parse_binary_pattern (parser);
	}
	else if (parser->token.type == ASE_STX_TOKEN_KEYWORD) { 
		n = __parse_keyword_pattern (parser);
	}
	else {
		parser->error_code = ASE_STX_PARSER_ERROR_MESSAGE_SELECTOR;
		n = -1;
	}

	parser->temporary_count = parser->argument_count;
	return n;
}

static int __parse_unary_pattern (ase_stx_parser_t* parser)
{
	/* TODO: check if the method name exists */

	if (ase_stx_name_adds(
		&parser->method_name, parser->token.name.buffer) == -1) {
		parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
		return -1;
	}

	GET_TOKEN (parser);
	return 0;
}

static int __parse_binary_pattern (ase_stx_parser_t* parser)
{
	/* TODO: check if the method name exists */

	if (ase_stx_name_adds(
		&parser->method_name, parser->token.name.buffer) == -1) {
		parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
		return -1;
	}

	GET_TOKEN (parser);
	if (parser->token.type != ASE_STX_TOKEN_IDENT) {
		parser->error_code = ASE_STX_PARSER_ERROR_ARGUMENT_NAME;
		return -1;
	}

	if (parser->argument_count >= ase_countof(parser->temporaries)) {
		parser->error_code = ASE_STX_PARSER_ERROR_TOO_MANY_ARGUMENTS;
		return -1;
	}

	/* TODO: check for duplicate entries...in instvars */
	parser->temporaries[parser->argument_count] = 
		ase_stx_token_yield (&parser->token, 0);
	if (parser->temporaries[parser->argument_count] == ASE_NULL) {
		parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
		return -1;
	}
	parser->argument_count++;

	GET_TOKEN (parser);
	return 0;
}

static int __parse_keyword_pattern (ase_stx_parser_t* parser)
{
	do {
		if (ase_stx_name_adds(
			&parser->method_name, parser->token.name.buffer) == -1) {
			parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
			return -1;
		}

		GET_TOKEN (parser);
		if (parser->token.type != ASE_STX_TOKEN_IDENT) {
			parser->error_code = ASE_STX_PARSER_ERROR_ARGUMENT_NAME;
			return -1;
		}

		if (__is_pseudo_variable(&parser->token)) {
			parser->error_code = ASE_STX_PARSER_ERROR_PSEUDO_VARIABLE;
			return -1;
		}

		if (parser->argument_count >= ase_countof(parser->temporaries)) {
			parser->error_code = ASE_STX_PARSER_ERROR_TOO_MANY_ARGUMENTS;
			return -1;
		}

		parser->temporaries[parser->argument_count] = 
			ase_stx_token_yield (&parser->token, 0);
		if (parser->temporaries[parser->argument_count] == ASE_NULL) {
			parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
			return -1;
		}

/* TODO: check for duplicate entries...in instvars/arguments */
		parser->argument_count++;

		GET_TOKEN (parser);
	} while (parser->token.type == ASE_STX_TOKEN_KEYWORD);

	/* TODO: check if the method name exists */
	/* if it exists, collapse arguments */
ase_printf (ASE_T("METHOD NAME ==> [%s]\n"), parser->method_name.buffer);

	return 0;
}

static int __parse_temporaries (ase_stx_parser_t* parser)
{
	/* 
	 * <temporaries> ::= '|' <temporary variable list> '|'
	 * <temporary variable list> ::= identifier*
	 */

	if (!__is_vbar_token(&parser->token)) return 0;

	GET_TOKEN (parser);
	while (parser->token.type == ASE_STX_TOKEN_IDENT) {
		if (parser->temporary_count >= ase_countof(parser->temporaries)) {
			parser->error_code = ASE_STX_PARSER_ERROR_TOO_MANY_TEMPORARIES;
			return -1;
		}

		if (__is_pseudo_variable(&parser->token)) {
			parser->error_code = ASE_STX_PARSER_ERROR_PSEUDO_VARIABLE;
			return -1;
		}

		parser->temporaries[parser->temporary_count] = 
			ase_stx_token_yield (&parser->token, 0);
		if (parser->temporaries[parser->temporary_count] == ASE_NULL) {
			parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
			return -1;
		}

/* TODO: check for duplicate entries...in instvars/arguments/temporaries */
		parser->temporary_count++;

		GET_TOKEN (parser);
	}
	if (!__is_vbar_token(&parser->token)) {
		parser->error_code = ASE_STX_PARSER_ERROR_TEMPORARIES_NOT_CLOSED;
		return -1;
	}

	GET_TOKEN (parser);
	return 0;
}

static int __parse_primitive (ase_stx_parser_t* parser)
{
	/* 
	 * <primitive> ::= '<' 'primitive:' number '>'
	 */

	int prim_no;

	if (!__is_primitive_opener(&parser->token)) return 0;
	GET_TOKEN (parser);

	if (parser->token.type != ASE_STX_TOKEN_KEYWORD ||
	    ase_strcmp (parser->token.name.buffer, ASE_T("primitive:")) != 0) {
		parser->error_code = ASE_STX_PARSER_ERROR_PRIMITIVE_KEYWORD;
		return -1;
	}

	GET_TOKEN (parser); /* TODO: only integer */
	if (parser->token.type != ASE_STX_TOKEN_NUMLIT) {
		parser->error_code = ASE_STX_PARSER_ERROR_PRIMITIVE_NUMBER;
		return -1;
	}

/*TODO: more checks the validity of the primitive number */
	if (!ase_stristype(parser->token.name.buffer, ase_isdigit)) {
		parser->error_code = ASE_STX_PARSER_ERROR_PRIMITIVE_NUMBER;
		return -1;
	}

	ASE_STRTOI (prim_no, parser->token.name.buffer, ASE_NULL, 10);
	if (prim_no < 0 || prim_no > 0xFF) {
		parser->error_code = ASE_STX_PARSER_ERROR_PRIMITIVE_NUMBER_RANGE;
		return -1;
	}

	EMIT_DO_PRIMITIVE (parser, prim_no);

	GET_TOKEN (parser);
	if (!__is_primitive_closer(&parser->token)) {
		parser->error_code = ASE_STX_PARSER_ERROR_PRIMITIVE_NOT_CLOSED;
		return -1;
	}

	GET_TOKEN (parser);
	return 0;
}

static int __parse_statements (ase_stx_parser_t* parser)
{
	/*
	 * <statements> ::= (ORIGINAL->maybe wrong)
	 * 	(<return statement> ['.'] ) |
	 * 	(<expression> ['.' [<statements>]])
	 * <statements> ::= (REVISED->correct?)
	 * 	<statement> ['. [<statements>]]
	 */

	while (parser->token.type != ASE_STX_TOKEN_END) {
		if (__parse_statement (parser) == -1) return -1;

		if (parser->token.type == ASE_STX_TOKEN_PERIOD) {
			GET_TOKEN (parser);
			continue;
		}

		if (parser->token.type != ASE_STX_TOKEN_END) {
			parser->error_code = ASE_STX_PARSER_ERROR_NO_PERIOD;
			return -1;
		}
	}

	EMIT_CODE (parser, RETURN_RECEIVER);
	return 0;
}

static int __parse_block_statements (ase_stx_parser_t* parser)
{
	while (parser->token.type != ASE_STX_TOKEN_RBRACKET && 
	       parser->token.type != ASE_STX_TOKEN_END) {

		if (__parse_statement(parser) == -1) return -1;
		if (parser->token.type != ASE_STX_TOKEN_PERIOD) break;
		GET_TOKEN (parser);
	}

	return 0;
}

static int __parse_statement (ase_stx_parser_t* parser)
{
	/* 
	 * <statement> ::= <return statement> | <expression>
	 * <return statement> ::= returnOperator <expression> 
	 * returnOperator ::= '^'
	 */

	if (parser->token.type == ASE_STX_TOKEN_RETURN) {
		GET_TOKEN (parser);
		if (__parse_expression(parser) == -1) return -1;
		EMIT_RETURN_FROM_MESSAGE (parser);
	}
	else {
		if (__parse_expression(parser) == -1) return -1;
	}

	return 0;
}

static int __parse_expression (ase_stx_parser_t* parser)
{
	/*
	 * <expression> ::= <assignment> | <basic expression>
	 * <assignment> ::= <assignment target> assignmentOperator <expression>
	 * <basic expression> ::= <primary> [<messages> <cascaded messages>]
	 * <assignment target> ::= identifier
	 * assignmentOperator ::=  ':='
	 */
	ase_stx_t* stx = parser->stx;

	if (parser->token.type == ASE_STX_TOKEN_IDENT) {
		ase_char_t* ident = ase_stx_token_yield (&parser->token, 0);
		if (ident == ASE_NULL) {
			parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
			return -1;
		}

		GET_TOKEN (parser);
		if (parser->token.type == ASE_STX_TOKEN_ASSIGN) {
			GET_TOKEN (parser);
			if (__parse_assignment(parser, ident) == -1) {
				ase_free (ident);
				return -1;
			}
		}
		else {
			if (__parse_basic_expression(parser, ident) == -1) {
				ase_free (ident);
				return -1;
			}
		}

		ase_free (ident);
	}
	else {
		if (__parse_basic_expression(parser, ASE_NULL) == -1) return -1;
	}

	return 0;
}

static int __parse_basic_expression (
	ase_stx_parser_t* parser, const ase_char_t* ident)
{
	/*
	 * <basic expression> ::= <primary> [<messages> <cascaded messages>]
	 */
	ase_bool_t is_super;

	if (__parse_primary(parser, ident, &is_super) == -1) return -1;
	if (parser->token.type != ASE_STX_TOKEN_END &&
	    parser->token.type != ASE_STX_TOKEN_PERIOD) {
		if (__parse_message_continuation(parser, is_super) == -1) return -1;
	}
	return 0;
}

static int __parse_assignment (
	ase_stx_parser_t* parser, const ase_char_t* target)
{
	/*
	 * <assignment> ::= <assignment target> assignmentOperator <expression>
	 */

	ase_word_t i;
	ase_stx_t* stx = parser->stx;

	for (i = parser->argument_count; i < parser->temporary_count; i++) {
		if (ase_strcmp (target, parser->temporaries[i]) == 0) {
			if (__parse_expression(parser) == -1) return -1;
			EMIT_STORE_TEMPORARY_LOCATION (parser, i);
			return 0;
		}
	}

	if (ase_stx_get_instance_variable_index (
		stx, parser->method_class, target, &i) == 0) {
		if (__parse_expression(parser) == -1) return -1;
		EMIT_STORE_RECEIVER_VARIABLE (parser, i);
		return 0;
	}

	if (ase_stx_lookup_class_variable (
		stx, parser->method_class, target) != stx->nil) {
		if (__parse_expression(parser) == -1) return -1;

		/* TODO */
		EMIT_CODE_TEST (parser, ASE_T("ASSIGN_CLASSVAR #"), target);
		//EMIT_STORE_CLASS_VARIABLE (parser, target);
		return 0;
	}

	/* TODO: IMPLEMENT POOL DICTIONARIES */

	/* TODO: IMPLEMENT GLOBLAS, but i don't like this idea */

	parser->error_code = ASE_STX_PARSER_ERROR_UNDECLARED_NAME;
	return -1;
}

static int __parse_primary (
	ase_stx_parser_t* parser, const ase_char_t* ident, ase_bool_t* is_super)
{
	/*
	 * <primary> ::=
	 * 	identifier | <literal> | 
	 * 	<block constructor> | ( '('<expression>')' )
	 */

	ase_stx_t* stx = parser->stx;

	if (ident == ASE_NULL) {
		int pos;
		ase_word_t literal;

		*is_super = ase_false;

		if (parser->token.type == ASE_STX_TOKEN_IDENT) {
			if (__parse_primary_ident(parser, 
				parser->token.name.buffer, is_super) == -1) return -1;
			GET_TOKEN (parser);
		}
		else if (parser->token.type == ASE_STX_TOKEN_CHARLIT) {
			pos = __add_character_literal(
				parser, parser->token.name.buffer[0]);
			if (pos == -1) return -1;
			EMIT_PUSH_LITERAL_CONSTANT (parser, pos);
			GET_TOKEN (parser);
		}
		else if (parser->token.type == ASE_STX_TOKEN_STRLIT) {
			pos = __add_string_literal (parser,
				parser->token.name.buffer, parser->token.name.size);
			if (pos == -1) return -1;
			EMIT_PUSH_LITERAL_CONSTANT (parser, pos);
			GET_TOKEN (parser);
		}
		else if (parser->token.type == ASE_STX_TOKEN_NUMLIT) {
			/* TODO: other types of numbers, negative numbers, etc */
			ase_word_t tmp;
			ASE_STRTOI (tmp, parser->token.name.buffer, ASE_NULL, 10);
			literal = ASE_STX_TO_SMALLINT(tmp);
			pos = __add_literal(parser, literal);
			if (pos == -1) return -1;
			EMIT_PUSH_LITERAL_CONSTANT (parser, pos);
			GET_TOKEN (parser);
		}
		else if (parser->token.type == ASE_STX_TOKEN_SYMLIT) {
			pos = __add_symbol_literal (parser,
				parser->token.name.buffer, parser->token.name.size);
			if (pos == -1) return -1;
			EMIT_PUSH_LITERAL_CONSTANT (parser, pos);
			GET_TOKEN (parser);
		}
		else if (parser->token.type == ASE_STX_TOKEN_LBRACKET) {
			GET_TOKEN (parser);
			if (__parse_block_constructor(parser) == -1) return -1;
		}
		else if (parser->token.type == ASE_STX_TOKEN_APAREN) {
			/* TODO: array literal */
		}
		else if (parser->token.type == ASE_STX_TOKEN_LPAREN) {
			GET_TOKEN (parser);
			if (__parse_expression(parser) == -1) return -1;
			if (parser->token.type != ASE_STX_TOKEN_RPAREN) {
				parser->error_code = ASE_STX_PARSER_ERROR_NO_RPAREN;
				return -1;
			}
			GET_TOKEN (parser);
		}
		else {
			parser->error_code = ASE_STX_PARSER_ERROR_PRIMARY;
			return -1;
		}
	}
	else {
		/*if (__parse_primary_ident(parser, parser->token.name.buffer) == -1) return -1;*/
		if (__parse_primary_ident(parser, ident, is_super) == -1) return -1;
	}

	return 0;
}

static int __parse_primary_ident (
	ase_stx_parser_t* parser, const ase_char_t* ident, ase_bool_t* is_super)
{
	ase_word_t i;
	ase_stx_t* stx = parser->stx;

	*is_super = ase_false;

	if (ase_strcmp(ident, ASE_T("self")) == 0) {
		EMIT_CODE (parser, PUSH_RECEIVER);
		return 0;
	}
	else if (ase_strcmp(ident, ASE_T("super")) == 0) {
		*is_super = ase_true;
		EMIT_CODE (parser, PUSH_RECEIVER);
		return 0;
	}
	else if (ase_strcmp(ident, ASE_T("nil")) == 0) {
		EMIT_CODE (parser, PUSH_NIL);
		return 0;
	}
	else if (ase_strcmp(ident, ASE_T("true")) == 0) {
		EMIT_CODE (parser, PUSH_TRUE);
		return 0;
	}
	else if (ase_strcmp(ident, ASE_T("false")) == 0) {
		EMIT_CODE (parser, PUSH_FALSE);
		return 0;
	}

	/* Refer to __parse_assignment for identifier lookup */

	for (i = 0; i < parser->temporary_count; i++) {
		if (ase_strcmp(ident, parser->temporaries[i]) == 0) {
			EMIT_PUSH_TEMPORARY_LOCATION (parser, i);
			return 0;
		}
	}

	if (ase_stx_get_instance_variable_index (
		stx, parser->method_class, ident, &i) == 0) {
		EMIT_PUSH_RECEIVER_VARIABLE (parser, i);
		return 0;
	}	

	/* TODO: what is the best way to look up a class variable? */
	/* 1. Use the class containing it and using its position */
	/* 2. Use a primitive method after pushing the name as a symbol */
	/* 3. Implement a vm instruction to do it */
/*
	if (ase_stx_lookup_class_variable (
		stx, parser->method_class, ident) != stx->nil) {
		//EMIT_LOOKUP_CLASS_VARIABLE (parser, ident);
		return 0;
	}
*/

	/* TODO: IMPLEMENT POOL DICTIONARIES */

	/* TODO: IMPLEMENT GLOBLAS, but i don't like this idea */

	parser->error_code = ASE_STX_PARSER_ERROR_UNDECLARED_NAME;
	return -1;
}

static int __parse_block_constructor (ase_stx_parser_t* parser)
{
	/*
	 * <block constructor> ::= '[' <block body> ']'
	 * <block body> ::= [<block argument>* '|']
	 * 	[<temporaries>] [<statements>]
	 * <block argument> ::= ':'  identifier
	 */

	if (parser->token.type == ASE_STX_TOKEN_COLON) {
		do {
			GET_TOKEN (parser);

			if (parser->token.type != ASE_STX_TOKEN_IDENT) {
				parser->error_code = ASE_STX_PARSER_ERROR_BLOCK_ARGUMENT_NAME;
				return -1;
			}

			/* TODO : store block arguments */
			GET_TOKEN (parser);
		} while (parser->token.type == ASE_STX_TOKEN_COLON);
			
		if (!__is_vbar_token(&parser->token)) {
			parser->error_code = ASE_STX_PARSER_ERROR_BLOCK_ARGUMENT_LIST;
			return -1;
		}

		GET_TOKEN (parser);
	}

	/* TODO: create a block closure */
	if (__parse_temporaries(parser) == -1) return -1;
	if (__parse_block_statements(parser) == -1) return -1;

	if (parser->token.type != ASE_STX_TOKEN_RBRACKET) {
		parser->error_code = ASE_STX_PARSER_ERROR_BLOCK_NOT_CLOSED;
		return -1;
	}

	GET_TOKEN (parser);

	/* TODO: do special treatment for block closures */

	return 0;
}

static int __parse_message_continuation (
	ase_stx_parser_t* parser, ase_bool_t is_super)
{
	/*
	 * <messages> ::=
	 * 	(<unary message>+ <binary message>* [<keyword message>] ) |
	 * 	(<binary message>+ [<keyword message>] ) |
	 * 	<keyword message>
	 * <cascaded messages> ::= (';' <messages>)*
	 */
	if (__parse_keyword_message(parser, is_super) == -1) return -1;

	while (parser->token.type == ASE_STX_TOKEN_SEMICOLON) {
		EMIT_CODE_TEST (parser, ASE_T("DoSpecial(DUP_RECEIVER(CASCADE))"), ASE_T(""));
		GET_TOKEN (parser);

		if (__parse_keyword_message(parser, ase_false) == -1) return -1;
		EMIT_CODE_TEST (parser, ASE_T("DoSpecial(POP_TOP)"), ASE_T(""));
	}

	return 0;
}

static int __parse_keyword_message (ase_stx_parser_t* parser, ase_bool_t is_super)
{
	/*
	 * <keyword message> ::= (keyword <keyword argument> )+
	 * <keyword argument> ::= <primary> <unary message>* <binary message>*
	 */

	ase_stx_name_t name;
	ase_word_t pos;
	ase_bool_t is_super2;
	int nargs = 0, n;

	if (__parse_binary_message (parser, is_super) == -1) return -1;
	if (parser->token.type != ASE_STX_TOKEN_KEYWORD) return 0;

	if (ase_stx_name_open(&name, 0) == ASE_NULL) {
		parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
		return -1;
	}
	
	do {
		if (ase_stx_name_adds(&name, parser->token.name.buffer) == -1) {
			parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
			ase_stx_name_close (&name);
			return -1;
		}

		GET_TOKEN (parser);
		if (__parse_primary(parser, ASE_NULL, &is_super2) == -1) {
			ase_stx_name_close (&name);
			return -1;
		}

		if (__parse_binary_message(parser, is_super2) == -1) {
			ase_stx_name_close (&name);
			return -1;
		}

		nargs++;
		/* TODO: check if it has too many arguments.. */
	} while (parser->token.type == ASE_STX_TOKEN_KEYWORD);

	pos = __add_symbol_literal (parser, name.buffer, name.size);
	if (pos == -1) {
		ase_stx_name_close (&name);
		return -1;
	}

	n = (is_super)?
		__emit_send_to_super(parser,nargs,pos):
		__emit_send_to_self(parser,nargs,pos);
	if (n == -1) {
		ase_stx_name_close (&name);
		return -1;
	}

	ase_stx_name_close (&name);
	return 0;
}

static int __parse_binary_message (ase_stx_parser_t* parser, ase_bool_t is_super)
{
	/*
	 * <binary message> ::= binarySelector <binary argument>
	 * <binary argument> ::= <primary> <unary message>*
	 */
	ase_word_t pos;
	ase_bool_t is_super2;
	int n;

	if (__parse_unary_message (parser, is_super) == -1) return -1;

	while (parser->token.type == ASE_STX_TOKEN_BINARY) {
		ase_char_t* op = ase_stx_token_yield (&parser->token, 0);
		if (op == ASE_NULL) {
			parser->error_code = ASE_STX_PARSER_ERROR_MEMORY;
			return -1;
		}

		GET_TOKEN (parser);
		if (__parse_primary(parser, ASE_NULL, &is_super2) == -1) {
			ase_free (op);
			return -1;
		}

		if (__parse_unary_message(parser, is_super2) == -1) {
			ase_free (op);
			return -1;
		}

		pos = __add_symbol_literal (parser, op, ase_strlen(op));
		if (pos == -1) {
			ase_free (op);
			return -1;
		}

		n = (is_super)?
			__emit_send_to_super(parser,2,pos):
			__emit_send_to_self(parser,2,pos);
		if (n == -1) {
			ase_free (op);
			return -1;
		}

		ase_free (op);
	}

	return 0;
}

static int __parse_unary_message (ase_stx_parser_t* parser, ase_bool_t is_super)
{
	/* <unary message> ::= unarySelector */

	ase_word_t pos;
	int n;

	while (parser->token.type == ASE_STX_TOKEN_IDENT) {
		pos = __add_symbol_literal (parser,
			parser->token.name.buffer, parser->token.name.size);
		if (pos == -1) return -1;

		n = (is_super)?
			__emit_send_to_super(parser,0,pos):
			__emit_send_to_self(parser,0,pos);
		if (n == -1) return -1;

		GET_TOKEN (parser);
	}

	return 0;
}

static int __get_token (ase_stx_parser_t* parser)
{
	ase_cint_t c;

	do {
		if (__skip_spaces(parser) == -1) return -1;
		if (parser->curc == ASE_T('"')) {
			GET_CHAR (parser);
			if (__skip_comment(parser) == -1) return -1;
		}
		else break;
	} while (1);

	c = parser->curc;
	ase_stx_token_clear (&parser->token);

	if (c == ASE_T_EOF) {
		parser->token.type = ASE_STX_TOKEN_END;
	}
	else if (ase_isalpha(c)) {
		if (__get_ident(parser) == -1) return -1;
	}
	else if (ase_isdigit(c)) {
		if (__get_numlit(parser, ase_false) == -1) return -1;
	}
	else if (c == ASE_T('$')) {
		GET_CHAR (parser);
		if (__get_charlit(parser) == -1) return -1;
	}
	else if (c == ASE_T('\'')) {
		GET_CHAR (parser);
		if (__get_strlit(parser) == -1) return -1;
	}
	else if (c == ASE_T(':')) {
		parser->token.type = ASE_STX_TOKEN_COLON;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);

		c = parser->curc;
		if (c == ASE_T('=')) {
			parser->token.type = ASE_STX_TOKEN_ASSIGN;
			ADD_TOKEN_CHAR(parser, c);
			GET_CHAR (parser);
		}
	}
	else if (c == ASE_T('^')) {
		parser->token.type = ASE_STX_TOKEN_RETURN;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == ASE_T('[')) {
		parser->token.type = ASE_STX_TOKEN_LBRACKET;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == ASE_T(']')) {
		parser->token.type = ASE_STX_TOKEN_RBRACKET;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == ASE_T('(')) {
		parser->token.type = ASE_STX_TOKEN_LPAREN;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == ASE_T(')')) {
		parser->token.type = ASE_STX_TOKEN_RPAREN;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == ASE_T('#')) {
		/*ADD_TOKEN_CHAR(parser, c);*/
		GET_CHAR (parser);

		c = parser->curc;
		if (c == ASE_T_EOF) {
			parser->error_code = ASE_STX_PARSER_ERROR_LITERAL;
			return -1;
		}
		else if (c == ASE_T('(')) {
			ADD_TOKEN_CHAR(parser, c);
			parser->token.type = ASE_STX_TOKEN_APAREN;
			GET_CHAR (parser);
		}
		else if (c == ASE_T('\'')) {
			GET_CHAR (parser);
			if (__get_strlit(parser) == -1) return -1;
			parser->token.type = ASE_STX_TOKEN_SYMLIT;
		}
		else if (!__is_closing_char(c) && !ase_isspace(c)) {
			do {
				ADD_TOKEN_CHAR(parser, c);
				GET_CHAR (parser);
				c = parser->curc;
			} while (!__is_closing_char(c) && !ase_isspace(c));

			parser->token.type = ASE_STX_TOKEN_SYMLIT;
		}
		else {
			parser->error_code = ASE_STX_PARSER_ERROR_LITERAL;
			return -1;
		}
	}
	else if (c == ASE_T('.')) {
		parser->token.type = ASE_STX_TOKEN_PERIOD;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == ASE_T(';')) {
		parser->token.type = ASE_STX_TOKEN_SEMICOLON;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (__is_binary_char(c)) {
		if (__get_binary(parser) == -1) return -1;
	}
	else {
		parser->error_code = ASE_STX_PARSER_ERROR_CHAR;
		return -1;
	}

//ase_printf (ASE_T("TOKEN: %s\n"), parser->token.name.buffer);
	return 0;
}

static int __get_ident (ase_stx_parser_t* parser)
{
	/*
	 * identifier ::= letter (letter | digit)*
	 * keyword ::= identifier ':'
	 */

	ase_cint_t c = parser->curc;
	parser->token.type = ASE_STX_TOKEN_IDENT;

	do {
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
		c = parser->curc;
	} while (ase_isalnum(c));

	if (c == ASE_T(':')) {
		ADD_TOKEN_CHAR (parser, c);
		parser->token.type = ASE_STX_TOKEN_KEYWORD;
		GET_CHAR (parser);
	}

	return 0;
}

static int __get_numlit (ase_stx_parser_t* parser, ase_bool_t negated)
{
	/* 
	 * <number literal> ::= ['-'] <number>
	 * <number> ::= integer | float | scaledDecimal
	 * integer ::= decimalInteger  | radixInteger
	 * decimalInteger ::= digits
	 * digits ::= digit+
	 * radixInteger ::= radixSpecifier  'r' radixDigits
	 * radixSpecifier := digits
	 * radixDigits ::= (digit | uppercaseAlphabetic)+
	 * float ::=  mantissa [exponentLetter exponent]
	 * mantissa ::= digits'.' digits
	 * exponent ::= ['-']decimalInteger
	 * exponentLetter ::= 'e' | 'd' | 'q'
	 * scaledDecimal ::= scaledMantissa 's' [fractionalDigits]
	 * scaledMantissa ::= decimalInteger | mantissa
	 * fractionalDigits ::= decimalInteger
	 */

	ase_cint_t c = parser->curc;
	parser->token.type = ASE_STX_TOKEN_NUMLIT;

	do {
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
		c = parser->curc;
	} while (ase_isalnum(c));

	/* TODO; more */
	return 0;
}

static int __get_charlit (ase_stx_parser_t* parser)
{
	/* 
	 * character_literal ::= '$' character
	 * character ::= "Any character in the implementation-defined character set"
	 */

	ase_cint_t c = parser->curc; /* even a new-line or white space would be taken */
	if (c == ASE_T_EOF) {
		parser->error_code = ASE_STX_PARSER_ERROR_CHARLIT;
		return -1;
	}	

	parser->token.type = ASE_STX_TOKEN_CHARLIT;
	ADD_TOKEN_CHAR(parser, c);
	GET_CHAR (parser);
	return 0;
}

static int __get_strlit (ase_stx_parser_t* parser)
{
	/* 
	 * string_literal ::= stringDelimiter stringBody stringDelimiter
	 * stringBody ::= (nonStringDelimiter | (stringDelimiter stringDelimiter)*)
	 * stringDelimiter ::= '''    "a single quote"
	 */

	/* TODO: C-like string */

	ase_cint_t c = parser->curc;
	parser->token.type = ASE_STX_TOKEN_STRLIT;

	do {
		do {
			ADD_TOKEN_CHAR (parser, c);
			GET_CHAR (parser);
			c = parser->curc;

			if (c == ASE_T_EOF) {
				parser->error_code = ASE_STX_PARSER_ERROR_STRLIT;
				return -1;
			}
		} while (c != ASE_T('\''));

		GET_CHAR (parser);
		c = parser->curc;
	} while (c == ASE_T('\''));

	return 0;
}

static int __get_binary (ase_stx_parser_t* parser)
{
	/* 
	 * binarySelector ::= binaryCharacter+
	 */

	ase_cint_t c = parser->curc;
	ADD_TOKEN_CHAR (parser, c);

	if (c == ASE_T('-')) {
		GET_CHAR (parser);
		c = parser->curc;
		if (ase_isdigit(c)) return __get_numlit(parser,ase_true);
	}
	else {
		GET_CHAR (parser);
		c = parser->curc;
	}

	/* up to 2 characters only */
	if (__is_binary_char(c)) {
		ADD_TOKEN_CHAR (parser, c);
		GET_CHAR (parser);
		c = parser->curc;
	}

	/* or up to any occurrences */
	/*
	while (__is_binary_char(c)) {
		ADD_TOKEN_CHAR (parser, c);
		GET_CHAR (parser);
		c = parser->curc;
	}
	*/

	parser->token.type = ASE_STX_TOKEN_BINARY;
	return 0;
}

static int __skip_spaces (ase_stx_parser_t* parser)
{
	while (ase_isspace(parser->curc)) GET_CHAR (parser);
	return 0;
}

static int __skip_comment (ase_stx_parser_t* parser)
{
	while (parser->curc != ASE_T('"')) GET_CHAR (parser);
	GET_CHAR (parser);
	return 0;
}

static int __get_char (ase_stx_parser_t* parser)
{
	ase_cint_t c;

	if (parser->ungotc_count > 0) {
		parser->curc = parser->ungotc[parser->ungotc_count--];
	}
	else {
		if (parser->input_func (
			ASE_STX_PARSER_INPUT_CONSUME, 
			parser->input_owner, (void*)&c) == -1) {
			parser->error_code = ASE_STX_PARSER_ERROR_INPUT;
			return -1;
		}
		parser->curc = c;
	}
	return 0;
}

static int __unget_char (ase_stx_parser_t* parser, ase_cint_t c)
{
	if (parser->ungotc_count >= ase_countof(parser->ungotc)) return -1;
	parser->ungotc[parser->ungotc_count++] = c;
	return 0;
}

static int __open_input (ase_stx_parser_t* parser, void* input)
{
	if (parser->input_func(
		ASE_STX_PARSER_INPUT_OPEN, 
		(void*)&parser->input_owner, input) == -1) {
		parser->error_code = ASE_STX_PARSER_ERROR_INPUT;
		return -1;
	}

	parser->error_code = ASE_STX_PARSER_ERROR_NONE;
	parser->curc = ASE_T_EOF;
	parser->ungotc_count = 0;
	return 0;
}

static int __close_input (ase_stx_parser_t* parser)
{
	if (parser->input_func(
		ASE_STX_PARSER_INPUT_CLOSE, 
		parser->input_owner, ASE_NULL) == -1) {
		parser->error_code = ASE_STX_PARSER_ERROR_INPUT;
		return -1;
	}

	return 0;
}


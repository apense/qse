/*
 * $Id: parser.c,v 1.80 2006-01-30 16:44:03 bacon Exp $
 */

#include <xp/stx/parser.h>
#include <xp/stx/object.h>
#include <xp/stx/class.h>
#include <xp/stx/method.h>
#include <xp/stx/symbol.h>
#include <xp/stx/bytecode.h>
#include <xp/stx/dict.h>
#include <xp/stx/misc.h>

static int __parse_method (
	xp_stx_parser_t* parser, 
	xp_word_t method_class, void* input);
static int __finish_method (xp_stx_parser_t* parser);

static int __parse_message_pattern (xp_stx_parser_t* parser);
static int __parse_unary_pattern (xp_stx_parser_t* parser);
static int __parse_binary_pattern (xp_stx_parser_t* parser);
static int __parse_keyword_pattern (xp_stx_parser_t* parser);

static int __parse_temporaries (xp_stx_parser_t* parser);
static int __parse_primitive (xp_stx_parser_t* parser);
static int __parse_statements (xp_stx_parser_t* parser);
static int __parse_block_statements (xp_stx_parser_t* parser);
static int __parse_statement (xp_stx_parser_t* parser);
static int __parse_expression (xp_stx_parser_t* parser);

static int __parse_assignment (
	xp_stx_parser_t* parser, const xp_char_t* target);
static int __parse_basic_expression (
	xp_stx_parser_t* parser, const xp_char_t* ident);
static int __parse_primary (
	xp_stx_parser_t* parser, const xp_char_t* ident, xp_bool_t* is_super);
static int __parse_primary_ident (
	xp_stx_parser_t* parser, const xp_char_t* ident, xp_bool_t* is_super);

static int __parse_block_constructor (xp_stx_parser_t* parser);
static int __parse_message_continuation (
	xp_stx_parser_t* parser, xp_bool_t is_super);
static int __parse_keyword_message (
	xp_stx_parser_t* parser, xp_bool_t is_super);
static int __parse_binary_message (
	xp_stx_parser_t* parser, xp_bool_t is_super);
static int __parse_unary_message (
	xp_stx_parser_t* parser, xp_bool_t is_super);

static int __get_token (xp_stx_parser_t* parser);
static int __get_ident (xp_stx_parser_t* parser);
static int __get_numlit (xp_stx_parser_t* parser, xp_bool_t negated);
static int __get_charlit (xp_stx_parser_t* parser);
static int __get_strlit (xp_stx_parser_t* parser);
static int __get_binary (xp_stx_parser_t* parser);
static int __skip_spaces (xp_stx_parser_t* parser);
static int __skip_comment (xp_stx_parser_t* parser);
static int __get_char (xp_stx_parser_t* parser);
static int __unget_char (xp_stx_parser_t* parser, xp_cint_t c);
static int __open_input (xp_stx_parser_t* parser, void* input);
static int __close_input (xp_stx_parser_t* parser);

xp_stx_parser_t* xp_stx_parser_open (xp_stx_parser_t* parser, xp_stx_t* stx)
{
	if (parser == XP_NULL) {
		parser = (xp_stx_parser_t*)
			xp_malloc (xp_sizeof(xp_stx_parser_t));		
		if (parser == XP_NULL) return XP_NULL;
		parser->__dynamic = xp_true;
	}
	else parser->__dynamic = xp_false;

	if (xp_stx_name_open (&parser->method_name, 0) == XP_NULL) {
		if (parser->__dynamic) xp_free (parser);
		return XP_NULL;
	}

	if (xp_stx_token_open (&parser->token, 0) == XP_NULL) {
		xp_stx_name_close (&parser->method_name);
		if (parser->__dynamic) xp_free (parser);
		return XP_NULL;
	}

	if (xp_arr_open (
		&parser->bytecode, 256, 
		xp_sizeof(xp_byte_t), XP_NULL) == XP_NULL) {
		xp_stx_name_close (&parser->method_name);
		xp_stx_token_close (&parser->token);
		if (parser->__dynamic) xp_free (parser);
		return XP_NULL;
	}

	parser->stx = stx;
	parser->error_code = XP_STX_PARSER_ERROR_NONE;

	parser->temporary_count = 0;
	parser->argument_count = 0;
	parser->literal_count = 0;

	parser->curc = XP_CHAR_EOF;
	parser->ungotc_count = 0;

	parser->input_owner = XP_NULL;
	parser->input_func = XP_NULL;
	return parser;
}

void xp_stx_parser_close (xp_stx_parser_t* parser)
{
	while (parser->temporary_count > 0) {
		xp_free (parser->temporaries[--parser->temporary_count]);
	}
	parser->argument_count = 0;

	xp_arr_close (&parser->bytecode);
	xp_stx_name_close (&parser->method_name);
	xp_stx_token_close (&parser->token);

	if (parser->__dynamic) xp_free (parser);
}

#define GET_CHAR(parser) \
	do { if (__get_char(parser) == -1) return -1; } while (0)
#define UNGET_CHAR(parser,c) \
	do { if (__unget_char(parser,c) == -1) return -1; } while (0)
#define GET_TOKEN(parser) \
	do { if (__get_token(parser) == -1) return -1; } while (0)
#define ADD_TOKEN_CHAR(parser,c) \
	do {  \
		if (xp_stx_token_addc (&(parser)->token, c) == -1) { \
			(parser)->error_code = XP_STX_PARSER_ERROR_MEMORY; \
			return -1; \
		} \
	} while (0)
	
const xp_char_t* xp_stx_parser_error_string (xp_stx_parser_t* parser)
{
	static const xp_char_t* msg[] =
	{
		XP_TEXT("no error"),

		XP_TEXT("input fucntion not ready"),
		XP_TEXT("input function error"),
		XP_TEXT("out of memory"),

		XP_TEXT("invalid character"),
		XP_TEXT("incomplete character literal"),
		XP_TEXT("incomplete string literal"),
		XP_TEXT("incomplete literal"),

		XP_TEXT("message selector"),
		XP_TEXT("invalid argument name"),
		XP_TEXT("too many arguments"),

		XP_TEXT("invalid primitive type"),
		XP_TEXT("primitive number expected"),
		XP_TEXT("primitive number out of range"),
		XP_TEXT("primitive not closed"),

		XP_TEXT("temporary list not closed"),
		XP_TEXT("too many temporaries"),
		XP_TEXT("cannot redefine pseudo variable"),
		XP_TEXT("invalid primary/expression-start"),

		XP_TEXT("no period at end of statement"),
		XP_TEXT("no closing parenthesis"),
		XP_TEXT("block argument name missing"),
		XP_TEXT("block argument list not closed"),
		XP_TEXT("block not closed"),

		XP_TEXT("undeclared name"),
		XP_TEXT("too many literals")
	};

	if (parser->error_code >= 0 && 
	    parser->error_code < xp_countof(msg)) return msg[parser->error_code];

	return XP_TEXT("unknown error");
}

static INLINE xp_bool_t __is_pseudo_variable (const xp_stx_token_t* token)
{
	return token->type == XP_STX_TOKEN_IDENT &&
		(xp_strcmp(token->name.buffer, XP_TEXT("self")) == 0 ||
		 xp_strcmp(token->name.buffer, XP_TEXT("super")) == 0 ||
		 xp_strcmp(token->name.buffer, XP_TEXT("nil")) == 0 ||
		 xp_strcmp(token->name.buffer, XP_TEXT("true")) == 0 ||
		 xp_strcmp(token->name.buffer, XP_TEXT("false")) == 0);
}

static INLINE xp_bool_t __is_vbar_token (const xp_stx_token_t* token)
{
	return 
		token->type == XP_STX_TOKEN_BINARY &&
		token->name.size == 1 &&
		token->name.buffer[0] == XP_CHAR('|');
}

static INLINE xp_bool_t __is_primitive_opener (const xp_stx_token_t* token)
{
	return 
		token->type == XP_STX_TOKEN_BINARY &&
		token->name.size == 1 &&
		token->name.buffer[0] == XP_CHAR('<');
}

static INLINE xp_bool_t __is_primitive_closer (const xp_stx_token_t* token)
{
	return 
		token->type == XP_STX_TOKEN_BINARY &&
		token->name.size == 1 &&
		token->name.buffer[0] == XP_CHAR('>');
}

static INLINE xp_bool_t __is_binary_char (xp_cint_t c)
{
	/*
	 * binaryCharacter ::=
	 * 	'!' | '%' | '&' | '*' | '+' | ',' | 
	 * 	'/' | '<' | '=' | '>' | '?' | '@' | 
	 * 	'\' | '~' | '|' | '-'
	 */

	return
		c == XP_CHAR('!') || c == XP_CHAR('%') ||
		c == XP_CHAR('&') || c == XP_CHAR('*') ||
		c == XP_CHAR('+') || c == XP_CHAR(',') ||
		c == XP_CHAR('/') || c == XP_CHAR('<') ||
		c == XP_CHAR('=') || c == XP_CHAR('>') ||
		c == XP_CHAR('?') || c == XP_CHAR('@') ||
		c == XP_CHAR('\\') || c == XP_CHAR('|') ||
		c == XP_CHAR('~') || c == XP_CHAR('-');
}

static INLINE xp_bool_t __is_closing_char (xp_cint_t c)
{
	return 
		c == XP_CHAR('.') || c == XP_CHAR(']') ||
		c == XP_CHAR(')') || c == XP_CHAR(';') ||
		c == XP_CHAR('\"') || c == XP_CHAR('\'');
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
	xp_stx_parser_t* parser, const xp_char_t* high, const xp_char_t* low)
{
	xp_printf (XP_TEXT("CODE: %s %s\n"), high, low);
	return 0;
}

static INLINE int __emit_code (xp_stx_parser_t* parser, xp_byte_t code)
{
	if (xp_arr_adddatum(&parser->bytecode, &code) == XP_NULL) {
		parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
		return -1;
	}

	return 0;
}

static INLINE int __emit_stack_positional (
	xp_stx_parser_t* parser, int opcode, int pos)
{
	xp_assert (pos >= 0x0 && pos <= 0xFF);

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
	xp_stx_parser_t* parser, int nargs, int selector)
{
	xp_assert (nargs >= 0x00 && nargs <= 0xFF);
	xp_assert (selector >= 0x00 && selector <= 0xFF);

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
	xp_stx_parser_t* parser, int nargs, int selector)
{
	xp_assert (nargs >= 0x00 && nargs <= 0xFF);
	xp_assert (selector >= 0x00 && selector <= 0xFF);

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

static INLINE int __emit_do_primitive (xp_stx_parser_t* parser, int no)
{
	xp_assert (no >= 0x0 && no <= 0xFFF);

	EMIT_CODE (parser, DO_PRIMITIVE | ((no >> 8) & 0x0F));
	EMIT_CODE (parser, no & 0xFF);

	return 0;
}

static int __add_literal (xp_stx_parser_t* parser, xp_word_t literal)
{
	xp_word_t i;

	for (i = 0; i < parser->literal_count; i++) {
		/* 
		 * it would remove redundancy of symbols and small integers. 
		 * more complex redundacy check may be done somewhere else 
		 * like in __add_string_literal.
		 */
		if (parser->literals[i] == literal) return i;
	}

	if (parser->literal_count >= xp_countof(parser->literals)) {
		parser->error_code = XP_STX_PARSER_ERROR_TOO_MANY_LITERALS;
		return -1;
	}

	parser->literals[parser->literal_count++] = literal;
	return parser->literal_count - 1;
}

static int __add_character_literal (xp_stx_parser_t* parser, xp_char_t ch)
{
	xp_word_t i, c, literal;
	xp_stx_t* stx = parser->stx;

	for (i = 0; i < parser->literal_count; i++) {
		c = XP_STX_IS_SMALLINT(parser->literals[i])? 
			stx->class_smallinteger: XP_STX_CLASS (stx, parser->literals[i]);
		if (c != stx->class_character) continue;

		if (ch == XP_STX_CHAR_AT(stx,parser->literals[i],0)) return i;
	}

	literal = xp_stx_instantiate (
		stx, stx->class_character, &ch, XP_NULL, 0);
	return __add_literal (parser, literal);
}

static int __add_string_literal (
	xp_stx_parser_t* parser, const xp_char_t* str, xp_word_t size)
{
	xp_word_t i, c, literal;
	xp_stx_t* stx = parser->stx;

	for (i = 0; i < parser->literal_count; i++) {
		c = XP_STX_IS_SMALLINT(parser->literals[i])? 
			stx->class_smallinteger: XP_STX_CLASS (stx, parser->literals[i]);
		if (c != stx->class_string) continue;

		if (xp_strxncmp (str, size, 
			XP_STX_DATA(stx,parser->literals[i]), 
			XP_STX_SIZE(stx,parser->literals[i])) == 0) return i;
	}

	literal = xp_stx_instantiate (
		stx, stx->class_string, XP_NULL, str, size);
	return __add_literal (parser, literal);
}

static int __add_symbol_literal (
	xp_stx_parser_t* parser, const xp_char_t* str, xp_word_t size)
{
	xp_stx_t* stx = parser->stx;
	return __add_literal (parser, xp_stx_new_symbolx(stx, str, size));
}

int xp_stx_parser_parse_method (
	xp_stx_parser_t* parser, xp_word_t method_class, void* input)
{
	int n;

	if (parser->input_func == XP_NULL) { 
		parser->error_code = XP_STX_PARSER_ERROR_INPUT_FUNC;
		return -1;
	}

	parser->method_class = method_class;
	if (__open_input(parser, input) == -1) return -1;
	n = __parse_method (parser, method_class, input);
	if (__close_input(parser) == -1) return -1;

	return n;
}

static int __parse_method (
	xp_stx_parser_t* parser, xp_word_t method_class, void* input)
{
	/*
	 * <method definition> ::= 
	 * 	<message pattern> [<temporaries>] [<primitive>] [<statements>]
	 */

	GET_CHAR (parser);
	GET_TOKEN (parser);

	xp_stx_name_clear (&parser->method_name);
	xp_arr_clear (&parser->bytecode);

	while (parser->temporary_count > 0) {
		xp_free (parser->temporaries[--parser->temporary_count]);
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

static int __finish_method (xp_stx_parser_t* parser)
{
	xp_stx_t* stx = parser->stx;
	xp_stx_class_t* class_obj;
	xp_stx_method_t* method_obj;
	xp_word_t method, selector;

	xp_assert (parser->bytecode.size != 0);

	class_obj = (xp_stx_class_t*)
		XP_STX_OBJECT(stx, parser->method_class);

	if (class_obj->methods == stx->nil) {
		/* TODO: reconfigure method dictionary size */
		class_obj->methods = xp_stx_instantiate (
			stx, stx->class_system_dictionary, 
			XP_NULL, XP_NULL, 64);
	}
	xp_assert (class_obj->methods != stx->nil);

	selector = xp_stx_new_symbolx (
		stx, parser->method_name.buffer, parser->method_name.size);

	method = xp_stx_instantiate(stx, stx->class_method, 
		XP_NULL, parser->literals, parser->literal_count);
	method_obj = (xp_stx_method_t*)XP_STX_OBJECT(stx, method);

	/* TODO: text saving must be optional */
	/*method_obj->text = xp_stx_instantiate (
		stx, stx->class_string, XP_NULL, 
		parser->text, xp_strlen(parser->text));
	*/
	method_obj->selector = selector;
	method_obj->bytecodes = xp_stx_instantiate (
		stx, stx->class_bytearray, XP_NULL, 
		parser->bytecode.buf, parser->bytecode.size);

	/* TODO: better way to store argument count & temporary count */
	method_obj->tmpcount = 
		XP_STX_TO_SMALLINT(parser->temporary_count - parser->argument_count);
	method_obj->argcount = XP_STX_TO_SMALLINT(parser->argument_count);

	xp_stx_dict_put (stx, class_obj->methods, selector, method);
	return 0;
}

static int __parse_message_pattern (xp_stx_parser_t* parser)
{
	/* 
	 * <message pattern> ::= 
	 * 	<unary pattern> | <binary pattern> | <keyword pattern>
	 * <unary pattern> ::= unarySelector
	 * <binary pattern> ::= binarySelector <method argument>
	 * <keyword pattern> ::= (keyword  <method argument>)+
	 */
	int n;

	if (parser->token.type == XP_STX_TOKEN_IDENT) { 
		n = __parse_unary_pattern (parser);
	}
	else if (parser->token.type == XP_STX_TOKEN_BINARY) { 
		n = __parse_binary_pattern (parser);
	}
	else if (parser->token.type == XP_STX_TOKEN_KEYWORD) { 
		n = __parse_keyword_pattern (parser);
	}
	else {
		parser->error_code = XP_STX_PARSER_ERROR_MESSAGE_SELECTOR;
		n = -1;
	}

	parser->temporary_count = parser->argument_count;
	return n;
}

static int __parse_unary_pattern (xp_stx_parser_t* parser)
{
	/* TODO: check if the method name exists */

	if (xp_stx_name_adds(
		&parser->method_name, parser->token.name.buffer) == -1) {
		parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
		return -1;
	}

	GET_TOKEN (parser);
	return 0;
}

static int __parse_binary_pattern (xp_stx_parser_t* parser)
{
	/* TODO: check if the method name exists */

	if (xp_stx_name_adds(
		&parser->method_name, parser->token.name.buffer) == -1) {
		parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
		return -1;
	}

	GET_TOKEN (parser);
	if (parser->token.type != XP_STX_TOKEN_IDENT) {
		parser->error_code = XP_STX_PARSER_ERROR_ARGUMENT_NAME;
		return -1;
	}

	if (parser->argument_count >= xp_countof(parser->temporaries)) {
		parser->error_code = XP_STX_PARSER_ERROR_TOO_MANY_ARGUMENTS;
		return -1;
	}

	/* TODO: check for duplicate entries...in instvars */
	parser->temporaries[parser->argument_count] = 
		xp_stx_token_yield (&parser->token, 0);
	if (parser->temporaries[parser->argument_count] == XP_NULL) {
		parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
		return -1;
	}
	parser->argument_count++;

	GET_TOKEN (parser);
	return 0;
}

static int __parse_keyword_pattern (xp_stx_parser_t* parser)
{
	do {
		if (xp_stx_name_adds(
			&parser->method_name, parser->token.name.buffer) == -1) {
			parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
			return -1;
		}

		GET_TOKEN (parser);
		if (parser->token.type != XP_STX_TOKEN_IDENT) {
			parser->error_code = XP_STX_PARSER_ERROR_ARGUMENT_NAME;
			return -1;
		}

		if (__is_pseudo_variable(&parser->token)) {
			parser->error_code = XP_STX_PARSER_ERROR_PSEUDO_VARIABLE;
			return -1;
		}

		if (parser->argument_count >= xp_countof(parser->temporaries)) {
			parser->error_code = XP_STX_PARSER_ERROR_TOO_MANY_ARGUMENTS;
			return -1;
		}

		parser->temporaries[parser->argument_count] = 
			xp_stx_token_yield (&parser->token, 0);
		if (parser->temporaries[parser->argument_count] == XP_NULL) {
			parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
			return -1;
		}

/* TODO: check for duplicate entries...in instvars/arguments */
		parser->argument_count++;

		GET_TOKEN (parser);
	} while (parser->token.type == XP_STX_TOKEN_KEYWORD);

	/* TODO: check if the method name exists */
	/* if it exists, collapse arguments */
xp_printf (XP_TEXT("METHOD NAME ==> [%s]\n"), parser->method_name.buffer);

	return 0;
}

static int __parse_temporaries (xp_stx_parser_t* parser)
{
	/* 
	 * <temporaries> ::= '|' <temporary variable list> '|'
	 * <temporary variable list> ::= identifier*
	 */

	if (!__is_vbar_token(&parser->token)) return 0;

	GET_TOKEN (parser);
	while (parser->token.type == XP_STX_TOKEN_IDENT) {
		if (parser->temporary_count >= xp_countof(parser->temporaries)) {
			parser->error_code = XP_STX_PARSER_ERROR_TOO_MANY_TEMPORARIES;
			return -1;
		}

		if (__is_pseudo_variable(&parser->token)) {
			parser->error_code = XP_STX_PARSER_ERROR_PSEUDO_VARIABLE;
			return -1;
		}

		parser->temporaries[parser->temporary_count] = 
			xp_stx_token_yield (&parser->token, 0);
		if (parser->temporaries[parser->temporary_count] == XP_NULL) {
			parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
			return -1;
		}

/* TODO: check for duplicate entries...in instvars/arguments/temporaries */
		parser->temporary_count++;

		GET_TOKEN (parser);
	}
	if (!__is_vbar_token(&parser->token)) {
		parser->error_code = XP_STX_PARSER_ERROR_TEMPORARIES_NOT_CLOSED;
		return -1;
	}

	GET_TOKEN (parser);
	return 0;
}

static int __parse_primitive (xp_stx_parser_t* parser)
{
	/* 
	 * <primitive> ::= '<' 'primitive:' number '>'
	 */

	int prim_no;

	if (!__is_primitive_opener(&parser->token)) return 0;
	GET_TOKEN (parser);

	if (parser->token.type != XP_STX_TOKEN_KEYWORD ||
	    xp_strcmp (parser->token.name.buffer, XP_TEXT("primitive:")) != 0) {
		parser->error_code = XP_STX_PARSER_ERROR_PRIMITIVE_KEYWORD;
		return -1;
	}

	GET_TOKEN (parser); /* TODO: only integer */
	if (parser->token.type != XP_STX_TOKEN_NUMLIT) {
		parser->error_code = XP_STX_PARSER_ERROR_PRIMITIVE_NUMBER;
		return -1;
	}

/*TODO: more checks the validity of the primitive number */
	if (!xp_stristype(parser->token.name.buffer, xp_isdigit)) {
		parser->error_code = XP_STX_PARSER_ERROR_PRIMITIVE_NUMBER;
		return -1;
	}

	XP_STRTOI (prim_no, parser->token.name.buffer, XP_NULL, 10);
	if (prim_no < 0 || prim_no > 0xFF) {
		parser->error_code = XP_STX_PARSER_ERROR_PRIMITIVE_NUMBER_RANGE;
		return -1;
	}

	EMIT_DO_PRIMITIVE (parser, prim_no);

	GET_TOKEN (parser);
	if (!__is_primitive_closer(&parser->token)) {
		parser->error_code = XP_STX_PARSER_ERROR_PRIMITIVE_NOT_CLOSED;
		return -1;
	}

	GET_TOKEN (parser);
	return 0;
}

static int __parse_statements (xp_stx_parser_t* parser)
{
	/*
	 * <statements> ::= (ORIGINAL->maybe wrong)
	 * 	(<return statement> ['.'] ) |
	 * 	(<expression> ['.' [<statements>]])
	 * <statements> ::= (REVISED->correct?)
	 * 	<statement> ['. [<statements>]]
	 */

	while (parser->token.type != XP_STX_TOKEN_END) {
		if (__parse_statement (parser) == -1) return -1;

		if (parser->token.type == XP_STX_TOKEN_PERIOD) {
			GET_TOKEN (parser);
			continue;
		}

		if (parser->token.type != XP_STX_TOKEN_END) {
			parser->error_code = XP_STX_PARSER_ERROR_NO_PERIOD;
			return -1;
		}
	}

	EMIT_CODE (parser, RETURN_RECEIVER);
	return 0;
}

static int __parse_block_statements (xp_stx_parser_t* parser)
{
	while (parser->token.type != XP_STX_TOKEN_RBRACKET && 
	       parser->token.type != XP_STX_TOKEN_END) {

		if (__parse_statement(parser) == -1) return -1;
		if (parser->token.type != XP_STX_TOKEN_PERIOD) break;
		GET_TOKEN (parser);
	}

	return 0;
}

static int __parse_statement (xp_stx_parser_t* parser)
{
	/* 
	 * <statement> ::= <return statement> | <expression>
	 * <return statement> ::= returnOperator <expression> 
	 * returnOperator ::= '^'
	 */

	if (parser->token.type == XP_STX_TOKEN_RETURN) {
		GET_TOKEN (parser);
		if (__parse_expression(parser) == -1) return -1;
		EMIT_RETURN_FROM_MESSAGE (parser);
	}
	else {
		if (__parse_expression(parser) == -1) return -1;
	}

	return 0;
}

static int __parse_expression (xp_stx_parser_t* parser)
{
	/*
	 * <expression> ::= <assignment> | <basic expression>
	 * <assignment> ::= <assignment target> assignmentOperator <expression>
	 * <basic expression> ::= <primary> [<messages> <cascaded messages>]
	 * <assignment target> ::= identifier
	 * assignmentOperator ::=  ':='
	 */
	xp_stx_t* stx = parser->stx;

	if (parser->token.type == XP_STX_TOKEN_IDENT) {
		xp_char_t* ident = xp_stx_token_yield (&parser->token, 0);
		if (ident == XP_NULL) {
			parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
			return -1;
		}

		GET_TOKEN (parser);
		if (parser->token.type == XP_STX_TOKEN_ASSIGN) {
			GET_TOKEN (parser);
			if (__parse_assignment(parser, ident) == -1) {
				xp_free (ident);
				return -1;
			}
		}
		else {
			if (__parse_basic_expression(parser, ident) == -1) {
				xp_free (ident);
				return -1;
			}
		}

		xp_free (ident);
	}
	else {
		if (__parse_basic_expression(parser, XP_NULL) == -1) return -1;
	}

	return 0;
}

static int __parse_basic_expression (
	xp_stx_parser_t* parser, const xp_char_t* ident)
{
	/*
	 * <basic expression> ::= <primary> [<messages> <cascaded messages>]
	 */
	xp_bool_t is_super;

	if (__parse_primary(parser, ident, &is_super) == -1) return -1;
	if (parser->token.type != XP_STX_TOKEN_END &&
	    parser->token.type != XP_STX_TOKEN_PERIOD) {
		if (__parse_message_continuation(parser, is_super) == -1) return -1;
	}
	return 0;
}

static int __parse_assignment (
	xp_stx_parser_t* parser, const xp_char_t* target)
{
	/*
	 * <assignment> ::= <assignment target> assignmentOperator <expression>
	 */

	xp_word_t i;
	xp_stx_t* stx = parser->stx;

	for (i = parser->argument_count; i < parser->temporary_count; i++) {
		if (xp_strcmp (target, parser->temporaries[i]) == 0) {
			if (__parse_expression(parser) == -1) return -1;
			EMIT_STORE_TEMPORARY_LOCATION (parser, i);
			return 0;
		}
	}

	if (xp_stx_get_instance_variable_index (
		stx, parser->method_class, target, &i) == 0) {
		if (__parse_expression(parser) == -1) return -1;
		EMIT_STORE_RECEIVER_VARIABLE (parser, i);
		return 0;
	}

	if (xp_stx_lookup_class_variable (
		stx, parser->method_class, target) != stx->nil) {
		if (__parse_expression(parser) == -1) return -1;

		/* TODO */
		EMIT_CODE_TEST (parser, XP_TEXT("ASSIGN_CLASSVAR #"), target);
		//EMIT_STORE_CLASS_VARIABLE (parser, target);
		return 0;
	}

	/* TODO: IMPLEMENT POOL DICTIONARIES */

	/* TODO: IMPLEMENT GLOBLAS, but i don't like this idea */

	parser->error_code = XP_STX_PARSER_ERROR_UNDECLARED_NAME;
	return -1;
}

static int __parse_primary (
	xp_stx_parser_t* parser, const xp_char_t* ident, xp_bool_t* is_super)
{
	/*
	 * <primary> ::=
	 * 	identifier | <literal> | 
	 * 	<block constructor> | ( '('<expression>')' )
	 */

	xp_stx_t* stx = parser->stx;

	if (ident == XP_NULL) {
		int pos;
		xp_word_t literal;

		*is_super = xp_false;

		if (parser->token.type == XP_STX_TOKEN_IDENT) {
			if (__parse_primary_ident(parser, 
				parser->token.name.buffer, is_super) == -1) return -1;
			GET_TOKEN (parser);
		}
		else if (parser->token.type == XP_STX_TOKEN_CHARLIT) {
			pos = __add_character_literal(
				parser, parser->token.name.buffer[0]);
			if (pos == -1) return -1;
			EMIT_PUSH_LITERAL_CONSTANT (parser, pos);
			GET_TOKEN (parser);
		}
		else if (parser->token.type == XP_STX_TOKEN_STRLIT) {
			pos = __add_string_literal (parser,
				parser->token.name.buffer, parser->token.name.size);
			if (pos == -1) return -1;
			EMIT_PUSH_LITERAL_CONSTANT (parser, pos);
			GET_TOKEN (parser);
		}
		else if (parser->token.type == XP_STX_TOKEN_NUMLIT) {
			/* TODO: other types of numbers, negative numbers, etc */
			xp_word_t tmp;
			XP_STRTOI (tmp, parser->token.name.buffer, XP_NULL, 10);
			literal = XP_STX_TO_SMALLINT(tmp);
			pos = __add_literal(parser, literal);
			if (pos == -1) return -1;
			EMIT_PUSH_LITERAL_CONSTANT (parser, pos);
			GET_TOKEN (parser);
		}
		else if (parser->token.type == XP_STX_TOKEN_SYMLIT) {
			pos = __add_symbol_literal (parser,
				parser->token.name.buffer, parser->token.name.size);
			if (pos == -1) return -1;
			EMIT_PUSH_LITERAL_CONSTANT (parser, pos);
			GET_TOKEN (parser);
		}
		else if (parser->token.type == XP_STX_TOKEN_LBRACKET) {
			GET_TOKEN (parser);
			if (__parse_block_constructor(parser) == -1) return -1;
		}
		else if (parser->token.type == XP_STX_TOKEN_APAREN) {
			/* TODO: array literal */
		}
		else if (parser->token.type == XP_STX_TOKEN_LPAREN) {
			GET_TOKEN (parser);
			if (__parse_expression(parser) == -1) return -1;
			if (parser->token.type != XP_STX_TOKEN_RPAREN) {
				parser->error_code = XP_STX_PARSER_ERROR_NO_RPAREN;
				return -1;
			}
			GET_TOKEN (parser);
		}
		else {
			parser->error_code = XP_STX_PARSER_ERROR_PRIMARY;
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
	xp_stx_parser_t* parser, const xp_char_t* ident, xp_bool_t* is_super)
{
	xp_word_t i;
	xp_stx_t* stx = parser->stx;

	*is_super = xp_false;

	if (xp_strcmp(ident, XP_TEXT("self")) == 0) {
		EMIT_CODE (parser, PUSH_RECEIVER);
		return 0;
	}
	else if (xp_strcmp(ident, XP_TEXT("super")) == 0) {
		*is_super = xp_true;
		EMIT_CODE (parser, PUSH_RECEIVER);
		return 0;
	}
	else if (xp_strcmp(ident, XP_TEXT("nil")) == 0) {
		EMIT_CODE (parser, PUSH_NIL);
		return 0;
	}
	else if (xp_strcmp(ident, XP_TEXT("true")) == 0) {
		EMIT_CODE (parser, PUSH_TRUE);
		return 0;
	}
	else if (xp_strcmp(ident, XP_TEXT("false")) == 0) {
		EMIT_CODE (parser, PUSH_FALSE);
		return 0;
	}

	/* Refer to __parse_assignment for identifier lookup */

	for (i = 0; i < parser->temporary_count; i++) {
		if (xp_strcmp(ident, parser->temporaries[i]) == 0) {
			EMIT_PUSH_TEMPORARY_LOCATION (parser, i);
			return 0;
		}
	}

	if (xp_stx_get_instance_variable_index (
		stx, parser->method_class, ident, &i) == 0) {
		EMIT_PUSH_RECEIVER_VARIABLE (parser, i);
		return 0;
	}	

	/* TODO: what is the best way to look up a class variable? */
	/* 1. Use the class containing it and using its position */
	/* 2. Use a primitive method after pushing the name as a symbol */
	/* 3. Implement a vm instruction to do it */
/*
	if (xp_stx_lookup_class_variable (
		stx, parser->method_class, ident) != stx->nil) {
		//EMIT_LOOKUP_CLASS_VARIABLE (parser, ident);
		return 0;
	}
*/

	/* TODO: IMPLEMENT POOL DICTIONARIES */

	/* TODO: IMPLEMENT GLOBLAS, but i don't like this idea */

	parser->error_code = XP_STX_PARSER_ERROR_UNDECLARED_NAME;
	return -1;
}

static int __parse_block_constructor (xp_stx_parser_t* parser)
{
	/*
	 * <block constructor> ::= '[' <block body> ']'
	 * <block body> ::= [<block argument>* '|']
	 * 	[<temporaries>] [<statements>]
	 * <block argument> ::= ':'  identifier
	 */

	if (parser->token.type == XP_STX_TOKEN_COLON) {
		do {
			GET_TOKEN (parser);

			if (parser->token.type != XP_STX_TOKEN_IDENT) {
				parser->error_code = XP_STX_PARSER_ERROR_BLOCK_ARGUMENT_NAME;
				return -1;
			}

			/* TODO : store block arguments */
			GET_TOKEN (parser);
		} while (parser->token.type == XP_STX_TOKEN_COLON);
			
		if (!__is_vbar_token(&parser->token)) {
			parser->error_code = XP_STX_PARSER_ERROR_BLOCK_ARGUMENT_LIST;
			return -1;
		}

		GET_TOKEN (parser);
	}

	/* TODO: create a block closure */
	if (__parse_temporaries(parser) == -1) return -1;
	if (__parse_block_statements(parser) == -1) return -1;

	if (parser->token.type != XP_STX_TOKEN_RBRACKET) {
		parser->error_code = XP_STX_PARSER_ERROR_BLOCK_NOT_CLOSED;
		return -1;
	}

	GET_TOKEN (parser);

	/* TODO: do special treatment for block closures */

	return 0;
}

static int __parse_message_continuation (
	xp_stx_parser_t* parser, xp_bool_t is_super)
{
	/*
	 * <messages> ::=
	 * 	(<unary message>+ <binary message>* [<keyword message>] ) |
	 * 	(<binary message>+ [<keyword message>] ) |
	 * 	<keyword message>
	 * <cascaded messages> ::= (';' <messages>)*
	 */
	if (__parse_keyword_message(parser, is_super) == -1) return -1;

	while (parser->token.type == XP_STX_TOKEN_SEMICOLON) {
		EMIT_CODE_TEST (parser, XP_TEXT("DoSpecial(DUP_RECEIVER(CASCADE))"), XP_TEXT(""));
		GET_TOKEN (parser);

		if (__parse_keyword_message(parser, xp_false) == -1) return -1;
		EMIT_CODE_TEST (parser, XP_TEXT("DoSpecial(POP_TOP)"), XP_TEXT(""));
	}

	return 0;
}

static int __parse_keyword_message (xp_stx_parser_t* parser, xp_bool_t is_super)
{
	/*
	 * <keyword message> ::= (keyword <keyword argument> )+
	 * <keyword argument> ::= <primary> <unary message>* <binary message>*
	 */

	xp_stx_name_t name;
	xp_word_t pos;
	xp_bool_t is_super2;
	int nargs = 0, n;

	if (__parse_binary_message (parser, is_super) == -1) return -1;
	if (parser->token.type != XP_STX_TOKEN_KEYWORD) return 0;

	if (xp_stx_name_open(&name, 0) == XP_NULL) {
		parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
		return -1;
	}
	
	do {
		if (xp_stx_name_adds(&name, parser->token.name.buffer) == -1) {
			parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
			xp_stx_name_close (&name);
			return -1;
		}

		GET_TOKEN (parser);
		if (__parse_primary(parser, XP_NULL, &is_super2) == -1) {
			xp_stx_name_close (&name);
			return -1;
		}

		if (__parse_binary_message(parser, is_super2) == -1) {
			xp_stx_name_close (&name);
			return -1;
		}

		nargs++;
		/* TODO: check if it has too many arguments.. */
	} while (parser->token.type == XP_STX_TOKEN_KEYWORD);

	pos = __add_symbol_literal (parser, name.buffer, name.size);
	if (pos == -1) {
		xp_stx_name_close (&name);
		return -1;
	}

	n = (is_super)?
		__emit_send_to_super(parser,nargs,pos):
		__emit_send_to_self(parser,nargs,pos);
	if (n == -1) {
		xp_stx_name_close (&name);
		return -1;
	}

	xp_stx_name_close (&name);
	return 0;
}

static int __parse_binary_message (xp_stx_parser_t* parser, xp_bool_t is_super)
{
	/*
	 * <binary message> ::= binarySelector <binary argument>
	 * <binary argument> ::= <primary> <unary message>*
	 */
	xp_word_t pos;
	xp_bool_t is_super2;
	int n;

	if (__parse_unary_message (parser, is_super) == -1) return -1;

	while (parser->token.type == XP_STX_TOKEN_BINARY) {
		xp_char_t* op = xp_stx_token_yield (&parser->token, 0);
		if (op == XP_NULL) {
			parser->error_code = XP_STX_PARSER_ERROR_MEMORY;
			return -1;
		}

		GET_TOKEN (parser);
		if (__parse_primary(parser, XP_NULL, &is_super2) == -1) {
			xp_free (op);
			return -1;
		}

		if (__parse_unary_message(parser, is_super2) == -1) {
			xp_free (op);
			return -1;
		}

		pos = __add_symbol_literal (parser, op, xp_strlen(op));
		if (pos == -1) {
			xp_free (op);
			return -1;
		}

		n = (is_super)?
			__emit_send_to_super(parser,2,pos):
			__emit_send_to_self(parser,2,pos);
		if (n == -1) {
			xp_free (op);
			return -1;
		}

		xp_free (op);
	}

	return 0;
}

static int __parse_unary_message (xp_stx_parser_t* parser, xp_bool_t is_super)
{
	/* <unary message> ::= unarySelector */

	xp_word_t pos;
	int n;

	while (parser->token.type == XP_STX_TOKEN_IDENT) {
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

static int __get_token (xp_stx_parser_t* parser)
{
	xp_cint_t c;

	do {
		if (__skip_spaces(parser) == -1) return -1;
		if (parser->curc == XP_CHAR('"')) {
			GET_CHAR (parser);
			if (__skip_comment(parser) == -1) return -1;
		}
		else break;
	} while (1);

	c = parser->curc;
	xp_stx_token_clear (&parser->token);

	if (c == XP_CHAR_EOF) {
		parser->token.type = XP_STX_TOKEN_END;
	}
	else if (xp_isalpha(c)) {
		if (__get_ident(parser) == -1) return -1;
	}
	else if (xp_isdigit(c)) {
		if (__get_numlit(parser, xp_false) == -1) return -1;
	}
	else if (c == XP_CHAR('$')) {
		GET_CHAR (parser);
		if (__get_charlit(parser) == -1) return -1;
	}
	else if (c == XP_CHAR('\'')) {
		GET_CHAR (parser);
		if (__get_strlit(parser) == -1) return -1;
	}
	else if (c == XP_CHAR(':')) {
		parser->token.type = XP_STX_TOKEN_COLON;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);

		c = parser->curc;
		if (c == XP_CHAR('=')) {
			parser->token.type = XP_STX_TOKEN_ASSIGN;
			ADD_TOKEN_CHAR(parser, c);
			GET_CHAR (parser);
		}
	}
	else if (c == XP_CHAR('^')) {
		parser->token.type = XP_STX_TOKEN_RETURN;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == XP_CHAR('[')) {
		parser->token.type = XP_STX_TOKEN_LBRACKET;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == XP_CHAR(']')) {
		parser->token.type = XP_STX_TOKEN_RBRACKET;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == XP_CHAR('(')) {
		parser->token.type = XP_STX_TOKEN_LPAREN;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == XP_CHAR(')')) {
		parser->token.type = XP_STX_TOKEN_RPAREN;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == XP_CHAR('#')) {
		/*ADD_TOKEN_CHAR(parser, c);*/
		GET_CHAR (parser);

		c = parser->curc;
		if (c == XP_CHAR_EOF) {
			parser->error_code = XP_STX_PARSER_ERROR_LITERAL;
			return -1;
		}
		else if (c == XP_CHAR('(')) {
			ADD_TOKEN_CHAR(parser, c);
			parser->token.type = XP_STX_TOKEN_APAREN;
			GET_CHAR (parser);
		}
		else if (c == XP_CHAR('\'')) {
			GET_CHAR (parser);
			if (__get_strlit(parser) == -1) return -1;
			parser->token.type = XP_STX_TOKEN_SYMLIT;
		}
		else if (!__is_closing_char(c) && !xp_isspace(c)) {
			do {
				ADD_TOKEN_CHAR(parser, c);
				GET_CHAR (parser);
				c = parser->curc;
			} while (!__is_closing_char(c) && !xp_isspace(c));

			parser->token.type = XP_STX_TOKEN_SYMLIT;
		}
		else {
			parser->error_code = XP_STX_PARSER_ERROR_LITERAL;
			return -1;
		}
	}
	else if (c == XP_CHAR('.')) {
		parser->token.type = XP_STX_TOKEN_PERIOD;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (c == XP_CHAR(';')) {
		parser->token.type = XP_STX_TOKEN_SEMICOLON;
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
	}
	else if (__is_binary_char(c)) {
		if (__get_binary(parser) == -1) return -1;
	}
	else {
		parser->error_code = XP_STX_PARSER_ERROR_CHAR;
		return -1;
	}

//xp_printf (XP_TEXT("TOKEN: %s\n"), parser->token.name.buffer);
	return 0;
}

static int __get_ident (xp_stx_parser_t* parser)
{
	/*
	 * identifier ::= letter (letter | digit)*
	 * keyword ::= identifier ':'
	 */

	xp_cint_t c = parser->curc;
	parser->token.type = XP_STX_TOKEN_IDENT;

	do {
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
		c = parser->curc;
	} while (xp_isalnum(c));

	if (c == XP_CHAR(':')) {
		ADD_TOKEN_CHAR (parser, c);
		parser->token.type = XP_STX_TOKEN_KEYWORD;
		GET_CHAR (parser);
	}

	return 0;
}

static int __get_numlit (xp_stx_parser_t* parser, xp_bool_t negated)
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

	xp_cint_t c = parser->curc;
	parser->token.type = XP_STX_TOKEN_NUMLIT;

	do {
		ADD_TOKEN_CHAR(parser, c);
		GET_CHAR (parser);
		c = parser->curc;
	} while (xp_isalnum(c));

	/* TODO; more */
	return 0;
}

static int __get_charlit (xp_stx_parser_t* parser)
{
	/* 
	 * character_literal ::= '$' character
	 * character ::= "Any character in the implementation-defined character set"
	 */

	xp_cint_t c = parser->curc; /* even a new-line or white space would be taken */
	if (c == XP_CHAR_EOF) {
		parser->error_code = XP_STX_PARSER_ERROR_CHARLIT;
		return -1;
	}	

	parser->token.type = XP_STX_TOKEN_CHARLIT;
	ADD_TOKEN_CHAR(parser, c);
	GET_CHAR (parser);
	return 0;
}

static int __get_strlit (xp_stx_parser_t* parser)
{
	/* 
	 * string_literal ::= stringDelimiter stringBody stringDelimiter
	 * stringBody ::= (nonStringDelimiter | (stringDelimiter stringDelimiter)*)
	 * stringDelimiter ::= '''    "a single quote"
	 */

	/* TODO: C-like string */

	xp_cint_t c = parser->curc;
	parser->token.type = XP_STX_TOKEN_STRLIT;

	do {
		do {
			ADD_TOKEN_CHAR (parser, c);
			GET_CHAR (parser);
			c = parser->curc;

			if (c == XP_CHAR_EOF) {
				parser->error_code = XP_STX_PARSER_ERROR_STRLIT;
				return -1;
			}
		} while (c != XP_CHAR('\''));

		GET_CHAR (parser);
		c = parser->curc;
	} while (c == XP_CHAR('\''));

	return 0;
}

static int __get_binary (xp_stx_parser_t* parser)
{
	/* 
	 * binarySelector ::= binaryCharacter+
	 */

	xp_cint_t c = parser->curc;
	ADD_TOKEN_CHAR (parser, c);

	if (c == XP_CHAR('-')) {
		GET_CHAR (parser);
		c = parser->curc;
		if (xp_isdigit(c)) return __get_numlit(parser,xp_true);
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

	parser->token.type = XP_STX_TOKEN_BINARY;
	return 0;
}

static int __skip_spaces (xp_stx_parser_t* parser)
{
	while (xp_isspace(parser->curc)) GET_CHAR (parser);
	return 0;
}

static int __skip_comment (xp_stx_parser_t* parser)
{
	while (parser->curc != XP_CHAR('"')) GET_CHAR (parser);
	GET_CHAR (parser);
	return 0;
}

static int __get_char (xp_stx_parser_t* parser)
{
	xp_cint_t c;

	if (parser->ungotc_count > 0) {
		parser->curc = parser->ungotc[parser->ungotc_count--];
	}
	else {
		if (parser->input_func (
			XP_STX_PARSER_INPUT_CONSUME, 
			parser->input_owner, (void*)&c) == -1) {
			parser->error_code = XP_STX_PARSER_ERROR_INPUT;
			return -1;
		}
		parser->curc = c;
	}
	return 0;
}

static int __unget_char (xp_stx_parser_t* parser, xp_cint_t c)
{
	if (parser->ungotc_count >= xp_countof(parser->ungotc)) return -1;
	parser->ungotc[parser->ungotc_count++] = c;
	return 0;
}

static int __open_input (xp_stx_parser_t* parser, void* input)
{
	if (parser->input_func(
		XP_STX_PARSER_INPUT_OPEN, 
		(void*)&parser->input_owner, input) == -1) {
		parser->error_code = XP_STX_PARSER_ERROR_INPUT;
		return -1;
	}

	parser->error_code = XP_STX_PARSER_ERROR_NONE;
	parser->curc = XP_CHAR_EOF;
	parser->ungotc_count = 0;
	return 0;
}

static int __close_input (xp_stx_parser_t* parser)
{
	if (parser->input_func(
		XP_STX_PARSER_INPUT_CLOSE, 
		parser->input_owner, XP_NULL) == -1) {
		parser->error_code = XP_STX_PARSER_ERROR_INPUT;
		return -1;
	}

	return 0;
}

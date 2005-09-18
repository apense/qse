#include <xp/lsp/lsp.h>
#include <xp/bas/stdio.h>
#include <xp/bas/ctype.h>
#include <xp/bas/stdcli.h>
#include <xp/bas/locale.h>

#ifdef __linux
#include <mcheck.h>
#endif

static int get_char (int cmd, void* owner, void* arg)
{
	xp_cint_t c;

	switch (cmd) {
	case XP_LSP_IO_OPEN:
	case XP_LSP_IO_CLOSE:
		return 0;

	case XP_LSP_IO_CHAR:
		c = xp_fgetc (xp_stdin);
		if (c == XP_CHAR_EOF) {
			if (xp_ferror(xp_stdin)) return -1;
		}
		
		break;

	case XP_LSP_IO_STR:
		return -1;
	}

	return 0;
}

static int put_char (int cmd, void* owner, void* arg)
{

	switch (cmd) {
	case XP_LSP_IO_OPEN:
	case XP_LSP_IO_CLOSE:
		return 0;

	case XP_LSP_IO_CHAR:
		xp_fputc (*(xp_char_t*)arg, xp_stdout);
		break;

	case XP_LSP_IO_STR:
		xp_fputs ((xp_char_t*)arg, xp_stdout);
		break;
	}

	return 0;
}


int to_int (const xp_char_t* str)
{
	int r = 0;

	while (*str != XP_CHAR('\0')) {
		if (!xp_isdigit(*str))	break;
		r = r * 10 + (*str - XP_CHAR('0'));
		str++;
	}

	return r;
}

#include <locale.h>

int handle_cli_error (
	const xp_cli_t* cli, int code, 
	const xp_char_t* name, const xp_char_t* value)
{
	xp_printf (XP_TEXT("usage: %s /memory=nnn /increment=nnn\n"), cli->verb);

	if (code == XP_CLI_ERROR_INVALID_OPTNAME) {
		xp_printf (XP_TEXT("unknown option - %s\n"), name);
	}
	else if (code == XP_CLI_ERROR_MISSING_OPTNAME) {
		xp_printf (XP_TEXT("missing option - %s\n"), name);
	}
	else if (code == XP_CLI_ERROR_REDUNDANT_OPTVAL) {
		xp_printf (XP_TEXT("redundant value %s for %s\n"), value, name);
	}
	else if (code == XP_CLI_ERROR_MISSING_OPTVAL) {
		xp_printf (XP_TEXT("missing value for %s\n"), name);
	}
	else if (code == XP_CLI_ERROR_MEMORY) {
		xp_printf (XP_TEXT("memory error in processing %s\n"), name);
	}
	else {
		xp_printf (XP_TEXT("error code: %d\n"), code);
	}

	return -1;
}

xp_cli_t* parse_cli (int argc, xp_char_t* argv[])
{
	static const xp_char_t* optsta[] =
	{
		XP_TEXT("/"), XP_TEXT("--"), XP_NULL
	};

	static xp_cliopt_t opts[] =
	{
		{ XP_TEXT("memory"), XP_CLI_OPTNAME | XP_CLI_OPTVAL },
        { XP_TEXT("increment"), XP_CLI_OPTNAME | XP_CLI_OPTVAL },
        { XP_NULL, 0 }
    };

	static xp_cli_t cli =
	{
		handle_cli_error,
		optsta,
		XP_TEXT("="),
		opts
	};

	if (xp_parsecli (argc, argv, &cli) == -1) return XP_NULL;
	return &cli;
}

int xp_main (int argc, xp_char_t* argv[])
{
	xp_lsp_t* lsp;
	xp_lsp_obj_t* obj;
	xp_cli_t* cli;
	int mem, inc;

#ifdef __linux
	mtrace ();
#endif

	if (xp_setlocale () == -1) {
		xp_fprintf (xp_stderr,
			XP_TEXT("error: cannot set locale\n"));
		return -1;
	}

	if ((cli = parse_cli (argc, argv)) == XP_NULL) return -1;
	mem = to_int(xp_getclioptval(cli, XP_TEXT("memory")));
	inc = to_int(xp_getclioptval(cli, XP_TEXT("increment")));
	xp_clearcli (cli);

	if (mem <= 0) {
		xp_fprintf (xp_stderr,
			XP_TEXT("error: invalid memory size given\n"));
		return -1;
	}

	lsp = xp_lsp_open (XP_NULL, mem, inc);
	if (lsp == XP_NULL) {
		xp_fprintf (xp_stderr, 
			XP_TEXT("error: cannot create a lsp instance\n"));
		return -1;
	}

	xp_printf (XP_TEXT("LSP 0.0001\n"));

	xp_lsp_attach_input (lsp, get_char);
	xp_lsp_attach_output (lsp, put_char);

	for (;;) {
		xp_printf (XP_TEXT("%s> "), argv[0]);

		obj = xp_lsp_read (lsp);
		if (obj == XP_NULL) {
			if (lsp->errnum != XP_LSP_ERR_END && 
			    lsp->errnum != XP_LSP_ERR_ABORT) {
				xp_fprintf (xp_stderr, 
					XP_TEXT("error while reading: %d\n"), lsp->errnum);
			}

			if (lsp->errnum < XP_LSP_ERR_SYNTAX) break;
			continue;
		}

		if ((obj = xp_lsp_eval (lsp, obj)) != XP_NULL) {
			xp_lsp_print (lsp, obj);
			xp_printf (XP_TEXT("\n"));
		}
		else {
			if (lsp->errnum == XP_LSP_ERR_ABORT) break;
			xp_fprintf (xp_stderr, 
				XP_TEXT("error while evaluating: %d\n"), lsp->errnum);
		}
	}

	xp_lsp_close (lsp);

#ifdef __linux
	muntrace ();
#endif
	return 0;
}



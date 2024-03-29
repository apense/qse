/*
 * $Id$
 *
    Copyright (c) 2006-2014 Chung, Hyung-Hwan. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "sed.h"
#include <qse/sed/stdsed.h>
#include <qse/cmn/str.h>
#include <qse/cmn/sio.h>
#include "../cmn/mem.h"

typedef struct xtn_in_t xtn_in_t;
struct xtn_in_t
{
	qse_sed_iostd_t* ptr;
	qse_sed_iostd_t* cur;
	qse_size_t       mempos;
};

typedef struct xtn_out_t xtn_out_t;
struct xtn_out_t
{
	qse_sed_iostd_t* ptr;
	qse_str_t*       memstr;
};

typedef struct xtn_t xtn_t;
struct xtn_t
{
	struct
	{
		xtn_in_t in;
		qse_char_t last;
		int newline_squeezed;
	} s;
	struct
	{
		xtn_in_t  in;
		xtn_out_t out;
	} e;
};


static int int_to_str (qse_size_t val, qse_char_t* buf, qse_size_t buflen)
{
	qse_size_t t;
	qse_size_t rlen = 0;

	t = val;
	if (t == 0) rlen++;
	else
	{
		/* non-zero values */
		if (t < 0) { t = -t; rlen++; }
		while (t > 0) { rlen++; t /= 10; }
	}

	if (rlen >= buflen) return -1; /* buffer too small */

	buf[rlen] = QSE_T('\0');

	t = val;
	if (t == 0) buf[0] = QSE_T('0'); 
	else
	{
		if (t < 0) t = -t;

		/* fill in the buffer with digits */
		while (t > 0) 
		{
			buf[--rlen] = (qse_char_t)(t % 10) + QSE_T('0');
			t /= 10;
		}

		/* insert the negative sign if necessary */
		if (val < 0) buf[--rlen] = QSE_T('-');
	}

	return 0;
}

qse_sed_t* qse_sed_openstd (qse_size_t xtnsize)
{
	return qse_sed_openstdwithmmgr (QSE_MMGR_GETDFL(), xtnsize);
}

qse_sed_t* qse_sed_openstdwithmmgr (qse_mmgr_t* mmgr, qse_size_t xtnsize)
{
	return qse_sed_open (mmgr, QSE_SIZEOF(xtn_t) + xtnsize);
}

void* qse_sed_getxtnstd (qse_sed_t* sed)
{
	return (void*)((xtn_t*)QSE_XTN(sed) + 1);
}

static int verify_iostd_in  (qse_sed_t* sed, qse_sed_iostd_t in[])
{
	qse_size_t i;

	if (in[0].type == QSE_SED_IOSTD_NULL)
	{
		/* if 'in' is specified, it must contains at least one 
		 * valid entry */
		qse_sed_seterrnum (sed, QSE_SED_EINVAL, QSE_NULL);
		return -1;
	}

	for (i = 0; in[i].type != QSE_SED_IOSTD_NULL; i++)
	{
		if (in[i].type != QSE_SED_IOSTD_FILE &&
		    in[i].type != QSE_SED_IOSTD_STR &&
		    in[i].type != QSE_SED_IOSTD_SIO)
		{
			qse_sed_seterrnum (sed, QSE_SED_EINVAL, QSE_NULL);
			return -1;
		}
	}

	return 0;
}

static qse_sio_t* open_sio_file (qse_sed_t* sed, const qse_char_t* file, int flags)
{
	qse_sio_t* sio;

	sio = qse_sio_open (sed->mmgr, 0, file, flags);
	if (sio == QSE_NULL)
	{
		qse_cstr_t ea;
		ea.ptr = file;
		ea.len = qse_strlen (file);
		qse_sed_seterrnum (sed, QSE_SED_EIOFIL, &ea);
	}
	return sio;
}

struct sio_std_name_t
{
	const qse_char_t* ptr;
	qse_size_t        len;
};

static struct sio_std_name_t sio_std_names[] =
{
	{ QSE_T("stdin"),   5 },
	{ QSE_T("stdout"),  6 },
	{ QSE_T("stderr"),  6 }
};

static qse_sio_t* open_sio_std (qse_sed_t* sed, qse_sio_std_t std, int flags)
{
	qse_sio_t* sio;

	sio = qse_sio_openstd (sed->mmgr, 0, std, flags);
	if (sio == QSE_NULL)
	{
		qse_cstr_t ea;
		ea.ptr = sio_std_names[std].ptr;
		ea.len = sio_std_names[std].len;
		qse_sed_seterrnum (sed, QSE_SED_EIOFIL, &ea);
	}
	return sio;
}

static void close_main_stream (
	qse_sed_t* sed, qse_sed_io_arg_t* arg, qse_sed_iostd_t* io)
{
	switch (io->type)
	{
		case QSE_SED_IOSTD_FILE:
			qse_sio_close (arg->handle);
			break;

		case QSE_SED_IOSTD_STR:
			/* nothing to do for input. 
			 * i don't close xtn->e.out.memstr intentionally.
			 * i close this in qse_awk_execstd()
			 */
			break;

		case QSE_SED_IOSTD_SIO:
			/* nothing to do */
			break;

		default:
			/* do nothing */
			break;
	}

}

static int open_input_stream (
	qse_sed_t* sed, qse_sed_io_arg_t* arg, qse_sed_iostd_t* io, xtn_in_t* base)
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (sed);

	QSE_ASSERT (io != QSE_NULL);
	switch (io->type)
	{
		case QSE_SED_IOSTD_FILE:
		{
			qse_sio_t* sio;
			
			if (io->u.file.path == QSE_NULL || 
			    (io->u.file.path[0] == QSE_T('-') && 
			     io->u.file.path[1] == QSE_T('\0')))
			{
				sio = open_sio_std (
					sed, QSE_SIO_STDIN, 
					QSE_SIO_READ | QSE_SIO_IGNOREMBWCERR
				);
			}
			else
			{
				sio = open_sio_file (
					sed, io->u.file.path,
					QSE_SIO_READ | QSE_SIO_IGNOREMBWCERR);
			}
			if (sio == QSE_NULL) return -1;
			if (io->u.file.cmgr) qse_sio_setcmgr (sio, io->u.file.cmgr);
			arg->handle = sio;
			break;
		}

		case QSE_SED_IOSTD_STR:
			/* don't store anything to arg->handle */
			base->mempos = 0;
			break;

		case QSE_SED_IOSTD_SIO:
			arg->handle = io->u.sio;
			break;

		default:
			QSE_ASSERTX (
				!"should never happen",
				"io-type must be one of SIO,FILE,STR"
			);
			qse_sed_seterrnum (sed, QSE_SED_EINTERN, QSE_NULL);
			return -1;
	}


	if (base == &xtn->s.in)
	{
		/* reset script location */
		if (io->type == QSE_SED_IOSTD_FILE) 
		{
			qse_sed_setcompid (
				sed, 
				((io->u.file.path == QSE_NULL)? 
					sio_std_names[QSE_SIO_STDIN].ptr: io->u.file.path)
			);
		}
		else 
		{
			qse_char_t buf[64];

			/* format an identifier to be something like M#1, S#5 */
			buf[0] = (io->type == QSE_SED_IOSTD_STR)? QSE_T('M'): QSE_T('S');
			buf[1] = QSE_T('#');
			int_to_str (io - xtn->s.in.ptr, &buf[2], QSE_COUNTOF(buf) - 2);

			/* don't care about failure int_to_str() though it's not 
			 * likely to happen */
			qse_sed_setcompid (sed, buf); 
		}
		sed->src.loc.line = 1; 
		sed->src.loc.colm = 1;
	}
	return 0;
}

static int open_output_stream (qse_sed_t* sed, qse_sed_io_arg_t* arg, qse_sed_iostd_t* io)
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (sed);

	QSE_ASSERT (io != QSE_NULL);
	switch (io->type)
	{
		case QSE_SED_IOSTD_FILE:
		{
			qse_sio_t* sio;
			if (io->u.file.path == QSE_NULL ||
			    (io->u.file.path[0] == QSE_T('-') && 
			     io->u.file.path[1] == QSE_T('\0')))
			{
				sio = open_sio_std (
					sed, QSE_SIO_STDOUT,
					QSE_SIO_WRITE |
					QSE_SIO_CREATE |
					QSE_SIO_TRUNCATE |
					QSE_SIO_IGNOREMBWCERR |
					QSE_SIO_LINEBREAK
				);
			}
			else
			{
				sio = open_sio_file (
					sed, io->u.file.path,
					QSE_SIO_WRITE |
					QSE_SIO_CREATE |
					QSE_SIO_TRUNCATE |
					QSE_SIO_IGNOREMBWCERR
				);
			}
			if (sio == QSE_NULL) return -1;
			if (io->u.file.cmgr) qse_sio_setcmgr (sio, io->u.file.cmgr);
			arg->handle = sio;
			break;
		}

		case QSE_SED_IOSTD_STR:
			/* don't store anything to arg->handle */
			xtn->e.out.memstr = qse_str_open (sed->mmgr, 0, 512);
			if (xtn->e.out.memstr == QSE_NULL)
			{
				qse_sed_seterrnum (sed, QSE_SED_ENOMEM, QSE_NULL);
				return -1;
			}
			break;

		case QSE_SED_IOSTD_SIO:
			arg->handle = io->u.sio;
			break;

		default:
			QSE_ASSERTX (
				!"should never happen",
				"io-type must be one of SIO,FILE,STR"
			);
			qse_sed_seterrnum (sed, QSE_SED_EINTERN, QSE_NULL);
			return -1;
	}

	return 0;
}

static qse_ssize_t read_input_stream (
	qse_sed_t* sed, qse_sed_io_arg_t* arg,
	qse_char_t* buf, qse_size_t len, xtn_in_t* base)
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (sed);
	qse_sed_iostd_t* io, * next;
	void* old, * new;
	qse_ssize_t n = 0;

	if (len > QSE_TYPE_MAX(qse_ssize_t)) len = QSE_TYPE_MAX(qse_ssize_t);

	do
	{
		io = base->cur;

		if (base == &xtn->s.in && xtn->s.newline_squeezed) 
		{
			xtn->s.newline_squeezed = 0;
			goto open_next;
		}

		QSE_ASSERT (io != QSE_NULL);

		if (io->type == QSE_SED_IOSTD_STR)
		{
			n = 0;
			while (base->mempos < io->u.str.len && n < len)
				buf[n++] = io->u.str.ptr[base->mempos++];
		}
		else n = qse_sio_getstrn (arg->handle, buf, len);

		if (n != 0) 
		{
			if (n <= -1)
			{
				if (io->type == QSE_SED_IOSTD_FILE)
				{
					qse_cstr_t ea;
					if (io->u.file.path)
					{
						ea.ptr = io->u.file.path;
						ea.len = qse_strlen (io->u.file.path);
					}
					else
					{
						ea.ptr = sio_std_names[QSE_SIO_STDIN].ptr;
						ea.len = sio_std_names[QSE_SIO_STDIN].len;
					}
					qse_sed_seterrnum (sed, QSE_SED_EIOFIL, &ea);
				}
			}
			else if (base == &xtn->s.in)
			{
				xtn->s.last = buf[n-1];
			}

			break;
		}
	
		/* ============================================= */
		/* == end of file on the current input stream == */
		/* ============================================= */

		if (base == &xtn->s.in && xtn->s.last != QSE_T('\n'))
		{
			/* TODO: different line termination convension */
			buf[0] = QSE_T('\n'); 
			n = 1;
			xtn->s.newline_squeezed = 1;
			break;
		}

	open_next:
		next = base->cur + 1;
		if (next->type == QSE_SED_IOSTD_NULL) 
		{
			/* no next stream available - return 0 */	
			break; 
		}

		old = arg->handle;

		/* try to open the next input stream */
		if (open_input_stream (sed, arg, next, base) <= -1)
		{
			/* failed to open the next input stream */
			if (next->type == QSE_SED_IOSTD_FILE)
			{
				qse_cstr_t ea;
				if (next->u.file.path)
				{
					ea.ptr = next->u.file.path;
					ea.len = qse_strlen (next->u.file.path);
				}
				else
				{
					ea.ptr = sio_std_names[QSE_SIO_STDIN].ptr;
					ea.len = sio_std_names[QSE_SIO_STDIN].len;
				}
				qse_sed_seterrnum (sed, QSE_SED_EIOFIL, &ea);
			}

			n = -1;
			break;
		}

		/* successfuly opened the next input stream */
		new = arg->handle;

		arg->handle = old;

		/* close the previous stream */
		close_main_stream (sed, arg, io);

		arg->handle = new;

		base->cur++;
	}
	while (1);

	return n;
}

static qse_ssize_t s_in (
	qse_sed_t* sed, qse_sed_io_cmd_t cmd, qse_sed_io_arg_t* arg,
	qse_char_t* buf, qse_size_t len)
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (sed);

	switch (cmd)
	{
		case QSE_SED_IO_OPEN:
		{
			if (open_input_stream (sed, arg, xtn->s.in.cur, &xtn->s.in) <= -1) return -1;
			return 1;
		}

		case QSE_SED_IO_CLOSE:
		{
			close_main_stream (sed, arg, xtn->s.in.cur);
			return 0;
		}

		case QSE_SED_IO_READ:
		{
			return read_input_stream (sed, arg, buf, len, &xtn->s.in);
		}

		default:
		{
			QSE_ASSERTX (
				!"should never happen",
				"cmd must be one of OPEN,CLOSE,READ"
			);
			qse_sed_seterrnum (sed, QSE_SED_EINTERN, QSE_NULL);
			return -1;
		}
	}
}

static qse_ssize_t x_in (
	qse_sed_t* sed, qse_sed_io_cmd_t cmd, qse_sed_io_arg_t* arg,
	qse_char_t* buf, qse_size_t len)
{
	qse_sio_t* sio;
	xtn_t* xtn = (xtn_t*) QSE_XTN (sed);

	switch (cmd)
	{
		case QSE_SED_IO_OPEN:
		{
			if (arg->path == QSE_NULL)
			{
				/* no file specified. console stream */
				if (xtn->e.in.ptr == QSE_NULL) 
				{
					/* QSE_NULL passed into qse_sed_exec() for input */
					sio = open_sio_std (
						sed, QSE_SIO_STDIN, QSE_SIO_READ | QSE_SIO_IGNOREMBWCERR);
					if (sio == QSE_NULL) return -1;
					arg->handle = sio;
				}
				else
				{
					if (open_input_stream (sed, arg, xtn->e.in.cur, &xtn->e.in) <= -1) return -1;
				}
			}
			else
			{
				sio = open_sio_file (sed, arg->path, QSE_SIO_READ | QSE_SIO_IGNOREMBWCERR);
				if (sio == QSE_NULL) return -1;
				arg->handle = sio;
			}

			return 1;
		}

		case QSE_SED_IO_CLOSE:
		{
			if (arg->path == QSE_NULL)
			{
				/* main data stream */
				if (xtn->e.in.ptr == QSE_NULL) 
					qse_sio_close (arg->handle);
				else
					close_main_stream (sed, arg, xtn->e.in.cur);
			}
			else
			{
				qse_sio_close (arg->handle);
			}

			return 0;
		}

		case QSE_SED_IO_READ:
		{
			if (arg->path == QSE_NULL)
			{
				/* main data stream */
				if (xtn->e.in.ptr == QSE_NULL)
				{
					qse_ssize_t n;
					n = qse_sio_getstrn (arg->handle, buf, len);
					if (n <= -1)
					{
						qse_cstr_t ea;
						ea.ptr = sio_std_names[QSE_SIO_STDIN].ptr;
						ea.len = sio_std_names[QSE_SIO_STDIN].len;
						qse_sed_seterrnum (sed, QSE_SED_EIOFIL, &ea);
					}
					return n;
				}
				else
					return read_input_stream (sed, arg, buf, len, &xtn->e.in);
			}
			else
			{
				qse_ssize_t n;
				n = qse_sio_getstrn (arg->handle, buf, len);
				if (n <= -1)
				{
					qse_cstr_t ea;
					ea.ptr = arg->path;
					ea.len = qse_strlen (arg->path);
					qse_sed_seterrnum (sed, QSE_SED_EIOFIL, &ea);
				}
				return n;
			}
		}

		default:
			QSE_ASSERTX (
				!"should never happen",
				"cmd must be one of OPEN,CLOSE,READ"
			);
			qse_sed_seterrnum (sed, QSE_SED_EINTERN, QSE_NULL);
			return -1;
	}
}

static qse_ssize_t x_out (
	qse_sed_t* sed, qse_sed_io_cmd_t cmd, qse_sed_io_arg_t* arg,
	qse_char_t* dat, qse_size_t len)
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (sed);
	qse_sio_t* sio;

	switch (cmd)
	{
		case QSE_SED_IO_OPEN:
		{
			if (arg->path == QSE_NULL)
			{
				/* main data stream */

				if (xtn->e.out.ptr == QSE_NULL) 
				{
					/* QSE_NULL passed into qse_sed_execstd() for output */
					sio = open_sio_std (
						sed, QSE_SIO_STDOUT,
						QSE_SIO_WRITE |
						QSE_SIO_CREATE |
						QSE_SIO_TRUNCATE |
						QSE_SIO_IGNOREMBWCERR |
						QSE_SIO_LINEBREAK
					);
					if (sio == QSE_NULL) return -1;
					arg->handle = sio;
				}
				else
				{
					if (open_output_stream (sed, arg, xtn->e.out.ptr) <= -1) return -1;
				}
			}
			else
			{

				sio = open_sio_file (
					sed, arg->path,
					QSE_SIO_WRITE |
					QSE_SIO_CREATE |
					QSE_SIO_TRUNCATE |
					QSE_SIO_IGNOREMBWCERR
				);
				if (sio == QSE_NULL) return -1;
				arg->handle = sio;
			}

			return 1;
		}

		case QSE_SED_IO_CLOSE:
		{
			if (arg->path == QSE_NULL)
			{
				if (xtn->e.out.ptr == QSE_NULL) 
					qse_sio_close (arg->handle);
				else
					close_main_stream (sed, arg, xtn->e.out.ptr);
			}
			else
			{
				qse_sio_close (arg->handle);
			}
			return 0;
		}

		case QSE_SED_IO_WRITE:
		{
			if (arg->path == QSE_NULL)
			{
				/* main data stream */
				if (xtn->e.out.ptr == QSE_NULL)
				{
					qse_ssize_t n;
					n = qse_sio_putstrn (arg->handle, dat, len);
					if (n <= -1)
					{
						qse_cstr_t ea;
						ea.ptr = sio_std_names[QSE_SIO_STDOUT].ptr;
						ea.len = sio_std_names[QSE_SIO_STDOUT].len;
						qse_sed_seterrnum (sed, QSE_SED_EIOFIL, &ea);
					}
					return n;
				}
				else
				{
					qse_sed_iostd_t* io = xtn->e.out.ptr;
					if (io->type == QSE_SED_IOSTD_STR)
					{
						if (len > QSE_TYPE_MAX(qse_ssize_t)) len = QSE_TYPE_MAX(qse_ssize_t);

						if (qse_str_ncat (xtn->e.out.memstr, dat, len) == (qse_size_t)-1)
						{
							qse_sed_seterrnum (sed, QSE_SED_ENOMEM, QSE_NULL);
							return -1;
						}

						return len;
					}
					else
					{
						qse_ssize_t n;
						n = qse_sio_putstrn (arg->handle, dat, len);
						if (n <= -1)
						{
							qse_cstr_t ea;
							if (io->u.file.path)
							{
								ea.ptr = io->u.file.path;
								ea.len = qse_strlen(io->u.file.path);
							}
							else
							{
								ea.ptr = sio_std_names[QSE_SIO_STDOUT].ptr;
								ea.len = sio_std_names[QSE_SIO_STDOUT].len;
							}
							qse_sed_seterrnum (sed, QSE_SED_EIOFIL, &ea);
						}
						return n;
					}
				}
			}
			else
			{
				qse_ssize_t n;
				n = qse_sio_putstrn (arg->handle, dat, len);
				if (n <= -1)
				{
					qse_cstr_t ea;
					ea.ptr = arg->path;
					ea.len = qse_strlen(arg->path);
					qse_sed_seterrnum (sed, QSE_SED_EIOFIL, &ea);
				}
				return n;
			}
		}
	
		default:
			QSE_ASSERTX (
				!"should never happen",
				"cmd must be one of OPEN,CLOSE,WRITE"
			);
			qse_sed_seterrnum (sed, QSE_SED_EINTERN, QSE_NULL);
			return -1;
	}
}

int qse_sed_compstd (qse_sed_t* sed, qse_sed_iostd_t in[], qse_size_t* count)
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (sed);
	int ret;

	if (in == QSE_NULL)
	{
		/* it requires a valid array unlike qse_sed_execstd(). */
		qse_sed_seterrnum (sed, QSE_SED_EINVAL, QSE_NULL);
		return -1;
	}
	if (verify_iostd_in (sed, in) <= -1) return -1;

	QSE_MEMSET (&xtn->s, 0, QSE_SIZEOF(xtn->s));
	xtn->s.in.ptr = in;
	xtn->s.in.cur = in;

	ret = qse_sed_comp (sed, s_in);

	if (count) *count = xtn->s.in.cur - xtn->s.in.ptr;

	return ret;
}

int qse_sed_execstd (
	qse_sed_t* sed, qse_sed_iostd_t in[], qse_sed_iostd_t* out)
{
	int n;
	xtn_t* xtn = (xtn_t*) QSE_XTN (sed);

	if (in && verify_iostd_in (sed, in) <= -1) return -1;

	if (out)
	{
		if (out->type != QSE_SED_IOSTD_FILE &&
		    out->type != QSE_SED_IOSTD_STR &&
		    out->type != QSE_SED_IOSTD_SIO)
		{
			qse_sed_seterrnum (sed, QSE_SED_EINVAL, QSE_NULL);
			return -1;
		}
	}

	QSE_MEMSET (&xtn->e, 0, QSE_SIZEOF(xtn->e));
	xtn->e.in.ptr = in;
	xtn->e.in.cur = in;
	xtn->e.out.ptr = out;

	n = qse_sed_exec (sed, x_in, x_out);

	if (out && out->type == QSE_SED_IOSTD_STR)
	{
		if (n >= 0)
		{
			QSE_ASSERT (xtn->e.out.memstr != QSE_NULL);
			qse_str_yield (xtn->e.out.memstr, &out->u.str, 0);
		}
		if (xtn->e.out.memstr) qse_str_close (xtn->e.out.memstr);
	}

	return n;
}

int qse_sed_compstdfile (
	qse_sed_t* sed, const qse_char_t* file, qse_cmgr_t* cmgr)
{
	qse_sed_iostd_t in[2];

	in[0].type = QSE_SED_IOSTD_FILE;
	in[0].u.file.path = file;
	in[0].u.file.cmgr = cmgr;
	in[1].type = QSE_SED_IOSTD_NULL;

	return qse_sed_compstd (sed, in, QSE_NULL);
}

int qse_sed_compstdstr (qse_sed_t* sed, const qse_char_t* script)
{
	qse_sed_iostd_t in[2];

	in[0].type = QSE_SED_IOSTD_STR;
	in[0].u.str.ptr = (qse_char_t*)script;
	in[0].u.str.len = qse_strlen(script);
	in[1].type = QSE_SED_IOSTD_NULL;

	return qse_sed_compstd (sed, in, QSE_NULL);
}

int qse_sed_compstdxstr (qse_sed_t* sed, const qse_cstr_t* script)
{
	qse_sed_iostd_t in[2];

	in[0].type = QSE_SED_IOSTD_STR;
	in[0].u.str = *script;
	in[1].type = QSE_SED_IOSTD_NULL;

	return qse_sed_compstd (sed, in, QSE_NULL);
}

int qse_sed_execstdfile (
	qse_sed_t* sed, const qse_char_t* infile,
	const qse_char_t* outfile, qse_cmgr_t* cmgr)
{
	qse_sed_iostd_t in[2];
	qse_sed_iostd_t out;
	qse_sed_iostd_t* pin = QSE_NULL, * pout = QSE_NULL;

	if (infile)
	{
		in[0].type = QSE_SED_IOSTD_FILE;
		in[0].u.file.path = infile;
		in[0].u.file.cmgr = cmgr;
		in[1].type = QSE_SED_IOSTD_NULL;
		pin = in;
	}

	if (outfile)
	{
		out.type = QSE_SED_IOSTD_FILE;
		out.u.file.path = outfile;
		out.u.file.cmgr = cmgr;
		pout = &out;
	}

	return qse_sed_execstd (sed, pin, pout);
}

int qse_sed_execstdxstr (
	qse_sed_t* sed, const qse_cstr_t* instr,
	qse_cstr_t* outstr, qse_cmgr_t* cmgr)
{
	qse_sed_iostd_t in[2];
	qse_sed_iostd_t out;
	int n;

	in[0].type = QSE_SED_IOSTD_STR;
	in[0].u.str = *instr;
	in[1].type = QSE_SED_IOSTD_NULL;

	out.type = QSE_SED_IOSTD_STR;

	n = qse_sed_execstd (sed, in, &out);

	if (n >= 0) *outstr = out.u.str;

	return n;
}

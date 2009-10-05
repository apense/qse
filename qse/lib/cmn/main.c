/*
 * $Id: main.c 463 2008-12-09 06:52:03Z baconevi $
 *
    Copyright 2006-2009 Chung, Hyung-Hwan.
    This file is part of QSE.

    QSE is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as 
    published by the Free Software Foundation, either version 3 of 
    the License, or (at your option) any later version.

    QSE is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public 
    License along with QSE. If not, see <http://www.gnu.org/licenses/>.
 */

#include <qse/cmn/main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#if defined(_WIN32) && !defined(__MINGW32__)

int qse_runmain (int argc, qse_achar_t* argv[], int(*mf) (int,qse_char_t*[]))
{
	return mf (argc, argv);
}

#elif defined(QSE_CHAR_IS_WCHAR)

int qse_runmain (int argc, qse_achar_t* argv[], int(*mf) (int,qse_char_t*[]))
{
	int i, ret;
	qse_char_t** v;

	setlocale (LC_ALL, "");

	v = (qse_char_t**) malloc (argc * QSE_SIZEOF(qse_char_t*));
	if (v == NULL) return -1;

	for (i = 0; i < argc; i++) v[i] = NULL;
	for (i = 0; i < argc; i++) 
	{
		qse_size_t n, len, rem;
		char* p = argv[i];

		len = 0; rem = strlen (p);
		while (*p != '\0')
		{
			int x = mblen (p, rem); 
			if (x == -1) 
			{
				ret = -1;
				goto exit_main;
			}
			if (x == 0) break;
			p += x; rem -= x; len++;
		}

	#if (defined(vms) || defined(__vms)) && (QSE_SIZEOF_VOID_P >= 8)
		v[i] = (qse_char_t*) _malloc32 ((len+1)*QSE_SIZEOF(qse_char_t));
	#else
		v[i] = (qse_char_t*) malloc ((len+1)*QSE_SIZEOF(qse_char_t));
	#endif
		if (v[i] == NULL) 
		{
			ret = -1;
			goto exit_main;
		}

		n = mbstowcs (v[i], argv[i], len);
		if (n == (size_t)-1) 
		{
			/* error */
			return -1;
		}

		if (n == len) v[i][len] = QSE_T('\0');
	}

	/* TODO: envp... */
	//ret = mf (argc, v, NULL);
	ret = mf (argc, v);

exit_main:
	for (i = 0; i < argc; i++) 
	{
		if (v[i] != NULL) free (v[i]);
	}
	free (v);

	return ret;
}

#else

int qse_runmain (int argc, qse_achar_t* argv[], int(*mf) (int,qse_char_t*[]))
{
	return mf (argc, argv);
}

#endif
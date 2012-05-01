/*
 * $Id: str-pbrk.c 576 2011-09-23 14:52:22Z hyunghwan.chung $
 *
    Copyright 2006-2011 Chung, Hyung-Hwan.
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

#include <qse/cmn/str.h>

qse_mchar_t* qse_mbspbrk (const qse_mchar_t* str1, const qse_mchar_t* str2)
{
	const qse_mchar_t* p1, * p2;

	for (p1 = str1; *p1 != QSE_MT('\0'); p1++)
	{
		for (p2 = str2; *p2 != QSE_MT('\0'); p2++)
		{
			if (*p2 == *p1) return (qse_mchar_t*)p1;
		}
	}

	return QSE_NULL;
}

qse_mchar_t* qse_mbsxpbrk (
	const qse_mchar_t* str1, qse_size_t len, const qse_mchar_t* str2)
{
	const qse_mchar_t* p1, * p2;
	const qse_mchar_t* e1 = str1 + len;

	for (p1 = str1; p1 < e1; p1++)
	{
		for (p2 = str2; *p2 != QSE_MT('\0'); p2++)
		{
			if (*p2 == *p1) return (qse_mchar_t*)p1;
		}
	}

	return QSE_NULL;
}


qse_mchar_t* qse_mbsrpbrk (const qse_mchar_t* str1, const qse_mchar_t* str2)
{
	const qse_mchar_t* p1, * p2;

	for (p1 = str1; *p1 != QSE_MT('\0'); p1++);

	while (p1 > str1)
	{
		p1--;
		for (p2 = str2; *p2 != QSE_MT('\0'); p2++)
		{
			if (*p2 == *p1) return (qse_mchar_t*)p1;
		}
	}

	return QSE_NULL;
}

qse_mchar_t* qse_mbsxrpbrk (const qse_mchar_t* str1, qse_size_t len, const qse_mchar_t* str2)
{
	const qse_mchar_t* p1, * p2;

	p1 = str1 + len;

	while (p1 > str1)
	{
		p1--;
		for (p2 = str2; *p2 != QSE_MT('\0'); p2++)
		{
			if (*p2 == *p1) return (qse_mchar_t*)p1;
		}
	}

	return QSE_NULL;
}


qse_wchar_t* qse_wcspbrk (const qse_wchar_t* str1, const qse_wchar_t* str2)
{
	const qse_wchar_t* p1, * p2;

	for (p1 = str1; *p1 != QSE_WT('\0'); p1++)
	{
		for (p2 = str2; *p2 != QSE_WT('\0'); p2++)
		{
			if (*p2 == *p1) return (qse_wchar_t*)p1;
		}
	}

	return QSE_NULL;
}

qse_wchar_t* qse_wcsxpbrk (
	const qse_wchar_t* str1, qse_size_t len, const qse_wchar_t* str2)
{
	const qse_wchar_t* p1, * p2;
	const qse_wchar_t* e1 = str1 + len;

	for (p1 = str1; p1 < e1; p1++)
	{
		for (p2 = str2; *p2 != QSE_WT('\0'); p2++)
		{
			if (*p2 == *p1) return (qse_wchar_t*)p1;
		}
	}

	return QSE_NULL;
}


qse_wchar_t* qse_wcsrpbrk (const qse_wchar_t* str1, const qse_wchar_t* str2)
{
	const qse_wchar_t* p1, * p2;

	for (p1 = str1; *p1 != QSE_WT('\0'); p1++);

	while (p1 > str1)
	{
		p1--;
		for (p2 = str2; *p2 != QSE_WT('\0'); p2++)
		{
			if (*p2 == *p1) return (qse_wchar_t*)p1;
		}
	}

	return QSE_NULL;
}

qse_wchar_t* qse_wcsxrpbrk (const qse_wchar_t* str1, qse_size_t len, const qse_wchar_t* str2)
{
	const qse_wchar_t* p1, * p2;

	p1 = str1 + len;

	while (p1 > str1)
	{
		p1--;
		for (p2 = str2; *p2 != QSE_WT('\0'); p2++)
		{
			if (*p2 == *p1) return (qse_wchar_t*)p1;
		}
	}

	return QSE_NULL;
}
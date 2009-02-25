/*
 * $Id: Awk.cpp 75 2009-02-22 14:10:34Z hyunghwan.chung $
 *
   Copyright 2006-2009 Chung, Hyung-Hwan.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#include <qse/awk/Awk.hpp>
#include <qse/cmn/str.h>
#include "../cmn/mem.h"
#include "awk.h"

#include <qse/utl/stdio.h>
/////////////////////////////////
QSE_BEGIN_NAMESPACE(QSE)
/////////////////////////////////

//////////////////////////////////////////////////////////////////
// Awk::Source
//////////////////////////////////////////////////////////////////

struct xtn_t
{
	Awk* awk;
};

struct rxtn_t
{
	Awk::Run* run;
};

Awk::Source::Source (Mode mode): mode (mode), handle (QSE_NULL)
{
}

Awk::Source::Mode Awk::Source::getMode () const
{
	return this->mode;
}

const void* Awk::Source::getHandle () const
{
	return this->handle;
}

void Awk::Source::setHandle (void* handle)
{
	this->handle = handle;
}

//////////////////////////////////////////////////////////////////
// Awk::RIO
//////////////////////////////////////////////////////////////////

Awk::RIO::RIO (rtx_t* rtx, riod_t* riod): rtx (rtx), riod (riod)
{
}

const Awk::char_t* Awk::RIO::getName () const
{
	return this->riod->name;
}

const void* Awk::RIO::getHandle () const
{
	return this->riod->handle;
}

void Awk::RIO::setHandle (void* handle)
{
	this->riod->handle = handle;
}

Awk::RIO::operator Awk::Awk* () const 
{
	rxtn_t* rxtn = (rxtn_t*) QSE_XTN (this->rtx);
	return rxtn->run->awk;
}

Awk::RIO::operator Awk::awk_t* () const 
{
	return qse_awk_rtx_getawk (this->rtx);
}

Awk::RIO::operator Awk::riod_t* () const
{
	return this->riod;
}

Awk::RIO::operator Awk::rtx_t* () const
{
	return this->rtx;
}

//////////////////////////////////////////////////////////////////
// Awk::Pipe
//////////////////////////////////////////////////////////////////

Awk::Pipe::Pipe (rtx_t* rtx, riod_t* riod): RIO (rtx, riod)
{
}

Awk::Pipe::Mode Awk::Pipe::getMode () const
{
	return (Mode)riod->mode;
}

//////////////////////////////////////////////////////////////////
// Awk::File
//////////////////////////////////////////////////////////////////

Awk::File::File (rtx_t* rtx, riod_t* riod): RIO (rtx, riod)
{
}

Awk::File::Mode Awk::File::getMode () const
{
	return (Mode)riod->mode;
}

//////////////////////////////////////////////////////////////////
// Awk::Console
//////////////////////////////////////////////////////////////////

Awk::Console::Console (rtx_t* rtx, riod_t* riod): 
	RIO (rtx, riod), filename(QSE_NULL)
{
}

Awk::Console::~Console ()
{
	if (filename != QSE_NULL)
	{
		qse_awk_free ((awk_t*)this, filename);
	}
}

int Awk::Console::setFileName (const char_t* name)
{
	if (riod->mode == READ)
	{
		return qse_awk_rtx_setfilename (
			this->rtx, name, qse_strlen(name));
	}
	else
	{
		return qse_awk_rtx_setofilename (
			this->rtx, name, qse_strlen(name));
	}
}

int Awk::Console::setFNR (long_t fnr)
{
	qse_awk_val_t* tmp;
	int n;

	tmp = qse_awk_rtx_makeintval (this->rtx, fnr);
	if (tmp == QSE_NULL) return -1;

	qse_awk_rtx_refupval (this->rtx, tmp);
	n = qse_awk_rtx_setgbl (this->rtx, QSE_AWK_GBL_FNR, tmp);
	qse_awk_rtx_refdownval (this->rtx, tmp);

	return n;
}

Awk::Console::Mode Awk::Console::getMode () const
{
	return (Mode)riod->mode;
}

//////////////////////////////////////////////////////////////////
// Awk::Argument
//////////////////////////////////////////////////////////////////

Awk::Argument::Argument (Run& run): run (&run), val (QSE_NULL)
{
	this->inum = 0;
	this->rnum = 0.0;

	this->str.ptr = QSE_NULL;
	this->str.len = 0;
}

Awk::Argument::Argument (Run* run): run (run), val (QSE_NULL)
{
	this->inum = 0;
	this->rnum = 0.0;

	this->str.ptr = QSE_NULL;
	this->str.len = 0;
}

Awk::Argument::Argument (): run (QSE_NULL), val (QSE_NULL)
{
	this->inum = 0;
	this->rnum = 0.0;

	this->str.ptr = QSE_NULL;
	this->str.len = 0;
}

Awk::Argument::~Argument ()
{
	clear ();
}

void Awk::Argument::clear ()
{
	if (this->val == QSE_NULL)
	{
		/* case 1. not initialized.
		 * case 2. initialized with the second init.
		 * none of the cases creates a new string so the sttring
		 * that str.ptr is pointing at doesn't have to be freed */
		this->str.ptr = QSE_NULL;
		this->str.len = 0;
	}
	else if (QSE_AWK_VAL_TYPE(this->val) == QSE_AWK_VAL_MAP)
	{
		QSE_ASSERT (this->run != QSE_NULL);

		/* when the value is a map, str.ptr and str.len are
		 * used for index iteration in getFirstIndex & getNextIndex */
		qse_awk_rtx_refdownval (this->run->run, this->val);
		this->val = QSE_NULL;
	}
	else
	{
		QSE_ASSERT (this->run != QSE_NULL);

		if (this->str.ptr != QSE_NULL)
		{
			if (QSE_AWK_VAL_TYPE(this->val) != QSE_AWK_VAL_STR)
			{
				awk_t* awk = this->run->awk->awk;
				qse_awk_free (awk, this->str.ptr);
			}

			this->str.ptr = QSE_NULL;
			this->str.len = 0;
		}

		if (this->val != QSE_NULL) 
		{
			qse_awk_rtx_refdownval (this->run->run, this->val);
			this->val = QSE_NULL;
		}

	}

	this->rnum = 0.0;
	this->inum = 0;
}

void* Awk::Argument::operator new (size_t n, awk_t* awk) throw ()
{
	void* ptr = qse_awk_alloc (awk, QSE_SIZEOF(awk) + n);
	if (ptr == QSE_NULL) return QSE_NULL;

	*(awk_t**)ptr = awk;
	return (char*)ptr+QSE_SIZEOF(awk);
}

void* Awk::Argument::operator new[] (size_t n, awk_t* awk) throw ()
{
	void* ptr = qse_awk_alloc (awk, QSE_SIZEOF(awk) + n);
	if (ptr == QSE_NULL) return QSE_NULL;

	*(awk_t**)ptr = awk;
	return (char*)ptr+QSE_SIZEOF(awk);
}

#if !defined(__BORLANDC__)
void Awk::Argument::operator delete (void* ptr, awk_t* awk)
{
	qse_awk_free (awk, (char*)ptr-QSE_SIZEOF(awk));
}

void Awk::Argument::operator delete[] (void* ptr, awk_t* awk)
{
	qse_awk_free (awk, (char*)ptr-QSE_SIZEOF(awk));
}
#endif

void Awk::Argument::operator delete (void* ptr)
{
	void* p = (char*)ptr-QSE_SIZEOF(awk_t*);
	qse_awk_free (*(awk_t**)p, p);
}

void Awk::Argument::operator delete[] (void* ptr)
{
	void* p = (char*)ptr-QSE_SIZEOF(awk_t*);
	qse_awk_free (*(awk_t**)p, p);
}

int Awk::Argument::init (val_t* v)
{
	// this method is used internally only
	// and should never be called more than once 
	QSE_ASSERT (this->val == QSE_NULL);
	QSE_ASSERT (v != QSE_NULL);

	qse_awk_rtx_refupval (this->run->run, v);
	this->val = v;

	if (QSE_AWK_VAL_TYPE(v) == QSE_AWK_VAL_STR)
	{
		int n = qse_awk_rtx_valtonum (
			this->run->run, v, &this->inum, &this->rnum);
		if (n == 0) 
		{
			this->rnum = (qse_real_t)this->inum;
			this->str.ptr = ((qse_awk_val_str_t*)this->val)->ptr;
			this->str.len = ((qse_awk_val_str_t*)this->val)->len;
			return 0;
		}
		else if (n == 1) 
		{
			this->inum = (qse_long_t)this->rnum;
			this->str.ptr = ((qse_awk_val_str_t*)this->val)->ptr;
			this->str.len = ((qse_awk_val_str_t*)this->val)->len;
			return 0;
		}
	}
	else if (QSE_AWK_VAL_TYPE(v) == QSE_AWK_VAL_INT)
	{
		this->inum = ((qse_awk_val_int_t*)v)->val;
		this->rnum = (qse_real_t)((qse_awk_val_int_t*)v)->val;

		this->str.ptr = qse_awk_rtx_valtostr (
			this->run->run, v, 0, QSE_NULL, &this->str.len);
		if (this->str.ptr != QSE_NULL) return 0;
	}
	else if (QSE_AWK_VAL_TYPE(v) == QSE_AWK_VAL_REAL)
	{
		this->inum = (qse_long_t)((qse_awk_val_real_t*)v)->val;
		this->rnum = ((qse_awk_val_real_t*)v)->val;

		this->str.ptr = qse_awk_rtx_valtostr (
			this->run->run, v, 0, QSE_NULL, &this->str.len);
		if (this->str.ptr != QSE_NULL) return 0;
	}
	else if (QSE_AWK_VAL_TYPE(v) == QSE_AWK_VAL_NIL)
	{
		this->inum = 0;
		this->rnum = 0.0;

		this->str.ptr = qse_awk_rtx_valtostr (
			this->run->run, v, 0, QSE_NULL, &this->str.len);
		if (this->str.ptr != QSE_NULL) return 0;
	}
	else if (QSE_AWK_VAL_TYPE(v) == QSE_AWK_VAL_MAP)
	{
		this->inum = 0;
		this->rnum = 0.0;
		this->str.ptr = QSE_NULL;
		this->str.len = 0;
		return 0;
	}

	// an error has occurred
	qse_awk_rtx_refdownval (this->run->run, v);
	this->val = QSE_NULL;
	return -1;
}

int Awk::Argument::init (const char_t* str, size_t len)
{
	QSE_ASSERT (this->val == QSE_NULL);

	this->str.ptr = (char_t*)str;
	this->str.len = len;

	if (qse_awk_rtx_strtonum (this->run->run, 
		str, len, &this->inum, &this->rnum) == 0)
	{
		this->rnum = (real_t)this->inum;
	}
	else
	{
		this->inum = (long_t)this->rnum;
	}

	return 0;
}


Awk::long_t Awk::Argument::toInt () const
{
	return this->inum;
}

Awk::real_t Awk::Argument::toReal () const
{
	return this->rnum;
}

const Awk::char_t* Awk::Argument::toStr (size_t* len) const
{

	if (this->val != QSE_NULL && 
	    QSE_AWK_VAL_TYPE(this->val) == QSE_AWK_VAL_MAP)
	{
		*len = 0;
		return QSE_T("");
	}
	else if (this->str.ptr == QSE_NULL)
	{
		*len = 0;
		return QSE_T("");
	}
	else
	{
		*len = this->str.len;
		return this->str.ptr;
	}
}

bool Awk::Argument::isIndexed () const
{
	if (this->val == QSE_NULL) return false;
	return QSE_AWK_VAL_TYPE(this->val) == QSE_AWK_VAL_MAP;
}

int Awk::Argument::getIndexed (const char_t* idxptr, Awk::Argument& val) const
{
	return getIndexed (idxptr, qse_strlen(idxptr), val);
}

int Awk::Argument::getIndexed (
	const char_t* idxptr, size_t idxlen, Awk::Argument& val) const
{
	val.clear ();

	// not initialized yet. val is just nil. not an error
	if (this->val == QSE_NULL) return 0;
	// not a map. val is just nil. not an error 
	if (QSE_AWK_VAL_TYPE(this->val) != QSE_AWK_VAL_MAP) return 0;

	// get the value from the map.
	qse_awk_val_map_t* m = (qse_awk_val_map_t*)this->val;
	pair_t* pair = qse_map_search (m->map, idxptr, idxlen);

	// the key is not found. it is not an error. val is just nil 
	if (pair == QSE_NULL) return 0; 

	// if val.init fails, it should return an error 
	return val.init ((val_t*)QSE_MAP_VPTR(pair));
}

int Awk::Argument::getIndexed (long_t idx, Argument& val) const
{
	val.clear ();

	// not initialized yet. val is just nil. not an error
	if (this->val == QSE_NULL) return 0;

	// not a map. val is just nil. not an error 
	if (QSE_AWK_VAL_TYPE(this->val) != QSE_AWK_VAL_MAP) return 0;

	char_t ri[128];

	int rl = Awk::sprintf (
		(awk_t*)this->run->awk, ri, QSE_COUNTOF(ri), 
	#if QSE_SIZEOF_LONG_LONG > 0
		QSE_T("%lld"), (long long)idx
	#elif QSE_SIZEOF___INT64 > 0
		QSE_T("%I64d"), (__int64)idx
	#elif QSE_SIZEOF_LONG > 0
		QSE_T("%ld"), (long)idx
	#elif QSE_SIZEOF_INT > 0
		QSE_T("%d"), (int)idx
	#else
		#error unsupported size	
	#endif
		);

	if (rl < 0)
	{
		run->setError (ERR_INTERN, 0, QSE_NULL, 0);
		return -1;
	}

	// get the value from the map.
	qse_awk_val_map_t* m = (qse_awk_val_map_t*)this->val;
	pair_t* pair = qse_map_search (m->map, ri, rl);

	// the key is not found. it is not an error. val is just nil 
	if (pair == QSE_NULL) return 0; 

	// if val.init fails, it should return an error 
	return val.init ((val_t*)QSE_MAP_VPTR(pair));
}

int Awk::Argument::getFirstIndex (Awk::Argument& val) const
{
	val.clear ();

	if (this->val == QSE_NULL) return -1;
	if (QSE_AWK_VAL_TYPE(this->val) != QSE_AWK_VAL_MAP) return -1;

	qse_size_t buckno;
	qse_awk_val_map_t* m = (qse_awk_val_map_t*)this->val;
	pair_t* pair = qse_map_getfirstpair (m->map, &buckno);
	if (pair == QSE_NULL) return 0; // no more key

	if (val.init (
		(qse_char_t*)QSE_MAP_KPTR(pair),
		QSE_MAP_KLEN(pair)) == -1) return -1;

	// reuse the string field as an interator.
	this->str.ptr = (char_t*)pair;
	this->str.len = buckno;

	return 1;
}

int Awk::Argument::getNextIndex (Awk::Argument& val) const
{
	val.clear ();

	if (this->val == QSE_NULL) return -1;
	if (QSE_AWK_VAL_TYPE(this->val) != QSE_AWK_VAL_MAP) return -1;

	qse_awk_val_map_t* m = (qse_awk_val_map_t*)this->val;

	pair_t* pair = (pair_t*)this->str.ptr;
	qse_size_t buckno = this->str.len;
		
	pair = qse_map_getnextpair (m->map, pair, &buckno);
	if (pair == QSE_NULL) return 0;

	if (val.init (
		(qse_char_t*)QSE_MAP_KPTR(pair),
		QSE_MAP_KLEN(pair)) == -1) return -1;

	// reuse the string field as an interator.
	this->str.ptr = (char_t*)pair;
	this->str.len = buckno;
	return 1;
}

//////////////////////////////////////////////////////////////////
// Awk::Return
//////////////////////////////////////////////////////////////////

Awk::Return::Return (Run& run): run(&run), val(qse_awk_val_nil)
{
}

Awk::Return::Return (Run* run): run(run), val(qse_awk_val_nil)
{
}

Awk::Return::~Return ()
{
	clear ();
}

Awk::val_t* Awk::Return::toVal () const
{
	return this->val; 
}

Awk::Return::operator Awk::val_t* () const 
{
	return this->val; 
}

int Awk::Return::set (long_t v)
{
	QSE_ASSERT (this->run != QSE_NULL);

	qse_awk_val_t* x = qse_awk_rtx_makeintval (this->run->run, v);
	if (x == QSE_NULL) return -1;

	qse_awk_rtx_refdownval (this->run->run, this->val);
	this->val = x;
	qse_awk_rtx_refupval (this->run->run, this->val);

	return 0;
}

int Awk::Return::set (real_t v)
{
	QSE_ASSERT (this->run != QSE_NULL);

	qse_awk_val_t* x = qse_awk_rtx_makerealval (this->run->run, v);
	if (x == QSE_NULL) return -1;

	qse_awk_rtx_refdownval (this->run->run, this->val);
	this->val = x;
	qse_awk_rtx_refupval (this->run->run, this->val);

	return 0;
}

int Awk::Return::set (const char_t* ptr, size_t len)
{
	QSE_ASSERT (this->run != QSE_NULL);

	qse_awk_val_t* x = qse_awk_rtx_makestrval (this->run->run, ptr, len);
	if (x == QSE_NULL) return -1;

	qse_awk_rtx_refdownval (this->run->run, this->val);
	this->val = x;
	qse_awk_rtx_refupval (this->run->run, this->val);
	return 0;
}

bool Awk::Return::isIndexed () const
{
	if (this->val == QSE_NULL) return false;
	return QSE_AWK_VAL_TYPE(this->val) == QSE_AWK_VAL_MAP;
}

int Awk::Return::setIndexed (const char_t* idx, size_t iln, long_t v)
{
	QSE_ASSERT (this->run != QSE_NULL);

	int opt = this->run->awk->getOption();
	if ((opt & OPT_MAPTOVAR) == 0)
	{
		/* refer to run_return in run.c */
		this->run->setError (ERR_MAPNOTALLOWED, 0, QSE_NULL, 0);
		return -1;
	}

	if (QSE_AWK_VAL_TYPE(this->val) != QSE_AWK_VAL_MAP)
	{
		qse_awk_val_t* x = qse_awk_rtx_makemapval (this->run->run);
		if (x == QSE_NULL) return -1;

		qse_awk_rtx_refupval (this->run->run, x);

		qse_awk_val_t* x2 = qse_awk_rtx_makeintval (this->run->run, v);
		if (x2 == QSE_NULL)
		{
			qse_awk_rtx_refdownval (this->run->run, x);
			return -1;
		}

		qse_awk_rtx_refupval (this->run->run, x2);

		pair_t* pair = qse_map_upsert (
			((qse_awk_val_map_t*)x)->map,
			(char_t*)idx, iln, x2, 0);
		if (pair == QSE_NULL)
		{
			qse_awk_rtx_refdownval (this->run->run, x2);
			qse_awk_rtx_refdownval (this->run->run, x);
			this->run->setError (ERR_NOMEM, 0, QSE_NULL, 0);
			return -1;
		}

		qse_awk_rtx_refdownval (this->run->run, this->val);
		this->val = x;
	}
	else
	{
		qse_awk_val_t* x2 = qse_awk_rtx_makeintval (this->run->run, v);
		if (x2 == QSE_NULL) return -1;

		qse_awk_rtx_refupval (this->run->run, x2);

		pair_t* pair = qse_map_upsert (
			((qse_awk_val_map_t*)this->val)->map, 
			(char_t*)idx, iln, x2, 0);
		if (pair == QSE_NULL)
		{
			qse_awk_rtx_refdownval (this->run->run, x2);
			this->run->setError (ERR_NOMEM, 0, QSE_NULL, 0);
			return -1;
		}
	}

	return 0;
}

int Awk::Return::setIndexed (const char_t* idx, size_t iln, real_t v)
{
	QSE_ASSERT (this->run != QSE_NULL);

	int opt = this->run->awk->getOption();
	if ((opt & OPT_MAPTOVAR) == 0)
	{
		/* refer to run_return in run.c */
		this->run->setError (ERR_MAPNOTALLOWED, 0, QSE_NULL, 0);
		return -1;
	}

	if (QSE_AWK_VAL_TYPE(this->val) != QSE_AWK_VAL_MAP)
	{
		qse_awk_val_t* x = qse_awk_rtx_makemapval (this->run->run);
		if (x == QSE_NULL) return -1;

		qse_awk_rtx_refupval (this->run->run, x);

		qse_awk_val_t* x2 = qse_awk_rtx_makerealval (this->run->run, v);
		if (x2 == QSE_NULL)
		{
			qse_awk_rtx_refdownval (this->run->run, x);
			return -1;
		}

		qse_awk_rtx_refupval (this->run->run, x2);

		pair_t* pair = qse_map_upsert (
			((qse_awk_val_map_t*)x)->map, 
			(char_t*)idx, iln, x2, 0);
		if (pair == QSE_NULL)
		{
			qse_awk_rtx_refdownval (this->run->run, x2);
			qse_awk_rtx_refdownval (this->run->run, x);
			this->run->setError (ERR_NOMEM, 0, QSE_NULL, 0);
			return -1;
		}

		qse_awk_rtx_refdownval (this->run->run, this->val);
		this->val = x;
	}
	else
	{
		qse_awk_val_t* x2 = qse_awk_rtx_makerealval (this->run->run, v);
		if (x2 == QSE_NULL) return -1;

		qse_awk_rtx_refupval (this->run->run, x2);

		pair_t* pair = qse_map_upsert (
			((qse_awk_val_map_t*)this->val)->map,
			(char_t*)idx, iln, x2, 0);
		if (pair == QSE_NULL)
		{
			qse_awk_rtx_refdownval (this->run->run, x2);
			this->run->setError (ERR_NOMEM, 0, QSE_NULL, 0);
			return -1;
		}
	}

	return 0;
}

int Awk::Return::setIndexed (const char_t* idx, size_t iln, const char_t* str, size_t sln)
{
	QSE_ASSERT (this->run != QSE_NULL);

	int opt = this->run->awk->getOption();
	if ((opt & OPT_MAPTOVAR) == 0)
	{
		/* refer to run_return in run.c */
		this->run->setError (ERR_MAPNOTALLOWED, 0, QSE_NULL, 0);
		return -1;
	}

	if (QSE_AWK_VAL_TYPE(this->val) != QSE_AWK_VAL_MAP)
	{
		qse_awk_val_t* x = qse_awk_rtx_makemapval (this->run->run);
		if (x == QSE_NULL) return -1;

		qse_awk_rtx_refupval (this->run->run, x);

		qse_awk_val_t* x2 = qse_awk_rtx_makestrval (this->run->run, str, sln);
		if (x2 == QSE_NULL)
		{
			qse_awk_rtx_refdownval (this->run->run, x);
			return -1;
		}

		qse_awk_rtx_refupval (this->run->run, x2);

		pair_t* pair = qse_map_upsert (
			((qse_awk_val_map_t*)x)->map, 
			(char_t*)idx, iln, x2, 0);
		if (pair == QSE_NULL)
		{
			qse_awk_rtx_refdownval (this->run->run, x2);
			qse_awk_rtx_refdownval (this->run->run, x);
			this->run->setError (ERR_NOMEM, 0, QSE_NULL, 0);
			return -1;
		}

		qse_awk_rtx_refdownval (this->run->run, this->val);
		this->val = x;
	}
	else
	{
		qse_awk_val_t* x2 = qse_awk_rtx_makestrval (this->run->run, str, sln);
		if (x2 == QSE_NULL) return -1;

		qse_awk_rtx_refupval (this->run->run, x2);

		pair_t* pair = qse_map_upsert (
			((qse_awk_val_map_t*)this->val)->map, 
			(char_t*)idx, iln, x2, 0);
		if (pair == QSE_NULL)
		{
			qse_awk_rtx_refdownval (this->run->run, x2);
			this->run->setError (ERR_NOMEM, 0, QSE_NULL, 0);
			return -1;
		}
	}

	return 0;
}

int Awk::Return::setIndexed (long_t idx, long_t v)
{
	char_t ri[128];

	int rl = Awk::sprintf (
		(awk_t*)this->run->awk, ri, QSE_COUNTOF(ri), 
	#if QSE_SIZEOF_LONG_LONG > 0
		QSE_T("%lld"), (long long)idx
	#elif QSE_SIZEOF___INT64 > 0
		QSE_T("%I64d"), (__int64)idx
	#elif QSE_SIZEOF_LONG > 0
		QSE_T("%ld"), (long)idx
	#elif QSE_SIZEOF_INT > 0
		QSE_T("%d"), (int)idx
	#else
		#error unsupported size	
	#endif
		);

	if (rl < 0)
	{
		this->run->setError (ERR_INTERN, 0, QSE_NULL, 0);
		return -1;
	}

	return setIndexed (ri, rl, v);
}

int Awk::Return::setIndexed (long_t idx, real_t v)
{
	char_t ri[128];

	int rl = Awk::sprintf (
		(awk_t*)this->run->awk, ri, QSE_COUNTOF(ri), 
	#if QSE_SIZEOF_LONG_LONG > 0
		QSE_T("%lld"), (long long)idx
	#elif QSE_SIZEOF___INT64 > 0
		QSE_T("%I64d"), (__int64)idx
	#elif QSE_SIZEOF_LONG > 0
		QSE_T("%ld"), (long)idx
	#elif QSE_SIZEOF_INT > 0
		QSE_T("%d"), (int)idx
	#else
		#error unsupported size	
	#endif
		);

	if (rl < 0)
	{
		this->run->setError (ERR_INTERN, 0, QSE_NULL, 0);
		return -1;
	}

	return setIndexed (ri, rl, v);
}

int Awk::Return::setIndexed (long_t idx, const char_t* str, size_t sln)
{
	char_t ri[128];

	int rl = Awk::sprintf (
		(awk_t*)this->run->awk, ri, QSE_COUNTOF(ri), 
	#if QSE_SIZEOF_LONG_LONG > 0
		QSE_T("%lld"), (long long)idx
	#elif QSE_SIZEOF___INT64 > 0
		QSE_T("%I64d"), (__int64)idx
	#elif QSE_SIZEOF_LONG > 0
		QSE_T("%ld"), (long)idx
	#elif QSE_SIZEOF_INT > 0
		QSE_T("%d"), (int)idx
	#else
		#error unsupported size	
	#endif
		);

	if (rl < 0)
	{
		this->run->setError (ERR_INTERN, 0, QSE_NULL, 0);
		return -1;
	}

	return setIndexed (ri, rl, str, sln);
}

void Awk::Return::clear ()
{
	qse_awk_rtx_refdownval (this->run->run, this->val);
	this->val = qse_awk_val_nil;
}

//////////////////////////////////////////////////////////////////
// Awk::Run
//////////////////////////////////////////////////////////////////

Awk::Run::Run (Awk* awk): awk (awk), run (QSE_NULL)
{
}

Awk::Run::Run (Awk* awk, rtx_t* run): awk (awk), run (run), data (QSE_NULL)
{
	QSE_ASSERT (this->run != QSE_NULL);
}

Awk::Run::~Run ()
{
}

Awk::Run::operator Awk* () const 
{
	return this->awk;
}

Awk::Run::operator Awk::rtx_t* () const 
{
	return this->run;
}

void Awk::Run::stop () const
{
	QSE_ASSERT (this->run != QSE_NULL);
	qse_awk_rtx_stop (this->run);
}

bool Awk::Run::isStop () const
{
	QSE_ASSERT (this->run != QSE_NULL);
	return qse_awk_rtx_shouldstop (this->run)? true: false;
}

Awk::ErrorCode Awk::Run::getErrorCode () const
{
	QSE_ASSERT (this->run != QSE_NULL);
	return (ErrorCode)qse_awk_rtx_geterrnum (this->run);
}

Awk::size_t Awk::Run::getErrorLine () const
{
	QSE_ASSERT (this->run != QSE_NULL);
	return qse_awk_rtx_geterrlin (this->run);
}

const Awk::char_t* Awk::Run::getErrorMessage () const
{
	QSE_ASSERT (this->run != QSE_NULL);
	return qse_awk_rtx_geterrmsg (this->run);
}

void Awk::Run::setError (ErrorCode code)
{
	QSE_ASSERT (this->run != QSE_NULL);
	qse_awk_rtx_seterror (this->run, code, 0, QSE_NULL);
}

void Awk::Run::setError (ErrorCode code, size_t line)
{
	QSE_ASSERT (this->run != QSE_NULL);
	qse_awk_rtx_seterror (this->run, code, line, QSE_NULL);
}

void Awk::Run::setError (ErrorCode code, size_t line, const char_t* arg)
{
	QSE_ASSERT (this->run != QSE_NULL);
	qse_cstr_t x = { arg, qse_strlen(arg) };
	qse_awk_rtx_seterror (this->run, code, line, &x);
}

void Awk::Run::setError (
	ErrorCode code, size_t line, const char_t* arg, size_t len)
{
	QSE_ASSERT (this->run != QSE_NULL);
	qse_cstr_t x = { arg, len };
	qse_awk_rtx_seterror (this->run, code, line, &x);
}

void Awk::Run::setErrorWithMessage (
	ErrorCode code, size_t line, const char_t* msg)
{
	QSE_ASSERT (this->run != QSE_NULL);
	qse_awk_rtx_seterrmsg (this->run, code, line, msg);
}

int Awk::Run::setGlobal (int id, long_t v)
{
	QSE_ASSERT (this->run != QSE_NULL);

	qse_awk_val_t* tmp = qse_awk_rtx_makeintval (run, v);
	if (tmp == QSE_NULL) return -1;

	qse_awk_rtx_refupval (run, tmp);
	int n = qse_awk_rtx_setgbl (this->run, id, tmp);
	qse_awk_rtx_refdownval (run, tmp);
	return n;
}

int Awk::Run::setGlobal (int id, real_t v)
{
	QSE_ASSERT (this->run != QSE_NULL);

	qse_awk_val_t* tmp = qse_awk_rtx_makerealval (run, v);
	if (tmp == QSE_NULL) return -1;

	qse_awk_rtx_refupval (run, tmp);
	int n = qse_awk_rtx_setgbl (this->run, id, tmp);
	qse_awk_rtx_refdownval (run, tmp);
	return n;
}

int Awk::Run::setGlobal (int id, const char_t* ptr, size_t len)
{
	QSE_ASSERT (run != QSE_NULL);

	qse_awk_val_t* tmp = qse_awk_rtx_makestrval (run, ptr, len);
	if (tmp == QSE_NULL) return -1;

	qse_awk_rtx_refupval (run, tmp);
	int n = qse_awk_rtx_setgbl (this->run, id, tmp);
	qse_awk_rtx_refdownval (run, tmp);
	return n;
}

int Awk::Run::setGlobal (int id, const Return& gbl)
{
	QSE_ASSERT (this->run != QSE_NULL);

	return qse_awk_rtx_setgbl (this->run, id, gbl.toVal());
}

int Awk::Run::getGlobal (int id, Argument& gbl) const
{
	QSE_ASSERT (this->run != QSE_NULL);

	gbl.clear ();
	return gbl.init (qse_awk_rtx_getgbl(this->run,id));
}

void Awk::Run::setData (void* data)
{
	this->data = data;
}

void* Awk::Run::getData () const
{
	return this->data;
}

//////////////////////////////////////////////////////////////////
// Awk
//////////////////////////////////////////////////////////////////

Awk::Awk (): awk (QSE_NULL), functionMap (QSE_NULL),
	sourceIn (Source::READ), sourceOut (Source::WRITE),
	errnum (ERR_NOERR), errlin (0), runCallback (false)

{
	this->errmsg[0] = QSE_T('\0');

	mmgr.alloc   = allocMem;
	mmgr.realloc = reallocMem;
	mmgr.free    = freeMem;
	mmgr.data    = this;
}

Awk::~Awk ()
{
}

Awk::operator Awk::awk_t* () const
{
	return this->awk;
}

Awk::ErrorCode Awk::getErrorCode () const
{
	return this->errnum;
}

Awk::size_t Awk::getErrorLine () const
{
	return this->errlin;
}

const Awk::char_t* Awk::getErrorMessage () const
{
	return this->errmsg;
}

void Awk::setError (ErrorCode code)
{
	setError (code, 0, QSE_NULL, 0);
}

void Awk::setError (ErrorCode code, size_t line)
{
	setError (code, line, QSE_NULL, 0);
}

void Awk::setError (ErrorCode code, size_t line, const char_t* arg)
{
	setError (code, line, arg, qse_strlen(arg));
}

void Awk::setError (ErrorCode code, size_t line, const char_t* arg, size_t len)
{
	if (awk != QSE_NULL)
	{
		qse_cstr_t x = { arg, len };
		qse_awk_seterror (awk, code, line, &x);
		retrieveError ();
	}
	else
	{
		this->errnum = code;
		this->errlin = line;

		const char_t* es = qse_awk_geterrstr (
			QSE_NULL, (qse_awk_errnum_t)code);
		qse_strxcpy (this->errmsg, QSE_COUNTOF(this->errmsg), es);
	}
}

void Awk::setErrorWithMessage (ErrorCode code, size_t line, const char_t* msg)
{
	if (awk != QSE_NULL)
	{
		qse_awk_seterrmsg (awk, code, line, msg);
		retrieveError ();
	}
	else
	{
		this->errnum = code;
		this->errlin = line;
		qse_strxcpy (this->errmsg, QSE_COUNTOF(this->errmsg), msg);
	}
}

void Awk::clearError ()
{
	this->errnum = ERR_NOERR;
	this->errlin = 0;
	this->errmsg[0] = QSE_T('\0');
}

void Awk::retrieveError ()
{
	if (this->awk == QSE_NULL) 
	{
		clearError ();
	}
	else
	{
		int num;
		const char_t* msg;

		qse_awk_geterror (this->awk, &num, &this->errlin, &msg);
		this->errnum = (ErrorCode)num;
		qse_strxcpy (this->errmsg, QSE_COUNTOF(this->errmsg), msg);
	}
}

void Awk::retrieveError (rtx_t* rtx)
{
	int num;
	const char_t* msg;

	qse_awk_rtx_geterror (rtx, &num, &this->errlin, &msg);
	this->errnum = (ErrorCode)num;
	qse_strxcpy (this->errmsg, QSE_COUNTOF(this->errmsg), msg);
}

int Awk::open ()
{
	QSE_ASSERT (awk == QSE_NULL && functionMap == QSE_NULL);

	qse_awk_prm_t prm;
	prm.pow     = pow;
	prm.sprintf = sprintf;
	prm.isccls  = isType;
	prm.toccls  = transCase;

	awk = qse_awk_open (&mmgr, QSE_SIZEOF(xtn_t), &prm);
	if (awk == QSE_NULL)
	{
		setError (ERR_NOMEM);
		return -1;
	}

	// associate this Awk object with the underlying awk object
	xtn_t* xtn = (xtn_t*) QSE_XTN (awk);
	xtn->awk = this;

	//functionMap = qse_map_open (
	//	this, 512, 70, freeFunctionMapValue, QSE_NULL, 
	//	qse_awk_getmmgr(awk));
	functionMap = qse_map_open (
		qse_awk_getmmgr(awk), QSE_SIZEOF(this), 512, 70);
	if (functionMap == QSE_NULL)
	{
		qse_awk_close (awk);
		awk = QSE_NULL;

		setError (ERR_NOMEM);
		return -1;
	}

	*(Awk**)QSE_XTN(functionMap) = this;
	qse_map_setcopier (functionMap, QSE_MAP_KEY, QSE_MAP_COPIER_INLINE);
	qse_map_setfreeer (functionMap, QSE_MAP_VAL, freeFunctionMapValue);
	qse_map_setscale (functionMap, QSE_MAP_KEY, QSE_SIZEOF(qse_char_t));

	runCallback = false;
	return 0;
}

void Awk::close ()
{
	if (functionMap != QSE_NULL)
	{
		qse_map_close (functionMap);
		functionMap = QSE_NULL;
	}

	if (awk != QSE_NULL) 
	{
		qse_awk_close (awk);
		awk = QSE_NULL;
	}

	clearError ();
	runCallback = false;
}

void Awk::setOption (int opt)
{
	QSE_ASSERT (awk != QSE_NULL);
	qse_awk_setoption (awk, opt);
}

int Awk::getOption () const
{
	QSE_ASSERT (awk != QSE_NULL);
	return qse_awk_getoption (awk);
}

void Awk::setMaxDepth (int ids, size_t depth)
{
	QSE_ASSERT (awk != QSE_NULL);
	qse_awk_setmaxdepth (awk, ids, depth);
}

Awk::size_t Awk::getMaxDepth (int id) const
{
	QSE_ASSERT (awk != QSE_NULL);
	return qse_awk_getmaxdepth (awk, id);
}

const Awk::char_t* Awk::getErrorString (ErrorCode num) const
{
	QSE_ASSERT (awk != QSE_NULL);
	return qse_awk_geterrstr (awk, (qse_awk_errnum_t)num);
}

int Awk::setErrorString (ErrorCode num, const char_t* str)
{
	QSE_ASSERT (awk != QSE_NULL);
	return qse_awk_seterrstr (awk, (qse_awk_errnum_t)num, str);
}

int Awk::getWord (
	const char_t* ow, qse_size_t owl,
	const char_t** nw, qse_size_t* nwl)
{
	QSE_ASSERT (awk != QSE_NULL);
	return qse_awk_getword (awk, ow, owl, nw, nwl);
}

int Awk::setWord (const char_t* ow, const char_t* nw)
{
	return setWord (ow, qse_strlen(ow), nw, qse_strlen(nw));
}

int Awk::setWord (
	const char_t* ow, qse_size_t owl,
	const char_t* nw, qse_size_t nwl)
{
	QSE_ASSERT (awk != QSE_NULL);
	return qse_awk_setword (awk, ow, owl, nw, nwl);
}

int Awk::unsetWord (const char_t* ow)
{
	return unsetWord (ow, qse_strlen(ow));
}

int Awk::unsetWord (const char_t* ow, qse_size_t owl)
{
	QSE_ASSERT (awk != QSE_NULL);
	return qse_awk_setword (awk, ow, owl, QSE_NULL, 0);
}

int Awk::unsetAllWords ()
{
	QSE_ASSERT (awk != QSE_NULL);
	return qse_awk_setword (awk, QSE_NULL, 0, QSE_NULL, 0);
}

int Awk::parse ()
{
	QSE_ASSERT (awk != QSE_NULL);

	qse_awk_sio_t sio;

	sio.in = sourceReader;
	sio.out = sourceWriter;

	int n = qse_awk_parse (awk, &sio);
	if (n == -1) retrieveError ();
	return n;
}

int Awk::run (const char_t** args, size_t nargs)
{
	QSE_ASSERT (awk != QSE_NULL);

	size_t i;
	qse_awk_rio_t rio;
	qse_awk_rcb_t rcb;
	qse_xstr_t* runarg = QSE_NULL;

	// note that the run field is set below after qse_awk_rtx_open() is
	// executed.
	Run runctx (this);

	rio.pipe    = pipeHandler;
	rio.file    = fileHandler;
	rio.console = consoleHandler;

	if (runCallback)
	{
		QSE_MEMSET (&rcb, 0, QSE_SIZEOF(rcb));
		// TODO: deprecate onRunStart and onRunEnd
		//rcb.on_start   = onRunStart;
		//rcb.on_end     = onRunEnd;
		rcb.on_enter     = onRunEnter;
		rcb.on_statement = onRunStatement;
		rcb.on_exit      = onRunExit;
		rcb.data         = &runctx;
	}
	
	if (nargs > 0)
	{
		runarg = (qse_xstr_t*) qse_awk_alloc (
			awk, QSE_SIZEOF(qse_xstr_t)*(nargs+1));

		if (runarg == QSE_NULL)
		{
			setError (ERR_NOMEM);
			return -1;
		}

		for (i = 0; i < nargs; i++)
		{
			runarg[i].len = qse_strlen (args[i]);
			runarg[i].ptr = qse_awk_strxdup (awk, args[i], runarg[i].len);
			if (runarg[i].ptr == QSE_NULL)
			{
				while (i > 0) qse_awk_free (awk, runarg[--i].ptr);
				qse_awk_free (awk, runarg);
				setError (ERR_NOMEM);
				return -1;
			}
		}

		runarg[i].ptr = QSE_NULL;
		runarg[i].len = 0;
	}
	
	int n = 0;
	rtx_t* rtx = qse_awk_rtx_open (
		awk, QSE_SIZEOF(rxtn_t), &rio, (qse_cstr_t*)runarg);
	if (rtx == QSE_NULL) 
	{
		retrieveError();
		n = -1;
	}
	else
	{
		runctx.run = rtx;

		rxtn_t* rxtn = (rxtn_t*) QSE_XTN (rtx);
		rxtn->run = &runctx;

		if (runCallback) qse_awk_rtx_setrcb (rtx, &rcb);
		n = qse_awk_rtx_loop (rtx);
		if (n == -1) retrieveError (rtx);
		qse_awk_rtx_close (rtx);
	}

	if (runarg != QSE_NULL) 
	{
		while (i > 0) qse_awk_free (awk, runarg[--i].ptr);
		qse_awk_free (awk, runarg);
	}

	return n;
}

void Awk::stop ()
{
	QSE_ASSERT (awk != QSE_NULL);
	qse_awk_stopall (awk);
}

int Awk::dispatchFunction (Run* run, const char_t* name, size_t len)
{
	pair_t* pair;
	awk_t* awk = run->awk->awk;

	//awk = qse_awk_rtx_getawk (run);

	pair = qse_map_search (functionMap, name, len);
	if (pair == QSE_NULL) 
	{
		run->setError (ERR_FUNNONE, 0, name, len);
		return -1;
	}

	FunctionHandler handler;
       	handler = *(FunctionHandler*)QSE_MAP_VPTR(pair);	

	size_t i, nargs = qse_awk_rtx_getnargs(run->run);

	//Argument* args = QSE_NULL;
	//try { args = new Argument [nargs]; } catch (...)  {}
	Argument* args = new(awk) Argument[nargs];
	if (args == QSE_NULL) 
	{
		run->setError (ERR_NOMEM, 0, QSE_NULL, 0);
		return -1;
	}

	for (i = 0; i < nargs; i++)
	{
		args[i].run = run; // dirty late initialization 
		                   // due to c++ array creation limitation.

		val_t* v = qse_awk_rtx_getarg (run->run, i);
		if (args[i].init (v) == -1)
		{
			run->setError (ERR_NOMEM, 0, QSE_NULL, 0);
			delete[] args;
			return -1;
		}
	}
	
	Return ret (run);

	int n = (this->*handler) (*run, ret, args, nargs, name, len);

	delete[] args;

	if (n <= -1) 
	{
		/* this is really the handler error. the underlying engine 
		 * will take care of the error code. */
		return -1;
	}

	qse_awk_rtx_setretval (run->run, ret);
	return 0;
}

int Awk::addGlobal (const char_t* name)
{
	QSE_ASSERT (awk != QSE_NULL);

	int n = qse_awk_addgbl (awk, name, qse_strlen(name));
	if (n == -1) retrieveError ();
	return n;
}

int Awk::deleteGlobal (const char_t* name)
{
	QSE_ASSERT (awk != QSE_NULL);
	int n = qse_awk_delgbl (awk, name, qse_strlen(name));
	if (n == -1) retrieveError ();
	return n;
}

int Awk::addFunction (
	const char_t* name, size_t minArgs, size_t maxArgs, 
	FunctionHandler handler)
{
	QSE_ASSERT (awk != QSE_NULL);

	FunctionHandler* tmp = (FunctionHandler*) 
		qse_awk_alloc (awk, QSE_SIZEOF(handler));
	if (tmp == QSE_NULL)
	{
		setError (ERR_NOMEM);
		return -1;
	}

	//QSE_MEMCPY (tmp, &handler, QSE_SIZEOF(handler));
	*tmp = handler;
	
	size_t nameLen = qse_strlen(name);

	void* p = qse_awk_addfnc (awk, name, nameLen,
	                          0, minArgs, maxArgs, QSE_NULL, 
	                          functionHandler);
	if (p == QSE_NULL) 
	{
		qse_awk_free (awk, tmp);
		retrieveError ();
		return -1;
	}

	pair_t* pair = qse_map_upsert (
		functionMap, (char_t*)name, nameLen, tmp, 0);
	if (pair == QSE_NULL)
	{
		qse_awk_delfnc (awk, name, nameLen);
		qse_awk_free (awk, tmp);

		setError (ERR_NOMEM);
		return -1;
	}

	return 0;
}

int Awk::deleteFunction (const char_t* name)
{
	QSE_ASSERT (awk != QSE_NULL);

	size_t nameLen = qse_strlen(name);

	int n = qse_awk_delfnc (awk, name, nameLen);
	if (n == 0) qse_map_delete (functionMap, name, nameLen);
	else retrieveError ();

	return n;
}

void Awk::enableRunCallback ()
{
	runCallback = true;
}

void Awk::disableRunCallback ()
{
	runCallback = false;
}

bool Awk::onRunEnter (Run& run)
{
	return true;
}

void Awk::onRunExit (Run& run, const Argument& ret)
{
}

void Awk::onRunStatement (Run& run, size_t line)
{
}

Awk::ssize_t Awk::sourceReader (
	awk_t* awk, qse_awk_sio_cmd_t cmd, char_t* data, size_t count)
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (awk);

	switch (cmd)
	{
		case QSE_AWK_SIO_OPEN:
			return xtn->awk->openSource (xtn->awk->sourceIn);
		case QSE_AWK_SIO_CLOSE:
			return xtn->awk->closeSource (xtn->awk->sourceIn);
		case QSE_AWK_SIO_READ:
			return xtn->awk->readSource (xtn->awk->sourceIn, data, count);
		case QSE_AWK_SIO_WRITE:
			return -1;
	}

	return -1;
}

Awk::ssize_t Awk::sourceWriter (
	awk_t* awk, qse_awk_sio_cmd_t cmd, char_t* data, size_t count)
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (awk);

	switch (cmd)
	{
		case QSE_AWK_SIO_OPEN:
			return xtn->awk->openSource (xtn->awk->sourceOut);
		case QSE_AWK_SIO_CLOSE:
			return xtn->awk->closeSource (xtn->awk->sourceOut);
		case QSE_AWK_SIO_WRITE:
			return xtn->awk->writeSource (xtn->awk->sourceOut, data, count);
		case QSE_AWK_SIO_READ:
			return -1;
	}

	return -1;
}

Awk::ssize_t Awk::pipeHandler (
	rtx_t* rtx, qse_awk_rio_cmd_t cmd, riod_t* riod,
	char_t* data, size_t count)
{
	rxtn_t* rxtn = (rxtn_t*) QSE_XTN (rtx);
	Awk* awk = rxtn->run->awk;

	QSE_ASSERT ((riod->type & 0xFF) == QSE_AWK_RIO_PIPE);

	Pipe pipe (rtx, riod);

	switch (cmd)
	{
		case QSE_AWK_RIO_OPEN:
			return awk->openPipe (pipe);
		case QSE_AWK_RIO_CLOSE:
			return awk->closePipe (pipe);

		case QSE_AWK_RIO_READ:
			return awk->readPipe (pipe, data, count);
		case QSE_AWK_RIO_WRITE:
			return awk->writePipe (pipe, data, count);

		case QSE_AWK_RIO_FLUSH:
			return awk->flushPipe (pipe);

		case QSE_AWK_RIO_NEXT:
			return -1;
	}

	return -1;
}

Awk::ssize_t Awk::fileHandler (
	rtx_t* rtx, qse_awk_rio_cmd_t cmd, riod_t* riod,
	char_t* data, size_t count)
{
	rxtn_t* rxtn = (rxtn_t*) QSE_XTN (rtx);
	Awk* awk = rxtn->run->awk;

	QSE_ASSERT ((riod->type & 0xFF) == QSE_AWK_RIO_FILE);

	File file (rtx, riod);

	switch (cmd)
	{
		case QSE_AWK_RIO_OPEN:
			return awk->openFile (file);
		case QSE_AWK_RIO_CLOSE:
			return awk->closeFile (file);

		case QSE_AWK_RIO_READ:
			return awk->readFile (file, data, count);
		case QSE_AWK_RIO_WRITE:
			return awk->writeFile (file, data, count);

		case QSE_AWK_RIO_FLUSH:
			return awk->flushFile (file);

		case QSE_AWK_RIO_NEXT:
			return -1;
	}

	return -1;
}

Awk::ssize_t Awk::consoleHandler (
	rtx_t* rtx, qse_awk_rio_cmd_t cmd, riod_t* riod,
	char_t* data, size_t count)
{
	rxtn_t* rxtn = (rxtn_t*) QSE_XTN (rtx);
	Awk* awk = rxtn->run->awk;

	QSE_ASSERT ((riod->type & 0xFF) == QSE_AWK_RIO_CONSOLE);

	Console console (rtx, riod);

	switch (cmd)
	{
		case QSE_AWK_RIO_OPEN:
			return awk->openConsole (console);
		case QSE_AWK_RIO_CLOSE:
			return awk->closeConsole (console);

		case QSE_AWK_RIO_READ:
			return awk->readConsole (console, data, count);
		case QSE_AWK_RIO_WRITE:
			return awk->writeConsole (console, data, count);

		case QSE_AWK_RIO_FLUSH:
			return awk->flushConsole (console);
		case QSE_AWK_RIO_NEXT:
			return awk->nextConsole (console);
	}

	return -1;
}

int Awk::functionHandler (rtx_t* rtx, const char_t* name, size_t len)
{
	rxtn_t* rxtn = (rxtn_t*) QSE_XTN (rtx);
	return rxtn->run->awk->dispatchFunction (rxtn->run, name, len);
}	

void Awk::freeFunctionMapValue (map_t* map, void* dptr, size_t dlen)
{
	Awk* awk = *(Awk**) QSE_XTN (map);
	qse_awk_free (awk->awk, dptr);
}

int Awk::onRunEnter (rtx_t* run, void* data)
{
	Run* r = (Run*)data;
	return r->awk->onRunEnter(*r)? 0: -1;
}

void Awk::onRunExit (rtx_t* run, val_t* ret, void* data)
{
	Run* r = (Run*)data;

	Argument x (r);
	if (x.init (ret) == -1) 
		qse_awk_rtx_seterrnum (r->run, ERR_NOMEM);
	else r->awk->onRunExit (*r, x);
}

void Awk::onRunStatement (rtx_t* run, size_t line, void* data)
{
	Run* r = (Run*)data;
	r->awk->onRunStatement (*r, line);
}

void* Awk::allocMem (void* data, size_t n)
{
	return ((Awk*)data)->allocMem (n);
}

void* Awk::reallocMem (void* data, void* ptr, size_t n)
{
	return ((Awk*)data)->reallocMem (ptr, n);
}

void Awk::freeMem (void* data, void* ptr)
{
	((Awk*)data)->freeMem (ptr);
}

Awk::bool_t Awk::isType (awk_t* awk, cint_t c, qse_ccls_id_t type) 
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (awk);
	return xtn->awk->isType (c, (ccls_id_t)type);
}

Awk::cint_t Awk::transCase (awk_t* awk, cint_t c, qse_ccls_id_t type) 
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (awk);
	return xtn->awk->transCase (c, (ccls_id_t)type);
}

Awk::real_t Awk::pow (awk_t* awk, real_t x, real_t y)
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (awk);
	return xtn->awk->pow (x, y);
}
	
int Awk::sprintf (awk_t* awk, char_t* buf, size_t size,
                  const char_t* fmt, ...)
{
	xtn_t* xtn = (xtn_t*) QSE_XTN (awk);

	va_list ap;
	va_start (ap, fmt);
	int n = xtn->awk->vsprintf (buf, size, fmt, ap);
	va_end (ap);
	return n;
}

/////////////////////////////////
QSE_END_NAMESPACE(QSE)
/////////////////////////////////

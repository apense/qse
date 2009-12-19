/*
 * $Id$
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

#include <qse/cmn/Mmgr.hpp>

/////////////////////////////////
QSE_BEGIN_NAMESPACE(QSE)
/////////////////////////////////

void* Mmgr::alloc_mem (void* udd, size_t n) 
{
	return ((Mmgr*)udd)->allocMem (n);
}

void* Mmgr::realloc_mem (void* udd, void* ptr, size_t n) 
{
	return ((Mmgr*)udd)->reallocMem (ptr, n);
}

void Mmgr::free_mem (void* udd, void* ptr) 
{
	return ((Mmgr*)udd)->freeMem (ptr);
}

/////////////////////////////////
QSE_END_NAMESPACE(QSE)
/////////////////////////////////

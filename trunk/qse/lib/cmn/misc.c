/*
 * $Id: misc.c 278 2009-09-04 13:08:19Z hyunghwan.chung $
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

#include <qse/cmn/misc.h>

const qse_char_t* qse_basename (const qse_char_t* path)
{
	const qse_char_t* p, * last = QSE_NULL;

	for (p = path; *p != QSE_T('\0'); p++)
	{
		if (*p == QSE_T('/')) last = p;
	#ifdef _WIN32
		else if (*p == QSE_T('\\')) last = p;
	#endif
	}

	return (last == QSE_NULL)? path: (last + 1);
}

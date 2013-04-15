/*
 * $Id: object.c 118 2008-03-03 11:21:33Z baconevi $
 */

#include <qse/stx/object.h>
#include <qse/stx/memory.h>
#include <qse/stx/symbol.h>
#include <qse/stx/class.h>
#include <qse/stx/misc.h>

/* n: number of instance variables */
qse_word_t qse_stx_alloc_word_object (
	qse_stx_t* stx, const qse_word_t* data, qse_word_t nfields, 
	const qse_word_t* variable_data, qse_word_t variable_nfields)
{
	qse_stx_objref_t idx;
	qse_word_t n;
	qse_stx_word_object_t* obj;

	QSE_ASSERT (stx->nil == QSE_STX_NIL);

	/* bytes to allocated =
	 *     (number of instance variables + 
	 *      number of variable instance variables) * word_size 
	 */
	n = nfields + variable_nfields;

/* TODO: check if n is larger then header.access nfield bits can represent...
 *       then.... reject .... */

	idx = qse_stx_memory_alloc (
		&stx->memory,
		n * QSE_SIZEOF(qse_word_t) + QSE_SIZEOF(qse_stx_object_t));
	if (idx >= stx->memory.capacity) return QSE_STX_OBJREF_INVALID; /* failed TODO: return a difference value OBJIDX_INVALID */

	idx = QSE_STX_TOOBJREF(idx);
	obj = QSE_STX_WORDOBJPTR(stx,idx);

	obj->header.class = stx->nil;
	obj->header.access = (n << 2) | QSE_STX_WORD_INDEXED;

	if (variable_data == QSE_NULL) 
	{
		while (n > nfields) obj->data[--n] = stx->nil;
	}
	else 
	{
		while (n > nfields) {
			n--; obj->data[n] = variable_data[n - nfields];
		}
	}

	if (data == QSE_NULL) 
	{
		while (n > 0) obj->data[--n] = stx->nil;
	}
	else 
	{
		while (n > 0) 
		{
			n--; obj->data[n] = data[n];
		}
	}

	return idx;
}

/* n: number of bytes */
qse_word_t qse_stx_alloc_byte_object (
	qse_stx_t* stx, const qse_byte_t* data, qse_word_t n)
{
	qse_word_t idx;
	qse_stx_byte_object_t* obj;

	QSE_ASSERT (stx->nil == QSE_STX_NIL);

/* TODO: check if n is larger then header.access nfield bits can represent...
 *       then.... reject .... */

	idx = qse_stx_memory_alloc (
		&stx->memory, n + QSE_SIZEOF(qse_stx_object_t));
	if (idx >= stx->memory.capacity) return idx; /* failed */

	idx = QSE_STX_TOOBJIDX(idx);
	obj = QSE_STX_BYTE_OBJECT(stx,idx);
	obj->header.class = stx->nil;
	obj->header.access = (n << 2) | QSE_STX_BYTE_INDEXED;

	if (data == QSE_NULL) 
	{
		while (n-- > 0) obj->data[n] = 0;
	}
	else 
	{
		while (n-- > 0) obj->data[n] = data[n];
	}

	return idx;
}

qse_word_t qse_stx_alloc_char_object (
	qse_stx_t* stx, const qse_char_t* str)
{
	return (str == QSE_NULL)?
		qse_stx_alloc_char_objectx (stx, QSE_NULL, 0):
		qse_stx_alloc_char_objectx (stx, str, qse_strlen(str));
}

/* n: number of characters */
qse_word_t qse_stx_alloc_char_objectx (
	qse_stx_t* stx, const qse_char_t* str, qse_word_t n)
{
	qse_word_t idx;
	qse_stx_char_object_t* obj;

	QSE_ASSERT (stx->nil == QSE_STX_NIL);

/* TODO: check if n is larger then header.access nfield bits can represent...
 *       then.... reject .... */

	idx = qse_stx_memory_alloc (&stx->memory, 
		(n + 1) * QSE_SIZEOF(qse_char_t) + QSE_SIZEOF(qse_stx_object_t));
	if (idx >= stx->memory.capacity) return idx; /* failed */

	idx = QSE_STX_TOOBJIDX(idx);
	obj = QSE_STX_CHAR_OBJECT(stx,idx);
	obj->header.class = stx->nil;
	obj->header.access = (n << 2) | QSE_STX_CHAR_INDEXED;
	obj->data[n] = QSE_T('\0');

	if (str == QSE_NULL) 
	{
		while (n-- > 0) obj->data[n] = QSE_T('\0');
	}
	else 
	{
		while (n-- > 0) obj->data[n] = str[n];
	}

	return idx;
}

qse_word_t qse_stx_allocn_char_object (qse_stx_t* stx, ...)
{
	qse_word_t idx, n = 0;
	const qse_char_t* p;
	qse_va_list ap;
	qse_stx_char_object_t* obj;

	QSE_ASSERT (stx->nil == QSE_STX_NIL);

	qse_va_start (ap, stx);
	while ((p = qse_va_arg(ap, const qse_char_t*)) != QSE_NULL) {
		n += qse_strlen(p);
	}
	qse_va_end (ap);

	idx = qse_stx_memory_alloc (&stx->memory, 
		(n + 1) * QSE_SIZEOF(qse_char_t) + QSE_SIZEOF(qse_stx_object_t));
	if (idx >= stx->memory.capacity) return idx; /* failed */

	idx = QSE_STX_TOOBJIDX(idx);
	obj = QSE_STX_CHAR_OBJECT(stx,idx);
	obj->header.class = stx->nil;
	obj->header.access = (n << 2) | QSE_STX_CHAR_INDEXED;
	obj->data[n] = QSE_T('\0');

	qse_va_start (ap, stx);
	n = 0;
	while ((p = qse_va_arg(ap, const qse_char_t*)) != QSE_NULL) 
	{
		while (*p != QSE_T('\0')) 
		{
			/*QSE_STX_CHAR_AT(stx,idx,n++) = *p++;*/
			obj->data[n++] = *p++;
		}
	}
	qse_va_end (ap);

	return idx;
}

qse_word_t qse_stx_hash_object (qse_stx_t* stx, qse_word_t objref)
{
	qse_word_t hv;

	if (QSE_STX_ISSMALLINT(objref)) 
	{
		qse_word_t tmp = QSE_STX_FROMSMALLINT(objref);
		hv = qse_stx_hash(&tmp, QSE_SIZEOF(tmp));
	}
	else if (QSE_STX_ISCHAROBJECT(stx,objref)) 
	{
		/* the additional null is not taken into account */
		hv = qse_stx_hash (QSE_STX_DATA(stx,objref),
			QSE_STX_SIZE(stx,objref) * QSE_SIZEOF(qse_char_t));
	}
	else if (QSE_STX_ISBYTEOBJECT(stx,objref)) 
	{
		hv = qse_stx_hash (
			QSE_STX_DATA(stx,objref), QSE_STX_SIZE(stx,objref));
	}
	else 
	{
		QSE_ASSERT (QSE_STX_ISWORDOBJECT(stx,objref));
		hv = qse_stx_hash (QSE_STX_DATA(stx,objref),
			QSE_STX_SIZE(stx,objref) * QSE_SIZEOF(qse_word_t));
	}

	return hv;
}

qse_word_t qse_stx_instantiate (
	qse_stx_t* stx, qse_stx_objref_t class, const void* data, 
	const void* variable_data, qse_word_t variable_nfields)
{
	qse_stx_class_t* class_ptr;
	qse_word_t spec, nfields, inst;
	int indexable;

	QSE_ASSERT (class != stx->class_smallinteger);
	class_ptr = (qse_stx_class_t*)QSE_STX_OBJPTR(stx, class);

	/* don't instantiate a metaclass whose instance must be 
	   created in a different way */
	/* TODO: maybe delete the following line */
	QSE_ASSERT (QSE_STX_CLASS(class) != stx->class_metaclass);
	QSE_ASSERT (QSE_STX_ISSMALLINT(class_obj->spec));

	spec = QSE_STX_FROMSMALLINT(class_obj->spec);
	nfields = (spec >> QSE_STX_SPEC_INDEXABLE_BITS);
	indexable = spec & QSE_STX_SPEC_INDEXABLE_MASK;

	switch (indexable)
	{
		case QSE_STX_SPEC_BYTE_INDEXABLE:
			/* variable-size byte class */
			QSE_ASSERT (nfields == 0 && data == QSE_NULL);
			inst = qse_stx_alloc_byte_object(
				stx, variable_data, variable_nfields);
			break;

		case QSE_STX_SPEC_CHAR_INDEXABLE:
			/* variable-size char class */
			QSE_ASSERT (nfields == 0 && data == QSE_NULL);
			inst = qse_stx_alloc_char_objectx(
				stx, variable_data, variable_nfields);
			break;

		case QSE_STX_SPEC_WORD_INDEXABLE:
			/* variable-size class */
			inst = qse_stx_alloc_word_object (
				stx, data, nfields, variable_data, variable_nfields);
			break;

		case QSE_STX_SPEC_NOT_INDEXABLE:
			/* fixed size */
			QSE_ASSERT (indexable == QSE_STX_SPEC_NOT_INDEXABLE);
			QSE_ASSERT (variable_nfields == 0 && variable_data == QSE_NULL);
			inst = qse_stx_alloc_word_object (
				stx, data, nfields, QSE_NULL, 0);
			break;

		default:
			/* this should never happen */	
			QSE_ASSERTX (0, "this should never happen");
			inst = QSE_STX_OBJREF_INVALID;
	}

	if (inst != QSE_STX_OBJREF_INVALID) 
		QSE_STX_CLASSOF(stx,inst) = class;
	return inst;
}

qse_word_t qse_stx_class (qse_stx_t* stx, qse_stx_objref_t obj)
{
	return QSE_STX_ISSMALLINT(obj)? 
		stx->class_smallinteger: QSE_STX_CLASS(stx,obj);
}

qse_word_t qse_stx_classof (qse_stx_t* stx, qse_stx_objref_t obj)
{
	return QSE_STX_ISSMALLINT(obj)? 
		stx->class_smallinteger: QSE_STX_CLASS(stx,obj);
}

qse_word_t qse_stx_sizeof (qse_stx_t* stx, qse_stx_objref_t obj)
{
	return QSE_STX_ISSMALLINT(obj)? 1: QSE_STX_SIZE(stx,obj);
}

/*
 * $Id$
 *
    Copyright 2006-2012 Chung, Hyung-Hwan.
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
    License along with QSE. If not, see <htrd://www.gnu.org/licenses/>.
 */

#ifndef _QSE_NET_HTTPD_H_
#define _QSE_NET_HTTPD_H_

#include <qse/types.h>
#include <qse/macros.h>
#include <qse/net/htre.h>
#include <qse/net/htrd.h>
#include <qse/cmn/nwad.h>
#include <qse/cmn/time.h>

typedef struct qse_httpd_t        qse_httpd_t;
typedef struct qse_httpd_server_t qse_httpd_server_t;
typedef struct qse_httpd_client_t qse_httpd_client_t;

enum qse_httpd_errnum_t
{
	QSE_HTTPD_ENOERR,
	QSE_HTTPD_EOTHER,
	QSE_HTTPD_ENOIMPL,
	QSE_HTTPD_ESYSERR,
	QSE_HTTPD_EINTERN,

	QSE_HTTPD_ENOMEM,
	QSE_HTTPD_EINVAL,
	QSE_HTTPD_EACCES,
	QSE_HTTPD_ENOENT,
	QSE_HTTPD_EEXIST,
	QSE_HTTPD_EINTR,
	QSE_HTTPD_EPIPE,
	QSE_HTTPD_EAGAIN,

	QSE_HTTPD_ECONN,
	QSE_HTTPD_ENOBUF,  /* no buffer available */
	QSE_HTTPD_EDISCON, /* client disconnnected */
	QSE_HTTPD_EBADREQ, /* bad request */
	QSE_HTTPD_ETASK
};
typedef enum qse_httpd_errnum_t qse_httpd_errnum_t;

enum qse_httpd_option_t
{
	QSE_HTTPD_MUTECLIENT   = (1 << 0),
	QSE_HTTPD_CGIERRTONUL  = (1 << 1),
	QSE_HTTPD_CGINOCLOEXEC = (1 << 2),
	QSE_HTTPD_CGINOCHUNKED = (1 << 3)
};

typedef struct qse_httpd_stat_t qse_httpd_stat_t;
struct qse_httpd_stat_t
{
	int        isdir;
	qse_long_t dev;
	qse_long_t ino;
	qse_foff_t size;
	qse_ntime_t mtime;
};

typedef struct qse_httpd_peer_t qse_httpd_peer_t;
struct qse_httpd_peer_t
{
	qse_nwad_t nwad;
	qse_nwad_t local; /* local side address facing the peer */
	qse_ubi_t  handle;
};

enum qse_httpd_mux_mask_t
{
	QSE_HTTPD_MUX_READ  = (1 << 0),
	QSE_HTTPD_MUX_WRITE = (1 << 1)
};

typedef int (*qse_httpd_muxcb_t) (
	qse_httpd_t* httpd,
	void*        mux,
	qse_ubi_t    handle,
	int          mask, /* ORed of qse_httpd_mux_mask_t */
	void*        cbarg
);

typedef struct qse_httpd_dirent_t qse_httpd_dirent_t;

struct qse_httpd_dirent_t
{
	qse_mchar_t*     name;
	qse_httpd_stat_t stat;
};

typedef struct qse_httpd_scb_t qse_httpd_scb_t;
struct qse_httpd_scb_t
{
	struct
	{
		int (*open) (qse_httpd_t* httpd, qse_httpd_server_t* server);
		void (*close) (qse_httpd_t* httpd, qse_httpd_server_t* server);
		int (*accept) (qse_httpd_t* httpd, qse_httpd_server_t* server, qse_httpd_client_t* client);
	} server;

	struct
	{
		int (*open) (qse_httpd_t* httpd, qse_httpd_peer_t* peer);
		void (*close) (qse_httpd_t* httpd, qse_httpd_peer_t* peer);
		int (*connected) (qse_httpd_t* httpd, qse_httpd_peer_t* peer);

		qse_ssize_t (*recv) (
			qse_httpd_t* httpd, 
			qse_httpd_peer_t* peer,
			qse_mchar_t* buf, qse_size_t bufsize);

		qse_ssize_t (*send) (
			qse_httpd_t* httpd,
			qse_httpd_peer_t* peer,
			const qse_mchar_t* buf, qse_size_t bufsize);
	} peer;

	struct
	{
		void* (*open)   (qse_httpd_t* httpd, qse_httpd_muxcb_t muxcb);
		void  (*close)  (qse_httpd_t* httpd, void* mux);
		int   (*addhnd) (qse_httpd_t* httpd, void* mux, qse_ubi_t handle, int mask, void* cbarg);
		int   (*delhnd) (qse_httpd_t* httpd, void* mux, qse_ubi_t handle);
		int   (*poll)   (qse_httpd_t* httpd, void* mux, const qse_ntime_t* tmout);

		int (*readable) (
			qse_httpd_t* httpd, qse_ubi_t handle, const qse_ntime_t* tmout);
		int (*writable) (
			qse_httpd_t* httpd, qse_ubi_t handle, const qse_ntime_t* tmout);
	} mux;

	struct
	{
		int (*stat) (
			qse_httpd_t* httpd, const qse_mchar_t* path, 
			qse_httpd_stat_t* stat);
			
		int (*ropen) (
			qse_httpd_t* httpd, const qse_mchar_t* path, 
			qse_ubi_t* handle);
		int (*wopen) (
			qse_httpd_t* httpd, const qse_mchar_t* path, 
			qse_ubi_t* handle);
		void (*close) (qse_httpd_t* httpd, qse_ubi_t handle);

		qse_ssize_t (*read) (
			qse_httpd_t* httpd, qse_ubi_t handle,
			qse_mchar_t* buf, qse_size_t len);
		qse_ssize_t (*write) (
			qse_httpd_t* httpd, qse_ubi_t handle,
			const qse_mchar_t* buf, qse_size_t len);
	} file;

	struct
	{
		int (*open) (
			qse_httpd_t* httpd, const qse_mchar_t* path, 
			qse_ubi_t* handle);
		void (*close) (qse_httpd_t* httpd, qse_ubi_t handle);
		int (*read) (
			qse_httpd_t* httpd, qse_ubi_t handle,
			qse_httpd_dirent_t* ent);
	} dir;

	struct
	{
		void (*close) (
			qse_httpd_t* httpd,
			qse_httpd_client_t* client);

		void (*shutdown) (
			qse_httpd_t* httpd,
			qse_httpd_client_t* client);

		/* action */
		qse_ssize_t (*recv) (
			qse_httpd_t* httpd, 
			qse_httpd_client_t* client,
			qse_mchar_t* buf, qse_size_t bufsize);

		qse_ssize_t (*send) (
			qse_httpd_t* httpd,
			qse_httpd_client_t* client,
			const qse_mchar_t* buf, qse_size_t bufsize);

		qse_ssize_t (*sendfile) (
			qse_httpd_t* httpd,
			qse_httpd_client_t* client,
			qse_ubi_t handle, qse_foff_t* offset, qse_size_t count);

		/* event notification */
		int (*accepted) (
			qse_httpd_t* httpd,
			qse_httpd_client_t* client);  /* optional */
		void (*closed) (
			qse_httpd_t* httpd,
			qse_httpd_client_t* client);  /* optional */
	} client;

};

typedef struct qse_httpd_rcb_t qse_httpd_rcb_t;
struct qse_httpd_rcb_t
{
	int (*peek_request) (
		qse_httpd_t* httpd, qse_httpd_client_t* client, qse_htre_t* req);
	int (*handle_request) (
		qse_httpd_t* httpd, qse_httpd_client_t* client, qse_htre_t* req);

	int (*format_err) (
		qse_httpd_t* httpd, qse_httpd_client_t* client, 
		int code, qse_mchar_t* buf, int bufsz);
	int (*format_dir) (
		qse_httpd_t* httpd, qse_httpd_client_t* client, 
		const qse_mchar_t* qpath, const qse_httpd_dirent_t* dirent,
		qse_mchar_t* buf, int bufsz);
};

/* -------------------------------------------------------------------------- */

typedef struct qse_httpd_task_t qse_httpd_task_t;

typedef int (*qse_httpd_task_init_t) (
	qse_httpd_t*        httpd,
	qse_httpd_client_t* client,
	qse_httpd_task_t*   task
);

typedef void (*qse_httpd_task_fini_t) (	
	qse_httpd_t*        httpd,
	qse_httpd_client_t* client,
	qse_httpd_task_t*   task
);

typedef int (*qse_httpd_task_main_t) (
	qse_httpd_t*        httpd,
	qse_httpd_client_t* client,
	qse_httpd_task_t*   task
);

enum qse_httpd_task_trigger_mask_t
{
	QSE_HTTPD_TASK_TRIGGER_READ      = (1 << 0),
	QSE_HTTPD_TASK_TRIGGER_WRITE     = (1 << 1),
	QSE_HTTPD_TASK_TRIGGER_READABLE  = (1 << 2),
	QSE_HTTPD_TASK_TRIGGER_WRITABLE  = (1 << 3)
};

typedef struct qse_httpd_task_trigger_t qse_httpd_task_trigger_t;
struct qse_httpd_task_trigger_t
{
	int       mask; /* QSE_HTTPD_TASK_TRIGGER_READ | QSE_HTTPD_TASK_TRIGGER_WRITE */
	qse_ubi_t handle;
};

#define QSE_HTTPD_TASK_TRIGGER_MAX 3

struct qse_httpd_task_t
{
	/* == PUBLIC  == */

	/* you must not call another entask functions from within 
	 * an initailizer. you can call entask functions from within 
	 * a finalizer and a main function. */
	qse_httpd_task_init_t    init;
	qse_httpd_task_fini_t    fini;
	qse_httpd_task_main_t    main;
	qse_httpd_task_trigger_t trigger[QSE_HTTPD_TASK_TRIGGER_MAX];
	void*                    ctx;

	/* == PRIVATE  == */
	qse_httpd_task_t*     prev;
	qse_httpd_task_t*     next;
};

enum qse_httpd_sctype_t 
{
	QSE_HTTPD_SERVER,
	QSE_HTTPD_CLIENT
};
typedef enum qse_httpd_sctype_t  qse_httpd_sctype_t;

struct qse_httpd_client_t
{
	/* == PRIVATE == */
	qse_httpd_sctype_t       type;

	/* == PUBLIC  == */
	qse_ubi_t                handle;
	qse_ubi_t                handle2;
	qse_nwad_t               remote_addr;
	qse_nwad_t               local_addr;
	qse_nwad_t               orgdst_addr;
	qse_httpd_server_t*      server;
	int                      initial_ifindex;

	/* == PRIVATE == */
	qse_htrd_t*              htrd;
	int                      status;
	qse_httpd_task_trigger_t trigger[QSE_HTTPD_TASK_TRIGGER_MAX];
	qse_ntime_t              last_active;

	qse_httpd_client_t*      prev;
	qse_httpd_client_t*      next;
 
	qse_httpd_client_t*      bad_next;

	struct
	{
		int count;
		qse_httpd_task_t* head;
		qse_httpd_task_t* tail;
	} task;
};

enum qse_httpd_server_flag_t
{
	QSE_HTTPD_SERVER_ACTIVE     = (1 << 0),
	QSE_HTTPD_SERVER_SECURE     = (1 << 1),
	QSE_HTTPD_SERVER_BINDTONWIF = (1 << 2)
};

typedef void (*qse_httpd_server_predetach_t) (
	qse_httpd_t*        httpd,
	qse_httpd_server_t* server
);

struct qse_httpd_server_t
{
	qse_httpd_sctype_t       type;

	/* ---------------------------------------------- */
	int          flags;
	qse_nwad_t   nwad; /* binding address */
	unsigned int nwif; /* interface number to bind to */

	/* set by server.open callback */
	qse_ubi_t  handle;

	/* private  */
	qse_httpd_server_predetach_t predetach;
	qse_httpd_server_t*          next;
	qse_httpd_server_t*          prev;
};

/* -------------------------------------------------------------------------- */

/**
 * The qse_httpd_rsrc_type_t defines the resource type than can 
 * be entasked with qse_httpd_entaskrsrc().
 */
enum qse_httpd_rsrc_type_t
{
	QSE_HTTPD_RSRC_AUTH,
	QSE_HTTPD_RSRC_CGI,
	QSE_HTTPD_RSRC_DIR,
	QSE_HTTPD_RSRC_ERR,
	QSE_HTTPD_RSRC_FILE,
	QSE_HTTPD_RSRC_PROXY,
	QSE_HTTPD_RSRC_RELOC,
	QSE_HTTPD_RSRC_REDIR,
	QSE_HTTPD_RSRC_TEXT
};
typedef enum qse_httpd_rsrc_type_t qse_httpd_rsrc_type_t;

typedef struct qse_httpd_rsrc_t qse_httpd_rsrc_t;

struct qse_httpd_rsrc_t
{
	qse_httpd_rsrc_type_t type;
	union 
	{
		struct
		{
			const qse_mchar_t* realm;
		} auth;
		struct 
		{
			const qse_mchar_t* path;
			const qse_mchar_t* script;
			const qse_mchar_t* suffix;
			const qse_mchar_t* docroot;
			int nph;
		} cgi;	
		struct
		{
			const qse_mchar_t* path;
		} dir;

		struct
		{
			int code;
		} err;

		struct
		{
			const qse_mchar_t* path;
			const qse_mchar_t* mime;
		} file;

		struct
		{
			qse_nwad_t dst;
			qse_nwad_t src;
		} proxy;

		struct
		{
			const qse_mchar_t* dst;
		} reloc;	

		struct
		{
			const qse_mchar_t* dst;
		} redir;	

		struct
		{
			const qse_mchar_t* ptr;
			const qse_mchar_t* mime;
		} text;
	} u;
};

/**
 * The qse_httpd_ecb_close_t type defines the callback function
 * called when an httpd object is closed.
 */
typedef void (*qse_httpd_ecb_close_t) (
	qse_httpd_t* httpd  /**< httpd */
);

/**
 * The qse_httpd_ecb_t type defines an event callback set.
 * You can register a callback function set with
 * qse_httpd_pushecb().  The callback functions in the registered
 * set are called in the reverse order of registration.
 */
typedef struct qse_httpd_ecb_t qse_httpd_ecb_t;
struct qse_httpd_ecb_t
{
	/**
	 * called by qse_httpd_close().
	 */
	qse_httpd_ecb_close_t close;

	/* internal use only. don't touch this field */
	qse_httpd_ecb_t* next;
};

typedef struct qse_httpd_server_cbstd_t qse_httpd_server_cbstd_t;
struct qse_httpd_server_cbstd_t
{
	int (*makersrc) (qse_httpd_t* httpd, qse_httpd_client_t* client, qse_htre_t* req, qse_httpd_rsrc_t* rsrc); /* required */
	void (*freersrc) (qse_httpd_t* httpd, qse_httpd_client_t* client, qse_htre_t* req, qse_httpd_rsrc_t* rsrc); /* optional */
};

typedef struct qse_httpd_server_cgistd_t qse_httpd_server_cgistd_t;
struct qse_httpd_server_cgistd_t
{
	const qse_mchar_t* ext;
	qse_size_t         len;
	int                nph;
};

typedef struct qse_httpd_server_mimestd_t qse_httpd_server_mimestd_t;
struct qse_httpd_server_mimestd_t
{
	const qse_mchar_t* ext;
	const qse_mchar_t* type;
};

/**
 * The qse_httpd_server_idxstd_t type defines a structure to hold
 * an index file name.
 */
typedef struct qse_httpd_server_idxstd_t qse_httpd_server_idxstd_t;
struct qse_httpd_server_idxstd_t
{
	const qse_mchar_t* name;
};

enum qse_httpd_server_optstd_t
{
	QSE_HTTPD_SERVER_DOCROOT = 0, /* const qse_mchar_t* */
	QSE_HTTPD_SERVER_REALM,       /* const qse_mchar_t* */
	QSE_HTTPD_SERVER_AUTH,        /* const qse_mchar_t* */
	QSE_HTTPD_SERVER_DIRCSS,      /* const qse_mchar_t* */
	QSE_HTTPD_SERVER_ERRCSS,      /* const qse_mchar_t* */

	QSE_HTTPD_SERVER_CBSTD,       /* qse_httpd_server_cbstd_t* */
	QSE_HTTPD_SERVER_CGISTD,      /* qse_httpd_server_cgistd_t[] */
	QSE_HTTPD_SERVER_MIMESTD,     /* qse_httpd_server_mimestd_t[] */
	QSE_HTTPD_SERVER_IDXSTD       /* qse_httpd_server_idxstd_t[] */
};
typedef enum qse_httpd_server_optstd_t qse_httpd_server_optstd_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The qse_httpd_open() function creates a httpd processor.
 */
QSE_EXPORT qse_httpd_t* qse_httpd_open (
	qse_mmgr_t* mmgr,   /**< memory manager */
	qse_size_t  xtnsize /**< extension size in bytes */
);

/**
 * The qse_httpd_close() function destroys a httpd processor.
 */
QSE_EXPORT void qse_httpd_close (
	qse_httpd_t* httpd 
);

QSE_EXPORT qse_mmgr_t* qse_httpd_getmmgr (
	qse_httpd_t* httpd
); 

QSE_EXPORT void* qse_httpd_getxtn (
	qse_httpd_t* httpd
);

QSE_EXPORT qse_httpd_errnum_t qse_httpd_geterrnum (
	qse_httpd_t* httpd
);

QSE_EXPORT void qse_httpd_seterrnum (
	qse_httpd_t*       httpd,
	qse_httpd_errnum_t errnum
);

QSE_EXPORT int qse_httpd_getoption (
	qse_httpd_t* httpd
);

QSE_EXPORT void qse_httpd_setoption (
	qse_httpd_t* httpd,
	int          option
);

/**
 * The qse_httpd_popecb() function pops an httpd event callback set
 * and returns the pointer to it. If no callback set can be popped,
 * it returns #QSE_NULL.
 */
QSE_EXPORT qse_httpd_ecb_t* qse_httpd_popecb (
	qse_httpd_t* httpd /**< httpd */
);

/**
 * The qse_httpd_pushecb() function register a runtime callback set.
 */
QSE_EXPORT void qse_httpd_pushecb (
	qse_httpd_t*     httpd, /**< httpd */
	qse_httpd_ecb_t* ecb  /**< callback set */
);

/**
 * The qse_httpd_loop() function starts a httpd server loop.
 */
QSE_EXPORT int qse_httpd_loop (
	qse_httpd_t*        httpd, 
	qse_httpd_scb_t*    scb,
	qse_httpd_rcb_t*    rcb,
	const qse_ntime_t*  tmout
);

/**
 * The qse_httpd_stop() function requests to stop qse_httpd_loop()
 */
QSE_EXPORT void qse_httpd_stop (
	qse_httpd_t* httpd
);

#define qse_httpd_getserverxtn(httpd,server) ((void*)(server+1))

QSE_EXPORT qse_httpd_server_t* qse_httpd_attachserver (
	qse_httpd_t*                 httpd,
	const qse_httpd_server_t*    tmpl,
	qse_httpd_server_predetach_t predetach,	
	qse_size_t                   xtnsize
);

QSE_EXPORT void qse_httpd_detachserver (
	qse_httpd_t*        httpd,
	qse_httpd_server_t* server
);

QSE_EXPORT void qse_httpd_discardcontent (
	qse_httpd_t*        httpd,
	qse_htre_t*         req
);

QSE_EXPORT void qse_httpd_completecontent (
	qse_httpd_t*        httpd,
	qse_htre_t*         req
);


/**
 * The qse_httpd_setname() function changes the string
 * to be used as the value for the server header. 
 */
QSE_EXPORT void qse_httpd_setname (
	qse_httpd_t*       httpd,
	const qse_mchar_t* name
);


/**
 * The qse_httpd_getname() function returns the
 * pointer to the string used as the value for the server
 * header.
 */
QSE_EXPORT qse_mchar_t* qse_httpd_getname (
	qse_httpd_t* httpd
);

/**
 * The qse_httpd_fmtgmtimetobb() function converts a numeric time @a nt
 * to a string and stores it in a built-in buffer. 
 * If @a nt is QSE_NULL, the current time is used.
 */
QSE_EXPORT const qse_mchar_t* qse_httpd_fmtgmtimetobb (
	qse_httpd_t*       httpd,
	const qse_ntime_t* nt,
	int                idx
);

#define qse_httpd_gettaskxtn(httpd,task) ((void*)(task+1))

QSE_EXPORT qse_httpd_task_t* qse_httpd_entask (
	qse_httpd_t*            httpd,
	qse_httpd_client_t*     client,
	qse_httpd_task_t*       pred,
	const qse_httpd_task_t* task,
	qse_size_t              xtnsize
);

/* -------------------------------------------- */

QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskdisconnect (
	qse_httpd_t*            httpd,
	qse_httpd_client_t*     client,
	qse_httpd_task_t*       pred
);


QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskformat (
	qse_httpd_t*            httpd,
	qse_httpd_client_t*     client,
	qse_httpd_task_t*       pred,
	const qse_mchar_t*      fmt,
	...
);

/* -------------------------------------------- */

QSE_EXPORT qse_httpd_task_t* qse_httpd_entasktext (
	qse_httpd_t*            httpd,
	qse_httpd_client_t*     client,
	qse_httpd_task_t*       pred,
	const qse_mchar_t*      text,
	const qse_mchar_t*      mime,
	qse_htre_t*             req
);

QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskerr (
     qse_httpd_t*              httpd,
	qse_httpd_client_t*       client,
	qse_httpd_task_t*         pred,
     int                       code, 
	qse_htre_t*               req
);

QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskcontinue (
     qse_httpd_t*              httpd,
	qse_httpd_client_t*       client,
	qse_httpd_task_t*         pred,
	qse_htre_t*               req
);

/**
 * The qse_httpd_entaskauth() function adds a basic authorization task.
 */
QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskauth (
     qse_httpd_t*              httpd,
	qse_httpd_client_t*       client,
	qse_httpd_task_t*         pred,
	const qse_mchar_t*        realm,
	qse_htre_t*               req
);

QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskreloc (
     qse_httpd_t*              httpd,
	qse_httpd_client_t*       client,
	qse_httpd_task_t*         pred,
	const qse_mchar_t*        dst,
	qse_htre_t*               req
);

QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskredir (
     qse_httpd_t*              httpd,
	qse_httpd_client_t*       client,
	qse_httpd_task_t*         pred,
	const qse_mchar_t*        dst,
	qse_htre_t*               req
);


QSE_EXPORT qse_httpd_task_t* qse_httpd_entasknomod (
     qse_httpd_t*              httpd,
	qse_httpd_client_t*       client,
	qse_httpd_task_t*         pred,
	qse_htre_t*               req
);

QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskdir (
	qse_httpd_t*              httpd,
	qse_httpd_client_t*       client,
	qse_httpd_task_t*         pred,
	const qse_mchar_t*        name,
	qse_htre_t*               req
);

QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskfile (
	qse_httpd_t*              httpd,
	qse_httpd_client_t*       client,
	qse_httpd_task_t*         pred,
	const qse_mchar_t*        name,
	const qse_mchar_t*        mime,
	qse_htre_t*               req
);

QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskcgi (
	qse_httpd_t*              httpd,
	qse_httpd_client_t*       client,
	qse_httpd_task_t*         pred,
	const qse_mchar_t*        path,
	const qse_mchar_t*        script,
	const qse_mchar_t*        suffix,
	const qse_mchar_t*        docroot,
	int                       nph,
	qse_htre_t*               req
);

QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskproxy (
	qse_httpd_t*            httpd,
	qse_httpd_client_t*     client,
	qse_httpd_task_t*       pred,
	const qse_nwad_t*       dst,
	const qse_nwad_t*       src,
	qse_htre_t*             req
);

/* -------------------------------------------- */

QSE_EXPORT qse_httpd_task_t* qse_httpd_entaskrsrc (
	qse_httpd_t*            httpd,
	qse_httpd_client_t*     client,
	qse_httpd_task_t*       pred,
	qse_httpd_rsrc_t*       rsrc,
	qse_htre_t*             req
);

/* -------------------------------------------- */

QSE_EXPORT void* qse_httpd_allocmem (
	qse_httpd_t* httpd, 
	qse_size_t   size
);

QSE_EXPORT void* qse_httpd_reallocmem (
	qse_httpd_t* httpd,
	void*        ptr,
	qse_size_t   size
);

QSE_EXPORT void qse_httpd_freemem (
	qse_httpd_t* httpd,
	void*        ptr
);

/* -------------------------------------------- */

QSE_EXPORT qse_httpd_t* qse_httpd_openstd (
	qse_size_t xtnsize
);

QSE_EXPORT qse_httpd_t* qse_httpd_openstdwithmmgr (
	qse_mmgr_t* mmgr,
	qse_size_t  xtnsize
);

QSE_EXPORT void* qse_httpd_getxtnstd (
	qse_httpd_t* httpd
);

QSE_EXPORT qse_httpd_server_t* qse_httpd_attachserverstd (
	qse_httpd_t*                 httpd,
	const qse_char_t*            uri,
	qse_httpd_server_predetach_t predetach,	
	qse_size_t                   xtnsize
);

QSE_EXPORT int qse_httpd_getserveroptstd (
	qse_httpd_t*              httpd,
	qse_httpd_server_t*       server,
	qse_httpd_server_optstd_t id,
	void*                     value
);

QSE_EXPORT int qse_httpd_setserveroptstd (
	qse_httpd_t*              httpd,
	qse_httpd_server_t*       server,
	qse_httpd_server_optstd_t id,
	const void*               value
);

QSE_EXPORT void* qse_httpd_getserverxtnstd (
	qse_httpd_t*         httpd,
	qse_httpd_server_t*  server
);

QSE_EXPORT int qse_httpd_loopstd (
	qse_httpd_t*       httpd, 
	const qse_ntime_t* tmout
);


#ifdef __cplusplus
}
#endif

#endif

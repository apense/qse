
#include <qse/net/httpd.h>
#include <qse/cmn/stdio.h>
#include <qse/cmn/main.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>

#define MAX_SENDFILE_SIZE 4096
typedef struct httpd_xtn_t httpd_xtn_t;
struct httpd_xtn_t
{
	const qse_httpd_cbs_t* orgcbs;
};

static qse_htb_walk_t walk (qse_htb_t* htb, qse_htb_pair_t* pair, void* ctx)
{
qse_printf (QSE_T("HEADER OK %d[%S] %d[%S]\n"),  (int)QSE_HTB_KLEN(pair), QSE_HTB_KPTR(pair), (int)QSE_HTB_VLEN(pair), QSE_HTB_VPTR(pair));
	return QSE_HTB_WALK_FORWARD;
}

static int handle_request (
	qse_httpd_t* httpd, qse_httpd_client_t* client, qse_htre_t* req)
{
	int method;

#if 0
	httpd_xtn_t* xtn = (httpd_xtn_t*) qse_httpd_getxtn (httpd);
	return xtn->orgcbs->handle_request (httpd, client, req);
#endif

qse_printf (QSE_T("================================\n"));
qse_printf (QSE_T("REQUEST ==> [%S] version[%d.%d] method[%d]\n"), 
     qse_htre_getqpathptr(req),
     qse_htre_getmajorversion(req),
     qse_htre_getminorversion(req),
     qse_htre_getqmethod(req)
);
if (qse_htre_getqparamlen(req) > 0)
{
qse_printf (QSE_T("PARAMS ==> [%S]\n"), qse_htre_getqparamptr(req));
}

qse_htb_walk (&req->hdrtab, walk, QSE_NULL);
if (QSE_MBS_LEN(&req->content) > 0)
{
qse_printf (QSE_T("content = [%.*S]\n"),
          (int)QSE_MBS_LEN(&req->content),
          QSE_MBS_PTR(&req->content));
}

	method = qse_htre_getqmethod (req);

	if (method == QSE_HTTP_GET || method == QSE_HTTP_POST)
	{
		const qse_mchar_t* rangestr;
		qse_http_range_t range;
		int x;

		rangestr = qse_htre_gethdrval (req, "Range");
		if (rangestr && qse_parsehttprange (rangestr, &range) <= -1)
		{
			const qse_mchar_t* msg;
			msg = QSE_MT("<html><head><title>Requested range not satisfiable</title></head><body><b>REQUESTED RANGE NOT SATISFIABLE</b></body></html>");
			x = qse_httpd_entaskformat (httpd, client,
				QSE_MT("HTTP/%d.%d 416 Requested range not satisfiable\r\nContent-Length: %d\r\n\r\n%s\r\n\r\n"), 
				req->version.major, req->version.minor,
				(int)qse_mbslen(msg) + 4, msg
			);
			if (x <= -1) goto oops;
		}

		x = qse_httpd_entaskpath (
			httpd, client, 
			qse_htre_getqpathptr(req),
			(rangestr? &range: QSE_NULL),
			qse_htre_getversion(req)
		);
		if (x <= -1) goto oops;
	}
	else
	{
		const qse_mchar_t* msg = QSE_MT("<html><head><title>Method not allowed</title></head><body><b>REQUEST METHOD NOT ALLOWED</b></body></html>");
		if (qse_httpd_entaskformat (httpd, client,
			QSE_MT("HTTP/%d.%d 405 Method not allowed\r\nContent-Length: %d\r\n\r\n%s\r\n\r\n"), 
			req->version.major, req->version.minor,
			(int)qse_mbslen(msg) + 4, msg) <= -1) goto oops;
	}

	if (req->attr.connection_close)
	{
		if (qse_httpd_entaskdisconnect (httpd, client) <= -1) return -1;
	}

	return 0;

oops:
	qse_httpd_markclientbad (httpd, client);
	return 0;
}

static int handle_expect_continue (
	qse_httpd_t* httpd, qse_httpd_client_t* client, qse_htre_t* req)
{
	httpd_xtn_t* xtn = (httpd_xtn_t*) qse_httpd_getxtn (httpd);
	return xtn->orgcbs->handle_expect_continue (httpd, client, req);
}

static qse_httpd_t* httpd = NULL;

static void sigint (int sig)
{
	qse_httpd_stop (httpd);
}

static qse_httpd_cbs_t httpd_cbs =
{
	{ QSE_NULL },
	handle_request,
	handle_expect_continue
};

int httpd_main (int argc, qse_char_t* argv[])
{
	int n;
	httpd_xtn_t* xtn;

	if (argc != 2)
	{
		qse_fprintf (QSE_STDERR, QSE_T("Usage: %s <listener_uri>\n"), argv[0]);
		return -1;
	}


	httpd = qse_httpd_open (QSE_NULL, QSE_SIZEOF(httpd_xtn_t));
	if (httpd == QSE_NULL)
	{
		qse_fprintf (QSE_STDERR, QSE_T("Cannot open httpd\n"));
		return -1;
	}

	xtn = (httpd_xtn_t*)qse_httpd_getxtn (httpd);
	xtn->orgcbs = qse_httpd_getcbs (httpd);

	if (qse_httpd_addlisteners (httpd, argv[1]) <= -1)
	{
		qse_fprintf (QSE_STDERR, QSE_T("Failed to add httpd listeners\n"));
		qse_httpd_close (httpd);
		return -1;
	}

	qse_httpd_setcbs (httpd, &httpd_cbs);

	signal (SIGINT, sigint);
	signal (SIGPIPE, SIG_IGN);

	n = qse_httpd_loop (httpd, 0);

	signal (SIGINT, SIG_DFL);
	signal (SIGPIPE, SIG_DFL);

	qse_httpd_close (httpd);
	return n;
}

int qse_main (int argc, qse_achar_t* argv[])
{
	return qse_runmain (argc, argv, httpd_main);
}

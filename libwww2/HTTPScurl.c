/* Bare-bones HTTPS support for Mosaic.
** Executes curl or wget to retrieve pages, meaning this is a very
** dumb and simple module.
**
** Author: B. Watson <yalhcru@gmail.com>
**
** BUGS:
** - UTTERLY insecure!
** - POST forms won't work (does Mosaic even support them?)
** - User-agent reads "curl" (fixable)
** - No Referer headers get sent (fixable)
** - HTTP status not checked, no user-visible error messages (just
**   get a blank page on failure)
** - Throbber and progress bar don't get updated
** - Segfaults image/svg+xml MIME type
** - Redirects from https:// to http:// don't get noticed: the URL bar
**   still says https://, and this module is still used to load them,
**   instead of the actual HTTP module.
*/

#include <stdio.h>
/* some of these includes will be redundant */
#include "../config.h"
// #include <ui.h>
#include "HTUtils.h"
// #include "tcp.h"
#include "HTParse.h"
#include "HTAccess.h"
#include "HTML.h"
#include "HTFile.h"
#include "HTAlert.h"
#include "HTFormat.h"

#define PUTC(c) (*target->isa->put_character)(target, c)
#define PUTS(s) (*target->isa->put_string)(target, s)
#define PUTBLOCK(c, len) (*target->isa->put_block)(target, c, len)
#define END_TARGET (*target->isa->end_document)(target)
#define FREE_TARGET (*target->isa->free)(target)

struct _HTStructured {
	WWW_CONST HTStructuredClass * isa;
};

struct _HTStream {
	WWW_CONST HTStreamClass * isa;
};

#ifdef HAS_CURL

/* N.B. --insecure means *don't* check for valid SSL certs! Do not use
   this to log in to your bank!
   TODO: --referer --user-agent
	TODO: UI user pref for --insecure flag (check certificates)
*/
#define CURL_CMD "curl --include --insecure --silent --show-error --location %s 2>&1"

#define BUFFER_SIZE 4096

PRIVATE int curl_to_target(FILE *f, HTFormat format_out, HTStream *sink, HTParentAnchor *anchor)
{
	char buf[BUFFER_SIZE], content_type[100], *p, *q;
	int len, total = 0, redirect = 0;
	HTFormat format_in;
	HTStream *target;

	content_type[0] = '\0';
	/* parse the HTTP headers. all we actually need is Content-type.
		In case of a redirect, curl will give us multiple sets of headers
		(one per redirect). We only want to pay attention to the last set. */
	while( (fgets(buf, BUFFER_SIZE, f)) ) {
		if(strlen(buf) < 3) {
			/* got \r\n */
			if(!redirect) break;
			redirect = 0;
			continue;
		}

		/* fprintf(stderr, "header: %s\n", buf); */

		if(strncasecmp(buf, "Location: ", 10) == 0)
			redirect = 1;

		if(strncasecmp(buf, "Content-Type: ", 14) == 0) {
			p = buf + 14;
			q = content_type;
			while(*p && *p != '\r' && *p != '\n' && *p != ';' && *p != ' ' && *p != '+') {
				*q = *p;
				p++;
				q++;
			}
			*q = '\0';
		}
	}

	if(!content_type[0]) {
		/* no content-type, assume HTML */
		fprintf(stderr, "%s:%d: no Content-Type, assuming text/html\n", __FILE__, __LINE__);
		strcpy(content_type, "text/html");
	} else {
		fprintf(stderr, "%s:%d: content_type == \"%s\"\n", __FILE__, __LINE__, content_type);
	}

	format_in = HTAtom_for(content_type);
	target = HTStreamStack(format_in, format_out, 0, sink, anchor);
	if(!target) { /* TODO: error msg */
		fprintf(stderr, "%s:%d: target is NULL\n", __FILE__, __LINE__);
		return HT_NOT_LOADED;
	}

	/* all that's left is the actual content */
	while( (len = fread(buf, 1, BUFFER_SIZE, f)) > 0) {
		PUTBLOCK(buf, len);
		total += len;
	}

	/* TODO: if(!total) ...let the user know */
	fprintf(stderr, "%s:%d: read %d bytes\n", __FILE__, __LINE__, total);

	FREE_TARGET;
	return HT_LOADED;
}

PUBLIC int HTTPSLoad ARGS4 (
   WWW_CONST char *,    addr,
   HTParentAnchor *, anchor,
   HTFormat,      format_out,
   HTStream *,    sink
)
{
	char cmd[BUFFER_SIZE];
	int result = HT_LOADED;
	FILE *childf;

	// HTStructured *target = HTML_new(anchor, format_out, sink);

	/* TODO: length check! Can we just use snprintf() here? */
	sprintf(cmd, CURL_CMD, addr);
	fprintf(stderr, "%s:%d: got here, cmd: %s\n", __FILE__, __LINE__, cmd);

	if( (childf = popen(cmd, "r")) ) {
		result = curl_to_target(childf, format_out, sink, anchor);
		pclose(childf);
		/* TODO: check return status of curl */
	} else {
		// curl_failed(target);
		result = HT_NOT_LOADED;
	}
	return result;
}

#else
PUBLIC int HTTPSLoad ARGS4 (
   WWW_CONST char *,    addr,
   HTParentAnchor *, anchor,
   HTFormat,      format_out,
   HTStream *,    sink
)
{
	HTFormat *format_in;
	HTStructured *target;

	HTProgress("Unsupported");

	target = HTML_new(anchor, format_out, sink);
	PUTS(
"<h1>Unsupported</h1>"
"Support for HTTPS in Mosaic-CK requires cURL. See documentation."
	);
	END_TARGET;
	FREE_TARGET;

	return HT_LOADED;
}
#endif
PUBLIC HTProtocol HTTPS  = { "https", HTTPSLoad, 0 };

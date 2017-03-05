#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#define _GNU_SOURCE
#include <string.h>
#include <errno.h>
#include "http_parsing.h"
#include "tdate_parse.h"

#define SPACE_OR_TAB(x)  ((x) == ' '  || (x) == '\t')
#define CR_OR_NEWLINE(x) ((x) == '\r' || (x) == '\n')

/*---------------------------------------------------------------------------*/
static char* 
nre_strcasestr(const char* buf, const char* key)
{
    int n = strlen(key) - 1;
    const char *p = buf;

	while (*p) {
		while (*p && *p != *key) /* first character match */
			p++;

		if (*p == '\0') 
			return (NULL);
		
		if (!strncasecmp(p + 1, key + 1, n)) 
			return (char *)p;
		p++;
    }
	return NULL;
}
/*--------------------------------------------------------------------------*/
int 
find_http_header(char *data, int len)
{
	char *temp = data;
	int hdr_len = 0;
	char ch = data[len]; /* remember it */

	/* null terminate the string first */
	data[len] = 0;
	while (!hdr_len && (temp = strchr(temp, '\n')) != NULL) {
		temp++;
		if (*temp == '\n') 
			hdr_len = temp - data + 1;
		else if (len > 0 && *temp == '\r' && *(temp + 1) == '\n') 
			hdr_len = temp - data + 2;
	}
	data[len] = ch; /* put it back */

	/* terminate the header if found */
	if (hdr_len) 
		data[hdr_len-1] = 0;

	return hdr_len;
}
/*--------------------------------------------------------------------------*/
int
is_http_request(char * data, int len)
{
	if (len >= sizeof(HTTP_GET)-1 && 
			!strncmp(data, HTTP_GET, sizeof(HTTP_GET)-1))
			return GET;
	
	if (len >= sizeof(HTTP_POST)-1 &&
			!strncmp(data, HTTP_POST, sizeof(HTTP_POST)-1))
		return POST;

	return 0;
}
/*--------------------------------------------------------------------------*/
int
is_http_response(char * data, int len)
{
	if (len < (sizeof(HTTP_STR)-1))
		return 0;

	if (!strncmp(data, HTTP_STR, sizeof(HTTP_STR)-1))
		return 1;

	return 0;
}
/*---------------------------------------------------------------------------*/
char *
http_header_str_val(const char* buf, const char *key, const int keylen, 
					char* value, int value_len)
{
	char *temp = nre_strcasestr(buf, key);
	int i = 0;
	
	if (temp == NULL) {
		*value = 0;
		return NULL;
	}

	/* skip whitespace or tab */
	temp += keylen;
	while (*temp && SPACE_OR_TAB(*temp))
		temp++;

	/* if we reached the end of the line, forget it */
	if (*temp == '\0' || CR_OR_NEWLINE(*temp)) {
		*value = 0;
		return NULL;
	}

	/* copy value data */
	while (*temp && !CR_OR_NEWLINE(*temp) && i < value_len-1)
		value[i++] = *temp++;
	value[i] = 0;
	
	if (i == 0) {
		*value = 0;
		return NULL;
	}

	return value;
}
/*---------------------------------------------------------------------------*/
long int 
http_header_long_val(const char * response, const char* key, int key_len)
{
#define C_TYPE_LEN 50
	long int len;
	char value[C_TYPE_LEN];
	char *temp = http_header_str_val(response, key, key_len, value, C_TYPE_LEN);

	if (temp == NULL)
		return -1;

	len = strtol(temp, NULL, 10);
	if (errno == EINVAL || errno == ERANGE)
		return -1;

	return len;
}
/*--------------------------------------------------------------------------*/
int
http_parse_first_resp_line(const char* data, int len, int* scode, int* ver)
{
	const char *p = data;

	/* A typical first line: HTTP/1.1 200 OK */
	if (strncmp(p, HTTP_STR, sizeof(HTTP_STR)-1) != 0)
		return (0);

	/* version */
	p += sizeof(HTTP_STR);
	if (strncmp(p, "1.1", 3) == 0)
		*ver = HTTP_11;
	else if (strncmp(p, "1.0", 3) == 0)
		*ver = HTTP_10;
	else
		*ver = HTTP_09;
	
	/* status code */
	p += sizeof("1.1");
	*scode = strtol(p, NULL, 10);
	if (errno == EINVAL || errno == ERANGE)
		return 0;
	return 1;
}
/*--------------------------------------------------------------------------*/
time_t
http_header_date(const char* data, const char* field, int len)
{
	char buf[256];

	if (!http_header_str_val(data, field, len, buf, sizeof(buf)))
		return (time_t)-1;
	return httpdate_to_timet(buf);
}
/*--------------------------------------------------------------------------*/
int
http_check_header_field(const char* data, const char* field)
{
	if (nre_strcasestr(data, field))
		return 1;
	return 0;
}
/*--------------------------------------------------------------------------*/
char*
http_get_http_version_resp(char * data, int len, char* value, int value_len) 
{

	char * temp = data;
	int i = 0;
	
	if (len < (sizeof(HTTP_STR)-1)) {
		*value = 0;
		return NULL;
	}

	if (strncmp(data, HTTP_STR, sizeof(HTTP_STR)-1)) {
		*value = 0;
		return NULL;
	}
	
	while (*temp && !SPACE_OR_TAB(*temp) && i < value_len - 1)
		value[i++] = *temp++;
	value[i] = 0;
	
	return value;
}
/*--------------------------------------------------------------------------*/
char*
http_get_url(char * data, int data_len, char* value, int value_len)
{
	char *ret = data;
	char *temp;
	int i = 0;

	if (strncmp(data, HTTP_GET, sizeof(HTTP_GET)-1)) {
		*value = 0;
		return NULL;
	}
	
	ret += sizeof(HTTP_GET);
	while (*ret && SPACE_OR_TAB(*ret)) 
		ret++;

	temp = ret;
	while (*temp && *temp != ' ' && i < value_len - 1) {
		value[i++] = *temp++;
	}
	value[i] = 0;
	
	return ret;
}
/*---------------------------------------------------------------------------*/
int 
http_get_status_code(void * response)
{
	int code = 0;
	char* temp = response;
	
	while(*temp && !SPACE_OR_TAB(*temp++));
	
	code = strtol(temp, NULL, 10);
	if (errno == EINVAL || errno == ERANGE)
		return -1;
	
	return code;
}
/*---------------------------------------------------------------------------*/
int 
http_get_maxage(char *cache_ctl, int len) 
{
	#define MAXAGE	"max-age="
	#define SMAXAGE	"s-maxage="

	if(!*cache_ctl)
		return -1;
	
	char * temp = NULL;
	
	temp = nre_strcasestr(cache_ctl, MAXAGE);
	if (temp) {
		len = strtol(temp+sizeof(MAXAGE), NULL, 10);
		if (errno == EINVAL || errno == ERANGE)
			return -1;
		return len;
	}

	temp = nre_strcasestr(cache_ctl, SMAXAGE);
	if(temp) {
		len = strtol(temp+sizeof(SMAXAGE), NULL, 10);
		if (errno == EINVAL || errno == ERANGE)
			return -1;
		return len;
	}
	return -1;
}


#define NO_FCGI_DEFINES 1
#include <fcgi_stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define GET_BUFFER_MAX       104857600 // 100MB
#define DEVICE_PATH          "/dev/cdrom"
#define SERVER_NAME          "ODS/1.0"
#define USER_AGENT_STATIMAGE "CCURLBS::statImage"
#define USER_AGENT_DATAFORK  "CCURLBS::readDataFork"

int main();
void BoilerplateHeaders();
void Error400();
void Error404();
void Error416();
void ProcessHEAD();
void ProcessGET(long start, long end);

int main(void)
{
	openlog(SERVER_NAME, LOG_PID | LOG_NDELAY, LOG_DAEMON);

	while(FCGI_Accept() >= 0)
	{
		char* request_method = getenv("REQUEST_METHOD");
		char* user_agent = getenv("HTTP_USER_AGENT");
		char* range = getenv("HTTP_RANGE");
		long start = -1, end = -1;
		int ret;

		if (request_method != NULL && strcmp(request_method, "HEAD") == 0)
		{
			if (user_agent != NULL && strcmp(user_agent, USER_AGENT_STATIMAGE) != 0)
			{
				syslog(LOG_ERR, "Got bad user agent \"%s\".", user_agent);
				Error404();
				goto _DONE;
			}

			ProcessHEAD();
			goto _DONE;
		}

		if (request_method != NULL && strcmp(request_method, "GET") == 0)
		{
			if (user_agent != NULL && strcmp(user_agent, USER_AGENT_DATAFORK) != 0)
			{
				syslog(LOG_ERR, "Got bad user agent \"%s\".", user_agent);
				Error404();
				goto _DONE;
			}

			if (range == NULL || strlen(range) == 0)
			{
				syslog(LOG_ERR, "Missing Range header.");
				Error404();
				goto _DONE;
			}

			// STUPID ARCHITECTURE ASTRONAUTS MADE RFC7223 NEEDLESSLY DIFFICULT TO IMPLEMENT.
			// Yes sscanf is oh-so-wrong, but I am not going to include a fullblown HTTP header parser.
			// OK: bytes=123-456
			// OK: bytes=123-
			// OK: bytes=-123
			// TODO: bytes=0-0,-1
			ret = sscanf(range, "bytes=%ld-%ld", &start, &end);

			if (ret < 1)
			{
				syslog(LOG_ERR, "Illegal range header could not be parsed: \"%s\".", range);
				Error416();
				goto _DONE;
			}

			// Filter out obvious shit here
			if (ret == 2 && end < start)
			{
				syslog(LOG_ERR, "Illegal range header failed sanity test: \"%s\".", range);
				Error416();
				goto _DONE;
			}

			ProcessGET(start, end);
			goto _DONE;
		}

		syslog(LOG_ERR, "Illegal request method: \"%s\".", request_method);
		Error400(); // Bad Request

	_DONE:
		;
	}

	closelog();
	return 0;
}

void BoilerplateHeaders()
{
	char buf[512];
	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);

	FCGI_printf("Server: %s\r\n", SERVER_NAME);
	FCGI_printf("Date: %s\r\n", buf);
}

void Error400()
{
	syslog(LOG_NOTICE, "Returning 400 Bad Request response.");

	FCGI_printf("Status: 400 Bad Request\r\n");
	BoilerplateHeaders();
	FCGI_printf("Content-type: text/plain\r\n");
	FCGI_printf("\r\n");
	FCGI_printf("An error occurred.\r\n");
}

void Error404()
{
	syslog(LOG_NOTICE, "Returning 404 Not Found response.");

	FCGI_printf("Status: 404 Not Found\r\n");
	BoilerplateHeaders();
	FCGI_printf("Content-type: text/plain\r\n");
	FCGI_printf("\r\n");
	FCGI_printf("An error occurred.\r\n");
}

void Error416()
{
	syslog(LOG_NOTICE, "Returning 416 Range Not Satisfiable response.");

	FCGI_printf("Status: 416 Range Not Satisfiable\r\n");
	BoilerplateHeaders();
	FCGI_printf("Content-type: text/plain\r\n");
	FCGI_printf("\r\n");
	FCGI_printf("An error occurred.\r\n");
}

void ProcessHEAD()
{
	int fd;
	size_t numbytes;
	int opened = 0;

	fd = open(DEVICE_PATH, O_RDONLY | O_NONBLOCK);
	if (fd < 0)
	{
		syslog(LOG_ERR, "HEAD: Unable to open \"%s\": %s", DEVICE_PATH, strerror(errno));
		Error404();
		goto _DONE;
	}

	opened = 1;

 	if (ioctl(fd, BLKGETSIZE64, &numbytes)  < 0)
	{
		syslog(LOG_ERR, "HEAD: Unable to get size of disk: %s", strerror(errno));
		Error404();
		goto _DONE;
	}

	syslog(LOG_NOTICE, "Returning HEAD response with Content-Length of %ld bytes.", numbytes);

	FCGI_printf("Status: 200 OK\r\n");
	BoilerplateHeaders();
	FCGI_printf("Content-type: application/octet-stream\r\n");
	FCGI_printf("Accept-Ranges: bytes\r\n");
	FCGI_printf("Content-Length: %ld\r\n", numbytes);
	FCGI_printf("\r\n");

_DONE:
	if (opened)
		close(fd);
}

void ProcessGET(long start, long end)
{
	int fd;
	size_t numbytes;
	int opened = 0;
	char* buf = NULL;
	char* buf_ptr = NULL;
	long read_target = 0;
	long total_read = 0;

	syslog(LOG_NOTICE, "Processing GET response with start: %ld and end: %ld.", start, end);

	fd = open(DEVICE_PATH, O_RDONLY | O_NONBLOCK);
	if (fd < 0)
	{
		syslog(LOG_ERR, "GET: Unable to open \"%s\": %s", DEVICE_PATH, strerror(errno));
		Error404();
		goto _DONE;
	}

	opened = 1;

 	if (ioctl(fd, BLKGETSIZE64, &numbytes)  < 0)
	{
		syslog(LOG_ERR, "GET: Unable to get size of disk: %s", strerror(errno));
		Error404();
		goto _DONE;
	}

	if ((size_t)start > numbytes || (size_t)end > numbytes)
	{
		syslog(LOG_ERR, "GET: Illegal range beyond end of disk. Start: %ld, End: %ld, Disk: %ld", start, end, numbytes);
		Error416();
		goto _DONE;
	}

	if (lseek(fd, start, SEEK_SET) < start)
	{
		syslog(LOG_ERR, "GET: Unable to seek to starting offset %ld: %s", start, strerror(errno));
		Error416();
		goto _DONE;
	}

	// Need to figure out if its a shitty range request and fix it.
	if (end == -1)
		end = (long)numbytes;

	if (start < 0)
		start = (long)numbytes + start;

	if ((end - start) > GET_BUFFER_MAX)
	{
		// fuck you hacker
		syslog(LOG_ERR, "GET: Refusing to perform abnormally huge range request of %ld bytes.", end - start);
		Error416();
		goto _DONE;
	}
	else
		read_target = end - start;

	buf = malloc(sizeof(char) * read_target);
	buf_ptr = buf;

	while (read_target > 0)
	{
		size_t current = read(fd, buf_ptr, read_target);
		if (current <= 0)
		{
			if (current < 0)
			{
				syslog(LOG_ERR, "GET: Unable to read desired data range starting from %ld: %s", (start + total_read), strerror(errno));
				Error416();
				goto _DONE;
			}
			break;
		}
		else
		{
			total_read += current;
			read_target -= current;
			buf_ptr += current;
		}
	}

	*buf_ptr = 0;

	FCGI_printf("Status: 206 Partial Content\r\n");
	BoilerplateHeaders();
	FCGI_printf("Content-Type: application/octet-stream\r\n");
	FCGI_printf("Content-Range: bytes=%ld-%ld/%ld\r\n", start, start + total_read, numbytes);
	FCGI_printf("Content-Length: %ld\r\n", total_read);
	FCGI_printf("\r\n");

	FCGI_fwrite(buf, total_read, 1, FCGI_stdout);

_DONE:
	if (buf != NULL)
		free(buf);

	if (opened)
		close(fd);
}
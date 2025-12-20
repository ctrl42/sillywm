#define  VERSION "0.1"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <X11/Xlib.h>

#include "ipc.h" // shared structures

#define SOCKET_BASE "/tmp/sillywm-%s.sock"
int sockfd = 0;

void silly_send_ctrl(silly_cmdop cmd, int param1, int param2, char* data) {
	if (!sockfd) return;
	silly_ctrl_command ctrl;
	ctrl.cmd    = cmd;
	ctrl.param1 = param1;
	ctrl.param2 = param2;
	ctrl.len    = data ? strlen(data) + 1 : 0;

	send(sockfd, &ctrl, sizeof(silly_ctrl_command), 0); // header
	if (data) send(sockfd, data, ctrl.len, 0);          // extra info
	send(sockfd, (char*[]){0}, 1, 0);                   // just in case
}

void print_help(void) {
	char* help[] = {
		"sillyc configurator " VERSION,
		"by stx4.",
		""
		"we're currently undocumented. here's some examples instead:",
		"sillyc rc    ~/.sillyrc",
		"",
		"sillyc set   border_width 8",
		"sillyc size  +50 +50  (additive move to active)",
		"sillyc move  40 40    (absolute move to active)",
		"sillyc bind  Return \"dmenu-run\"",
		"sillyc kill           (kills focus)",
		"sillyc start \"xeyes -biblicallyAccurate\""
	};
	for (int i = 0; i < sizeof(help) / sizeof(help[0]); i++)
		fprintf(stderr, "%s\n", help[i]);
}

int main(int argc, char* argv[]) {
	if (argc < 2) goto help_fail;

	struct sockaddr_un addr = {0};	
	char sock_name[128];
	sprintf(sock_name, SOCKET_BASE, getenv("DISPLAY"));
	
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, sock_name);

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (
		sockfd < 0 ||
		connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0
	) goto fail;

	if        (!strcmp(argv[1], "start")) {
		if (argc < 3) goto help_fail;
		silly_send_ctrl(START, 0, 0, argv[2]);
	} else if (!strcmp(argv[1], "kill")) {
		silly_send_ctrl(KILL, 0, 0, NULL);
	} else if (!strcmp(argv[1], "bind")) {
		if (argc < 4) goto help_fail;
		silly_send_ctrl(BIND, XStringToKeysym(argv[2]), 0, argv[3]);
	}

	close(sockfd);
	return 0;

fail:
	fprintf(stderr, "could not connect to sillywm @ %s\n", sock_name);
	return 1;

help_fail:
	print_help();
	if (sockfd) close(sockfd);
	return 1;
}

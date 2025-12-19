#define MAX_IPC_SIZE 512

typedef enum { SET, BIND, START, KILL, MOVE, SIZE } silly_cmdop;
typedef struct {
	silly_cmdop cmd;
	int param1, param2;
	int len; // data block follows 
} silly_ctrl_command;

#define MAX_IPC_SIZE 512

typedef enum { SET, BIND, EXEC, KILL, ROLL, MOVE, SIZE, QUIT } silly_cmdop;
typedef struct {
	silly_cmdop cmd;
	int param1, param2;
	int len; // data block length
} silly_ctrl_command;

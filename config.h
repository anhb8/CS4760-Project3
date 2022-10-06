#ifndef CONFIG_H
#define MAX_PROCESS 20
enum state{idle,want_in,in_cs};
struct sharedM {
	int turn;
	int flag[MAX_PROCESS];

};

key_t SHM_KEY=1234;
#endif

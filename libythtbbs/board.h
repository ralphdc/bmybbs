/* board.c */
#ifndef __BOARD_H
#define __BOARD_H

#define BMNUM 16
struct boardheader {		/* This structure is used to hold data in */
	char filename[24];	/* the BOARDS files */
	char title[24];
	int clubnum;
	unsigned level;
	char flag;
	char secnumber1;	//这个就是此版面所在的区号，例如：'2','G'等等
	char secnumber2;
	char type[5];
	char bm[BMNUM][IDLEN + 1];
	int hiretime[BMNUM];
	int board_ctime;
	int board_mtime;
	char sec1[4];
	char sec2[4];
	char keyword[64];
	char unused[96];
};

struct boardmem {		/* used for caching files and boards */
	struct boardheader header;
	int lastpost;
	int total;
	short inboard;
	short bmonline;
	short bmcloak;
	int stocknum;
	int score;
	int unused[10];
};

struct boardmanager {		/* record in user directionary */
	char board[24];
	char bmpos;
	char unused;
	short bid;
};

struct myparam1 {		/* just use to pass a param to fillmboard() */
	struct userec user;
	int fd;
	short bid;
};

char *bm2str(char *buf, struct boardheader *bh);
char *sbm2str(char *buf, struct boardheader *bh);
int chk_BM(struct userec *, struct boardheader *bh, int isbig);
int chk_BM_id(char *, struct boardheader *);
int bmfilesync(struct userec *);
int fillmboard(struct boardheader *bh, struct myparam1 *param);

/**
 * 依据版面名称获取 boardmem 对象
 * 从 nju09/BBSLIB.c 复制而来，理应属于 libythtbbs 库的一部分。该方法将从 shm_bcache
 * 中递归的查找比对 board_name，若相同则返回 boardmem 地址。
 * @warning 该方法中不包含用户权限的校验。调用结束后不需要释放 boardmem 地址。
 * @param board_name 版面的英文名称
 * @see struct boardmem * getbcache(char *board)
 * @see struct boardmem * getboard(char *board)
 */
struct boardmem *getboardbyname(char *board_name);  // 从 nju09 移植，by IronBlood 20130805
#endif

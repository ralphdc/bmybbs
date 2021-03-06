#include "bbslib.h"

int
bbsdel_main()
{
	//不原子
	struct fileheader f;
	struct userec *u;
	char dir[80], board[80], file[80], *id;
	int num = 0, filetime, total;
	struct boardmem *x;
	struct mmapfile mf = { ptr:NULL };
	html_header(1);
	check_msg();
	if (!loginok || isguest)
		http_fatal("请先登录");
	changemode(EDIT);
	id = currentuser.userid;
	strsncpy(board, getparm("B"), 32);
	if (!board[0])
		strsncpy(board, getparm("board"), 32);
	strsncpy(file, getparm("F"), 30);
	if (!file[0])
		strsncpy(file, getparm("file"), 20);
	if (strncmp(file, "M.", 2) && strncmp(file, "G.", 2))
		http_fatal("错误的参数");
	if (strstr(file, ".."))
		http_fatal("错误的参数");
	filetime = atoi(file + 2);
	x = getboard(board);
	if (x == 0)
		http_fatal("版面错误");
	sprintf(dir, "boards/%s/.DIR", board);
	MMAP_TRY {
		if (mmapfile(dir, &mf) < 0) {
			MMAP_UNTRY;
			http_fatal("版面错误");
		}
		total = mf.size / sizeof (struct fileheader);
		num = Search_Bin(mf.ptr, filetime, 0, total - 1);
		if (num >= 0)
			memcpy(&f,mf.ptr+num*sizeof(struct fileheader) ,sizeof(struct fileheader));
	}
	MMAP_CATCH {
		num = -1;
	}
	MMAP_END mmapfile(NULL, &mf);
	if (num >= 0) {
		if (strcmp(id, f.owner))
//                  && !has_BM_perm(&currentuser, x))
			http_fatal("你无权删除该文");
		if (!strcmp(board, "syssecurity"))
			http_fatal("无权删除系统记录");
		del_record(dir, sizeof (struct fileheader), num);
		cancelpost(board, currentuser.userid, &f,
			   !strcmp(currentuser.userid, f.owner));
		updatelastpost(board);
		printf
		    ("删除成功.<br><a href='%s%s&S=%d'>返回本讨论区</a>",
		     showByDefMode(), board, num - 10);
		if (!strcmp(id, f.owner)) {
			u = getuser(f.owner);
			if (x->header.clubnum == 0 && !junkboard(board) && u) {
				if (u->numposts > 0) {
					u->numposts--;
					save_user_data(u);
				}
			}
		}
		http_quit();
	}
	printf("文件不存在, 删除失败.<br>\n");
	printf("<a href='bbsdoc?board=%s'>返回本讨论区</a>", board);
	http_quit();
	return 0;
}

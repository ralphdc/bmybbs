/*
    Pirate Bulletin Board System  
    Copyright (C) 1990, Edward Luke, lush@Athena.EE.MsState.EDU
    Eagles Bulletin Board System
    Copyright (C) 1992, Raymond Rocker, rocker@rock.b11.ingr.com
                        Guy Vega, gtvega@seabass.st.usm.edu
                        Dominic Tynes, dbtynes@seabass.st.usm.edu
    Firebird Bulletin Board System
    Copyright (C) 1996, Hsien-Tsung Chang, Smallpig.bbs@bbs.cs.ccu.edu.tw
                        Peng Piaw Foong, ppfoong@csie.ncu.edu.tw
    Copyright (C) 1999, KCN,Zhou Lin, kcn@cic.tsinghua.edu.cn
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#define EXTERN
#include "bbs.h"
#include "bbstelnet.h"
#include <sys/mman.h>

pid_t childpid;
static int loadkeys(struct one_key *key, char *name);
static int savekeys(struct one_key *key, char *name);
static void keyprint(char *buf, int key);
static int showkeyinfo(struct one_key *akey, int i);
static unsigned int setkeys(struct one_key *key);
static void myexec_cmd(int umode, int pager, const char *cmdfile,
		       const char *param);
static void datapipefd(int fds, int fdn);
static void exec_cmd(int umode, int pager, char *cmdfile, char *param1);
static int sendGoodWish(char *userid);
static void childreturn(int i);
static void escape_filename(char *fn);
static void bbs_zsendfile(char *filename);
static void get_load(double load[]);

static int
loadkeys(struct one_key *key, char *name)
{
	int i;
	FILE *fp;
	fp = fopen(name, "r");
	if (fp == NULL)
		return 0;
	i = 0;
	while (key[i].fptr != NULL) {
		fread(&(key[i].key), sizeof (int), 1, fp);
		i++;
	}
	fclose(fp);
	return 1;
}

static int
savekeys(struct one_key *key, char *name)
{
	int i;
	FILE *fp;
	fp = fopen(name, "w");
	if (fp == NULL)
		return 0;
	i = 0;
	while (key[i].fptr != NULL) {
		fwrite(&(key[i].key), sizeof (int), 1, fp);
		i++;
	}
	fclose(fp);
	return 1;
}

void
loaduserkeys()
{
	char tempname[STRLEN];
	setuserfile(tempname, "readkey");
	loadkeys(read_comms, tempname);
	setuserfile(tempname, "mailkey");
	loadkeys(mail_comms, tempname);
	setuserfile(tempname, "friendkey");
	loadkeys(friend_list, tempname);
	setuserfile(tempname, "rejectkey");
	loadkeys(reject_list, tempname);
}

int
modify_user_mode(mode)
int mode;
{
	if (uinfo.mode == mode)
		return 0;
	uinfo.mode = mode;
	update_ulist(&uinfo, utmpent);
	return 0;
}

int
x_csh()
{
	char buf[PASSLEN];
	int save_pager;
	int magic;

	return -1;

	if (!HAS_PERM(PERM_SYSOP)) {
		return -1;
	}
	if (!check_systempasswd()) {
		return -1;
	}
	modify_user_mode(SYSINFO);
	clear();
	getdata(1, 0, "请输入通行暗号: ", buf, PASSLEN, NOECHO, YEA);
	if (*buf == '\0' || !checkpasswd(currentuser.passwd, buf)) {
		prints("\n\n暗号不正确, 不能执行。\n");
		pressreturn();
		clear();
		return -1;
	}
	randomize();
	magic = rand() % 1000;
	prints("\nMagic Key: %d", magic * 3 - 1);
	getdata(4, 0, "Your Key : ", buf, PASSLEN, NOECHO, YEA);
	if (*buf == '\0' || !(atoi(buf) == magic)) {
		securityreport("Fail to shell out", "Fail to shell out");
		prints("\n\nKey 不正确, 不能执行。\n");
		pressreturn();
		clear();
		return -1;
	}
	securityreport("Shell out", "Shell out");
	modify_user_mode(SYSINFO);
	clear();
	refresh();
	save_pager = uinfo.pager;
	uinfo.pager = 0;
	update_utmp();
	do_exec("csh", NULL);
	uinfo.pager = save_pager;
	update_utmp();
	clear();
	return 0;
}

int
showperminfo(pbits, i, use_define)
unsigned int pbits;
int i, use_define;
{
	char buf[STRLEN];

	sprintf(buf, "%c. %-30s %3s", 'A' + i,
		(use_define) ? user_definestr[i] : permstrings[i],
		((pbits >> i) & 1 ? "ON" : "OFF"));
	move(i + 6 - ((i > 15) ? 16 : 0), 0 + ((i > 15) ? 40 : 0));
	prints(buf);
	return YEA;
}

static void
keyprint(char *buf, int key)
{
	if (isprint(key))
		sprintf(buf, "%c", key);
	else {
		switch (key) {
		case KEY_TAB:
			strcpy(buf, "TAB");
			break;
		case KEY_ESC:
			strcpy(buf, "ESC");
			break;
		case KEY_UP:
			strcpy(buf, "UP");
			break;
		case KEY_DOWN:
			strcpy(buf, "DOWN");
			break;
		case KEY_RIGHT:
			strcpy(buf, "RIGHT");
			break;
		case KEY_LEFT:
			strcpy(buf, "LEFT");
			break;
		case KEY_HOME:
			strcpy(buf, "HOME");
			break;
		case KEY_INS:
			strcpy(buf, "INS");
			break;
		case KEY_DEL:
			strcpy(buf, "DEL");
			break;
		case KEY_END:
			strcpy(buf, "END");
			break;
		case KEY_PGUP:
			strcpy(buf, "PGUP");
			break;
		case KEY_PGDN:
			strcpy(buf, "PGDN");
			break;
		default:
			if (isprint(key | 0140))
				sprintf(buf, "Ctrl+%c", key | 0140);
			else
				sprintf(buf, "%x", key);
		}
	}
}

static int
showkeyinfo(struct one_key *akey, int i)
{
	char buf[STRLEN];
	char buf2[15];
	keyprint(buf2, (akey + i)->key);
	sprintf(buf, "%c. %-26s %6s", '0' + i, (akey + i)->func, buf2);
	move(i + 2 - ((i > 19) ? 20 : 0), 0 + ((i > 19) ? 40 : 0));
	prints(buf);
	return YEA;
}

static unsigned int
setkeys(struct one_key *key)
{
	int i, j, done = NA;
	char choice[3];
	prints("请按下你要的代码来设定键盘，按 Enter 结束.\n");
	i = 0;
	while (key[i].fptr != NULL && i < 40) {
		showkeyinfo(key, i);
		i++;
	}
	while (!done) {
		getdata(t_lines - 1, 0, "选择(ENTER 结束): ", choice, 2, DOECHO,
			YEA);
		*choice = toupper(*choice);
		if (*choice == '\n' || *choice == '\0')
			done = YEA;
		else if (*choice < '0' || *choice > '0' + i - 1)
			bell();
		else {
			j = *choice - '0';
			move(t_lines - 1, 0);
			prints("请定义[\033[35m%s\033[m]的功能键:",
			       key[j].func);
			key[j].key = igetkey();
			showkeyinfo(key, j);
			/* d pbits ^= (1 << i);
			   if((*showfunc)( pbits, i ,YEA)==NA)
			   {
			   pbits ^= (1 << i);
			   } */
		}
	}
	pressreturn();
	return 0;
}

int
x_copykeys()
{
	char tempname[STRLEN];
	modify_user_mode(USERDEF);
	char ans[3];
	clear();
	move(0, 0);
	clrtoeol();

	prints("请选择你要恢复的默认键位,按 Enter 结束.\n");
	getdata(1, 0, "[0]离开 [1]版面1 [2]版面2 [3]邮箱 [4]好友 [5]坏人 [6]全部 (默认[0]):", ans, 2, DOECHO, YEA);

	if (ans[0] == '0' || ans[0] == '\n' || ans[0] == '\0')
			return 0;
	
	if (ans[0] == '3' || ans[0] == '6')
	{
		int i = 0;
		while (mail_default_comms[i].fptr != NULL) {
			mail_comms[i].key= mail_default_comms[i].key;
			i++;
		}
		setuserfile(tempname, "mailkey");
		savekeys(&mail_comms[0], tempname);
		
	}
	
	prints("设置完毕");
	pressreturn();
	return 0;
}

/*
int
x_copykeys()
{
	int i, toc = 0;
	char choice[3];
	FILE *fp;
	char buf[STRLEN];
	char tempname[STRLEN];
	modify_user_mode(USERDEF);
	clear();
	move(0, 0);
	clrtoeol();
	prints("请按下你要的代码来设定键盘类型,按 Enter 结束.\n");
	fp = fopen(MY_BBS_HOME "/etc/keys", "r");
	if (fp == NULL)
		return -1;
	i = 0;
	while (fgets(buf, STRLEN, fp) != NULL) {
		sprintf(tempname, "%c. %-26s", 'A' + i, buf);
		move(i + 2 - ((i > 19) ? 20 : 0), 0 + ((i > 19) ? 40 : 0));
		prints(tempname);
		i++;
	}
	getdata(t_lines - 1, 0, "选择(ENTER 结束): ", choice, 2, DOECHO, YEA);
	*choice = toupper(*choice);
	if (*choice == '\n' || *choice == '\0')
		return 0;
	else if (*choice < 'A' || *choice > 'A' + i - 1)
		bell();
	else {
		toc = 0;
		i = *choice - 'A';
		if (currentuser.userlevel & PERM_SYSOP) {
			getdata(t_lines - 1, 0,
				"是否把当前你自己的键盘定义设置为系统定义(Y/N):N ",
				choice, 2, DOECHO, YEA);
			if (*choice == 'Y')
				toc = 1;
		}
		if (toc) {
			sprintf(buf, MY_BBS_HOME "/etc/readkey.%d", i);
			savekeys(&read_comms[0], buf);
			sprintf(buf, MY_BBS_HOME "/etc/mailkey.%d", i);
			savekeys(&mail_comms[0], buf);
			sprintf(buf, MY_BBS_HOME "/etc/friendkey.%d", i);
			savekeys(&friend_list[0], buf);
			sprintf(buf, MY_BBS_HOME "/etc/rejectkey.%d", i);
			savekeys(&reject_list[0], buf);
		} else {
			sprintf(buf, MY_BBS_HOME "/etc/readkey.%d", i);
			loadkeys(&read_comms[0], buf);
			setuserfile(buf, "readkey");
			savekeys(&read_comms[0], buf);
			sprintf(buf, MY_BBS_HOME "/etc/mailkey.%d", i);
			loadkeys(&mail_comms[0], buf);
			setuserfile(buf, "mailkey");
			savekeys(&mail_comms[0], buf);
			sprintf(buf, MY_BBS_HOME "/etc/friendkey.%d", i);
			loadkeys(&friend_list[0], buf);
			setuserfile(buf, "friendkey");
			savekeys(&friend_list[0], buf);
			sprintf(buf, MY_BBS_HOME "/etc/rejectkey.%d", i);
			loadkeys(&reject_list[0], buf);
			setuserfile(buf, "rejectkey");
			savekeys(&reject_list[0], buf);
		}
		move(t_lines - 1, 0);
		prints("设置完毕");
	}
	pressreturn();
	return 0;
}
*/

int
x_setkeys()
{
	char tempname[STRLEN];
	modify_user_mode(USERDEF);
	clear();
	move(0, 0);
	clrtoeol();
	setkeys(&read_comms[0]);
	setuserfile(tempname, "readkey");
	savekeys(&read_comms[0], tempname);
	return 0;
}

int
x_setkeys2()
{
	char tempname[STRLEN];
	modify_user_mode(USERDEF);
	clear();
	move(0, 0);
	clrtoeol();
	setkeys(&read_comms[40]);
	setuserfile(tempname, "readkey");
	savekeys(&read_comms[0], tempname);
	return 0;
}

int
x_setkeys3()
{
	char tempname[STRLEN];
	modify_user_mode(USERDEF);
	clear();
	move(0, 0);
	clrtoeol();
	setkeys(&mail_comms[0]);
	setuserfile(tempname, "mailkey");
	savekeys(&mail_comms[0], tempname);
	return 0;
}

int
x_setkeys4()
{
	char tempname[STRLEN];
	modify_user_mode(USERDEF);
	clear();
	move(0, 0);
	clrtoeol();
	setkeys(&friend_list[0]);
	setuserfile(tempname, "friendkey");
	savekeys(&friend_list[0], tempname);
	return 0;
}

int
x_setkeys5()
{
	char tempname[STRLEN];
	modify_user_mode(USERDEF);
	clear();
	move(0, 0);
	clrtoeol();
	setkeys(&reject_list[0]);
	setuserfile(tempname, "rejectdkey");
	savekeys(&reject_list[0], tempname);
	return 0;
}

unsigned int
setperms(pbits, prompt, numbers, showfunc, param)
unsigned int pbits;
char *prompt;
int numbers;
int (*showfunc) (unsigned int, int, int);
int param;
{
	int lastperm = numbers - 1;
	int i, done = NA;
	char choice[3];

	move(4, 0);
	prints("请按下你要的代码来设定%s，按 Enter 结束.\n", prompt);
	move(6, 0);
	clrtobot();
/*    pbits &= (1 << numbers) - 1;*/
	for (i = 0; i <= lastperm; i++) {
		(*showfunc) (pbits, i, param);
	}
	while (!done) {
		getdata(t_lines - 1, 0, "选择(ENTER 结束): ", choice, 2, DOECHO,
			YEA);
		*choice = toupper(*choice);
		if (*choice == '\n' || *choice == '\0')
			done = YEA;
		else if (*choice < 'A' || *choice > 'A' + lastperm)
			bell();
		else {
			i = *choice - 'A';
			pbits ^= (1 << i);
			if ((*showfunc) (pbits, i, param) == NA) {
				pbits ^= (1 << i);
			}
		}
	}
	return (pbits);
}

int
x_level()
{
	int id, oldlevel;
	unsigned int newlevel;
	char content[1024];

	modify_user_mode(ADMIN);
	if (!check_systempasswd()) {
		return -1;
	}
	clear();
	move(0, 0);
	prints("Change User Priority\n");
	clrtoeol();
	move(1, 0);
	usercomplete("Enter userid to be changed: ", genbuf);
	if (genbuf[0] == '\0') {
		clear();
		return 0;
	}
	if (!(id = getuser(genbuf))) {
		move(3, 0);
		prints("Invalid User Id");
		clrtoeol();
		pressreturn();
		clear();
		return -1;
	}
	move(1, 0);
	clrtobot();
	move(2, 0);
	prints("Set the desired permissions for user '%s'\n", genbuf);
	newlevel =
	    setperms(lookupuser.userlevel, "权限", NUMPERMS, showperminfo, 0);
	move(2, 0);
	if (newlevel == lookupuser.userlevel)
		prints("User '%s' level NOT changed\n", lookupuser.userid);
	else {
		oldlevel = lookupuser.userlevel;
		lookupuser.userlevel = newlevel;
		{
			char secu[STRLEN];
			sprintf(secu, "修改 %s 的权限", lookupuser.userid);
			permtostr(oldlevel, genbuf);
			sprintf(content, "修改前的权限：%s\n修改后的权限：",
				genbuf);
			permtostr(lookupuser.userlevel, genbuf);
			strcat(content, genbuf);
			securityreport(secu, content);
		}
		substitute_record(PASSFILE, &lookupuser, sizeof (struct userec),
				  id);
		prints("User '%s' level changed\n", lookupuser.userid);
	}
	pressreturn();
	clear();
	return 0;
}

int
x_userdefine()
{
	int id;
	unsigned int newlevel;
	extern int nettyNN;

	modify_user_mode(USERDEF);
	if (!(id = getuser(currentuser.userid))) {
		move(3, 0);
		prints("错误的使用者 ID...");
		clrtoeol();
		pressreturn();
		clear();
		return 0;
	}
	move(1, 0);
	clrtobot();
	move(2, 0);
	newlevel =
	    setperms(lookupuser.userdefine, "参数", NUMDEFINES, showperminfo,
		     1);
	move(2, 0);
	if (newlevel == lookupuser.userdefine)
		prints("参数没有修改...\n");
	else {
		lookupuser.userdefine = newlevel;
		currentuser.userdefine = newlevel;
		if ((!convcode && !(newlevel & DEF_USEGB))
		    || (convcode && (newlevel & DEF_USEGB)))
			switch_code();
		substitute_record(PASSFILE, &lookupuser, sizeof (struct userec),
				  id);
		uinfo.pager |= FRIEND_PAGER;
		if (!(uinfo.pager & ALL_PAGER)) {
			if (!DEFINE(DEF_FRIENDCALL))
				uinfo.pager &= ~FRIEND_PAGER;
		}
		uinfo.pager &= ~ALLMSG_PAGER;
		uinfo.pager &= ~FRIENDMSG_PAGER;
		if (DEFINE(DEF_DELDBLCHAR))
			enabledbchar = 1;
		else
			enabledbchar = 0;
		if (DEFINE(DEF_FRIENDMSG)) {
			uinfo.pager |= FRIENDMSG_PAGER;
		}
		if (DEFINE(DEF_ALLMSG)) {
			uinfo.pager |= ALLMSG_PAGER;
			uinfo.pager |= FRIENDMSG_PAGER;
		}
		update_utmp();
		if (DEFINE(DEF_ACBOARD))
			nettyNN = NNread_init();
		prints("新的参数设定完成...\n\n");
	}
	iscolor = (DEFINE(DEF_COLOR)) ? 1 : 0;
	pressreturn();
	clear();
	return 0;
}

int
x_cloak()
{
	modify_user_mode(GMENU);
	uinfo.invisible = (uinfo.invisible) ? NA : YEA;
	update_utmp();
	if ((currentuser.userlevel & PERM_BOARDS))
		setbmstatus(1);
	if (!uinfo.in_chat) {
		move(1, 0);
		clrtoeol();
	        if(uinfo.invisible)				//add by mintbaggio@BMY for normal cloak	
			currentuser./*pseudo_*/lastlogout = time(NULL);
		else	currentuser./*pseudo_*/lastlogout = 0;
		substitute_record(PASSFILE, &currentuser, sizeof (currentuser),
                                  usernum);
		prints("隐身术 (cloak) 已经%s了!",
		       (uinfo.invisible) ? "启动" : "停止");
		pressreturn();
	}
	return 0;
}

void
x_edits()
{
	int aborted;
	char ans[7], buf[STRLEN];
	int ch, num, confirm;
	extern int WishNum;
	static char *const e_file[] =
	    { "plans", "signatures", "notes", "logout", "GoodWish", "bansite",
		NULL
	};
	static char *const explain_file[] =
	    { "个人说明档", "签名档", "自己的备忘录", "离站的画面",
		"底部流动信息", "禁止本ID登录的IP(每个IP占一行)",
		NULL
	};

	modify_user_mode(GMENU);
	clear();
	move(1, 0);
	prints("编修个人档案\n\n");
	for (num = 0; e_file[num] != NULL && explain_file[num] != NULL; num++) {
		prints("[[1;32m%d[m] %s\n", num + 1, explain_file[num]);
	}
	prints("[[1;32m%d[m] 都不想改\n", num + 1);

	getdata(num + 5, 0, "你要编修哪一项个人档案: ", ans, 2, DOECHO, YEA);
	if (ans[0] - '0' <= 0 || ans[0] - '0' > num || ans[0] == '\n'
	    || ans[0] == '\0')
		return;

	ch = ans[0] - '0' - 1;
	setuserfile(genbuf, e_file[ch]);
	move(3, 0);
	clrtobot();
	sprintf(buf, "(E)编辑 (D)删除 %s? [E]: ", explain_file[ch]);
	getdata(3, 0, buf, ans, 2, DOECHO, YEA);
	if (ans[0] == 'D' || ans[0] == 'd') {
		confirm = askyn("你确定要删除这个档案", NA, NA);
		if (confirm != 1) {
			move(5, 0);
			prints("取消删除行动\n");
			pressreturn();
			clear();
			return;
		}
		unlink(genbuf);
		move(5, 0);
		prints("%s 已删除\n", explain_file[ch]);
		pressreturn();
		if (ch == 4)
			WishNum = 9999;
		clear();
		return;
	}
	modify_user_mode(EDITUFILE);
	aborted = vedit(genbuf, NA, YEA);
	clear();
	if (!aborted) {
		prints("%s 更新过\n", explain_file[ch]);
		sprintf(buf, "edit %s", explain_file[ch]);
		if (!strcmp(e_file[ch], "signatures")) {
			set_numofsig();
			prints("系统重新设定以及读入你的签名档...");
		}
	} else
		prints("%s 取消修改\n", explain_file[ch]);
	pressreturn();
	if (ch == 4)
		WishNum = 9999;
}

void
a_edits()
{
	int aborted;
	char ans[7], buf[STRLEN], buf2[STRLEN];
	int ch, num, confirm;
	/*modified by pzhg 070412 for voteing for valid ids in spacific boards*/
	static char *const e_file[] =
	    { "../Welcome", "../Welcome2", "issue", "logout", "movie",
		"endline", "../vote/notes",
		"menu.ini", "badname0", "badname", "../.bad_email",
		"../.bansite",
		"../.blockmail",
		"autopost", "junkboards", "sysops", "prisonor", "untrust",
		"bbsnetA.ini", "bbsnet.ini", "filtertitle",
		"../ftphome/ftp_adm", "badwords", "sbadwords", "pbadwords",
		"../inndlog/newsfeeds.bbs", "spec_site", "secmlist","special","life", 
		"commendlist", "manager_team","./pop_register/mail.xjtu.edu.cn","./pop_register/stu.xjtu.edu.cn",
		"top10forbid","voteidboards","newboard","recommboard","guestbanip",NULL
	};
	static char *const explain_file[] =
	    { "特殊进站公布栏", "进站画面", "进站欢迎档", "离站画面",
		"活动看版", "屏幕底线", "公用备忘录", "menu.ini",
		"不可注册的 ID", "ID 中不能包含的字串", "不可确认之E-Mail",
		"不可上站之位址",
		"拒收E-mail黑名单", "每日自动送信档", "不算POST数的版",
		"管理者名单", "服刑人员名单", "不信任IP列表",
		"本地网络连线", "网络连线", "底线需要过滤的标题",
		"FTP管理员名单", "过滤词汇", "精简过滤词汇", "报警词汇",
		"转信版和新闻组对应", "穿梭IP限制次数", "主管站长", "id标识",
		"生命力设定", "推荐文章", "管理团队名单",
		"信箱注册mail.xjtu.edu.cn","信箱注册stu.xjtu.edu.cn",
		"不允许上10大版面","限制投票权版面","新开版面","推荐版面","禁止guest发文的ip",NULL
	};//modify by wjbta@bmy  增加id标识的显示, modify by mintbaggio 040406 for front page commend

	modify_user_mode(ADMIN);
	if (!check_systempasswd()) {
		return;
	}
	clear();
	move(0, 0);
	prints("编修系统档案\n\n");
	for (num = 0;
	     HAS_PERM(PERM_SYSOP) ? e_file[num] != NULL
	     && explain_file[num] != NULL : explain_file[num] != "menu.ini";
	     num++) {
		if (num >= 20)
			move(num - 20 + 2, 39);
		prints("[%2d] %s\n", num + 1, explain_file[num]);
	}
	if (num >= 20)
		move(num - 20 + 2, 39);
	prints("[%2d] 都不想改\n", num + 1);

	getdata(t_lines - 1, 0, "你要编修哪一项系统档案: ", ans, 3, DOECHO,
		YEA);
	ch = atoi(ans);
	if (!isdigit(ans[0]) || ch <= 0 || ch > num || ans[0] == '\n'
	    || ans[0] == '\0')
		return;
	ch -= 1;
	sprintf(buf2, "etc/%s", e_file[ch]);
	move(3, 0);
	clrtobot();
	sprintf(buf, "(E)编辑 (D)删除 %s? [E]: ", explain_file[ch]);
	getdata(3, 0, buf, ans, 2, DOECHO, YEA);
	if (ans[0] == 'D' || ans[0] == 'd') {
		confirm = askyn("你确定要删除这个系统档", NA, NA);
		if (confirm != 1) {
			move(5, 0);
			prints("取消删除行动\n");
			pressreturn();
			clear();
			return;
		}
		{
			char secu[STRLEN];
			sprintf(secu, "删除系统档案：%s", explain_file[ch]);
			securityreport(secu, secu);
		}
		unlink(buf2);
		move(5, 0);
		prints("%s 已删除\n", explain_file[ch]);
		pressreturn();
		clear();
		return;
	}
	modify_user_mode(EDITSFILE);
	aborted = vedit(buf2, NA, YEA);
	clear();
	if (aborted != -1) {
		prints("%s 更新过", explain_file[ch]);
		{
			char secu[STRLEN];
			sprintf(secu, "修改系统档案：%s", explain_file[ch]);
			securityreport(secu, secu);
		}

		if (!strcmp(e_file[ch], "../Welcome")) {
			unlink("Welcome.rec");
			prints("\nWelcome 记录档更新");
		}
	}
	pressreturn();
}

//added by pzhg for SysFiles2
void
a_edits2()
{
	int aborted;
	char ans[7], buf[STRLEN], buf2[STRLEN];
	int ch, num, confirm;
	static char *const e_file[] =
	    { "birthday","sysboards","adpost","ad_banner","ad_left", "secorder",NULL
	};
	static char *const explain_file[] =
	    { "生日欢迎画面","站务管理版面","滚动广告","Banner", "Left Ads","WWW讨论区顺序",NULL
	};

	modify_user_mode(ADMIN);
	if (!check_systempasswd()) {
		return;
	}
	clear();
	move(0, 0);
	prints("编修系统档案2\n\n");
	for (num = 0;
	     HAS_PERM(PERM_SYSOP) ? e_file[num] != NULL
	     && explain_file[num] != NULL : explain_file[num] != "menu.ini";
	     num++) {
		if (num >= 20)
			move(num - 20 + 2, 39);
		prints("[%2d] %s\n", num + 1, explain_file[num]);
	}
	if (num >= 20)
		move(num - 20 + 2, 39);
	prints("[%2d] 都不想改\n", num + 1);

	getdata(t_lines - 1, 0, "你要编修哪一项系统档案: ", ans, 3, DOECHO,
		YEA);
	ch = atoi(ans);
	if (!isdigit(ans[0]) || ch <= 0 || ch > num || ans[0] == '\n'
	    || ans[0] == '\0')
		return;
	ch -= 1;
	sprintf(buf2, "etc/%s", e_file[ch]);
	move(3, 0);
	clrtobot();
	sprintf(buf, "(E)编辑 (D)删除 %s? [E]: ", explain_file[ch]);
	getdata(3, 0, buf, ans, 2, DOECHO, YEA);
	if (ans[0] == 'D' || ans[0] == 'd') {
		confirm = askyn("你确定要删除这个系统档", NA, NA);
		if (confirm != 1) {
			move(5, 0);
			prints("取消删除行动\n");
			pressreturn();
			clear();
			return;
		}
		{
			char secu[STRLEN];
			sprintf(secu, "删除系统档案：%s", explain_file[ch]);
			securityreport(secu, secu);
		}
		unlink(buf2);
		move(5, 0);
		prints("%s 已删除\n", explain_file[ch]);
		pressreturn();
		clear();
		return;
	}
	modify_user_mode(EDITSFILE);
	aborted = vedit(buf2, NA, YEA);
	clear();
	if (aborted != -1) {
		prints("%s 更新过", explain_file[ch]);
		{
			char secu[STRLEN];
			sprintf(secu, "修改系统档案：%s", explain_file[ch]);
			securityreport(secu, secu);
		}
	}
	pressreturn();
}

void
x_lockscreen()
{
	char buf[PASSLEN + 1];
	time_t now;
	modify_user_mode(LOCKSCREEN);
	block_msg();
	move(9, 0);
	clrtobot();
	update_endline();
	buf[0] = '\0';
	char ubuf[3];
	char user_self[16];
	now = time(0);
	move(9, 0);
	prints
	    ("\n[1;37m       _       _____   ___     _   _   ___     ___       __"
	     "\n      ( )     (  _  ) (  _`\\  ( ) ( ) (  _`\\  (  _`\\    |  |"
	     "\n      | |     | ( ) | | ( (_) | |/'/' | (_(_) | | ) |   |  |"
	     "\n      | |  _  | | | | | |  _  | , <   |  _)_  | | | )   |  |"
	     "\n      | |_( ) | (_) | | (_( ) | |\\`\\  | (_( ) | |_) |   |==|"
	     "\n      (____/' (_____) (____/' (_) (_) (____/' (____/'   |__|[m\n");
	move(17,0);
	getdata(17,0,"锁屏理由? (1)吃饭去了 (2)和MM聊天 (3)别来烦我 (4)没理由 (5)自定义:",ubuf,3,DOECHO,YEA); //add by landefeng@BMY for 锁屏理由
	switch(ubuf[0]){
		case '1':
			modify_user_mode(USERDF1);break;
		case '2':
			modify_user_mode(USERDF2);break;
		case '3':
			modify_user_mode(USERDF3);break;
		case '5':						//add by leoncom@bmy 自定义锁屏理由
			move(17,0);
			clrtobot();
			update_endline();
			if(HAS_PERM(PERM_SELFLOCK))
            {
			uinfo.user_state_temp[0]='\0';                  //清除上次记录
			update_ulist(&uinfo,utmpent);
	                getdata(17,0,"请输入自定义理由:",user_self,9,DOECHO,YEA);
		        int i=0,flag=0;
			for(i=0;i<=7;i++){
			if(user_self[i]==' ')  
			{
				flag=1;
				break;
			}
			}
			if(!stringfilter(user_self,0)&&!flag)
			{
                         strcpy(uinfo.user_state_temp,user_self);
	                 update_ulist(&uinfo, utmpent);
			 move(17,0);
 			 clrtobot();
			 prints("您的当前锁屏理由为:%s",user_self);
			 modify_user_mode(USERDF4);break;
             }
			else
			{	
				move(17,0);
				clrtobot();
				prints("您输入的自定义理由含有不合适词汇或特殊字符，将以默认方式锁屏");
				modify_user_mode(LOCKSCREEN);
				break;
			}
		   }
			else
			{   
				move(17,0);
				clrtobot();
				prints("你被管理员取消自定义锁屏的权限，将以默认方式锁定");
				modify_user_mode(LOCKSCREEN);
				break;
			} 
		default:
			prints("默认锁屏");
			modify_user_mode(LOCKSCREEN);
	}
	move(18,0);
	clrtobot();      
	prints("[1;36m荧幕已在[33m %19s[36m 时被[32m %-12s [36m暂时锁住了...[m",
	     ctime(&now), currentuser.userid);
	while (*buf == '\0' || !checkpasswd(currentuser.passwd, buf)) {
		move(19, 0);
		clrtobot();
		update_endline();
		getdata(19, 0, buf[0] == '\0' ? "请输入你的密码以解锁: " :
			"你输入的密码有误，请重新输入你的密码以解锁: ", buf,
			PASSLEN, NOECHO, YEA);
	}
	unblock_msg();
}

int
heavyload(float maxload)
{
	double cpu_load[3];
	if (maxload == 0)
		maxload = 15;
	get_load(cpu_load);
	if (cpu_load[0] > maxload)
		return 1;
	else
		return 0;
}

static void
myexec_cmd(umode, pager, cmdfile, param)
int umode, pager;
const char *cmdfile, *param;
{
	char buf[STRLEN * 2], param1[256];
	int save_pager;
	pid_t childpid;
	int p[2];
	param1[0] = 0;
	if (param != NULL) {
		char *avoid = "&;!`'\"|?~<>^()[]{}$\n\r\\", *ptr;
		int n = strlen(avoid);
		strsncpy(param1, param, sizeof (param1));
		while (n > 0) {
			n--;
			ptr = strchr(param1, avoid[n]);
			if (ptr != NULL)
				*ptr = 0;
		}
	}

	if (!HAS_PERM(PERM_SYSOP) && heavyload(0)) {
		clear();
		prints("抱歉，目前系统负荷过重，此功能暂时不能执行...");
		pressanykey();
		return;
	}

	if (!dashf(cmdfile)) {
		move(2, 0);
		prints("no %s\n", cmdfile);
		pressreturn();
		return;
	}

	save_pager = uinfo.pager;
	if (pager == NA) {
		uinfo.pager = 0;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, p) < 0)
		return;

	modify_user_mode(umode);
	refresh();
	signal(SIGALRM, SIG_IGN);
	signal(SIGCHLD, SIG_DFL);
	childpid = fork();
	if (childpid == 0) {
		char pidstr[20];
		sprintf(pidstr, "%d", getppid());
		close(p[0]);
		if (p[1] != 0)
			dup2(p[1], 0);
		dup2(0, 1);
		dup2(0, 2);
		if (param1[0]) {
			snprintf(buf, sizeof (buf),
				 "%s exec %s \"%s\" %s %s %d",
				 currentuser.userid, cmdfile, param1,
				 currentuser.userid, uinfo.from, getppid());
			newtrace(buf);
			execl(cmdfile, cmdfile, param1, currentuser.userid,
			      uinfo.from, pidstr, NULL);
		} else {
			snprintf(buf, sizeof (buf), "%s exec %s %s %s %d",
				 currentuser.userid, cmdfile,
				 currentuser.userid, uinfo.from, getppid());
			newtrace(buf);
			execl(cmdfile, cmdfile, currentuser.userid, uinfo.from,
			      pidstr, NULL);
		}
		exit(0);
	} else if (childpid > 0) {
		close(p[1]);
		datapipefd(0, p[0]);
		close(p[0]);
		while (wait(NULL) != childpid)
			sleep(1);
	} else {
		close(p[0]);
		close(p[1]);
	}
	uinfo.pager = save_pager;
	signal(SIGCHLD, SIG_IGN);
	return;
}

static void
datapipefd(int fds, int fdn)
{
	fd_set rs;
	int retv, max;
	char buf[1024];

	max = 1 + ((fdn > fds) ? fdn : fds);
	FD_ZERO(&rs);
	while (1) {
		FD_SET(fds, &rs);
		FD_SET(fdn, &rs);
		retv = select(max, &rs, NULL, NULL, NULL);
		if (retv < 0) {
			if (errno != EINTR)
				break;
			continue;
		}
		if (FD_ISSET(fds, &rs)) {
#ifdef SSHBBS
			retv = ssh_read(fds, buf, sizeof (buf));
#else
			retv = read(fds, buf, sizeof (buf));
#endif
			if (retv > 0) {
				time(&now_t);
				uinfo.lasttime = now_t;
				if ((unsigned long) now_t -
				    (unsigned long) old > 60)
					update_utmp();
				write(fdn, buf, retv);
			} else if (retv == 0 || (retv < 0 && errno != EINTR))
				break;
			FD_CLR(fds, &rs);
		}
		if (FD_ISSET(fdn, &rs)) {
			retv = read(fdn, buf, sizeof (buf));
			if (retv > 0) {
#ifdef SSHBBS
				ssh_write(fds, buf, retv);
#else
				write(fds, buf, retv);
#endif
			} else if (retv == 0 || (retv < 0 && errno != EINTR))
				break;
			FD_CLR(fdn, &rs);
		}
	}
}

static void
exec_cmd(umode, pager, cmdfile, param1)
int umode, pager;
char *cmdfile, *param1;
{
	char buf[STRLEN * 2];
	int save_pager;

	{
		char *ptr = strchr(param1, ';');
		if (ptr != NULL)
			*ptr = 0;
	}

	if (num_useshell() >= 20) {
		clear();
		prints("太多人使用外部程式了，你等一下再用吧...");
		pressanykey();
		return;
	}
	if (!HAS_PERM(PERM_SYSOP) && heavyload(0)) {
		clear();
		prints("抱歉，目前系统负荷过重，此功能暂时不能执行...");
		pressanykey();
		return;
	}
	if (!dashf(cmdfile)) {
		move(2, 0);
		prints("no %s\n", cmdfile);
		pressreturn();
		return;
	}
	save_pager = uinfo.pager;
	if (pager == NA) {
		uinfo.pager = 0;
	}
	modify_user_mode(umode);
	sprintf(buf, "/bin/sh %s %s %s %d", cmdfile, param1, currentuser.userid,
		getpid());
	sprintf(genbuf, "%s exec %s", currentuser.userid, buf);
	newtrace(genbuf);
	do_exec(buf, NULL);
	uinfo.pager = save_pager;
	clear();
}

int
sendgoodwish(char *uid)
{
	return sendGoodWish(NULL);
}

static int
sendGoodWish(char *userid)
{
	FILE *fp, *mp;
	int tuid, i, count;
	time_t now;
	char buf[5][STRLEN], tmpbuf[STRLEN], filebuf[STRLEN];
	char uid[IDLEN + 1], *timestr, ans[8], uident[13], tmp[3];
	int cnt, n, fmode = NA;
	char wishlists[STRLEN];

	modify_user_mode(GOODWISH);
	clear();
	move(1, 0);
	prints("[0;1;32m留言本[m\n您可以在这里给您的朋友送去您的祝福，");
	prints("\n也可以为您给他/她捎上一句悄悄话。");
	move(6, 0);

	if (userid == NULL) {
		getdata(3, 0,
			"(1)给个人送去祝福 (2)给一群人送去祝福 (0) 取消 [1]",
			ans, 2, DOECHO, YEA);
		if (ans[0] == '0') {
			clear();
			return 0;
		}
		if (ans[0] != '2') {
			usercomplete("请输入他的 ID: ", uid);
			if (uid[0] == '\0') {
				clear();
				return 0;
			}
		} else {
			clear();
			sethomefile(wishlists, currentuser.userid, "wishlist");
			cnt = listfilecontent(wishlists);
			while (1) {
				getdata(0, 0,
					"(A)增加 (D)删除 (I)引入好友 (C)清除目前名单 (E)放弃 (S)送出?[S]： ",
					tmp, 2, DOECHO, YEA);
				if (tmp[0] == '\n' || tmp[0] == '\0'
				    || tmp[0] == 's' || tmp[0] == 'S') {
					clear();
					break;
				}
				if (tmp[0] == 'a' || tmp[0] == 'd'
				    || tmp[0] == 'A' || tmp[0] == 'D') {
					move(1, 0);
					if (tmp[0] == 'a' || tmp[0] == 'A')
						usercomplete
						    ("请依次输入使用者代号(只按 ENTER 结束输入): ",
						     uident);
					else
						namecomplete
						    ("请依次输入使用者代号(只按 ENTER 结束输入): ",
						     uident);
					move(1, 0);
					clrtoeol();
					if (uident[0] == '\0')
						continue;
					if (!getuser(uident)) {
						move(2, 0);
						prints
						    ("这个使用者代号是错误的.\n");
					}
				}
				switch (tmp[0]) {
				case 'A':
				case 'a':
					if (seek_in_file(wishlists, uident)) {
						move(2, 0);
						prints
						    ("已经列为收祝福人之一 \n");
						break;
					}
					addtofile(wishlists, uident);
					cnt++;
					break;
				case 'E':
				case 'e':
				case 'Q':
				case 'q':
					cnt = 0;
					break;
				case 'D':
				case 'd':
					if (seek_in_file(wishlists, uident)) {
						del_from_file(wishlists,
							      uident);
						cnt--;
					}
					break;
				case 'I':
				case 'i':
					n = 0;
					clear();
					for (i = cnt; n < uinfo.fnum; i++) {
						int key;
						move(2, 0);
						getuserid(uident,
							  uinfo.friend[n]);
						prints("%s\n", uident);
						move(3, 0);
						n++;
						prints
						    ("(A)全部加入 (Y)加入 (N)不加入 (Q)结束? [Y]:");
						if (!fmode)
							key = igetkey();
						else
							key = 'Y';
						if (key == 'q' || key == 'Q')
							break;
						if (key == 'A' || key == 'a') {
							fmode = YEA;
							key = 'Y';
						}
						if (key == '\0' || key == '\n'
						    || key == 'y' || key == 'Y') {
							if (!getuser(uident)) {
								move(4, 0);
								prints
								    ("这个使用者代号是错误的.\n");
								i--;
								continue;
							} else
							    if (seek_in_file
								(wishlists,
								 uident)) {
								i--;
								continue;
							}
							addtofile(wishlists,
								  uident);
							cnt++;
						}
					}	//for loop
					fmode = NA;
					clear();
					break;
				case 'C':
				case 'c':
					unlink(wishlists);
					cnt = 0;
					break;
				}
				if (strchr("EeQq", tmp[0]))
					break;
				move(5, 0);
				clrtobot();
				move(3, 0);
				clrtobot();
				listfilecontent(wishlists);
			}
			if (cnt <= 0)
				return 0;
			move(5, 0);
			clrtoeol();
			prints("[m【请输入您的留言】       ");
			move(6, 0);
			tmpbuf[0] = '\0';
			prints
			    ("您的留言[直接按 ENTER 结束留言，最多 5 句，每句最长 50 字符]:");
			for (count = 0; count < 5; count++) {
				getdata(7 + count, 0, ": ", tmpbuf, 51, DOECHO,
					YEA);
				if (tmpbuf[0] == '\0')
					break;;
				strcpy(buf[count], tmpbuf);
				tmpbuf[0] = '\0';
			}
			if (count == 0)
				return 0;
			sprintf(genbuf, "你确定要发送这条留言吗");
			move(9 + count, 0);
			if (askyn(genbuf, YEA, NA) == NA) {
				clear();
				return 0;
			}
			setuserfile(wishlists, "wishlist");
			if ((mp = fopen(wishlists, "r")) == NULL) {
				return -3;
			}
			for (n = 0; n < cnt; n++) {
				if (fgets(filebuf, STRLEN, mp) != NULL) {
					if (strtok(filebuf, " \n\r\t") != NULL)
						strcpy(uid, filebuf);
					else
						continue;
				}
				sethomefile(genbuf, uid, "GoodWish");
				if ((fp = fopen(genbuf, "a")) == NULL) {
					prints
					    ("无法开启该用户的底部流动信息文件，请通知站长...\n");
					pressanykey();
					return NA;
				}
				now = time(0);
				timestr = ctime(&now) + 11;
				*(timestr + 5) = '\0';
				for (i = 0; i < count; i++) {
					fprintf(fp, "%s(%s)[%d/%d]：%s\n",
						currentuser.userid, timestr,
						i + 1, count, buf[i]);
				}
				fclose(fp);
				sethomefile(genbuf, uid, "HaveNewWish");
				if ((fp = fopen(genbuf, "w+")) != NULL) {
					fputs("Have New Wish", fp);
					fclose(fp);
				}
				move(11 + count, 0);
				//prints("已经帮您送出您的留言了。\n");
				sprintf(genbuf, "%s sendgoodwish %s",
					currentuser.userid, uid);
				newtrace(genbuf);
			}	//for loop
			return 0;
		}
	} else
		strcpy(uid, userid);
	if (!(tuid = getuser(uid))) {
		move(7, 0);
		prints("[1m您输入的使用者代号( ID )不存在！[m\n");
		pressanykey();
		clear();
		return -1;
	}
	move(5, 0);
	clrtoeol();
	prints("[m【给 [1m%s[m 留言】       ", uid);
	move(6, 0);
	tmpbuf[0] = '\0';
	prints("您的留言[直接按 ENTER 结束留言，最多 5 句，每句最长 50 字符]:");
	for (count = 0; count < 5; count++) {
		getdata(7 + count, 0, ": ", tmpbuf, 51, DOECHO, YEA);
		if (tmpbuf[0] == '\0')
			break;;
		strcpy(buf[count], tmpbuf);
		tmpbuf[0] = '\0';
	}
	if (count == 0)
		return 0;

	sprintf(genbuf, "你确定要发送这条留言给 [1m%s[m 吗", uid);
	move(9 + count, 0);
	if (askyn(genbuf, YEA, NA) == NA) {
		clear();
		return 0;
	}
	sethomefile(genbuf, uid, "GoodWish");
	if ((fp = fopen(genbuf, "a")) == NULL) {
		prints("无法开启该用户的底部流动信息文件，请通知站长...\n");
		pressanykey();
		return NA;
	}
	now = time(0);
	timestr = ctime(&now) + 11;
	*(timestr + 5) = '\0';
	for (i = 0; i < count; i++) {
		fprintf(fp, "%s(%s)[%d/%d]：%s\n",
			currentuser.userid, timestr, i + 1, count, buf[i]);
	}
	fclose(fp);
	sethomefile(genbuf, uid, "HaveNewWish");
	if ((fp = fopen(genbuf, "w+")) != NULL) {
		fputs("Have New Wish", fp);
		fclose(fp);
	}
	move(11 + count, 0);
	prints("已经帮您送出您的留言了。\n");
	sprintf(genbuf, "%s sendgoodwish %s", currentuser.userid, uid);
	newtrace(genbuf);
	/*sprintf(genbuf,"把这条祝福发送给其他人么?");
	   if(askyn(genbuf,YEA,NA)==YEA){
	   usercomplete("请输入他的 ID: ", uid);
	   if (uid[0] == '\0') {
	   clear();
	   return 0;
	   }         
	   if (!(tuid = getuser(uid))) {
	   move(7, 0);
	   prints("\x1b[1m您输入的使用者代号( ID )不存在！\x1b[m\n");
	   pressanykey();
	   clear();
	   return -1;
	   }
	   goto loop;
	   } */
	pressanykey();
	clear();
	return 0;
}

/* ppfoong */
void
x_dict()
{
	char buf[STRLEN];
	char *s;
	//int whichdict;

	if (heavyload(0)) {
		clear();
		prints("抱歉，目前系统负荷过重，此功能暂时不能执行...");
		pressanykey();
		return;
	}
	modify_user_mode(DICT);
	clear();
	prints("\n[1;32m     _____  __        __   __");
	prints
	    ("\n    |     \\|__|.----.|  |_|__|.-----.-----.---.-.----.--.--.");
	prints
	    ("\n    |  --  |  ||  __||   _|  ||  _  |     |  _  |   _|  |  |");
	prints
	    ("\n    |_____/|__||____||____|__||_____|__|__|___._|__| |___  |");
	prints
	    ("\n                                                     |_____|[m");
	prints("\n\n\n欢迎使用本站的字典。");
	prints
	    ("\n本字典主要为[1;33m「英汉」[m部分, 但亦可作[1;33m「汉英」[m查询。");
	prints
	    ("\n\n系统将根据您所输入的字串, 自动判断您所要翻查的是英文字还是中文字。");
	prints("\n\n\n请输入您欲翻查的英文字或中文字, 或直接按 <ENTER> 取消。");
	getdata(15, 0, ">", buf, 30, DOECHO, YEA);
	if (buf[0] == '\0') {
		prints("\n您不想查了喔...");
		pressanykey();
		return;
	}
	for (s = buf; *s != '\0'; s++) {
		if (isspace(*s)) {
			prints("\n一次只能查一个字啦, 不能太贪心喔!!");
			pressanykey();
			return;
		}
	}
	myexec_cmd(DICT, YEA, "bin/cdict.sh", buf);
	sprintf(buf, "bbstmpfs/tmp/dict.%s.%d", currentuser.userid, uinfo.pid);
	if (dashf(buf)) {
		ansimore(buf, NA);
		if (askyn("要将结果寄回信箱吗", NA, NA) == YEA)
			mail_file(buf, currentuser.userid, "字典查询结果");
		unlink(buf);
	}
}

void
x_tt()
{
	myexec_cmd(TT, NA, "bin/tt", NULL);
	redoscr();
}

void
x_worker()
{
	myexec_cmd(WORKER, YEA, "bin/worker", NULL);
	redoscr();
}

void
x_tetris()
{
	myexec_cmd(TETRIS, NA, "bin/tetris", NULL);
	redoscr();
}

void
x_winmine()
{
	myexec_cmd(WINMINE, NA, "bin/winmine", NULL);
	redoscr();
}

void
x_winmine2()
{
	myexec_cmd(WINMINE2, NA, "bin/winmine2", NULL);
	redoscr();
}

void
x_recite()
{
	myexec_cmd(RECITE, NA, "bin/ptyexec", "bin/recite");
	redoscr();
}

void
x_ncce()
{
	myexec_cmd(NCCE, NA, "bin/ptyexec", "bin/ncce");
	redoscr();
}

void
x_chess()
{
	myexec_cmd(CHESS, NA, "bin/chc", NULL);
	redoscr();
}

void
x_qkmj()
{
	myexec_cmd(CHESS, NA, "bin/qkmj", NULL);
	redoscr();
}

void
x_quickcalc()
{
	clear();
	prints("\n-------数字运算, 输入help获得帮助------\n");
	myexec_cmd(QUICKCALC, NA, "bin/ptyexec", "bin/qc");
	redoscr();
}

void
x_freeip()
{
	clear();
	if (heavyload(2.5)) {
		prints("抱歉，目前系统负荷过重，此功能暂时不能执行...");
		pressanykey();
		return;
	}
	myexec_cmd(FREEIP, NA, "bin/ptyexec", "bin/freeip");
	redoscr();
}

void
x_showuser()
{
	char buf[STRLEN];

	modify_user_mode(SYSINFO);
	clear();
	stand_title("本站使用者资料查询");
	ansimore("etc/showuser.msg", NA);
	getdata(20, 0, "Parameter: ", buf, 30, DOECHO, YEA);
	if ((buf[0] == '\0') || dashf("bbstmpfs/tmp/showuser.result"))
		return;
	securityreport("查询使用者资料", "查询使用者资料");
	exec_cmd(SYSINFO, YEA, "bin/showuser.sh", buf);
	sprintf(buf, "bbstmpfs/tmp/showuser.result");
	if (dashf(buf)) {
		mail_file(buf, currentuser.userid, "使用者资料查询结果");
		unlink(buf);
	}
}

static void
childreturn(int i)
{

	int retv;
	while ((retv = waitpid(-1, NULL, WNOHANG | WUNTRACED)) > 0)
		if (childpid > 0 && retv == childpid)
			childpid = 0;
}

int
ent_bnet(char *cmd)
{
	int p[2];
#ifdef SSHBBS
	move(9, 0);
	clrtobot();
	prints("您目前是使用 ssh 方式连接 %s.\n"
	       "ssh 会对网络数据传输进行加密,保护您的密码和其它私人信息.\n"
	       "您必须知道,网络穿梭是通过本站连接到其它bbs,虽然从您的机器\n"
	       "到本站间的数据传输是加密的,但是从本站到另外的 BBS 站点间的\n"
	       "数据传输并没有加密,您的密码和其它私人信息有被第三方窃听的\n可能.\n",
	       MY_BBS_NAME);
	if (askyn("你确定要使用网络穿梭吗", NA, NA) != 1)
		return -1;
#endif
	signal(SIGALRM, SIG_IGN);
	signal(SIGCHLD, childreturn);
	modify_user_mode(BBSNET);
	do_delay(1);
	refresh();
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, p) < 0)
		return -1;
	if (p[0] <= 2 || p[1] <= 2) {
		int i = p[0] + p[1] + 1;
		dup2(p[0], i);
		dup2(p[1], i + 1);
		close(p[0]);
		close(p[1]);
		p[0] = i;
		p[1] = i + 1;
	}
	inBBSNET = 1;
	childpid = fork();
	if (childpid == 0) {
		close(p[1]);
		if (p[0] != 0)
			dup2(p[0], 0);
		dup2(0, 1);
		dup2(0, 2);
		execl("bin/bbsnet", "bbsnet",
		      (cmd[0] == 'A') ? "etc/bbsnetA.ini" : "etc/bbsnet.ini",
		      "bin/telnet", currentuser.userid, NULL);
		exit(0);
	} else if (childpid > 0) {
		close(p[0]);
		datapipefd(0, p[1]);
		close(p[1]);
	} else {
		close(p[0]);
		close(p[1]);
	}
	signal(SIGCHLD, SIG_IGN);
	inBBSNET = 0;
	redoscr();
//      r_msg();
	return 0;
}

int
x_denylevel()
{
	int id;
	char ans[7], content[1024];
	int oldlevel;
	modify_user_mode(ADMIN);
	if (!check_systempasswd()) {
		return -1;
	}
	clear();
	move(0, 0);
	prints("更改使用者基本权限\n");
	clrtoeol();
	move(1, 0);
	usercomplete("输入欲更改的使用者帐号: ", genbuf);
	if (genbuf[0] == '\0') {
		clear();
		return 0;
	}
	if (!(id = getuser(genbuf))) {
		move(3, 0);
		prints("Invalid User Id");
		clrtoeol();
		pressreturn();
		clear();
		return -1;
	}
	move(1, 0);
	clrtobot();
	move(2, 0);
	prints("设定使用者 '%s' 的基本权限 \n\n", genbuf);
	prints("(1) 封禁发表文章权利       (A) 恢复发表文章权利\n");
	prints("(2) 取消基本上站权利       (B) 恢复基本上站权利\n");
	prints("(3) 禁止进入聊天室         (C) 恢复进入聊天室权利\n");
	prints("(4) 禁止呼叫他人聊天       (D) 恢复呼叫他人聊天权利\n");
	prints("(5) 整理个人精华区         (E) 不能整理个人精华区\n");
	prints("(6) 禁止发送信件           (F) 恢复发信权利\n");
	prints("(7) 禁止使用签名档         (G) 恢复使用签名档权利\n");
	prints("(8) 禁止使用自定义锁屏     (H) 恢复使用自定义锁屏\n");
	getdata(13, 0, "请输入你的处理: ", ans, 3, DOECHO, YEA);
	oldlevel = lookupuser.userlevel;
	switch (ans[0]) {
	case '1':
		lookupuser.userlevel &= ~PERM_POST;
		break;
	case 'a':
	case 'A':
		lookupuser.userlevel |= PERM_POST;
		break;
	case '2':
		lookupuser.userlevel &= ~PERM_BASIC;
		break;
	case 'b':
	case 'B':
		lookupuser.userlevel |= PERM_BASIC;
		break;
	case '3':
		lookupuser.userlevel &= ~PERM_CHAT;
		break;
	case 'c':
	case 'C':
		lookupuser.userlevel |= PERM_CHAT;
		break;
	case '4':
		lookupuser.userlevel &= ~PERM_PAGE;
		break;
	case 'd':
	case 'D':
		lookupuser.userlevel |= PERM_PAGE;
		break;
	case '5':
		lookupuser.userlevel |= PERM_SPECIAL8;
		break;
	case 'e':
	case 'E':
		lookupuser.userlevel &= ~PERM_SPECIAL8;
		break;
	case '6':
		lookupuser.userlevel |= PERM_DENYMAIL;
		break;
	case 'f':
	case 'F':
		lookupuser.userlevel &= ~PERM_DENYMAIL;
		break;
	case '7':
		lookupuser.userlevel |= PERM_DENYSIG;
		{
			getdata(13, 0, "禁止使用签名档的原因：", genbuf, 40,
				DOECHO, YEA);
			sprintf(content,
				"您被禁止使用签名档，原因是：\n    %s\n\n"
				"(如果是因为签名档图片大小超，请参阅 Announce "
				"版的公告<<关于图片签名档的大小限制>>\n"
				"http://ytht.net/Ytht.Net/bbscon?B=Announce&F=M.1047666649.A )",
				genbuf);
			mail_buf(content, lookupuser.userid,
				 "您被禁止使用签名档");
		}
		break;
	case 'g':
	case 'G':
		lookupuser.userlevel &= ~PERM_DENYSIG;
		break;
	case '8':
		lookupuser.userlevel &= ~PERM_SELFLOCK;
		{
			getdata(13, 0, "禁止使用自定义锁屏的原因：", genbuf, 40,
				DOECHO, YEA);
			sprintf(content,
				"您被禁止使用自定义锁屏，原因如下:\n\n   %s\n",
				genbuf);
			mail_buf(content, lookupuser.userid,
				 "您被禁止使用自定义锁屏");
		}
		break;
    case 'h':
	case 'H':
		lookupuser.userlevel |= PERM_SELFLOCK;
		{
		       sprintf(content,"您的自定义锁屏封禁已经被解除，重新登录后可以继续使用");
		       mail_buf(content,lookupuser.userid,"您可以使用自定义锁屏");
		}
		break;
	default:
		prints("\n 使用者 '%s' 基本权利没有变更  %d\n", lookupuser.userid);
		pressreturn();
		clear();
		return 0;
	}
	{
		char secu[STRLEN];
		sprintf(secu, "修改 %s 的基本权限", lookupuser.userid);
		permtostr(oldlevel, genbuf);
		sprintf(content, "修改前的权限：%s\n修改后的权限：", genbuf);
		permtostr(lookupuser.userlevel, genbuf);
		strcat(content, genbuf);
		securityreport(secu, content);
	}

	substitute_record(PASSFILE, &lookupuser, sizeof (struct userec), id);
	clear();
	return 0;
}

int
s_checkid()
{
	char buf[256];
	char checkuser[20];
	int day, id;
	modify_user_mode(GMENU);
	clear();
	stand_title("调查ID发文情况\n");
	clrtoeol();
	move(2, 0);
	usercomplete("输入欲调查的使用者帐号: ", genbuf);
	if (genbuf[0] == '\0') {
		clear();
		return 0;
	}
	strcpy(checkuser, genbuf);
	if (!(id = getuser(genbuf))) {
		move(4, 0);
		prints("无效的使用者帐号");
		clrtoeol();
		pressreturn();
		clear();
		return 0;
	}
	getdata(5, 0, "输入天数(0-所有时间): ", buf, 7, DOECHO, YEA);
	day = atoi(buf);
	sprintf(buf,
		"/usr/bin/nice " MY_BBS_HOME "/bin/finddf %d %d %s > " MY_BBS_HOME
		"/bbstmpfs/tmp/checkid.%s 2>/dev/null", currentuser.userlevel,
		day, checkuser, currentuser.userid);
	if ((HAS_PERM(PERM_SYSOP) && heavyload(2.5))
	    || (!HAS_PERM(PERM_SYSOP) && heavyload(1.5))) {
		prints("系统负载过重, 无法执行本指令");
		pressreturn();
		return 1;
	}
	system(buf);
	sprintf(buf, "%s finddf %s %d", currentuser.userid, checkuser, day);
	newtrace(buf);
	sprintf(buf, MY_BBS_HOME "/bbstmpfs/tmp/checkid.%s",
		currentuser.userid);
	mail_file(buf, currentuser.userid, "\"System Report\"");
	prints("完毕");
	clrtoeol();
	pressreturn();
	clear();
	return 1;
}

char *
directfile(char *fpath, char *direct, char *filename)
{
	char *t;
	strcpy(fpath, direct);
	if ((t = strrchr(fpath, '/')) == NULL)
		exit(0);
	t++;
	strcpy(t, filename);
	return fpath;
}

static void
escape_filename(char *fn)
{
	static const char invalid[] =
	    { '/', '\\', '!', '&', '|', '*', '?', '`', '\'', '\"', ';', '<',
		'>', ':', '~', '(', ')', '[', ']', '{', '}', '$', '\n', '\r'
	};
	int i, j;

	for (i = 0; i < strlen(fn); i++)
		for (j = 0; j < sizeof (invalid); j++)
			if (fn[i] == invalid[j])
				fn[i] = '_';
}

int
zsend_file(char *from, char *title)
{
	char name[200], name1[200];
	char path[200];
	FILE *fr, *fw;
	char to[200];
	char buf[512], *fn = NULL;
	char attachfile[200];
	char attach_to_send[200];
	int len, isa, base64;

	ansimore("etc/zmodem", 0);
	move(14, 0);
	len = file_size(from);

	prints
	    ("此次传输共 %d bytes, 大约耗时 %d 秒（以 5k/s 计算）", len,
	     len / ZMODEM_RATE);
	move(t_lines - 1, 0);
	clrtoeol();
	strcpy(name, "N");

	getdata(t_lines - 1, 0,
		"您确定要使用Zmodem传输文件么?[y/N]", name, 2, DOECHO, YEA);
	if (toupper(name[0]) != 'Y')
		return FULLUPDATE;
	strncpy(name, title, 76);
	name[80] = '\0';
	escape_filename(name);
	move(t_lines - 2, 0);
	clrtoeol();
	prints("请输入文件名，为空则放弃");
	move(t_lines - 1, 0);
	clrtoeol();
	getdata(t_lines - 1, 0, "", name, 78, DOECHO, 0);
	if (name[0] == '\0')
		return FULLUPDATE;
	name[78] = '\0';
	escape_filename(name);
	sprintf(name1, "YTHT-%s-", currboard);
	strcat(name1, name);
	strcpy(name, name1);
	strcat(name1, ".TXT");
	snprintf(path, sizeof (path), PATHZMODEM "/%s.%d", currentuser.userid,
		 uinfo.pid);
	mkdir(path, 0770);
	sprintf(to, "%s/%s", path, name1);
	fr = fopen(from, "r");
	if (fr == NULL)
		return FULLUPDATE;
	fw = fopen(to, "w");
	if (fw == NULL) {
		fclose(fr);
		return FULLUPDATE;
	}
	while (fgets(buf, sizeof (buf), fr) != NULL) {
		base64 = isa = 0;
		if (!strncmp(buf, "begin 644", 10)) {
			isa = 1;
			base64 = 1;
			fn = buf + 10;
		} else if (checkbinaryattach(buf, fr, &len)) {
			isa = 1;
			base64 = 0;
			fn = buf + 18;
		}
		if (isa) {
			sprintf(attachfile, "%s-attach-%s",  name, fn);
			if (getattach(fr, buf, attachfile, path, base64, len, 0)) {
				fprintf(fw, "附件%s错误\n", fn);
			} else {
				sprintf(attach_to_send, "%s/%s", path, attachfile);
				bbs_zsendfile(attach_to_send);
				fprintf(fw, "附件%s\n", fn);
			}
		} else
			fputs(buf, fw);
	}
	fclose(fw);
	fclose(fr);
	bbs_zsendfile(to);
	return FULLUPDATE;
}

static void
bbs_zsendfile(char *filename)
{
	if (!dashf(filename))
		return;
	refresh();
	myexec_cmd(READING, NA, "bin/sz.sh", filename);
	unlink(filename);
}

static void
get_load(load)
double load[];
{
#if defined(LINUX)
	FILE *fp;
	fp = fopen("/proc/loadavg", "r");
	if (!fp)
		load[0] = load[1] = load[2] = 0;
	else {
		float av[3];
		fscanf(fp, "%g %g %g", av, av + 1, av + 2);
		fclose(fp);
		load[0] = av[0];
		load[1] = av[1];
		load[2] = av[2];
	}
#elif defined(BSD44)
	getloadavg(load, 3);
#else
	struct statstime rs;
	rstat("localhost", &rs);
	load[0] = rs.avenrun[0] / (double) (1 << 8);
	load[1] = rs.avenrun[1] / (double) (1 << 8);
	load[2] = rs.avenrun[2] / (double) (1 << 8);
#endif
}

void
inn_reload()
{
	char ans[4];

	getdata(t_lines - 1, 0, "重读配置吗 (Y/N)? [N]: ", ans, 2, DOECHO, YEA);
	if (ans[0] == 'Y' || ans[0] == 'y') {
		myexec_cmd(ADMIN, NA, "innd/ctlinnbbsd", "reload");
	}
}

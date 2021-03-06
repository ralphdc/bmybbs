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

#include "bbs.h"

int
d_board()
{
	struct boardheader binfo;
	int bid, ans;
	char bname[STRLEN];
	extern char lookgrp[];

	if (!HAS_PERM(PERM_BLEVELS)) {
		return -1;
	}
	modify_user_mode(ADMIN);
	if (!check_systempasswd()) {
		return -1;
	}
	clear();
	stand_title("删除讨论区");
	make_blist_full();
	move(1, 0);
	namecomplete("请输入讨论区: ", bname);
	if (bname[0] == '\0')
		return -1;
	bid = getbnum(bname);
	if (get_record(BOARDS, &binfo, sizeof (binfo), bid) == -1) {
		move(2, 0);
		prints("不正确的讨论区\n");
		pressreturn();
		clear();
		return -1;
	}
	ans = askyn("你确定要删除这个讨论区", NA, NA);
	if (ans != 1) {
		move(2, 0);
		prints("取消删除行动\n");
		pressreturn();
		clear();
		return -1;
	}
	{
		char secu[STRLEN];
		sprintf(secu, "删除讨论区：%s", binfo.filename);
		securityreport(secu, secu);
	}
	if (seek_in_file("0Announce/.Search", bname)) {
		move(4, 0);
		if (askyn("移除精华区", NA, NA) == YEA) {
			get_grp(binfo.filename);
			del_grp(lookgrp, binfo.filename, binfo.title);
		}
	}
	if (seek_in_file("etc/junkboards", bname))
		del_from_file("etc/junkboards", bname);
	if (seek_in_file("0Announce/.Search", bname))
		del_from_file("0Announce/.Search", bname);

	if (binfo.filename[0] == '\0')
		return -1;	/* rrr - precaution */
	sprintf(genbuf, "boards/%s", binfo.filename);
	deltree(genbuf);
	sprintf(genbuf, "vote/%s", binfo.filename);
	deltree(genbuf);
	sprintf(genbuf, " << '%s'被 %s 删除 >>",
		binfo.filename, currentuser.userid);
	memset(&binfo, 0, sizeof (binfo));
	strsncpy(binfo.title, genbuf, sizeof (binfo.title));
	binfo.level = PERM_SYSOP;
	substitute_record(BOARDS, &binfo, sizeof (binfo), bid);

	reload_boards();
	update_postboards();

	move(4, 0);
	prints("\n本讨论区已经删除...\n");
	pressreturn();
	clear();
	return 0;
}

void
offline()
{
	char buf[STRLEN];

	modify_user_mode(OFFLINE);
	clear();
	if (HAS_PERM(PERM_SYSOP) || HAS_PERM(PERM_BOARDS)
	    || HAS_PERM(PERM_ADMINMENU) || HAS_PERM(PERM_SEEULEVELS)) {
		move(1, 0);
		prints("\n\n您有重任在身, 不能随便自杀啦!!\n");
		pressreturn();
		clear();
		return;
	}
	if (currentuser.stay < 86400) {
		move(1, 0);

		prints("\n\n对不起, 您还未够资格执行此命令!!\n");
		prints("只有上站时间超过24小时的用户才能自杀.\n");
		pressreturn();
		clear();
		return;
	}

	getdata(1, 0, "请输入你的密码: ", buf, PASSLEN, NOECHO, YEA);
	if (*buf == '\0' || !checkpasswd(currentuser.passwd, buf)) {
		prints("\n\n很抱歉, 您输入的密码不正确。\n");
		pressreturn();
		clear();
		return;
	}
	getdata(3, 0, "请问你叫什么名字? ", buf, NAMELEN, DOECHO, YEA);
	if (*buf == '\0' || strcmp(buf, currentuser.realname)) {
		prints("\n\n很抱歉, 我并不认识你。\n");
		pressreturn();
		clear();
		return;
	}
	clear();
	move(1, 0);
	prints("[1;5;31m警告[0;1;31m 自杀和id死亡释放邮箱没有任何关系[m \n");
	prints("自杀仅仅是让您的id无法正常使用，而id的死亡只和生命力有关系\n");
	prints("如果有人告诉你自杀可以帮助您让您现有的id死亡并注册新的id，请离开这里\n");
	prints("然后把告诉你的人打一顿\n\n");
	prints
	    ("[1;5;31m警告[0;1;31m： 自杀后, 您的灵魂将升入天国或堕入地狱, 愿您安息");
	prints("\n\n\n[1;32m好难过喔.....[m\n\n\n");
	if (askyn("你确定要离开这个大家庭", NA, NA) == 1) {
		clear();
		set_safe_record();
		currentuser.dietime = currentuser.stay + 6*18 * 24 * 60 * 60;//改自杀恢复为6*18天   六道轮回+18层地狱
		substitute_record(PASSFILE, &currentuser, sizeof (currentuser),
				  usernum);
		Q_Goodbye();
		return;
		/*if(d_user(currentuser.userid)==1)
		   {
		   mail_info();
		   modify_user_mode( OFFLINE );
		   kick_user(&uinfo);
		   exit(0);
		   }
		 */
	}
}

int
online()
{
	char buf[STRLEN];
	struct tm *nowtime;
	time_t nowtimeins;
	modify_user_mode(OFFLINE);
	clear();
	nowtimeins = time(NULL);
	nowtime = localtime(&nowtimeins);
	if ((currentuser.stay <= currentuser.dietime)) {
		move(1, 0);
		prints("\n\n死期未满啊!!\n");
		prints("你要吓死人啊!\n");
		prints("你的死期还有 %d 分钟",
		       1 + (currentuser.dietime - currentuser.stay) / 60);
		pressreturn();
		clear();
		return -1;
	}
	getdata(1, 0, "请输入你的密码: ", buf, PASSLEN, NOECHO, YEA);
	if (*buf == '\0' || !checkpasswd(currentuser.passwd, buf)) {
		prints("\n\n很抱歉, 您输入的密码不正确。\n");
		pressreturn();
		clear();
		return -2;
	}
	getdata(3, 0, "请问你叫什么名字? ", buf, NAMELEN, DOECHO, YEA);
	if (*buf == '\0' || strcmp(buf, currentuser.realname)) {
		prints("\n\n很抱歉, 我并不认识你。\n");
		pressreturn();
		clear();
		return -3;
	}
	clear();
	move(1, 0);
	prints
	    ("\033[1;5;31m警告\033[0;1;31m： 生存,还是毁灭,是个值得考虑的问题");
	prints("\n\n\n\033[1;32m您可要想清楚.....\033[m\n\n\n");
	if (askyn("你确定要返世为人了吗?", NA, NA) == 1) {
		clear();
		currentuser.dietime = 0;
		substitute_record(PASSFILE, &currentuser, sizeof (currentuser),
				  usernum);
		return Q_Goodbye();
	}
	return 0;
}

void
getuinfo(fn)
FILE *fn;
{
	int num;
	char buf[40];

	fprintf(fn, "\n\n他的代号     : %s\n", currentuser.userid);
	fprintf(fn, "他的昵称     : %s\n", currentuser.username);
	fprintf(fn, "真实姓名     : %s\n", currentuser.realname);
	fprintf(fn, "居住住址     : %s\n", currentuser.address);
	fprintf(fn, "电子邮件信箱 : %s\n", currentuser.email);
	fprintf(fn, "真实 E-mail  : %s\n", currentuser.realmail);
	//fprintf(fn,"Ident 资料   : %s\n", currentuser.ident);
	fprintf(fn, "域名指向     : %s\n", currentuser.ip);
	fprintf(fn, "帐号建立日期 : %s", ctime(&currentuser.firstlogin));
	fprintf(fn, "最近光临日期 : %s", ctime(&currentuser.lastlogin));
	fprintf(fn, "最近光临机器 : %s\n", currentuser.lasthost);
	fprintf(fn, "最近离站时间 : %s", ctime(&currentuser.lastlogout));
	fprintf(fn, "上站次数     : %d 次\n", currentuser.numlogins);
	fprintf(fn, "文章数目     : %d\n", currentuser.numposts);
	fprintf(fn, "上站总时数   : %ld 小时 %ld 分钟\n",
		(long int) (currentuser.stay / 3600),
		(long int) ((currentuser.stay / 60) % 60));
	strcpy(buf, "bTCPRp#@XWBA#VS-DOM-F012345678");
	for (num = 0; num < 30; num++)
		if (!(currentuser.userlevel & (1 << num)))
			buf[num] = '-';
	buf[num] = '\0';
	fprintf(fn, "使用者权限   : %s\n\n", buf);
}

#if 0
static void
mail_info()
{
	FILE *fn;
	time_t now;
	char filename[STRLEN];

	now = time(0);
	sprintf(filename, "tmp/suicide.%s", currentuser.userid);
	if ((fn = fopen(filename, "w")) != NULL) {
		fprintf(fn,
			"[1m%s[m 已经在 [1m%24.24s[m 自杀了，以下是他的资料，请保留...",
			currentuser.userid, ctime(&now));
		getuinfo(fn);
		fprintf(fn,
			"\n                      [1m 系统自动发信系统留[m\n\n");
		fclose(fn);
		postfile(filename, "syssecurity", "自杀通知....", 2);
		unlink(filename);
	}
	if ((fn = fopen(filename, "w")) != NULL) {
		fprintf(fn, "大家好,\n\n");
		fprintf(fn, "我是 %s (%s)。 我已经离开这里了。\n\n",
			currentuser.userid, currentuser.username);
		fprintf(fn,
			"我不会更不可能忘记自 %s以来我在本站 %d 次 login 中总共 %d 分钟逗留期间的点点滴滴。\n",
			ctime(&currentuser.firstlogin), currentuser.numlogins,
			(int) currentuser.stay / 60);
		fprintf(fn, "请我的好友把 %s 从你们的好友名单中拿掉吧。\n\n",
			currentuser.userid);
		fprintf(fn, "或许有朝一日我会回来的。 珍重!! 再见!!\n\n\n");
		fprintf(fn, "%s 于 %24.24s 留.\n\n", currentuser.userid,
			ctime(&now));
		fclose(fn);
		postfile(filename, "notepad", "自杀留言....", 2);
		unlink(filename);
	}
}
#endif
int
d_user(cid)
char *cid;
{
	int id;

	if (uinfo.mode != OFFLINE) {
		modify_user_mode(ADMIN);
		if (!check_systempasswd()) {
			return -1;
		}
		clear();
		stand_title("删除使用者帐号");
		move(1, 0);
		usercomplete("请输入欲删除的使用者代号: ", genbuf);
		if (*genbuf == '\0') {
			clear();
			return 0;
		}
	} else
		strcpy(genbuf, cid);
	if (!(id = getuser(genbuf))) {
		move(3, 0);
		prints("错误的使用者代号...");
		clrtoeol();
		pressreturn();
		clear();
		return 0;
	}
/*    if (!isalpha(lookupuser.userid[0])) return 0;*/
	/* rrr - don't know how... */
	move(1, 0);
	if (uinfo.mode != OFFLINE)
		prints("删除使用者 '%s'.", genbuf);
	else
		prints(" %s 将离开这里", cid);
	clrtoeol();
	getdata(2, 0, "(Yes, or No) [N]: ", genbuf, 4, DOECHO, YEA);
	if (genbuf[0] != 'Y' && genbuf[0] != 'y') {	/* if not yes quit */
		move(2, 0);
		if (uinfo.mode != OFFLINE)
			prints("取消删除使用者...\n");
		else
			prints("你终于回心转意了，好高兴喔...");
		pressreturn();
		clear();
		return 0;
	}
	if (lookupuser.userid[0] == '\0' || !strcmp(lookupuser.userid, "SYSOP")) {
		prints("无法删除!!\n");
		pressreturn();
		clear();
		return 0;
	}
	if (uinfo.mode != OFFLINE) {
		char secu[STRLEN];
		sprintf(secu, "删除使用者：%s", lookupuser.userid);
		securityreport(secu, secu);
	}
	sprintf(genbuf, "mail/%c/%s", mytoupper(lookupuser.userid[0]),
		lookupuser.userid);
	deltree(genbuf);
	sprintf(genbuf, "home/%c/%s", mytoupper(lookupuser.userid[0]),
		lookupuser.userid);
	deltree(genbuf);
	lookupuser.userlevel = 0;
	strcpy(lookupuser.address, "");
	strcpy(lookupuser.username, "");
	strcpy(lookupuser.realname, "");
	strcpy(lookupuser.ip, "");
	strcpy(lookupuser.realmail, "");
	lookupuser.userid[0] = '\0';
	substitute_record(PASSFILE, &lookupuser, sizeof (lookupuser), id);
	setuserid(id, lookupuser.userid);
	move(2, 0);
	prints("%s 已经被灭绝了...\n", lookupuser.userid);
	pressreturn();

	clear();
	return 1;
}

int
kick_user(userinfo,mode)
struct user_info *userinfo;
int mode;
{
	int id, ind;
	struct user_info uin;
	struct userec kuinfo;
	char kickuser[40];
       char kickreason[STRLEN];
       char titlebuf[STRLEN];
       char contentbuf[STRLEN];
       char repbuf[STRLEN];
       char msgbuf[STRLEN];
	if (uinfo.mode != LUSERS && uinfo.mode != OFFLINE
	    && uinfo.mode != FRIEND) {
		modify_user_mode(ADMIN);
		stand_title("Kick User");
		move(1, 0);
		usercomplete("Enter userid to be kicked: ", kickuser);
		if (*kickuser == '\0') {
			clear();
			return 0;
		}
		if (!(id = getuser(kickuser))) {
			move(3, 0);
			prints("Invalid User Id");
			clrtoeol();
			pressreturn();
			clear();
			return 0;
		}
		move(1, 0);
		prints("Kick User '%s'.", kickuser);
		clrtoeol();
		getdata(2, 0, "(Yes, or No) [N]: ", genbuf, 4, DOECHO, YEA);
		if (genbuf[0] != 'Y' && genbuf[0] != 'y') {	/* if not yes quit */
			move(2, 0);
			prints("Aborting Kick User\n");
			pressreturn();
			clear();
			return 0;
		}
		search_record(PASSFILE, &kuinfo, sizeof (kuinfo),
			      (void *) cmpuids, kickuser);
		ind = search_ulist(&uin, t_cmpuids, id);
	} else {
		uin = *userinfo;
		strcpy(kickuser, uin.userid);
/*        id = getuser(kickuser);
        search_record(PASSFILE, &kuinfo, sizeof(kuinfo), cmpuids, kickuser);
        ind = search_ulist( &uin, t_cmpuids, id ); */
		ind = YEA;
	}


	//踢自己时 mode=1
       if(mode!=1)
       {
           	clear();
             	move(1,0);
              getdata(2,0,"踢人原因: ", kickreason,STRLEN,DOECHO,YEA);
        }
	if (uin.pid != 1
	    && (!ind || !uin.active || uin.pid <= 0
		|| (kill(uin.pid, 0) == -1))) {
		if (uinfo.mode != LUSERS && uinfo.mode != OFFLINE
		    && uinfo.mode != FRIEND) {
			move(3, 0);
			prints("User Has Logged Out");
			clrtoeol();
			pressreturn();
			clear();
		}
		return 0;
	} else if (kill(uin.pid, SIGHUP) < 0) {
		prints("User can't be kicked");
		pressreturn();
		clear();
		return 1;
	}
       sprintf(contentbuf, "%s",kickreason);
       sprintf(titlebuf,"%s将%s踢出站外", currentuser.userid, kickuser);
       securityreport(titlebuf,contentbuf);

       sprintf(repbuf,"您被%s强制离开本站",currentuser.userid);
       sprintf(msgbuf,"理由:%s\n",kickreason);
       mail_buf(msgbuf,kickuser, repbuf);
	sprintf(genbuf, "%s kick %s", currentuser.userid, kickuser);
	newtrace(genbuf);
	move(2, 0);
	if (uinfo.mode != LUSERS && uinfo.mode != OFFLINE
	    && uinfo.mode != FRIEND) {
		prints("User has been Kicked\n");
		pressreturn();
		clear();
	}
	return 1;
}

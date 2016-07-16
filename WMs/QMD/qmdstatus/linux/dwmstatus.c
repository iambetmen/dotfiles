#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

/* For volume display */
#include <alsa/asoundlib.h>

/* For MPD support */
#include <mpd/client.h>


/* Timezone */
char *tzchicago = "Asia/Jakarta";

/* Start ansi color codes */
char *colblack = "\x1b[0;30m";
char *colred = "\x1b[0;31m";
char *colgreen = "\x1b[0;32m";
char *colyellow = "\x1b[0;33m";
char *colblue = "\x1b[0;34m";
char *colmagenta = "\x1b[0;35m";
char *colcyan = "\x1b[0;36m";
char *colwhite = "\x1b[0;37m";
char *colbblack = "\x1b[1;30m";
char *colbred = "\x1b[1;31m";
char *colbgreen = "\x1b[1;32m";
char *colbyellow = "\x1b[1;33m";
char *colbblue = "\x1b[1;34m";
char *colbmagenta = "\x1b[1;35m";
char *colbcyan = "\x1b[1;36m";
char *colbwhite = "\x1b[1;37m";
/* End ansi color codes */

static Display *dpy;
XkbDescRec *kbdDescPtr;

char *
smprintf(char *fmt, ...) {
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname) {
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname) {
	char buf[129];
	time_t tim;
	struct tm *timtm;

	memset(buf, 0, sizeof(buf));
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str) {
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}


char *
get_volume() {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *s_elem;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, "default");
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);
    snd_mixer_selem_id_malloc(&s_elem);
    snd_mixer_selem_id_set_name(s_elem, "Master");

    elem = snd_mixer_find_selem(handle, s_elem);

    if (elem == NULL)
    {
        snd_mixer_selem_id_free(s_elem);
        snd_mixer_close(handle);

        exit(EXIT_FAILURE);
    }

    long int vol, max, min, percent;

    snd_mixer_handle_events(handle);
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_get_playback_volume(elem, 0, &vol);

    percent = (vol * 100) / max;

    snd_mixer_selem_id_free(s_elem);
    snd_mixer_close(handle);

    return smprintf("%ld", percent);
}

char *
getmpdstat() {
    struct mpd_song * song = NULL;
	const char * title = NULL;
	const char * artist = NULL;
	const char * uri = NULL;
	const char * titletag = NULL;
	const char * artisttag = NULL;
	char * retstr = NULL;
	int elapsed = 0, total = 0;
    struct mpd_connection * conn ;
    if (!(conn = mpd_connection_new("localhost", 0, 30000)) ||
        mpd_connection_get_error(conn)){
            return smprintf("");
    }

    mpd_command_list_begin(conn, true);
    mpd_send_status(conn);
    mpd_send_current_song(conn);
    mpd_command_list_end(conn);

    struct mpd_status* theStatus = mpd_recv_status(conn);
        if ((theStatus) && (mpd_status_get_state(theStatus) == MPD_STATE_PLAY)) {
                mpd_response_next(conn);
                song = mpd_recv_song(conn);
		titletag = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
		artisttag = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
                title = smprintf("%s",titletag);
                artist = smprintf("%s",artisttag);
		uri = smprintf("%s",mpd_song_get_uri(song));

                elapsed = mpd_status_get_elapsed_time(theStatus);
                total = mpd_status_get_total_time(theStatus);
                mpd_song_free(song);

		/* If the song isn't tagged, then just use the filename */
		if(artisttag == NULL && titletag == NULL) {
			retstr = smprintf("%s[%s%s%s | %s%.2d:%.2d%s/%.2d:%.2d%s]",
					colcyan, colyellow, uri, colcyan,
					colbyellow, elapsed/60, elapsed%60,
					colyellow, total/60,   total%60, colcyan);
		} else {
			retstr = smprintf("%s[%s%s%s | %s%s%s | %s%.2d:%.2d%s/%.2d:%.2d%s]",
					colcyan, colyellow, artist, colcyan, colred, title, colcyan,
					colbyellow, elapsed/60, elapsed%60,
					colyellow, total/60,   total%60, colcyan);
		}
                free((char*)title);
                free((char*)artist);
		free((char*)uri);
        }
        else retstr = smprintf("");
		mpd_response_finish(conn);
		mpd_connection_free(conn);
		return retstr;
}

char *
getMailCount() {

	char *mailCount = NULL;
	/* path to home directory */
	char *homedir = getenv("HOME");
	char *unreadFilename = NULL;
	/* Color of text for each mailbox */
	const char *mailcolor[3] = { colyellow, colred, colgreen };
	char mailnames[3] = { 's', 'n', 'o' };
	
	/* Filenames of our 3 mailboxes */
	char mboxes[3][6] = {"mbox1", "mbox2", "mbox3"};

	/* Number of unread mails on 3 accounts
	 * 0 - main account, 1 - backup account, 2 - junk account */
	int unreadMails[3] = {0, 0, 0};
	int i;

	for(i = 0; i < 3; i++) {
		/* build file name */
		unreadFilename = smprintf("%s/.unread/%s", homedir, mboxes[i]);
		/* read number of unread mails stored in file by external script */
		FILE *f = fopen(unreadFilename,"r");
		
		/* unreadMails is initialized to 0, so if we can't open the file, just skip */
		if(f != NULL) {
			fscanf(f,"%d",&unreadMails[i]);
			fclose(f);
		}

		/* free built file name */
		free(unreadFilename);

		/* If we have any unread mail, set the text color to mailcolor */
		//Changing color if it's 0 or not doesn't make sense if we hide when 0
		/* if(unreadMails[i] != 0) { mailcolor[i] = colbyellow; } */
	}

	/* Space for each mailbox string */
	char **mailStrings;
	mailStrings = malloc(sizeof(char*) * 3);
	
	/* Count up total mail - if none, display green envelope ✉ */
	char *envelope = "@";
	//char *envelope = "🖂";
	/* Initialize mailStrings to NULL in case they aren't used */
	int totalUnreadMail = 0;
	for(i = 0; i < 3; i++) {
		totalUnreadMail += unreadMails[i];
		mailStrings[i] = NULL;
	}

	if(totalUnreadMail != 0) {

		for(i = 0; i < 3; i++) {
			if(unreadMails[i] != 0) {
				mailStrings[i] = smprintf("%s[ %s%c %s| %s%d %s]", colcyan, mailcolor[i], mailnames[i], colcyan, colbyellow, unreadMails[i], colcyan);
			} else {
				mailStrings[i] = smprintf("");
			}
		}

		mailCount = smprintf("%s%s%s", mailStrings[0], mailStrings[1], mailStrings[2]);

	} else {

		//No mail
		mailCount = smprintf("%s[%s%s%s]", colcyan, colgreen, envelope, colcyan);

	}
	
	for(i = 0; i < 3; i++) {
		if(mailStrings[i] != NULL) { free(mailStrings[i]); }
	}
	free(mailStrings);

	return mailCount;

	/*
	   return smprintf("%s[%ss%s|%s%d%s]%s[%sn%s|%s%d%s]%s[%soo%s|%s%d%s]",
			colcyan,colyellow,colcyan,mailcolor[0],unreadMails[0],colcyan,
			colcyan,colyellow,colcyan,mailcolor[1],unreadMails[1],colcyan,
			colcyan,colyellow,colcyan,mailcolor[2],unreadMails[2],colcyan);
	*/
}

int
parse_netdev(unsigned long long int *receivedabs, unsigned long long int *sentabs) {
	char buf[255];
	char *datastart;
	static int bufsize;
	int rval;
	FILE *devfd;
	unsigned long long int receivedacc, sentacc;

	bufsize = 255;
	devfd = fopen("/proc/net/dev", "r");
	rval = 1;

	// Ignore the first two lines of the file
	fgets(buf, bufsize, devfd);
	fgets(buf, bufsize, devfd);

	while (fgets(buf, bufsize, devfd)) {
	    if ((datastart = strstr(buf, "lo:")) == NULL) {
		datastart = strstr(buf, ":");

		// With thanks to the conky project at http://conky.sourceforge.net/
		sscanf(datastart + 1, "%llu  %*d     %*d  %*d  %*d  %*d   %*d        %*d       %llu",\
		       &receivedacc, &sentacc);
		*receivedabs += receivedacc;
		*sentabs += sentacc;
		rval = 0;
	    }
	}

	fclose(devfd);
	return rval;
}

void
calculate_speed(char *speedstr, unsigned long long int newval, unsigned long long int oldval) {
	double speed;
	speed = (newval - oldval) / 1024.0;
	if (speed > 1024.0) {
	    speed /= 1024.0;
	    sprintf(speedstr, "%.3f M", speed);
	} else {
	    sprintf(speedstr, "%.2f K", speed);
	}
}

char *
get_netusage(unsigned long long int *rec, unsigned long long int *sent) {
	unsigned long long int newrec, newsent;
	newrec = newsent = 0;
	char downspeedstr[15], upspeedstr[15];

	if (parse_netdev(&newrec, &newsent)) {
	    fprintf(stdout, "Error when parsing /proc/net/dev file.\n");
	    exit(1);
	}

	calculate_speed(downspeedstr, newrec, *rec);
	calculate_speed(upspeedstr, newsent, *sent);

	*rec = newrec;
	*sent = newsent;
	
	return smprintf("%s[ %s↓ %s%s %s| %s↑ %s%s%s ]", 
			colcyan, colyellow, colbyellow, downspeedstr, 
			colcyan,
			colyellow, colbyellow, upspeedstr, colcyan);
}


int
runevery(time_t *ltime, int sec) {

	/* returns 1 if "sec" seconds elapsed and updates ltime */
	time_t now = time(NULL);

	if( difftime(now, *ltime) >= sec) {
		*ltime = now;
		return 1;
	} else {
		return 0;
	}
}

int
xkblayoutSetup() {

	// Turn off ignoring xkb extension, force its use
	XkbIgnoreExtension(False);

	char *dispName = "";
	int eventCode;
	int errorReturn;
	int majorVer = XkbMajorVersion;
	int minorVer = XkbMinorVersion;
	int reasonReturn;
	
	dpy = XkbOpenDisplay(dispName, &eventCode, &errorReturn, &majorVer, &minorVer, &reasonReturn);
	kbdDescPtr = XkbAllocKeyboard();

	return 0;
}


void
xkblayoutCleanup() {

	XkbFreeKeyboard(kbdDescPtr, 0, True);
	XCloseDisplay(dpy);

}

char *
getKeyboardLayout() {

	//Layout names are two char codes
	char activeLayout[3] = { '\0', '\0', '\0' };
	// At the moment, my system will only switch between four layouts max
	char *layoutColors[4] = { colgreen, colred, colyellow, colblack };

	// Grab name info
	XkbGetNames(dpy, XkbSymbolsNameMask, kbdDescPtr);

	// Get the actual symbol string, which looks like
	//    pc+us+ru:2+fr:3+de:4+.....
	//  where us, ru, fr, and de would be the available layouts, so we need to
	//  copy the two-letter layout code that is currently active
	// Layout 0 is at 0+3
	// Layout 1 is at 0+3+3
	// Layout 2 is at 0+3+3+5
	// Layout 3 is at 0+3+3+5+5

	// Alloc layoutSymbols
	char *layoutSymbols = strdup(XGetAtomName(dpy, kbdDescPtr->names->symbols));

	// Figure out which one is active and print it
	XkbStateRec xkbState;
	XkbGetState(dpy, XkbUseCoreKbd, &xkbState);
	int layoutOffset = xkbState.group*3 + 3;
	if(xkbState.group >= 2) {
		layoutOffset += (xkbState.group-1)*2;
	}
	// Copy the two-char code
	strncpy(activeLayout, &(layoutSymbols[layoutOffset]), 2);
	// print it
	//printf("%s\n", activeLayout);

	// clean up
	free(layoutSymbols);
	XkbFreeNames(kbdDescPtr, 0, True);

	return smprintf("%s[%s %s %s]", colcyan, layoutColors[(xkbState.group > 4 ? 4 : xkbState.group)], activeLayout, colcyan);

}



int
main(void) {
	char *status;
	char *tmchicago;
	char *alsavolume;
	char *mpdinfo;
	char *mailinfo;
	char *networkinfo;
	char *layoutinfo;
	static unsigned long long int rec, sent;
	time_t count5min = time(NULL);

	/*
	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}
	*/

	xkblayoutSetup();

	parse_netdev(&rec,&sent);
	mailinfo = getMailCount();
	for (;;sleep(1)) {
		/* Put together the pieces of our status */
		tmchicago = mktimes("%a, %b %d %I:%M", tzchicago);
		alsavolume = get_volume();
		mpdinfo = getmpdstat();
		networkinfo = get_netusage(&rec,&sent);
		layoutinfo = getKeyboardLayout();

		if(runevery(&count5min, 300)) {
			free(mailinfo);
			mailinfo = getMailCount();
		}

		/* Build the status string from our pieces */
		status = smprintf("%s %s %s %s %s[%s%s%%%s] %s[%s%s%s]", mpdinfo, networkinfo, mailinfo, layoutinfo, colcyan, colgreen, alsavolume, colcyan, colcyan, colyellow, tmchicago, colcyan);

		/* Send it to the wm for display */
		setstatus(status);

		/* Clean up our pieces */
		free(layoutinfo);
		free(tmchicago);
		free(alsavolume);
		free(mpdinfo);
		free(networkinfo);
		free(status);
	}

	free(mailinfo);

	xkblayoutCleanup();

	XCloseDisplay(dpy);

	return 0;
}


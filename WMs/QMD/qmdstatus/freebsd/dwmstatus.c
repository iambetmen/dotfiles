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

/* For MPD support */
#include <mpd/client.h>

/* For network support */
#include <net/if.h>
#include <ifaddrs.h>
char iface_towatch[4] = "re0";

/* sound */
#include <limits.h>
#include <sys/soundcard.h>
#include <fcntl.h>
static const char *snddevicenames[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;

/* Timezone */
char *tzchicago = "America/Chicago";

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
    
	int devmask = 0;
	int bar, foo;
	int rightvol, leftvol;
	char mixername[PATH_MAX] = "/dev/mixer";
	int baz = open(mixername, O_RDWR);
	char *retstr = NULL;

	ioctl(baz, SOUND_MIXER_READ_DEVMASK, &devmask);

	for(foo = 0; foo < SOUND_MIXER_NRDEVICES; foo++) {
		if(!((1 << foo) & devmask))
			continue;

		ioctl(baz, MIXER_READ(foo),&bar);

		if(strcmp(snddevicenames[foo],"vol") == 0) {
			leftvol = bar & 0x7f;
			rightvol = (bar >> 8) & 0x7f;

			if(retstr == NULL) {
				if(leftvol != rightvol) {
					retstr = smprintf("%3d:%d", leftvol, rightvol);
				} else {
					retstr = smprintf("%3d", leftvol);
				}
			}
		}
	}

	close(baz);

	return retstr;
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

	/* path to home directory */
	char *homedir = getenv("HOME");
	char *unreadFilename = NULL;
	/* Color of text indicating unread mail for each mailbox */
	char *mailcolor[3] = { colyellow, colyellow, colyellow };
	
	/* Filenames of our 3 mailboxes */
	char mboxes[3][6] = {"mbox1", "mbox2", "mbox3"};

	/* Number of unread mails on 3 accounts
	 * 0 - main account, 1 - backup account, 2 - junk account */
	int unreadMails[3] = {0, 0, 0};

	for(int i = 0; i < 3; i++) {
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
		if(unreadMails[i] != 0) { mailcolor[i] = colbyellow; } 
	}

	return smprintf("%s[%ss%s|%s%d%s]%s[%sn%s|%s%d%s]%s[%soo%s|%s%d%s]",
			colcyan,colyellow,colcyan,mailcolor[0],unreadMails[0],colcyan,
			colcyan,colyellow,colcyan,mailcolor[1],unreadMails[1],colcyan,
			colcyan,colyellow,colcyan,mailcolor[2],unreadMails[2],colcyan);
}

int
parse_netdev(unsigned long long int *receivedabs, unsigned long long int *sentabs) {

	// Largely taken from FreeBSD 10.2 netstat source in 'if.c'
	struct ifaddrs *ifap, *ifa;
	char *name;

	if(getifaddrs(&ifap) != 0) {
		return 1;
	}

	// Loop through available ifaddrs
	#define IFA_STAT(s) (((struct if_data *)ifa->ifa_data)->ifi_ ## s)
	for(ifa = ifap; ifa; ifa = ifa->ifa_next) {

		// Use AF_LINK to catch all traffic coming through link
		// Only care about AF_INET interfaces
		if(ifa->ifa_addr->sa_family == AF_INET) {

			name = ifa->ifa_name;
			//Is this the interface we're supposed to watch?
			if(strcmp(name,iface_towatch) == 0) {
				*receivedabs = IFA_STAT(ibytes);
				*sentabs = IFA_STAT(obytes);
			}

		}
	
	}

	return 0;
}

void
calculate_speed(char *speedstr, unsigned long long int newval, unsigned long long int oldval) {

	double speed;
	speed = (newval - oldval) / 1024.0;
	if (speed > 1024.0) {
	    speed /= 1024.0;
	    sprintf(speedstr, "%5.3f MB/s", speed);
	} else {
	    sprintf(speedstr, "%6.2f KB/s", speed);
	}

}

char *
get_netusage(unsigned long long int *rec, unsigned long long int *sent) {

	unsigned long long int newrec, newsent;
	newrec = newsent = 0;
	char downspeedstr[20], upspeedstr[20];

	if (parse_netdev(&newrec, &newsent)) {
	    fprintf(stdout, "Error fetching network traffic.\n");
	    exit(1);
	}

	calculate_speed(downspeedstr, newrec, *rec);
	calculate_speed(upspeedstr, newsent, *sent);

	*rec = newrec;
	*sent = newsent;
	
	return smprintf("%s[%sD %s%s %s| %sU %s%s%s]", 
			colcyan, colyellow, colred, downspeedstr, 
			colcyan,
			colyellow, colred, upspeedstr, colcyan);
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
main(void) {
	char *status;
	char *tmchicago;
	char *volume;
	char *mpdinfo;
	//char *mailinfo;
	char *networkinfo;
	static unsigned long long int rec, sent;
	//time_t count5min = time(NULL);

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "qmdstatus: cannot open display.\n");
		return 1;
	}

	parse_netdev(&rec,&sent);
	//mailinfo = getMailCount();
	for (;;sleep(1)) {
		/* Put together the pieces of our status */
		tmchicago = mktimes("%a, %b %d %I:%M", tzchicago);
		volume = get_volume();
		mpdinfo = getmpdstat();
		networkinfo = get_netusage(&rec,&sent);

		/*
		if(runevery(&count5min, 300)) {
			free(mailinfo);
			mailinfo = getMailCount();
		}
		*/

		/* Build the status string from our pieces */
		status = smprintf("%s %s %s[%s%s%s] %s[%s%s%s]", mpdinfo, networkinfo, colcyan, colgreen, volume, colcyan, colcyan, colyellow, tmchicago, colcyan);

		/* Send it to the wm for display */
		setstatus(status);

		/* Clean up our pieces */
		free(tmchicago);
		free(volume);
		free(mpdinfo);
		free(networkinfo);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}


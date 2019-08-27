/*
 * a simple urlopener
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>

#define NOFORK 0 /* if set we wont fork and execute programs */

#define LEN(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))
#define BUFF_SIZE 256 	/* size of malloc buffers (max program/extension list length) */
/*
 * maximum number of arguments for the program
 * the program will only attach ARG_LIMIT-2 arguments to your executed process, the rest are ignored
 * this is because one argument is saved for the url and one is saved for a NULL chraracter
 *
 * the program to execute itself is one argument
 */

#define ARG_LIMIT 20

char *programs[][2] =
{
	/*
	 * the file extensions and program to associate with them
	 * separate extensions using ','
	 */
	{"default", /* this is the default program */	"/usr/bin/qutebrowser"},
	{"jpg,jpeg,png,jpg:large,png:large,pnj",	"/usr/bin/feh"},
	{"gif,gifv,webm,mp4,mp3,wav,flac", 		"/usr/bin/mpv --loop --force-window=yes"},
	{"pdf",						"/usr/bin/mupdf"},
};

char *forceddomains[][2] =
{ 
	/* if the domain is listed here, the index into programs[x][]
	 * will be the number, don't add the protocol identifier
	 * the www subdomain is considered a different domain
	*/
	{"youtube.com",			"2"},
	{"m.youtube.com",		"2"},
	{"www.youtube.com",		"2"},
	{"youtu.be", 			"2"},
	{"v.redd.it", 			"2"},
	{"streamable.com",		"2"},
	{"www.streamable.com",		"2"},
	{"vimeo.com",			"2"},
	{"www.liveleak.com",		"2"},
	{"liveleak.com",		"2"},
	{"reddit.com",			"4"},
	{"www.reddit.com",		"4"},
	{"new.reddit.com",		"4"},
	{"old.reddit.com",		"4"},
	{"redd.it",			"4"},
	{"twitter.com",			"4"},
};

int
islink(char *url)
{
	/*
	 * terribly check if something has a "protocol://"
	 * if it does, we assume it is a link
	 */
	char *p = NULL;
	if ((p = strstr(url, "://")))
	{
		int s = (p-url);
		/*
		 * check if the number of characters before the '://' is >0 && <10 && contains a '.'
		 * 10 is a reasonable number plucked out of thin air, RFC 3986 does not state a maximum
		 * we would like to avoid false positives, not that it matters much
		 */
		if (s > 0 && s <= 10 && strchr(url, '.') != NULL)
			/* it's probably a link */
			return 1;
	}
	return 0;
}

void
upper(char *str)
{
	size_t i;
	for (i = 0; i < strlen(str); i++)
	{
		str[i] = toupper(str[i]);
	}
}

int
getext(char *url)
{
	/* 
	 * check if the extension of the url (if exists) is in our array
	 * if it is, return the index in the array, otherwise 0
	 */
	int i;
	int ret = 0;
	char *p = NULL;
	char *t, *buff;
	/* we don't want to modify the original url */
	char *curl = malloc(strlen(url)+1);
	if (curl == NULL)
	{
		perror("malloc");
		free(curl);
		return 0;
	}
	strcpy(curl, url);
	if ((p = strrchr(curl, '.')+1))
	{
		for (i = 0; i < LEN(programs); i++)
		{
			buff = malloc(BUFF_SIZE);
			if (buff == NULL)
			{
				perror("malloc");
				free(curl);
				return 0;
			}
			strncpy(buff, programs[i][0], BUFF_SIZE-1);
			t = strtok(buff, ",");
			upper(p);
			while (t != NULL)
			{
				upper(t);
				if (strcmp(t, p) == 0)
				{
					ret = i;
				}
				t = strtok(NULL, ",");
			}
			free(buff);
		}
	}
	free(curl);
	return ret;
}

int
checkforceddomains(char *url, int ext)
{
	/*
	 * check if the domain is one we should force to use a program
	 */
	
	/*
	 * checking process:
	 * domain is already probably a url
	 * check if first x characters after protocol identifier is in the list?
	 * return the number specified in the array
	*/
	int i;
	int ret = ext;
	char *p;
	char *tmp;
	char *buff = malloc(BUFF_SIZE);
	char *buff2 = malloc(BUFF_SIZE);
	if (buff == NULL || buff2 == NULL)
	{
		perror("malloc");
		goto exitdomaincheck; /* exit right away */
	}
	strncpy(buff, url, BUFF_SIZE-1);
	p = NULL;
	p = strstr(buff, "://");
	if (p == NULL)
		goto exitdomaincheck; /* exit right away */
	p += 3; /* get rid of the '://' */
	/*
	 * at this point we know:
	 * there exists '://'
	 * so we can tokenise a new buffer at '/' and _assume_ that is the domain
	 */
	strncpy(buff2, p, BUFF_SIZE-1);
	/* wow this is bad */
	tmp = strchr(buff2, '/');
	if (tmp == NULL) /* nothing specificed after the domain, just exit */
		goto exitdomaincheck;
	*(tmp) = '\0';
	/*
	 * now we need to loop through the array and check if there is a number to force
	 * we can reuse our first buffer and char pointer here
	 */
	for (i = 0; i < LEN(forceddomains); i++)
	{
		strncpy(buff, forceddomains[i][0], BUFF_SIZE-1);
		if (strcmp(buff, buff2) == 0)
		{
			if (NOFORK)
				printf("domain should be forced as %s\n", buff);
			ret = atoi(forceddomains[i][1]);
			goto exitdomaincheck;
		}
	}
exitdomaincheck:
	free(buff);
	free(buff2);
	return ret;
}

int
forkexecute(char *url)
{
	int z;
	char *buff;
	char *t;
	char *args[ARG_LIMIT];
	int ext = 0;
	ext = getext(url);
	/* check if the domain should be forced to a program */
	ext = checkforceddomains(url, ext);
	if (NOFORK)
		printf("program to run is: \"%s %s\"\n", programs[ext][1], url);
	if (NOFORK == 0)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			/* child process, we don't want to ignore signals */
			signal(SIGCHLD, SIG_DFL);
			/*
			 * we don't want std{out,err} to be associated with the terminal,
			 * but we also don't want to close it to avoid the file descriptors
			 * being re-used potentially leading to problems, so reopen them to /dev/null
			 */
			freopen("/dev/null", "w", stdout);
			freopen("/dev/null", "w", stderr);
			buff = malloc(BUFF_SIZE);
			if (buff == NULL)
			{
				perror("malloc");
				return -1;
			}
			/*
			 * the program we're calling might have arguments,
			 * so we tokenise the string and add each part to an array
			 * that we will use in execvp
			 */
			strncpy(buff, programs[ext][1], BUFF_SIZE-1);
			t = strtok(buff, " ");
			z = 0;
			while (t != NULL && z < ARG_LIMIT-2) /*save a position for the url and NULL */
			{
				args[z] = t;
				t = strtok(NULL, " ");
				z++;
			}
			args[z] = url;
			args[z+1] = (char *)0;
			execvp(args[0], args);
			_exit(1);
		}
		else if (pid == -1)
		{
			perror("fork");
			return -1;
		}
	}
	return 1;
}

int
main(int argc, char *argv[])
{
	int i;
	int link = 0;
	int ret = 1;
	/* we don't care about children */
	signal(SIGCHLD, SIG_IGN);
	for (i = 1; i < argc; i++)
	{
		/*
		 * loop through every argument, check if it is a link
		 * if we think it is a link, check if the extension exists in the array
		 * fork, execute
		 */
		link = islink(argv[i]);
		if (NOFORK)
			printf("%s is probably %s link\n", argv[i], (link == 1) ? "a" : "not a");
		if (link == 1)
		{
			ret = forkexecute(argv[i]);
			if (ret != 1)
				fprintf(stderr, "fork/execute failed for: %s", argv[i]);
		}
	}
	return 0;
}

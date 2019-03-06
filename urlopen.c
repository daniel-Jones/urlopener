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

#define NOFORK 1 /* if set we wont fork and execute programs */

#define LEN(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))
#define BUFFSIZE 256 /* size of malloc buffers (max program/extension list length) */

char *programs[][2] =
{
	{"default", /* this is the default program */	"/usr/bin/qutebrowser"},
	{"jpg,jpeg,png",				"/usr/bin/feh"},
	{"gif,gifv,webm,mp4,mp3,wav,flac", 		"/usr/bin/mpv --loop --force-window=yes"},
	{"pdf",						"/usr/bin/mupdf"}
};

char *forceddomains[][2] =
{
	/* if the odmain is listed here, the index into programs[][]
	 * will be the number, don't add the protocol identifier
	*/
	{"youtube.com",		"2"},
	{"youtu.be", 		"2"},
	{"streamable.com",	"2"}
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
		 * check if the number of characters before the '://' is >0 && <5 && contains a '.'
		 * 10 is a guess, i have no idea what the limit of a protocol identifier is
		 */
		if (s > 0 && s <= 10 && strchr(url, '.') != NULL)
			// it's probably a link
			return 1;
	}
	return 0;
}

int
getext(char *url)
{
	/* 
	 * check if the extension of the url (if exists) is in our array
	 * if it is, return the index on the array, otherwise 0
	 */
	 int ret = 0;
	char *p = NULL;
	if ((p = strrchr(url, '.')+1))
	{
		for (int i = 0; i < LEN(programs); i++)
		{
			char *buff = malloc(BUFFSIZE);
			if (buff == NULL)
				return 0;
			strncpy(buff, programs[i][0], BUFFSIZE-1);
			char *t = strtok(buff, ",");
			while (t != NULL)
			{
				if (strcmp(t, p) == 0)
				{
					ret = i;
				}
				t = strtok(NULL, ",");
			}
			free(buff);
		}
	}
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
	 * return the number in specified
	*/
	int ret = ext;
	char *buff = malloc(BUFFSIZE);
	char *buff2 = malloc(BUFFSIZE);
	if (buff == NULL || buff2 == NULL)
		goto exitdomaincheck; /* exit right away */
	strncpy(buff, url, BUFFSIZE-1);
	char *p = NULL;
	p = strstr(buff, "://");
	if (p == NULL)
		goto exitdomaincheck; /* exit right away */
	p += 3; /* get rid of the '://' */
	/*
	 * at this point we know:
	 * there exists '://'
	 * so we can tokenise a new buffer at '/' and _assume_ that is the domain
	 */
	strncpy(buff2, p, BUFFSIZE-1);
	// wow this is bad
	char *tmp = strchr(buff2, '/');
	if (tmp == NULL) /* nothing specificed after the domain, just exit */
		goto exitdomaincheck;
	*(tmp) = '\0';
	/*
	 * now we need to loop through the array and check if there is a number to force
	 * we can reuse our first buffer and char pointer here
	 */
	for (int i = 0; i < LEN(forceddomains); i++)
	{
		strncpy(buff, forceddomains[i][0], BUFFSIZE-1);
		if (strcmp(buff, buff2) == 0)
		{
			//printf("domain should be forced as %s\n", buff);
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
main(int argc, char *argv[])
{
	int link = 0;
	int ext = 0;
	// we don't care about children
	signal(SIGCHLD, SIG_IGN);
	for (int i = 1; i < argc; i++)
	{
		/*
		 * loop through every argument, check if it is a link
		 * if we think it is a link, check if the extension exists in the array
		 * fork, execute
		 */
		link = islink(argv[i]);
		//printf("%s is probably %s link\n", argv[i], (link == 1) ? "a" : "not a");
		if (link == 1)
		{
			ext = getext(argv[i]);
			// check if the domain should be forced to a program
			ext = checkforceddomains(argv[i], ext);
			printf("program to run is: \"%s %s\"\n", programs[ext][1], argv[i]);
			if (NOFORK == 0)
			{
				pid_t pid = fork();
				if (pid == 0)
				{
					// child process
					// close std{out,err}
					fclose(stdout);
					fclose(stderr);
					char *args[20];
					char *buff = malloc(BUFFSIZE);
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
					strncpy(buff, programs[ext][1], BUFFSIZE-1);
					char *t = strtok(buff, " ");
					int z = 0;
					while (t != NULL)
					{
						args[z] = t;
						t = strtok(NULL, " ");
						z++;
					}
					args[z] = argv[i];
					args[z+1] = (char *)0;
					execvp(args[0], args);
					_exit(1);
				}
				else if (pid == -1)
				{
					perror("fork error");
				}
			}
		}
	}
	return 0;
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <poll.h>

#include <pthread.h>
#include <signal.h>


#include "ixml.h"
#include "cfg.h"
#define MOD "CFG"
#include "mylog.h"
#include "defcfg.h"

#define DBG			LOGT
//#define DBG(...)	(void)0

const char prefix[] = "/tvh/cfg/";

static char *makepath(char *dst, const char *src)
{
	char *q = dst;
	const char *p = prefix;
	while (*p) *q++ = *p++;
	p = src;
	while (*p)
	{
		if (*p == ':') *q++ = '/';
		else *q++ = *p;
		p++;
	}
	*q = 0;
	return dst;
}
static char *makename(char *dst, const char *src)
{
	char *q = dst;
	const char *p = src+strlen(prefix);
	while (*p)
	{
		if (*p == '#')
		{
			*(q-1) = 0;
			break;
		}
		if (*p == '/') *q++ = ':';
		else *q++ = *p;
		p++;
	}
	*q = 0;
	return dst;
}

static char param[1024];

static void print(const char *fn, const char *val)
{
	FILE *f = fopen(fn, "w+t");
	if (f)
	{
		fprintf(f, "%s", val);
		fclose(f);
	}
	else
	{
		LOGW("import: failed to create file %s", fn);
	}
}
static int import(IXML_Node *node, const char *path)
{
	if (node == NULL)
	{
		LOGE("import: failed to find root node");
		return -1;
	}
	char fn[256];
	sprintf(fn, "%s/%s", path, ixmlNode_getNodeName(node));
	IXML_Node *text = ixmlNode_getFirstChild(node);
	if ((text && text->nodeType == eTEXT_NODE) || !text)
	{
		IXML_NamedNodeMap* attrs = ixmlNode_getAttributes(node);
		if (attrs && attrs->nodeItem)
		{
			mkdir(fn, 0644);
			char afn[256];
			for (; attrs && attrs->nodeItem; attrs = attrs->next)
			{
				sprintf(afn, "%s/%s", fn, ixmlNode_getNodeName(attrs->nodeItem));
				print(afn, ixmlNode_getNodeValue(attrs->nodeItem));
			}
			ixmlNamedNodeMap_free(attrs);
			sprintf(afn, "%s/#val", fn);
			if (text)
				print(afn, ixmlNode_getNodeValue(text));
			else
				print(afn, "");
		}
		else
		{
			if (text)
				print(fn, ixmlNode_getNodeValue(text));
			else
				print(fn, "");
		}
	}
	else
	{
		mkdir(fn, 0644);
		IXML_NodeList* nodes;
		for (nodes = ixmlNode_getChildNodes(node); nodes && nodes->nodeItem; nodes = nodes->next)
		{
			import(nodes->nodeItem, fn);
		}
		ixmlNodeList_free(nodes);
	}
	return 0;
}

static int export(const char *path, IXML_Node *node)
{
	if (node == NULL)
	{
		LOGE("export: failed to find root node");
		return -1;
	}
	char fn[256];
	DIR *d = opendir(path);
	if (d)
	{
		sprintf(fn, "%s/#val", path);
		FILE *f = fopen(fn, "rt");
		if (f)
		{
			char str[1024];
			if (fgets(str, 1023, f))
			{
				if (str[strlen(str)-1] == '\n') str[strlen(str)-1] = 0;
				IXML_Node *text = ixmlDocument_createTextNode(node->ownerDocument, str);
				if (text)
				{
					ixmlNode_appendChild(node, text);
				}
				else
				{
					LOGW("export: failed to create text node %s", str);
				}
			}
			fclose(f);
			struct dirent *de;
			while ((de = readdir(d)))
			{
				if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0 || strcmp(de->d_name, "#val") == 0) continue;
				sprintf(fn, "%s/%s", path, de->d_name);
				FILE *f = fopen(fn, "rt");
				if (f)
				{
					char str[1024];
					if (fgets(str, 1023, f))
					{
						if (str[strlen(str)-1] == '\n') str[strlen(str)-1] = 0;
						ixmlElement_setAttribute((IXML_Element *)node, de->d_name, str);
					}
					fclose(f);
				}
			}
			return 0;
		}
		struct dirent *de;
		while ((de = readdir(d)))
		{
			if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0 || strcmp(de->d_name, ".#") == 0) continue;
			sprintf(fn, "%s/%s", path, de->d_name);
			IXML_Node *temp = (IXML_Node *)ixmlDocument_createElement(node->ownerDocument, de->d_name);
			if (temp)
			{
				ixmlNode_appendChild(node, temp);
				export(fn, temp);
			}
			else
			{
				LOGW("export: failed to create node %s", de->d_name);
			}
		}
		closedir(d);
		return 0;
	}
	FILE *f = fopen(path, "rt");
	if (f)
	{
		char str[1024];
		if (fgets(str, 1023, f))
		{
			if (str[strlen(str)-1] == '\n') str[strlen(str)-1] = 0;
			IXML_Node *text = ixmlDocument_createTextNode(node->ownerDocument, str);
			if (text)
			{
				ixmlNode_appendChild(node, text);
			}
			else
			{
				LOGW("export: failed to create text node %s", str);
			}
		}
		else
		{
			LOGW("export: empty parameter");
		}
		fclose(f);
		return 0;
	}
	return -1;
}

int cfg_load(const char *fn)
{
	DBG("load(%s)", fn);
	// 1. Apply default configuration
	IXML_Document *pDoc = ixmlParseBuffer(DefCfg);
	if (pDoc == NULL)
	{
		LOGE("failed to parse DefCfg");
		return -1;
	}
	if (0 != import(ixmlNode_getFirstChild((IXML_Node *)pDoc), "/tvh"))
	{
		LOGE("failed to import default configuration");
		return -2;
	}
	ixmlDocument_free(pDoc);
	// 2. Load saved configuration
	pDoc = ixmlLoadDocument(fn);
	if (pDoc == NULL)
	{
		LOGW("failed to load /var/cfg.xml");
		return 1;
	}
	else
	{
		if (0 != import(ixmlNode_getFirstChild((IXML_Node *)pDoc), "/tvh"))
		{
			LOGE("failed to import saved configuration");
			return -3;
		}
		ixmlDocument_free(pDoc);
	}
	return 0;
}

int cfg_save(const char *fn)
{
	DBG("save(%s)", fn);
	// Save configuration
	IXML_Document *pDoc = ixmlParseBuffer("<?xml version=\"1.0\"?><cfg></cfg>");
	export("/tvh/cfg", ixmlNode_getFirstChild((IXML_Node *)pDoc));
	char *str = ixmlPrintDocument(pDoc);
	if (str)
	{
		FILE *cfg = fopen("/var/cfg.xml", "w+t");
		if (cfg)
		{
			fwrite(str, 1, strlen(str), cfg);
			fclose(cfg);
		}
		else
		{
			LOGE("failed to save /var/cfg.xml");
			return -2;
		}
		ixmlFreeDOMString(str);
	}
	else
	{
		LOGE("failed to print doc");
		return -1;
	}
	return 0;
}

static void touch(char *path)
{
	char *p = path+strlen(path);
	while (p != path+strlen(prefix)-2)
	{
		if (*p == '/' && *(p+1) != '#')
		{
			*p = 0;
			strcat(path, "/.#");
			FILE *f = fopen(path, "w+t");
			if (f) fclose(f);
		}
		p--;
	}
	
}

int cfg_add_param(const char *name)
{
	char path[256];
	touch(path);
	return 0;
}

int cfg_del_param(const char *name)
{
	char path[256];
	touch(path);
	return 0;
}

int cfg_get_list(const char *group, char *value, int len)
{
	if (NULL == value) return -1;
	char path[256];
	makepath(path, group);
	DIR *d = opendir(path);
	if (!d) return -2;
	value[0] = 0;
	struct dirent *de;
	while ((de = readdir(d)))
	{
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0 || strcmp(de->d_name, ".#") == 0) continue;
		if (strlen(value)+1+strlen(de->d_name) > len) break;
		strcat(value, de->d_name);
		strcat(value, ":");
	}
	closedir(d);
	if (value[strlen(value)-1] == ':') value[strlen(value)-1] = 0;
	return 0;
}

const char *cfg_get_param(const char *name)
{
	if (NULL == name) return NULL;
	char path[256];
	FILE *f = fopen(strcat(makepath(path, name), "/#val"), "rt");
	if (f == NULL)
	{
		f = fopen(makepath(path, name), "rb");
	}
	if (f == NULL) return NULL;
	param[fread(param, 1, 1023, f)] = 0;
	return param;
}

int cfg_put_param(const char *name, const char *value)
{
	if (NULL == name || NULL == value) return -1;
	char path[256];
	FILE *f = fopen(strcat(makepath(path, name), "/#val"), "w+t");
	if (f == NULL)
	{
		f = fopen(makepath(path, name), "w+b");
	}
	if (f == NULL) return -2;
	fwrite(value, 1, strlen(value), f);
	fflush(f);
	fclose(f);
	touch(path);
	return 0;
}

int cfg_get_str(const char *name, char *value, int len)
{
	const char *text = cfg_get_param(name);
	if (text && value && strncpy(value, text, len)) return 0;
	return -1;
}
int cfg_get_char(const char *name, char *value)
{
	const char *text = cfg_get_param(name);
	if (text && value && sscanf(text, "%hhd", value) == 1) return 0;
	return -1;
}
int cfg_get_short(const char *name, short *value)
{
	const char *text = cfg_get_param(name);
	if (text && value && sscanf(text, "%hd", value) == 1) return 0;
	return -1;
}
int cfg_get_int(const char *name, int *value)
{
	const char *text = cfg_get_param(name);
	if (text && value && sscanf(text, "%d", value) == 1) return 0;
	return -1;
}
int cfg_get_long(const char *name, long *value)
{
	const char *text = cfg_get_param(name);
	if (text && value && sscanf(text, "%ld", value) == 1) return 0;
	return -1;
}
int cfg_get_float(const char *name, float *value)
{
	const char *text = cfg_get_param(name);
	if (text && value && sscanf(text, "%f", value) == 1) return 0;
	return -1;
}
int cfg_get_addr(const char *name, struct in_addr *value)
{
	const char *text = cfg_get_param(name);
	if (text && inet_aton(text, value) != 0) return 0;
	return -1;
}

int cfg_put_str(const char *name, const char *value)
{
	return cfg_put_param(name, value);
}
int cfg_put_int(const char *name, int value)
{
	char text[16]; sprintf(text, "%d", value);
	return cfg_put_param(name, text);
}
int cfg_put_long(const char *name, long value)
{
	char text[16]; sprintf(text, "%ld", value);
	return cfg_put_param(name, text);
}
int cfg_put_float(const char *name, float value)
{
	char text[16]; sprintf(text, "%f", value);
	return cfg_put_param(name, text);
}
int cfg_put_addr(const char *name, struct in_addr value)
{
	char *text = inet_ntoa(value);
	return cfg_put_param(name, text);
}

/**********************************************************************************/

static int run = 0;
static pthread_t mon_thr;

static int fd = -1;
static struct
{
	int fd;
	char name[256];
	cfg_notify_t cb;
	int pd;
} watch[16];


static void *monitor(void *arg)
{
	struct inotify_event event[256];
	DBG("monitor thread started");
	while (run)
	{
		struct pollfd pfd = {fd, POLLIN, 0};
		int ret = poll(&pfd, 1, 10);
		if (ret < 0)
		{
			LOGE("failed to poll inotify");
			break;
		}
		else if (ret == 0)
		{
			continue;
		}
		int num = read(fd, event, 256*sizeof(struct inotify_event))/sizeof(struct inotify_event);
		int i;
		for (i = 0; i < num; i++)
		{
			if (event[i].mask &(IN_DELETE|IN_MODIFY))
			{
				int j;
				for (j = 0; j < 16; j++)
					if (event[i].wd == watch[j].fd)
						if (watch[j].cb)
							watch[j].cb(watch[j].pd);
			}
		}
	}
	DBG("monitor thread stopped");
	return NULL;
}

int cfg_subscribe(const char *name, cfg_notify_t cb, int pd)
{
	if (!name) return -1;
	DBG("subscribe(%s, %x, %d)", name, cb, pd);
	int i;
	if (cb)
	{
		if (fd < 0)
		{
			for (i = 0; i < 16; i++) watch[i].fd = -1;
			fd = inotify_init();
			if (fd < 0)
			{
				LOGE("failed to init inotify");
				return -2;
			}
			run = 1;
			if (pthread_create(&mon_thr, NULL, monitor, NULL))
			{
				LOGE("failed to create monitor thread");
				return 0;
			}
		}
		for (i = 0; i < 16; i++) if (watch[i].fd < 0) break;
		if (i >= 16) 
		{
			LOGE("no empty watch slot");
			return -2;
		}
		char path[256];
		strcpy(watch[i].name, name);
		watch[i].cb = cb;
		watch[i].pd = pd;
		watch[i].fd = inotify_add_watch(fd, makepath(path, name), IN_DELETE|IN_MODIFY);
	}
	else
	{
		int empty = 1;
		for (i = 0; i < 16; i++)
		{
			if (strcmp(name, watch[i].name) == 0 || strcmp(name, "*") == 0)
			{
				inotify_rm_watch(fd, watch[i].fd);
				watch[i].fd = -1;
			}
			if (watch[i].fd >= 0) empty = 0;
		}
		if (empty)
		{
			run = 0;
			pthread_join(mon_thr, NULL);
			close(fd);
			fd = -1;
		}
	}
	return 0;
}


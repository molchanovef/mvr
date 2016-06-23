#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/tcp.h>

#include "msg.h"
#define MOD "MSG"
#include "mylog.h"

//#define DBG			LOGT
#define DBG(...)	(void)0

#define CLIENTS_MAX	8
#define SERVERS_MAX	8
#define PORT		9001

static int inited = 0;
static struct
{
	unsigned short id;
	pthread_t thread;
	int socket;
	struct
	{
		int socket;
		pthread_t thread;
		char name[16];
	} client[CLIENTS_MAX];
} server;
static struct
{
	unsigned short id;
	int socket;
} client[SERVERS_MAX];
static msg_callback_t cb = NULL;
static void *server_fn(void *arg);
static void *client_fn(void *arg);

int msg_start_server(unsigned short id, msg_callback_t callback)
{
	if (!inited)
	{
		memset(&server, 0, sizeof(server));
		memset(client, 0, sizeof(client)*SERVERS_MAX);
		inited = 1;
	}
	if (server.socket) return 0;

	server.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server.socket < 0)
	{
		LOGE("Failed to create server socket");
		return -1;
	}
	unsigned int reuse = 1;
	if (setsockopt(server.socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(unsigned int)) < 0)
	{
		LOGW("Failed to set SO_REUSEADDR");
	}
	int keepalive = 1;
	if (setsockopt(server.socket, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(unsigned int)) < 0)
	{
		LOGW("Failed to set SO_KEEPALIVE");
	}
	int tcpnodelay = 1;
	if (setsockopt(server.socket, SOL_SOCKET, TCP_NODELAY, &tcpnodelay, sizeof(unsigned int)) < 0)
	{
		LOGW("Failed to set TCP_NODELAY");
	}
	struct sockaddr_in sa = {0};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = htons(PORT+id);
	if (bind(server.socket, (struct sockaddr *)&sa, sizeof(sa)) < 0) 
	{
		LOGE("Failed to bind server socket %d", server.socket);
		close(server.socket);
		server.socket = 0;
		return -1;
	}
	if (listen(server.socket, 5) < 0)
	{
		LOGE("Failed to listen on server socket");
		close(server.socket);
		server.socket = 0;
		return -1;
	}
	if (pthread_create(&server.thread, NULL, server_fn, callback))
	{
		LOGE("Failed to create server thread");
		close(server.socket);
		server.socket = 0;
		return -1;
	}
	pthread_detach(server.thread);
	server.id = id;
	cb = callback;
	return 0;
}

int msg_stop_server()
{
	if (!server.socket) return 1;

	if (server.thread) pthread_cancel(server.thread);
	if (server.socket) close(server.socket);
	int i; for (i = 0; i < CLIENTS_MAX; i++)
	{
		if (server.client[i].thread) pthread_cancel(server.client[i].thread);
		if (server.client[i].socket) close(server.client[i].socket);
	}
	memset(&server, 0, sizeof(server));
	cb = NULL;

	return 0;
}

int msg_start_client(unsigned short id, const char *name)
{
	if (!inited)
	{
		memset(&server, 0, sizeof(server));
		memset(client, 0, sizeof(client)*SERVERS_MAX);
		inited = 1;
	}
	if (id >= SERVERS_MAX) return -1;
	if (client[id].socket) return 0;

	client[id].socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client[id].socket < 0)
	{
		LOGE("Failed to create socket");
		return -1;
	}
	unsigned int reuse = 1;
	if (setsockopt(client[id].socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(unsigned int)) < 0)
	{
		LOGW("Failed to set SO_REUSEADDR");
	}
	int keepalive = 1;
	if (setsockopt(client[id].socket, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(unsigned int)) < 0)
	{
		LOGW("Failed to set SO_KEEPALIVE");
	}
	int tcpnodelay = 1;
	if (setsockopt(client[id].socket, SOL_SOCKET, TCP_NODELAY, &tcpnodelay, sizeof(unsigned int)) < 0)
	{
		LOGW("Failed to set TCP_NODELAY");
	}
	struct sockaddr_in sa = {0};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(PORT+id);
	while (connect(client[id].socket, (struct sockaddr *)&sa, sizeof(sa)) < 0)
	{
		close(client[id].socket);
		client[id].socket = 0;
		LOGE("Failed to connect to server");
		return -1;
	}
	DBG("Client connected");
	
	client[id].id = id;

	char str[256]; sprintf(str, "<?xml version=\"1.0\"?>\n<CONNECT>%s</CONNECT>\n\n", name);
	msg_send(id, str); usleep(10000);

	return 0;
}

int msg_stop_client(unsigned short id)
{
	if (id >= SERVERS_MAX) return -1;
	if (client[id].socket == 0) return 1;
	close(client[id].socket);
	client[id].socket = 0;

	return 0;
}

int msg_send(unsigned short id, const char *buf)
{
	if (id >= SERVERS_MAX) return -1;
	if (!client[id].socket)
	{
		LOGE("No socket");
		return -1;
	}
	int ret = send(client[id].socket, buf, strlen(buf), 0);
	if (ret < 0)
	{
		LOGE("Failed to write to socket");
		return -1;
	}
	if (ret != strlen(buf))
	{
		LOGW("Incomplete write to socket");
	}
	return ret;
}

int msg_recv(unsigned short id, char *buf, int len)
{
	if (id >= SERVERS_MAX) return -1;
	if (!client[id].socket)
	{
		LOGE("No socket");
		return -1;
	}
	int ret = recv(client[id].socket, buf, len, 0);
	if (ret < 0) return ret;
	buf[ret] = '\0';
	DBG("Received %d bytes: \"%s\"", ret, buf);
	return ret;
}

static int negotiate(unsigned short id, char *buf)
{
	if (!inited) return -100;
	if (id >= SERVERS_MAX) return -200;
DBG("%s", buf);
	if (id == server.id && cb)
	{	// local call
		xmlDoc *req = ixmlParseBuffer(buf);
		if (req == NULL)
		{
			LOGE("Failed to parse \"%s\"", buf);
			return -1;
		}
		xmlDoc *ans = ixmlDocument_createDocument();
		cb(req, ans);
		char *tmp = ixmlPrintDocument(ans);
		if (tmp)
		{
			strcpy(buf, tmp);
			free(tmp);
		}
		else
		{
			buf[0] = '\0';
		}
		ixmlDocument_free(ans);
		ixmlDocument_free(req);
	}
	else
	{	// remote call
		if (msg_send(id, buf) != strlen(buf)) return -2;
		int len = msg_recv(id, buf, 8191);
		if (len < 0) return -3;
		buf[len] = '\0';
	}
DBG("%s", buf);
	if (strstr(buf, "<ERR>"))
	{
		LOGE("%s", buf);
		return -4;
	}
	else if (strstr(buf, "<WRN>"))
	{
		LOGW("%s", buf);
		return -5;
	}
	return 0;
}

char *msg_cmd(unsigned short id, const char *cmd, const char *arg)
{
	char buf[8192];
	sprintf(buf, "<?xml version=\"1.0\"?><%s>%s</%s>\n\n", cmd, arg, cmd);
	if (negotiate(id, buf) < 0) return NULL;

	char str[1024];
	char *ret = NULL;
	xmlDoc *doc = ixmlParseBuffer(buf);
	if (doc)
	{
		const char *val = xmlGetNodeValue(xmlGetRootNode(doc));
		if (val)
		{
			strncpy(str, val, 1023);
			ret = str;
		}
		ixmlDocument_free(doc);
	}
	else
	{
		LOGE("Failed to parse XML answer");
	}
LOGI("CMD \"%s\" \"%s\"", cmd?cmd:"(null)", ret?ret:"(null)");
	return ret;
}

char *msg_get(unsigned short id, const char *name)
{
	char buf[8192];
	sprintf(buf, "<?xml version=\"1.0\"?>\n<GET>%s</GET>\n\n", name);
	if (negotiate(id, buf) < 0) return NULL;

	char str[1024];
	char *ret = NULL;
	xmlDoc *doc = ixmlParseBuffer(buf);
	if (doc)
	{
		const char *val = xmlGetNodeValue(xmlGetRootNode(doc));
		strncpy(str, val?val:"", 1023);
		ret = str;
		ixmlDocument_free(doc);
	}
	else
	{
		LOGE("Failed to parse XML answer");
	}
LOGI("GET \"%s\" \"%s\"", name?name:"(null)", ret?ret:"(null)");
	return ret;
}

int msg_put(unsigned short id, const char *name, const char *val)
{
	char buf[8192];
	sprintf(buf, "<?xml version=\"1.0\"?>\n<PUT><%s>%s</%s></PUT>\n\n", name, val, name);
	if (negotiate(id, buf) < 0) return -1;

LOGI("PUT %s \"%s\"", name, val);
	return 0;
}






static void *server_fn(void *arg)
{
	DBG("Server started");
	while (1)
	{
		int i;
		int is_able_to_accept = 0;
		for (i = 0; i < CLIENTS_MAX; i++)
		{
			if (server.client[i].thread == 0)
			{
				is_able_to_accept = 1;
				break;
			}
		}
		if (!is_able_to_accept)
		{
			LOGW("Too many clients");
			sleep(1);
			continue;
		}
		
		struct sockaddr_in sa = {0};
		int sal = sizeof(sa);
		server.client[i].socket = accept(server.socket, (struct sockaddr *)&sa, &sal);
		if (server.client[i].socket < 0)
		{
			LOGW("Failed to accept client");
			continue;
		}
		if (pthread_create(&server.client[i].thread, NULL, client_fn, (void *)i))
		{
			LOGW("Failed to create client thread");
			continue;	// FIXME
		}
		pthread_detach(server.client[i].thread);
	}
	close(server.socket);
	server.socket = 0;
	server.thread = 0;
	return NULL;
}

static void *client_fn(void *arg)
{
	int i = (int)arg;
	char buf[8192];
	int len = 0;
	DBG("Client connected");
	do
	{
		len = recv(server.client[i].socket, buf, 8191, 0);
		buf[len] = 0;
		DBG("Received from %s %d bytes: \"%s\"", server.client[i].name, len, buf);
		if (len <= 0) break;
		xmlDoc *req = ixmlParseBuffer(buf);
		if (req)
		{
			xmlNode *root = xmlGetRootNode(req);
			if (root)
			{
				if (strcmp(xmlGetNodeName(root), "CONNECT") == 0)
				{
					strncpy(server.client[i].name, xmlGetNodeValue(root), 15);
					LOGI("%s connected", server.client[i].name);
				}
				else
				{
					xmlDoc *ans = ixmlDocument_createDocument();
					if (ans)
					{
						if (cb) cb(req, ans);
						char *buf = ixmlPrintDocument(ans);
						if (buf)
						{
							send(server.client[i].socket, buf, strlen(buf), 0);
							free(buf);
						}
						else
						{
							LOGE("Failed to print XML answer");
							send(server.client[i].socket, "\n\n", 2, 0);
						}
						ixmlDocument_free(ans);
					}
					else
					{
						LOGE("Failed to create XML document");
						send(server.client[i].socket, "\n\n", 2, 0);
					}
				}
			}
			else
			{
				LOGE("Failed to get root element");
				send(server.client[i].socket, "\n\n", 2, 0);
			}
			ixmlDocument_free(req);
		}
		else
		{
			LOGE("Failed to parse XML request");
			send(server.client[i].socket, "\n\n", 2, 0);
		}
	} while (len > 0);
	close(server.client[i].socket);
	server.client[i].socket = 0;
	server.client[i].thread = 0;
	return NULL;
}

xmlNode *xmlGetRootNode(xmlDoc *doc)
{
	if (doc == NULL) return NULL;
	return ixmlNode_getFirstChild((xmlNode *)doc);
}
xmlNode *xmlGetNode(xmlNode *node, const char *name)
{
	if (node == NULL || name == NULL) return NULL;
	xmlNode *ret = NULL;
	IXML_NodeList* nodes;
	for (nodes = ixmlNode_getChildNodes(node); nodes && nodes->nodeItem; nodes = nodes->next)
	{
		if (strcmp(ixmlNode_getNodeName(nodes->nodeItem), name) == 0) 
		{
			ret = nodes->nodeItem;
			break;
		}
	}
	ixmlNodeList_free(nodes);
	if (ret == NULL)
	{	// try attributes
		IXML_NamedNodeMap* attrs;
		for (attrs = ixmlNode_getAttributes(node); attrs && attrs->nodeItem; attrs = attrs->next)
		{
			if (strcmp(ixmlNode_getNodeName(attrs->nodeItem), name) == 0) 
			{
				ret = attrs->nodeItem;
				break;
			}
		}
		ixmlNamedNodeMap_free(attrs);
	}
	return ret;
}
xmlNode *xmlGetNextNode(xmlNode *root, xmlNode *node)
{
	if (root == NULL) return NULL;
	xmlNode *ret = NULL;
	IXML_NodeList* nodes;
	for (nodes = ixmlNode_getChildNodes(root); nodes; nodes = nodes->next)
	{
		if (node == NULL) 
		{
			ret = nodes->nodeItem;
			break;
		}
		if ((node = nodes->nodeItem) && nodes->next)
		{
			ret = nodes->next->nodeItem;
			break;
		}
	}
	ixmlNodeList_free(nodes);
	return ret;
}
xmlNode *xmlGetChildNode(xmlNode *node, unsigned num)
{
	if (node == NULL) return NULL;
	xmlNode *ret = NULL;
	IXML_NodeList* nodes = ixmlNode_getChildNodes(node);
	while (nodes)
	{
		if (num-- == 0)
		{
			ret = nodes->nodeItem;
			break;
		}
		nodes = nodes->next;
	}
	ixmlNodeList_free(nodes);
	return ret;
}
const char *xmlGetNodeName(xmlNode *node)
{
	if (node == NULL) return NULL;
	return ixmlNode_getNodeName(node);
}
const char *xmlGetNodeValue(xmlNode *node)
{
	if (node == NULL) return NULL;
	if (node->nodeType == eELEMENT_NODE)
	{
		xmlNode *text = ixmlNode_getFirstChild(node);
		if (text && text->nodeType == eTEXT_NODE)
			return ixmlNode_getNodeValue(text);
		else
			return ixmlNode_getNodeName(node);
	}
	else if (node->nodeType == eATTRIBUTE_NODE)
	{
		return ixmlNode_getNodeValue(node);
		
		}
	else return NULL;
}
xmlNode *xmlPutNodeValue(xmlNode *node, const char *value)
{
	if (node == NULL || value == NULL) return NULL;
	xmlNode *ret = NULL;
	if (node->nodeType == eELEMENT_NODE)
	{
		ret = ixmlNode_getFirstChild(node);
		if (ret)
		{
			ixmlNode_setNodeValue(ret, value);
		}
		else
		{
			ret = ixmlDocument_createTextNode(node->ownerDocument, value);
			if (ret)
			{
				ixmlNode_appendChild(node, ret);
			}
		}
	}
	else if (node->nodeType == eATTRIBUTE_NODE)
	{
		// TODO
	}
	return ret;
}
xmlNode *xmlAddNode(xmlDoc *doc, const char *name)
{
	if (doc == NULL || name == NULL) return NULL;
	char path[256];
	strcpy(path, name);
	char *p = strtok(path, ":");
	xmlNode *node = ixmlNode_getFirstChild((IXML_Node *)doc);
	if (node == 0) node = (xmlNode *)doc;
	while (p && node)
	{
		xmlNode *temp = xmlGetNode(node, p);
		if (temp == NULL)
		{
			temp = (xmlNode *)ixmlDocument_createElement(doc, p);
			if (temp) ixmlNode_appendChild(node, temp);
		}
		node = temp;
		p = strtok(NULL, ":");
	}
	return node;
}
int xmlDelNode(xmlDoc *doc, const char *name)
{
	xmlNode *node = xmlFindNode(doc, name);
	if (node == NULL) return 1;
	xmlNode *removed = NULL;
	if (IXML_SUCCESS == ixmlNode_removeChild(node->parentNode?node->parentNode:(xmlNode *)node->ownerDocument, node, &removed))
	{
		if (removed) ixmlNode_free(removed);
		return 0;
	}
	return -1;
}
xmlNode *xmlFindNode(xmlDoc *doc, const char *name)
{
	if (doc == NULL || name == NULL) return NULL;
	xmlNode *node = ixmlNode_getFirstChild((IXML_Node *)doc);
	char path[256];
	strcpy(path, name);
	char *p = strtok(path, ":");
	while (p && node)
	{
		int n;
		if (sscanf(p, "%d", &n) == 1)
		{
			node = xmlGetChildNode(node, n);
		}
		else
		{
			node = xmlGetNode(node, p);
		}
		p = strtok(NULL, ":");
	}
	return node;
}
const char *xmlGetParam(xmlDoc *doc, const char *name)
{
	return xmlGetNodeValue(xmlFindNode(doc, name));
}
xmlNode *xmlPutParam(xmlDoc *doc, const char *name, const char *value)
{
	return xmlPutNodeValue(xmlFindNode(doc, name), value);
}


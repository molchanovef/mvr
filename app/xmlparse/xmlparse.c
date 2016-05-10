/*
   Simple test with libxml2 <http://xmlsoft.org>. It displays the name
   of the root element and the names of all its children (not
   descendents, just children).

   On Debian, compiles with:
   gcc -Wall -o read-xml2 $(xml2-config --cflags) $(xml2-config --libs) \
                    read-xml2.c    

*/

#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>

int
main(int argc, char **argv)
{
    xmlDoc         *document;
    xmlNode        *root, *first_child, *node;
    char           *filename;

    if (argc < 2) {
    	fprintf(stderr, "Usage: %s filename.xml\n", argv[0]);
    	return 1;
    }
    filename = argv[1];

	xmlDocPtr    doc;
	xmlNodePtr   cur;
	xmlChar      *uri;

	doc = xmlParseFile(filename);
	cur = xmlDocGetRootElement(doc);
    fprintf(stdout, "Root is <%s> (%i)\n", cur->name, cur->type);

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
//    	fprintf(stdout, "Child is <%s> (%i)\n", cur->name, cur->type);
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"Camera"))) {
			printf("\tCamera:\n");
			uri = xmlGetProp(cur, "name");
			printf("\t\tname: %s\n", uri);
			uri = xmlGetProp(cur, "url");
			printf("\t\turl: %s\n", uri);
			uri = xmlGetProp(cur, "latency");
			printf("\t\tlatency: %s\n", uri);
			uri = xmlGetProp(cur, "position");
			printf("\t\tposition: %s\n", uri);
			xmlFree(uri);
		}
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"WiFi"))) {
			printf("\tWiFi:\n");
			uri = xmlGetProp(cur, "ssid");
			printf("\t\tssid: %s\n", uri);
			uri = xmlGetProp(cur, "password");
			printf("\t\tpassword: %s\n", uri);
			xmlFree(uri);
		}
		cur = cur->next;
	}
	xmlFreeDoc(doc);
	return (1);
/*
    document = xmlReadFile(filename, NULL, 0);
    root = xmlDocGetRootElement(document);
    fprintf(stdout, "Root is <%s> (%i)\n", root->name, root->type);
    first_child = root->children;
    for (node = first_child; node; node = node->next) {
    	fprintf(stdout, "\t Child is <%s> (%i)\n", node->name, node->type);
    	xmlNode *sec_child, *nnode;
    	sec_child = node->children;
    	for(nnode = sec_child; nnode; nnode = nnode->next)
    	    	fprintf(stdout, "\t sec Child is <%s> (%i)\n", nnode->name, nnode->type);
    }
    fprintf(stdout, "...\n");
    return 0;*/
}

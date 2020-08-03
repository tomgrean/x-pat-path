/*
 * test.c
 * test program examples.
 *      Author: tomgrean at github dot com
 */

#include "xpatpath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if 0
int fd_feeder(void *vfd, char *buffer, int maxlen)
{
	int fd = (int)vfd;
	return (int)read(fd, buffer, maxlen);
}
#endif
static int cfile_feeder(void *vf, char *buffer, int maxlen)
{
	FILE *fp = vf;
	return (int) fread(buffer, 1, maxlen, fp);
}

static void traverseXML(struct XmlRoot *r)
{
	int i;
	for (i = 0; i < r->nodeUsed; ++i) {
		if (r->node[i].content) {
			printf("<%s> = %s\n", r->node[i].nodeName, r->node[i].content);
		}
	}
}
int test1()
{
	struct XmlRoot r;
	struct XmlParseParam xpp;
	FILE *pf;

	printf("==============function: %s\n", __func__);
	pf = fopen("testxml/web.xml", "rb");
	if (!pf) {
		return 1;
	}
	memset(&xpp, 0, sizeof(xpp));
	xpp.userData = pf;
	xpp.xmlNodeNum = 3067;

	printf("hello test\n");
	int ret = loadXML(&r, cfile_feeder, &xpp);
	printf("parse xml ret=%d\n", ret);
	fclose(pf);
	dumpXMLData(r.node);

	traverseXML(&r);

	printf("-----search on a node:\n");

	struct XmlNode *xn = getOneNodeByPath(r.node, "/web-app/servlet[servlet-name='jsp']/servlet-class");
	if (xn) {
		printf("value=%s\n", xn->content);
	}
	printf("xmlnodes used=%d allocated=%d\n", r.nodeUsed, r.nodeSize);
	freeXML(&r);

	return 0;
}

static const char *checker[] = {"web-app", "welcome-file-list", NULL};
static int xpathfilter(const char **path, int plen, const char **attr)
{
	int i;
	for (i = 0; i < plen && checker[i]; ++i) {
		if (strcmp(checker[i], path[i])) {
			return 1;
		}
	}
	return 0;
}
int test2()
{
	struct XmlRoot r;
	struct XmlParseParam xpp;
	FILE *pf;

	printf("==============function: %s\n", __func__);
	pf = fopen("testxml/web.xml", "rb");
	if (!pf) {
		return 1;
	}
	memset(&xpp, 0, sizeof(xpp));
	xpp.userData = pf;
	xpp.xmlNodeNum = 10;
	xpp.filterNode = xpathfilter;

	printf("hello test\n");
	int ret = loadXML(&r, cfile_feeder, &xpp);
	printf("parse xml ret=%d\n", ret);
	fclose(pf);
	dumpXMLData(r.node);

	printf("xmlnodes used=%d allocated=%d\n", r.nodeUsed, r.nodeSize);
	freeXML(&r);
	return 0;
}
int test3()
{
	struct XmlRoot r;
	struct XmlParseParam xpp;
	FILE *pf;

	printf("==============function: %s\n", __func__);
	pf = fopen("testxml/web.xml", "rb");
	if (!pf) {
		return 1;
	}
	memset(&xpp, 0, sizeof(xpp));
	xpp.userData = pf;
	xpp.xmlNodeNum = 10;
	xpp.filterNode = xpathfilter;

	printf("hello test3\n");
	int ret = loadXML(&r, cfile_feeder, &xpp);
	printf("parse xml ret=%d\n", ret);
	fclose(pf);
	dumpXMLData(r.node);

	printf("xmlnodes used=%d allocated=%d\n", r.nodeUsed, r.nodeSize);
	struct XmlNode *pn = addToChild(&r, r.node, "test_new_tag", "very short test content");
	if (pn) {
		const char *keys[] = {"Attr1", "Number", NULL};
		const char *vals[] = {"Value1", "2", NULL};
		ret = setNodeAttribute(pn, keys, vals);
		printf("add Attribute result=%d\n", ret);
	}
	pn = addToChild(&r, r.node, "new_tag_with_Child", NULL);
	if (pn) {
		addToChild(&r, pn, "new_tag_child", "Child Content");
		addToChild(&r, pn, "new_tag_child", NULL);
	}
	pn = getOneNodeByPath(r.node, "/web-app/welcome-file-list/welcome-file[2]");
	ret = deleteNode(&r, pn);
	printf("deleteNode result=%d\n", ret);
	dumpXMLData(r.node);
	freeXML(&r);
	return 0;
}


int main()
{
	test1();
	test2();
	test3();
	return 0;
}

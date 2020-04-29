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
int cfile_feeder(void *vf, char *buffer, int maxlen)
{
	FILE *fp = vf;
	return (int) fread(buffer, 1, maxlen, fp);
}

void traverseXML(struct XmlRoot *r)
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
	//dumpXMLData(r.node);

	traverseXML(&r);


	struct XmlNode *xn = getOneNodeByPath(r.node, "/web-app/servlet[servlet-name='jsp']/servlet-class");
	if (xn) {
		printf("xmlnodes used=%d allocated=%d\n value=%s\n", r.nodeUsed, r.nodeSize, xn->content);
		//printf("attr[files] = %s\n", getNodeAttribute(xn, "files"));
	}
	freeXML(&r);

	return 0;
}

int main()
{
	test1();
	return 0;
}

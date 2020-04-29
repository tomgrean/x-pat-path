/*
 * xpatpath.c
 *
 *  Created on: Jan 12, 2018
 *      Author: tomgrean at github dot com
 */


#include "xpatpath.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "libexpat/expat.h"

#define XML_BUFF_SIZE 1024
#define NODE_INIT_LEN 64

struct expat_user_data {
	struct XmlNode *currentNode;
	struct XmlRoot *root;
};

static void relocatePointers(struct XmlNode *pnew, struct expat_user_data *d)
{
	int i;
	intptr_t pdelta;
	if (pnew == d->root->node) {
		// luckily, no need to relocate.
		return;
	}
	pdelta = (intptr_t)pnew - (intptr_t)(d->root->node);
	d->currentNode = (struct XmlNode*)((intptr_t)(d->currentNode) + pdelta);
	for (i = 0; i < d->root->nodeUsed; ++i) {
		if (pnew[i].parent) {
			pnew[i].parent = (struct XmlNode*)((intptr_t)(pnew[i].parent) + pdelta);
		}
		if (pnew[i].children) {
			struct NodeChildren *pc;
			for (pc = pnew[i].children; pc; pc = pc->next) {
				pc->me = (struct XmlNode*)((intptr_t)(pc->me) + pdelta);
			}
		}
	}
}

static void addChildren(struct XmlNode *node, struct XmlNode *child)
{
	struct NodeChildren *newChild;

	newChild = malloc(sizeof(struct NodeChildren));
	memset(newChild, 0, sizeof(struct NodeChildren));
	newChild->me = child;

	if (node->children) {
		struct NodeChildren *pc = node->children;

		while (pc->next) {
			pc = pc->next;
		}
		pc->next = newChild;
	} else {
		node->children = newChild;
	}
}
static void XMLCALL thestart(void *data, const XML_Char *el, const XML_Char **attr)
{
	int i;
	struct expat_user_data *d = data;

	if (d->root->nodeUsed >= d->root->nodeSize) {
		struct XmlNode *temp = realloc(d->root->node, (d->root->nodeSize + NODE_INIT_LEN) * sizeof(struct XmlNode));
		if (temp) {
			relocatePointers(temp, d);
			d->root->node = temp;
			d->root->nodeSize = d->root->nodeSize + NODE_INIT_LEN;
		} else {
			//something really bad.
			loge("I gonna die.no memory!\n");
			exit(1);
		}
	}
	memset(&d->root->node[d->root->nodeUsed], 0, sizeof(struct XmlNode));
	// set parent relations
	d->root->node[d->root->nodeUsed].parent = d->currentNode;
	if (d->currentNode) {
		addChildren(d->currentNode, &d->root->node[d->root->nodeUsed]);
	}
	// set currentNode
	d->currentNode = &d->root->node[d->root->nodeUsed];
	d->currentNode->nodeName = strdup(el);
	if (attr[0]) {
		for (i = 0; attr[i]; i += 2) {
			d->currentNode->attribSize++;
		}
		d->currentNode->attribs = malloc(sizeof(struct NodeAttrib) * d->currentNode->attribSize);
		for (i = 0; attr[i]; i += 2) {
			d->currentNode->attribs[i/2].key = strdup(attr[i]);
			d->currentNode->attribs[i/2].val = strdup(attr[i + 1]);
		}
	}

	d->root->nodeUsed++;
}

static void XMLCALL theend(void *data, const XML_Char *el)
{
	struct expat_user_data *d = data;

	if (d->currentNode) {
		d->currentNode = d->currentNode->parent;
	}
}

static void XMLCALL thetext(void *data, const XML_Char *s, int len)
{
	struct expat_user_data *d = data;
	const char *pbegin, *pend;

	pbegin = s;
	pend = s + len - 1;

	for (; pbegin < pend; ++pbegin) {
		if (*pbegin != ' ' && *pbegin != '\t' && *pbegin != '\n' && *pbegin != '\r') {
			break;
		}
	}
	for (; pend >= pbegin; --pend) {
		if (*pend != ' ' && *pend != '\t' && *pend != '\n' && *pend != '\r') {
			break;
		}
	}

	if (pbegin <= pend) {
		int newlen;
		// may be called multiple times.

		newlen = pend - pbegin + 1;
		if (d->currentNode->content == NULL) {
			d->currentNode->content = malloc(newlen + 1);
			memcpy(d->currentNode->content, pbegin, newlen);
			d->currentNode->content[newlen] = '\0';
		} else {
			//extend
			int oldlen;
			char *newcontent;

			oldlen = strlen(d->currentNode->content);
			newcontent = malloc(oldlen + newlen + 1);
			memcpy(newcontent, d->currentNode->content, oldlen);
			memcpy(newcontent + oldlen, pbegin, newlen);
			newcontent[oldlen + newlen] = '\0';
			free(d->currentNode->content);
			d->currentNode->content = newcontent;
		}
	}
}
struct XmlNode *getOneNodeByPath(struct XmlNode *pn, const char *xpath, ...)
{
	va_list args;
	char *backer = NULL;
	char *saveptr;
#if 0
	int mSize = 128;// initial = 2*mSize
	int usedLen = mSize;
#endif

	if (!pn || !xpath) {
		return NULL;
	}

#if 1
	va_start(args, xpath);
	vasprintf(&backer, xpath, args);
	va_end(args);
#else
	while (usedLen >= mSize) {
		free(backer);
		mSize <<= 1;
		backer = malloc(mSize);
		va_start(args, xpath);
		usedLen = vsnprintf(backer, mSize, xpath, args);
		va_end(args);
	}
#endif

	saveptr = backer;
	do {
		char *p = strsep(&saveptr, "/");
		if (NULL == p || NULL == pn) {
			break;
		} else if ('\0' == *p) {
			continue;
		} else {
			struct NodeChildren *pc;
			const char *matchee[3] = {NULL, NULL, NULL};// type, key, value
			int len, idx = -1;
			len = strlen(p);

			if (p[len - 1] == ']') {
				char *pres;
				p[--len] = '\0';
				while (p[len] != '[' && len > 0) {
					--len;
				}
				p[len] = '\0';
				pres = p + len + 1;
				if ('0' <= *pres && *pres <= '9') {
					idx = (int)strtol(pres, NULL, 10);
				} else {
					matchee[0] = pres;
					if ('@' == *pres) {
						++pres;
					}
					matchee[1] = pres;
					while (*pres) {
						if ('=' == *pres) {
							*pres = '\0';
							++pres;
							matchee[2] = pres;
							if ('\'' == *pres || '\"' == *pres) {
								++pres;
								matchee[2] = pres;
								while (*pres) {
									++pres;
								}
								--pres;
								if ('\'' == *pres || '\"' == *pres) {
									*pres = '\0';
								}
							}
							break;
						}
						++pres;
					}
				}
			}
			if (xpath) {// flag the first node
				xpath = NULL;
				if (!strcmp(p, pn->nodeName)) {
					continue;
				} else {
					// not match root.
					pn = NULL;
					break;
				}
			}
			pc = pn->children;
			while (pc) {
				if (!strcmp(p, pc->me->nodeName)) {
					if (matchee[0]) {
						if (*matchee[0] == '@' && pc->me->attribSize > 0) {
							int i;
							for (i = 0; i < pc->me->attribSize; i++) {
								if (!strcmp(matchee[1], pc->me->attribs[i].key)) {
									if (!(matchee[2] && strcmp(matchee[2], pc->me->attribs[i].val))) {
										break;
									}
								}
							}
							if (i < pc->me->attribSize) {
								break;
							}
						} else {//match child
							struct NodeChildren *pcc;
							pcc = pc->me->children;
							while (pcc) {
								if (!strcmp(matchee[1], pcc->me->nodeName)) {
									char *value = pcc->me->content;
									if (!matchee[2] || (value && !strcmp(matchee[2], value))) {
										break;
									}
								}
								pcc = pcc->next;
							}
							if (pcc) {
								break;
							}
						}
					} else if (--idx <= 0) {
						break;
					}
				}
				pc = pc->next;
			}
			pn = pc ? pc->me : NULL;
		}
	} while (1);
	free(backer);
	return pn;
}

int loadXML(struct XmlRoot *root, int (*feeder)(void *, char *, int), struct XmlParseParam *param)
{
	struct expat_user_data user;
	char buff[XML_BUFF_SIZE];
	XML_Parser p;

	if (!(root && feeder && param)) {
		loge("invalid param!");
		return -1;
	}

	memset(&user, 0, sizeof(user));
	user.root = root;

	if (param->xmlNodeNum <= 0) {
		param->xmlNodeNum = NODE_INIT_LEN;
	}

	root->node = malloc(param->xmlNodeNum * sizeof(struct XmlNode));
	root->nodeSize = param->xmlNodeNum;
	root->nodeUsed = 0;

	p = XML_ParserCreate(NULL);
	if (!p) {
		return -2;
	}

	XML_SetUserData(p, &user);
	XML_SetElementHandler(p, thestart, theend);
	XML_SetCharacterDataHandler(p, thetext);
	for (;;) {
		int len;

		len = feeder(param->userData, buff, XML_BUFF_SIZE);
		if (len <= 0) {
			len = 0;//read eof or error
		}

		if (XML_Parse(p, buff, len, len <= 0) == XML_STATUS_ERROR) {
			loge("Parse error at line %lu:\n%s\n",
					XML_GetCurrentLineNumber(p),
					XML_ErrorString(XML_GetErrorCode(p)));
			break;
		}

		if (len <= 0) {
			break;
		}
	}
	XML_ParserFree(p);

	return 0;
}
static void dumpNode(FILE *target, struct XmlNode *node, int indent)
{
	struct NodeChildren *pc;
	int i;

	for (i = 0; i < indent; ++i) {
		fprintf(target, "\t");
	}
	fprintf(target, "<%s", node->nodeName);
	if (node->attribSize > 0) {
		for (i = 0; i < node->attribSize; ++i) {
			fprintf(target, " %s=\"%s\"", node->attribs[i].key, node->attribs[i].val);
		}
	}
	if (node->content) {
		fprintf(target, ">%s", node->content);
	} else {
		fprintf(target, ">\n");
	}
	pc = node->children;
	while (pc) {
		dumpNode(target, pc->me, indent >= 0 ? indent + 1 : -1);
		pc = pc->next;
	}
	for (i = 0; i < indent && !node->content; ++i) {
		fprintf(target, "\t");
	}
	fprintf(target, "</%s>\n", node->nodeName);
}

int storeXML(struct XmlNode *node, const char *fileName)
{
	FILE *xmlfile;
	if (!node || !fileName) {
		return 1;
	}
	xmlfile = fopen(fileName, "wb");
	if (!xmlfile) {
		return 2;
	}
	dumpNode(xmlfile, node, -1);
	return fclose(xmlfile);
}

void dumpXMLData(struct XmlNode *node)
{
	if (!node) {
		loge("NULL param");
		return;
	}
	dumpNode(stdout, node, 0);
}
void freeXML(struct XmlRoot *root)
{
	int i;
	if (!root) {
		return;
	}
	for (i = 0; i < root->nodeUsed; ++i) {
		struct XmlNode *pn;
		struct NodeChildren *pc;
		int x;
		pn = &root->node[i];
		for (x = 0; x < pn->attribSize; ++x) {
			free(pn->attribs[x].key);
			free(pn->attribs[x].val);
		}
		free(pn->attribs);
		free(pn->content);
		free(pn->nodeName);
		pc = pn->children;
		while (pc) {
			struct NodeChildren *v = pc;
			pc = pc->next;
			free(v);
		}
	}
	free(root->node);
}
// common xml functions
const char *pathBaseName(const char *path, int len)
{
	const char *p;

	if (!path || !path[0])
		return NULL;
	if (len <= 0)
		len = strlen(path);

	for (p = path + len - 1; p > path; --p) {
		if ('/' == *p) {
			return p + 1;
		}
	}
	return path;
}
const char *getNodeAttribute(struct XmlNode *pn, const char *key)
{
	int i;
	if (!pn || pn->attribSize <= 0 || !key) {
		return NULL;
	}
	for (i = 0; i < pn->attribSize; ++i) {
		if (!strcmp(key, pn->attribs[i].key)) {
			return pn->attribs[i].val;
		}
	}
	return NULL;
}

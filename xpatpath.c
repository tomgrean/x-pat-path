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
#define XPATH_BUF_SIZE 16

struct expat_user_data {
	struct XmlNode *currentNode;
	struct XmlRoot *root;
	struct XmlParseParam *param;
	int currentChildren;
	short parserState;// 0: normal, accepting, >=1: rejecting.
	short xlen; // size of xpathBuffer, xpathBuffer may be NULL if 0.
	const char **xpathBuffer; // array of nodeNames from root down to current node.
};

static void relocateXmlRoot(struct XmlNode *pnew, struct NodeChildren *pnewkids, struct XmlRoot *root)
{
	int i;
	intptr_t pdelta, cdelta;
	if (pnew == root->node && pnewkids == root->children) {
		// luckily, no need to relocate.
		return;
	}
	pdelta = (intptr_t)pnew - (intptr_t)(root->node);
	cdelta = (intptr_t)pnewkids - (intptr_t)(root->children);
	for (i = 0; i < root->nodeUsed; ++i) {
		if (pnew[i].parent) {
			pnew[i].parent = (struct XmlNode*)((intptr_t)(pnew[i].parent) + pdelta);
		}
		if (pnew[i].children) {
			struct NodeChildren *pc;
			pnew[i].children = (struct NodeChildren*)((intptr_t)(pnew[i].children) + cdelta);
			for (pc = pnew[i].children; pc; pc = pc->next) {
				pc->me = (struct XmlNode*)((intptr_t)(pc->me) + pdelta);
				if (pc->next) {
					pc->next = (struct NodeChildren*)((intptr_t)(pc->next) + cdelta);
				}
			}
		}
	}
}
static void relocatePointers(struct XmlNode *pnew, struct NodeChildren *pnewkids, struct expat_user_data *d)
{
	int i;
	intptr_t pdelta, cdelta;
	if (pnew == d->root->node && pnewkids == d->root->children) {
		// luckily, no need to relocate.
		return;
	}
	pdelta = (intptr_t)pnew - (intptr_t)(d->root->node);
	cdelta = (intptr_t)pnewkids - (intptr_t)(d->root->children);
	d->currentNode = (struct XmlNode*)((intptr_t)(d->currentNode) + pdelta);
	for (i = 0; i < d->root->nodeUsed; ++i) {
		if (pnew[i].parent) {
			pnew[i].parent = (struct XmlNode*)((intptr_t)(pnew[i].parent) + pdelta);
		}
		if (pnew[i].children) {
			struct NodeChildren *pc;
			pnew[i].children = (struct NodeChildren*)((intptr_t)(pnew[i].children) + cdelta);
			for (pc = pnew[i].children; pc; pc = pc->next) {
				pc->me = (struct XmlNode*)((intptr_t)(pc->me) + pdelta);
				if (pc->next) {
					pc->next = (struct NodeChildren*)((intptr_t)(pc->next) + cdelta);
				}
			}
		}
	}
}

//parent: d->currentNode, child: &d->root->node[d->root->nodeUsed]
static void addChildren(struct expat_user_data *d)
{
	struct NodeChildren *newChild;

	newChild = &d->root->children[d->currentChildren++];
	memset(newChild, 0, sizeof(struct NodeChildren));
	newChild->me = &d->root->node[d->root->nodeUsed];

	if (d->currentNode->children) {
		struct NodeChildren *pc = d->currentNode->children;

		while (pc->next) {
			pc = pc->next;
		}
		pc->next = newChild;
	} else {
		d->currentNode->children = newChild;
	}
}

static void ensureBufferSize(struct expat_user_data *d, int newlen)
{
	const char **temp;
	if (d->xlen >= newlen) {
		return;
	}
	temp = realloc(d->xpathBuffer, sizeof(char*) * 2 * d->xlen);
	if (temp) {
		d->xpathBuffer = temp;
		d->xlen *= 2;
	} else {
		free(d->xpathBuffer);
		loge("no char memory!\n");
	}
}

// d->xpathBuffer must not to be null.
// return the length
static int updateCurrentXPath(struct expat_user_data *d, struct XmlNode *pn)
{
	int x = 0;

	if (pn->parent) {
		x = updateCurrentXPath(d, pn->parent);
	}
	ensureBufferSize(d, x + 1);
	d->xpathBuffer[x] = pn->nodeName;
	return ++x;
}
static void XMLCALL thestart(void *data, const XML_Char *el, const XML_Char **attr)
{
	int i;
	struct expat_user_data *d = data;

	if (d->parserState > 0) {
		++(d->parserState);
		return;
	}
	if (d->param->filterNode && d->currentNode) {
		i = updateCurrentXPath(d, d->currentNode) + 1;
		ensureBufferSize(d, i);
		d->xpathBuffer[i - 1] = el;
		if (d->param->filterNode(d->xpathBuffer, i, attr)) {
			d->parserState = 1;
			return;
		}
	}

	if (d->root->nodeUsed >= d->root->nodeSize) {
		struct XmlNode *xn = realloc(d->root->node, (d->root->nodeSize + NODE_INIT_LEN) * sizeof(struct XmlNode));
		struct NodeChildren *nc = realloc(d->root->children, (d->root->nodeSize + NODE_INIT_LEN - 1) * sizeof(struct NodeChildren));
		if (xn) {
			relocatePointers(xn, nc, d);
			d->root->node = xn;
			d->root->children = nc;
			memset(&d->root->node[d->root->nodeSize], 0, NODE_INIT_LEN * sizeof(struct XmlNode));
			memset(&d->root->children[d->root->nodeSize - 1], 0, NODE_INIT_LEN * sizeof(struct NodeChildren));
			d->root->nodeSize = d->root->nodeSize + NODE_INIT_LEN;
		} else {
			//something really bad.
			loge("I gonna die.no memory!\n");
			exit(1);
		}
	}
	// set parent relations
	d->root->node[d->root->nodeUsed].parent = d->currentNode;
	if (d->currentNode) {
		addChildren(d);
	}
	// set currentNode
	d->currentNode = &d->root->node[d->root->nodeUsed];
	d->currentNode->nodeName = strdup(el);
	if (attr[0]) {
		int attribSize = 1;//plus one
		for (i = 0; attr[i]; i += 2) {
			++attribSize;
		}
		d->currentNode->attribs = malloc(sizeof(struct NodeAttribute) * attribSize);
		for (i = 0; attr[i]; i += 2) {
			d->currentNode->attribs[i/2].key = strdup(attr[i]);
			d->currentNode->attribs[i/2].val = strdup(attr[i + 1]);
		}
		d->currentNode->attribs[attribSize - 1].key = d->currentNode->attribs[attribSize - 1].val = NULL;
	}

	d->root->nodeUsed++;
}

static void XMLCALL theend(void *data, const XML_Char *el)
{
	struct expat_user_data *d = data;

	if (d->parserState > 0) {
		--(d->parserState);
		return;
	}

	if (d->currentNode) {
		d->currentNode = d->currentNode->parent;
	}
}

static void XMLCALL thetext(void *data, const XML_Char *s, int len)
{
	struct expat_user_data *d = data;
	const char *pbegin, *pend;

	if (d->parserState > 0) {
		return;
	}

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
						if (*matchee[0] == '@' && pc->me->attribs) {
							int i;
							for (i = 0; pc->me->attribs[i].key; i++) {
								if (!strcmp(matchee[1], pc->me->attribs[i].key)) {
									if (!(matchee[2] && strcmp(matchee[2], pc->me->attribs[i].val))) {
										i = -1;
										break;
									}
								}
							}
							if (i < 0) {
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
	user.param = param;

	if (param->xmlNodeNum <= 0) {
		param->xmlNodeNum = NODE_INIT_LEN;
	}

	p = XML_ParserCreate(NULL);
	if (!p) {
		return -2;
	}

	root->node = malloc(param->xmlNodeNum * sizeof(struct XmlNode));
	if (root->node) {
		memset(root->node, 0, param->xmlNodeNum * sizeof(struct XmlNode));
	}
	root->nodeSize = param->xmlNodeNum;
	root->nodeUsed = 0;
	root->children = malloc((param->xmlNodeNum - 1) * sizeof(struct NodeChildren));
	if (root->children) {
		memset(root->children, 0, (param->xmlNodeNum - 1) * sizeof(struct NodeChildren));
	}

	if (param->filterNode) {
		user.xpathBuffer = malloc(XPATH_BUF_SIZE * sizeof(char*));
		user.xlen = XPATH_BUF_SIZE;
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
	if (user.xpathBuffer) {
		free(user.xpathBuffer);
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
	if (node->attribs) {
		for (i = 0; node->attribs[i].key; ++i) {
			fprintf(target, " %s=\"%s\"", node->attribs[i].key, node->attribs[i].val);
		}
	}

	pc = node->children;
	if (pc) {
		if (node->content) {
			fprintf(target, ">%s\n", node->content);
		} else {
			fprintf(target, ">\n");
		}
	} else {
		if (node->content) {
			fprintf(target, ">%s", node->content);
		} else {
			fprintf(target, "/>\n");
		}
	}

	while (pc) {
		dumpNode(target, pc->me, indent >= 0 ? indent + 1 : -1);
		pc = pc->next;
	}

	pc = node->children;
	for (i = 0; i < indent && pc; ++i) {
		fprintf(target, "\t");
	}
	if (pc || node->content) {
		fprintf(target, "</%s>\n", node->nodeName);
	}
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
	for (i = 0; i < root->nodeSize; ++i) {
		struct XmlNode *pn;
		int x;
		pn = &root->node[i];
		if (pn->attribs) {
			for (x = 0; pn->attribs[x].key; ++x) {
				free(pn->attribs[x].key);
				free(pn->attribs[x].val);
			}
			free(pn->attribs);
		}
		free(pn->content);
		free(pn->nodeName);
	}
	free(root->node);
	free(root->children);
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
	if (!(pn && pn->attribs && key)) {
		return NULL;
	}
	for (i = 0; pn->attribs[i].key; ++i) {
		if (!strcmp(key, pn->attribs[i].key)) {
			return pn->attribs[i].val;
		}
	}
	return NULL;
}
struct XmlNode *addToChild(struct XmlRoot *root, struct XmlNode *parent, const char *nodeName, const char *nodeValue)
{
	int i;
	struct XmlNode *target = NULL;
	struct NodeChildren *targetChild = NULL;

	if (!(root && parent && nodeName)) {
		return NULL;
	}
	// realloc the root memory if nessesary
	if (root->nodeUsed >= root->nodeSize) {
		struct XmlNode *xn = realloc(root->node, (root->nodeSize + NODE_INIT_LEN) * sizeof(struct XmlNode));
		struct NodeChildren *nc = realloc(root->children, (root->nodeSize + NODE_INIT_LEN - 1) * sizeof(struct NodeChildren));
		if (xn) {
			relocateXmlRoot(xn, nc, root);
			root->node = xn;
			root->children = nc;
			memset(&root->node[root->nodeSize], 0, NODE_INIT_LEN * sizeof(struct XmlNode));
			memset(&root->children[root->nodeSize - 1], 0, NODE_INIT_LEN * sizeof(struct NodeChildren));
			root->nodeSize = root->nodeSize + NODE_INIT_LEN;
		} else {
			//something really bad.
			loge("I gonna die.no memory!\n");
			exit(1);
		}
	}
	// find the first usable place.
	// root->nodeUsed cannot be used as index now.
	for (i = 0; i < root->nodeSize; ++i) {
		target = &root->node[i];
		if (!target->nodeName) {
			break;
		}
	}
	for (i = 0; i < root->nodeSize - 1; ++i) {
		targetChild = &root->children[i];
		if (!targetChild->me) {
			break;
		}
	}
	if (!(target && targetChild)) {
		return NULL;
	}
	// set parent relations
	target->parent = parent;

	memset(target, 0, sizeof(struct XmlNode));
	memset(targetChild, 0, sizeof(struct NodeChildren));
	targetChild->me = target;

	if (parent->children) {
		struct NodeChildren *pc = parent->children;

		while (pc->next) {
			pc = pc->next;
		}
		pc->next = targetChild;
	} else {
		parent->children = targetChild;
	}

	// set currentNode
	target->nodeName = strdup(nodeName);
	target->content = (nodeValue && *nodeValue) ? strdup(nodeValue) : NULL;

	root->nodeUsed++;

	return target;
}
int setNodeAttribute(struct XmlNode *pn, const char *keys[], const char *vals[])
{
	int i;
	int attribSize = 0;

	if (!pn) {
		return __LINE__;
	}
	if (keys) {
		for (i = 0; keys[i]; ++i) {
			++attribSize;
		}
	}
	if (pn->attribs) {
		free(pn->attribs);
		pn->attribs = NULL;
	}
	if (attribSize) {
		++attribSize;// add the NULL tail
		pn->attribs = malloc(sizeof(struct NodeAttribute) * attribSize);
		for (i = 0; keys[i]; ++i) {
			pn->attribs[i].key = strdup(keys[i]);
			pn->attribs[i].val = vals[i] ? strdup(vals[i]) : NULL;
		}
		pn->attribs[attribSize - 1].key = pn->attribs[attribSize - 1].val = NULL;
	}
	return 0;
}
int deleteNode(struct XmlRoot *root, struct XmlNode *toDel)
{
	int i;
	if (!(root && toDel)) {
		return __LINE__;
	}
	// can not delete root
	if (toDel == root->node) {
		return __LINE__;
	}
	for (i = 0; i < root->nodeSize; ++i) {
		if (&root->node[i] == toDel) {
			struct NodeChildren *nc;
			int x;
			if (toDel->children) {
				// delete all children.
				for (nc = toDel->children; nc; nc = nc->next) {
					deleteNode(root, nc->me);
				}
			}
			free(toDel->nodeName);
			toDel->nodeName = NULL;
			if (toDel->attribs) {
				for (x = 0; toDel->attribs->key; ++x) {
					free(toDel->attribs->key);
					free(toDel->attribs->val);
				}
				free(toDel->attribs);
			}
			toDel->attribs = NULL;
			free(toDel->content);
			toDel->content = NULL;

			nc = toDel->parent->children;
			if (nc->me == toDel) {
				toDel->parent->children = nc->next;
				nc->me = NULL;
				nc->next = NULL;
			} else {
				for (; nc->next; nc = nc->next) {
					if (nc->next->me == toDel) {
						nc->next->me = NULL;
						nc->next = nc->next->next;
						//nc->next = xxxx;
						break;
					}
				}
			}
			root->nodeUsed--;
		}
	}
	return 0;
}


/*
 * xpatpath.h
 *
 *  Created on: Jan 12, 2018
 *      Author: tomgrean at github dot com
 */

#ifndef XPATPATH_H_
#define XPATPATH_H_

// macro function
#define loge(...) fprintf(stderr, __VA_ARGS__)

// ****XML node definations****
struct XmlParseParam {
	void *userData; // FILE pointer or file descriptor, etc.
	int xmlNodeNum; // estimated xml node number.
};
struct NodeAttribute {
	char *key;
	char *val;
};
struct XmlNode; // predeclare

struct NodeChildren {
	struct XmlNode *me;
	struct NodeChildren *next;
};

struct XmlNode {
	struct XmlNode *parent;
	struct NodeChildren *children;
	struct NodeAttribute *attribs; // array end with struct NodeAttribute { key = NULL, val = NULL }
	char *nodeName;
	char *content;
};

struct XmlRoot {
	int nodeUsed;
	int nodeSize;
	struct XmlNode *node;

	// XML must has a single root, so the number of children is nodeUsed - 1.
	struct NodeChildren *children;// array of pre allocated children. used internally.
};

// parse XML from a "feeder" function callback.
// root is a stack pointer or heap allocated by the caller.
// feeder is the callback function for data reading, a return of non-positive number indicates EOF or ERROR.
// param hold the FILE pointer/file descriptor, and will pass to "feeder" in "void *d".
int loadXML(struct XmlRoot *root, int (*feeder)(void *d, char *buffer, int maxlen), struct XmlParseParam *param);

// save the XML to the specified file name.
int storeXML(struct XmlNode *node, const char *fileName);

// free XML resources.
void freeXML(struct XmlRoot *root);

/*
parameter xpath follows partial xml xpath rules. examples:
/web-app/session-config
/web-app/mime-mapping[2]
/web-app/servlet[servlet-name='jsp']/servlet-class
/Server/Service/Connector[@port='8009']
NOTE: the array index start from 1.
return: 1 or 0 XML Node.
*/
struct XmlNode *getOneNodeByPath(struct XmlNode *pn, const char *xpath, ...);

// debug tool. to show out the xml content.
void dumpXMLData(struct XmlNode *node);

// ****common node functions****

// get the XML node attribute value the its key,
// return NULL if not found.
const char *getNodeAttribute(struct XmlNode *pn, const char *key);

// like the "basename" command, return the right-most part splitted by '/'.
// if len <= 0, len will be strlen(path)
// return NULL if PATH is bad
const char *pathBaseName(const char *path, int len);

#endif /* XPATPATH_H_ */

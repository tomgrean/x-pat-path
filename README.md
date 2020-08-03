# libxpatpath
Small XML parser library with EXPAT backend and some additional features:
* Simple XPATH support(see below).
* Filter XML tags while parsing an XML.

Parameter xpath follows partial xml xpath rules. examples:
```
/web-app/session-config
/web-app/mime-mapping[2]
/web-app/servlet[servlet-name='jsp']/servlet-class
/Server/Service/Connector[@port='8009']
```
NOTE: the array index start from 1.

Filter while parsing, will omit uninterested nodes in the XML. Makes the resulting data structure smaller, takes less memory. Especially useful when device memory is limited or the XML is relatively too large.

Example filter code:
```
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
```
and in main(), pass the xpathfilter function to loadXML();
```
struct XmlRoot r;
struct XmlParseParam xpp;
//... other init staff

memset(&xpp, 0, sizeof(xpp));
xpp.filterNode = xpathfilter;
ret = loadXML(&r, file_feeder, &xpp);
dumpXMLData(r.node);
//... other works with the XML data.
freeXML(&r);
```
XML node could be added or deleted by addToChild()/deleteNode() functions.
```
struct XmlNode *pn = addToChild(&r, r.node, "test_new_tag", "very short test content");
//...
pn = getOneNodeByPath(r.node, "/web-app/welcome-file-list/welcome-file[2]");
ret = deleteNode(&r, pn);
```

#include <xml.h>

int setNodeValue(xmlDocPtr doc, char * expression, char * value) {
	int i, size;
	xmlXPathObjectPtr xpathObject = getNodes(doc, expression);
	if (xpathObject == NULL) {
		xmlXPathFreeObject(xpathObject);
		return 0;
	}
	xmlNodeSetPtr node = xpathObject->nodesetval;
	size = (node) ? node->nodeNr : 0;
	for (i = size-1; i >=0; i--)
		xmlNodeSetContent(node->nodeTab[i], (xmlChar *) value);

	xmlXPathFreeObject(xpathObject);
	return 1;
}

char ** getNodeValue(xmlDocPtr doc, char * expression) {
	int size, length;
	char * temp;
	char ** arr;
	
	xmlXPathObjectPtr xpathObject = getNodes(doc, expression);
	if (xpathObject == NULL) {
		xmlXPathFreeObject(xpathObject);
		return NULL;
	}
	xmlNodeSetPtr nodes = xpathObject->nodesetval;
	size = (nodes) ? nodes->nodeNr : 0;
	
	if (size == 0) {
		xmlXPathFreeObject(xpathObject);
		return NULL;
	}

	arr = calloc(size+1, sizeof(char *));

	for (int i = 0; i < size; i++) {
		temp	= (char *) xmlNodeGetContent(nodes->nodeTab[i]);
		length	= (int)strlen(temp);
		arr[i]	= calloc(length+1, sizeof(char));
		strcpy(arr[i], temp);
		arr[i][length] = '\0';
		free(temp);
	}
	arr[size] = NULL;
	xmlXPathFreeObject(xpathObject);
	return arr;
}

xmlXPathObjectPtr getNodes(xmlDocPtr doc, char * expression) {
	xmlXPathContextPtr xpathContext;
	xmlXPathObjectPtr xpathObject;
	xmlChar * xpathExpr = (xmlChar*) expression;
	// Kontext für XPath Evaluierung generieren
	xpathContext = xmlXPathNewContext(doc);
	if (xpathContext == NULL) {
		xmlXPathFreeContext(xpathContext);
		printf("Error: unable to create new XPath context\n");
		return NULL;
	}
	// XPath Ausdruck evaluieren
	xpathObject = xmlXPathEvalExpression(xpathExpr, xpathContext);
	if (xpathObject == NULL) {
		xmlXPathFreeObject(xpathObject);
		xmlXPathFreeContext(xpathContext);
		printf("Error: unable to evaluate XPath expression\n");
		return NULL;
	}
	xmlXPathFreeContext(xpathContext);
	return xpathObject;
}

xmlDocPtr getdoc(char *docname) {
	xmlDocPtr doc;
	char msg[1024];
	doc = xmlParseFile(docname);
	if (doc == NULL) {
		snprintf(msg, sizeof(msg), "%s%s%s", 
				"Fehler beim Einlesen der XML-Datei", docname, " !\n");
		setError(msg, 0);
		return NULL;
	}
	return doc;
}

char ** getWellFormedXML(char ** trees) {
	size_t xmlLength;
	int count;
	
	for (count = 0; trees[count]; count++);
	
	char ** ret = calloc(count+1, sizeof(char *));
	
	for (int i = 0; i < count; i++) {
		xmlLength	= strlen(trees[i]);
		ret[i]		= calloc(xmlLength+XML_TAG_LEN+1, sizeof(char));
		strcpy(ret[i], XML_TAG);
		strcat(ret[i], trees[i]);
	}
	ret[count] = NULL;
	return ret;
}

char ** getTree(xmlDocPtr message, char * tag) {
	xmlXPathObjectPtr xpathObj;
	xmlNodeSetPtr nodeSet;
	xmlBufferPtr xmlBuff;
	char ** array;
	int countNodes, dumpSize;
	// Aufruf aller Knoten (Evaluierung mittels tag)
	xpathObj = getNodes(message, tag);
	nodeSet	 = xpathObj->nodesetval;
	if ((countNodes = (nodeSet) ? nodeSet->nodeNr : 0) == 0) {
		xmlXPathFreeNodeSet(nodeSet);
		return NULL;
	}

	// erstellt für jeden gefundenen Tag einen XML-Doc-String
	array = calloc(countNodes+1, sizeof(char*));
	for (int i = 0; i < countNodes; ++i) {
		xmlBuff	 = xmlBufferCreate();
		dumpSize = xmlNodeDump(xmlBuff, message, nodeSet->nodeTab[i], 0, 0);
		array[i] = calloc((dumpSize + 1), sizeof(char));
		array[i] = strcpy(array[i], (char *) xmlBuff->content);
		array[i][dumpSize] = '\0';
		xmlBufferFree(xmlBuff);
	}
	array[countNodes] = NULL;	// Array sicher abgeschlossen
	xmlXPathFreeObject(xpathObj);
	return array;
}

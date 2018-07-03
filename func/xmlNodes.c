#include <string.h>
#include <func.h>

int setNodeValue(xmlDocPtr doc, char * expression, char * value) {
	int i, size;
	xmlXPathObjectPtr xpathObject = getNodes(doc, expression);
	if (xpathObject == NULL) {
		xmlXPathFreeObject(xpathObject);
		free(value);
		return 0;
	}
	xmlNodeSetPtr node = xpathObject->nodesetval;
	size = (node) ? node->nodeNr : 0;
	for (i = size-1; i >=0; i--)
		xmlNodeSetContent(node->nodeTab[i], (xmlChar *) value);

	xmlXPathFreeObject(xpathObject);
	free(value);
	return 1;
}

char * getNodeValue(xmlDocPtr doc, char * expression) {
	int size, length;
	xmlXPathObjectPtr xpathObject = getNodes(doc, expression);
	if (xpathObject == NULL) {
		xmlXPathFreeObject(xpathObject);
		return NULL;
	}
	xmlNodeSetPtr nodes = xpathObject->nodesetval;
	size = (nodes) ? nodes->nodeNr : 0;
	if (size > 1)
		return NULL;
	char *tmp = (char *) nodes->nodeTab[0]->name;
	
	printf("Node Value: %s Length: %i\n", tmp, (int)strlen(tmp));
	
	length = (int)strlen(tmp);
//	char *value = malloc(length+1);
//	strcpy(value, tmp);
//	value[length] = '\0';
	return "test";
}

xmlXPathObjectPtr getNodes(xmlDocPtr doc, char * expression) {
	xmlXPathContextPtr xpathContext;
	xmlXPathObjectPtr xpathObject;
	xmlChar *xpathExpr = (xmlChar*) expression;
	// Kontext für XPath Evaluierung generieren
	xpathContext = xmlXPathNewContext(doc);
	if (xpathContext == NULL) {
		xmlFreeDoc(doc);
		printf("Error: unable to create new XPath context\n");
		return NULL;
	}
	// XPath Ausdruck evaluieren
	xpathObject = xmlXPathEvalExpression(xpathExpr, xpathContext);
	if (xpathObject == NULL) {
		xmlXPathFreeContext(xpathContext);
		xmlFreeDoc(doc);
		printf("Error: unable to evaluate XPath expression\n");
		return NULL;
	}
	xmlXPathFreeContext(xpathContext);
	return xpathObject;
}

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

char ** getNodeValue(xmlDocPtr doc, char * expression) {
	int i, size, length;
	char * temp;
	char ** arr;
	
	xmlXPathObjectPtr xpathObject = getNodes(doc, expression);
	if (xpathObject == NULL) {
		xmlXPathFreeObject(xpathObject);
		return NULL;
	}
	xmlNodeSetPtr nodes = xpathObject->nodesetval;
	size = (nodes) ? nodes->nodeNr : 0;
	
	if (size == 0)
		return NULL;

	arr = calloc(size+1, sizeof(char *));

	for (i = 0; i < size; i++) {
		temp	= (char *) xmlNodeGetContent(nodes->nodeTab[i]);
		length	= (int)strlen(temp);
		arr[i]	= calloc(length+1, sizeof(char));
		strcpy(arr[i], temp);
		arr[i][length] = '\0';
	}
	arr[size] = '\0';
	return arr;
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

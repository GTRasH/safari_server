
#define MAX_HASH 100

typedef struct xmlDocListElement {
	struct xmlDocListElement *next;
	xmlDocPtr ptr;
} xmlDocListElement;

typedef struct {
	xmlDocListElement *first;
	size_t size;
} xmlDocListHead;

typedef struct intersectionGeometry {
	struct intersectionGeometry *next;
	uint32_t referenceID;
	int timestamp;
	char * xml;
} intersectionGeometry;

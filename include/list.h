
#define MAX_HASH 100

/** \brief Listen-Element für SPat- und MAP-Nachrichten (Lesevorgang) */
typedef struct xmlDocListElement {
	struct xmlDocListElement *next;
	xmlDocPtr ptr;
} xmlDocListElement;

/** \brief Listen-Kopf für einfach verkettete Nachrichten-Liste */
typedef struct {
	xmlDocListElement *first;
	size_t size;
} xmlDocListHead;

/** \brief Element für MAP-Nachrichten in Hash-Table */
typedef struct intersectionGeometry {
	struct intersectionGeometry *next;
	uint32_t referenceID;
	int timestamp;
	char * xml;
} intersectionGeometry;

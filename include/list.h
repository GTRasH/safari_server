
typedef struct xmlDocListElement {
	struct xmlDocListElement *next;
	xmlDocPtr ptr;
} xmlDocListElement;

typedef struct {
	xmlDocListElement *first;
	size_t size;
} xmlDocListHead;

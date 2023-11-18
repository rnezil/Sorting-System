typedef struct {
	char itemColor; 	/* stores a letter indicating if the item is Black/White/Steel/Aluminum*/
	char stage; 		/* 0: part is built, 1: part not built, 2: part is shipped */
} element;

typedef struct link{
	element		e;
	struct link *next;
} link;

void	initLink	(link **newLink); // Allocates fresh memory and nullifies next pointer
void	destroyLink	(link **deadLink);
void 	setup		(link **h, link **t);
void 	clearQueue	(link **h, link **t);
void 	enqueue		(link **h, link **t, link **nL);
void 	dequeue		(link **h, link **deQueuedLink);
element firstValue	(link **h);
char 	isEmpty		(link **h);
int 	size		(link **h, link **t);


/*
 * 's' == steel
 * 'a' == aluminium
 * 'b' == black plastic
 * 'w' == white plastic
 */
typedef char Item;

typedef struct link{
	Item i;
	struct link *next;
} link;

void	initLink	(link **newLink); // Allocates fresh memory and nullifies next pointer
void	destroyLink	(link **oldLink);
void 	setup		(link **h, link **t);
void 	clearQueue	(link **h, link **t);
void 	enqueue		(link **h, link **t, link **nL);
void 	dequeue		(link **h, link **deQueuedLink);
element firstValue	(link **h);
char 	isEmpty		(link **h);
int 	size		(link **h, link **t);


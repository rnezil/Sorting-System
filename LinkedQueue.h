/*
 * 's' == steel
 * 'a' == aluminium
 * 'b' == black plastic
 * 'w' == white plastic
 */
typedef struct link{
	char itemType;
	struct link *next;
} link;

void	initLink	(link **newLink); // Allocates fresh memory and nullifies next pointer
void	destroyLink	(link **oldLink);
void 	setup		(link **h, link **t);
void 	clearQueue	(link **h, link **t);
void 	enqueue		(link **h, link **t, link **nL);
void 	dequeue		(link **h, link **deQueuedLink);
char	firstValue	(link **h);
char 	isEmpty		(link **h);
int 	size		(link **h, link **t);


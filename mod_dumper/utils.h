#ifndef _UTILS_H_
#define _UTILS_H_

typedef struct _node{
	
	unsigned long 	addr;
	char *		symbol;
	struct _node 	*left, *right;

} node, *nodep;

/******************************************
 * FuncName: get_addr                     *
 * Usage: convert string to hex           *
 * @Params:                               *
 *    char *p -> pointer to string        *
 * RetValue: int hex value                *
 * e.g: "0x12345678" -> 0x12345678        *
 ******************************************/
unsigned int get_addr(char *p)
{
	unsigned int ret;

	if(*p++ != '0' || *p++ !='x')
		return 0;
	ret = 0;
	while(*p){
		switch(*p){
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				ret = (ret << 4) + (*p - '0');
				break;
			case 'a': case 'b': case 'c':
			case 'd': case 'e': case 'f':
				ret = (ret << 4) + (*p - 'a') + 10;
				break;
			case 'A': case 'B': case 'C':
			case 'D': case 'E': case 'F':
				ret = (ret << 4) + (*p - 'A') + 10;
				break;
			default:
				return 0;
		}
		p++;
	}

	return ret;
}

unsigned long get_addr_2(char *p)
{
	unsigned long 	ret;
	int		i;

	ret = 0;
	for(i = 0; i < 8; i++, p++)
		switch(*p){
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				ret = (ret << 4) + (*p - '0');
				break;
			case 'a': case 'b': case 'c':
			case 'd': case 'e': case 'f':
				ret = (ret << 4) + (*p - 'a') + 10;
				break;
			case 'A': case 'B': case 'C':
			case 'D': case 'E': case 'F':
				ret = (ret << 4) + (*p - 'A') + 10;
				break;
			default:
				return 0;
		};
	
	return ret;

}
node * add_node(nodep root, unsigned long addr, const char *symbol)
{
	char 	*p;
	nodep	np, iterp;
	
	np = (nodep)malloc(sizeof(node));
	p = strdup(symbol);

	np->left = np->right = NULL;
	np->addr = addr;
	np->symbol = p;
	
	if(!root)
		return np;

	iterp = root;

	do{
		if(iterp->addr < addr)
			if(iterp->right)
				iterp = iterp->right;
			else{
				iterp->right = np;
				break;
			}
		else if(iterp->addr > addr)
			if(iterp->left)
				iterp = iterp->left;
			else{
				iterp->left = np;
				break;
			}
		else{
			free(np);
			free(iterp->symbol);
			iterp->symbol = p;
			break;
		}
	}while(1);
	
	return root;
	
}

char *find_symbol(nodep root, unsigned long addr)
{
	nodep	np;
	
	if(!root)
		return NULL;
	
	np = root;
	while(np){
		if(np->addr == addr)
			break;
		else if(np->addr > addr)
			np = np->left;
		else
			np = np->right;
	};
	if(np)
		return np->symbol;
	else
		return NULL;
}
		
#endif /* _UTILS_H_ */

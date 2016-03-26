%{
#include<stdio.h>
#include<string.h>
#include"global.h"
#include"bison.tab.h"
%}

%%
[a-zA-Z0-9/.‘<’‘>’'/‘‘%’]* {return STRING;}
(\|)+ {return PIPE;}
%%
int yywrap() {
	return 1;
}

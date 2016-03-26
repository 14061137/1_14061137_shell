#include<stdio.h>
int main(){
	char a[100] = {0};
	while(scanf("%s ", a) > 0){
		printf("%s\n",a);
	}
	return 0;
}
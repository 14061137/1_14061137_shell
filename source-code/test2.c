#include<stdio.h>
int main(){
	char a[100] = {0};
	int i = 0;
	while(scanf("%s ", a) > 0){
		i++;
	}
	printf("###%d ", i);
	return 0;
}
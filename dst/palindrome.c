#include <stdio.h>
int main() {
	int n;
	scanf("%d", &n);
	int good = 0;
	good |= n < 10;
	good |= n < 100 && n >= 10 && (n%10) == (n/10);
	good |= n < 1000 && n >= 100 && (n%10) == (n/100);
	good |= n < 10000 && n >= 1000 && (n%10) == (n/1000) && (n/100%10) == (n/10%10);
	if (good) {
		printf("Y\n");
	} else {
		printf("N\n");
	}
	return 0;
}

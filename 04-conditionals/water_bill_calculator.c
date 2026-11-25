#include <stdio.h>

int main(){
	
	float numberofwaterunits;
	float totalbill;
	
	printf("Enter the number of water units consumed.\t");
	scanf("%f",&numberofwaterunits);
	
	if(numberofwaterunits >= 60){
		totalbill = numberofwaterunits * 30;
	}
	else if(numberofwaterunits >= 30){
		totalbill = numberofwaterunits * 25;
	}
	else
	{
		totalbill = numberofwaterunits * 20;
	}
	
	printf("Units of water consumed: %.2f.\n",numberofwaterunits);
	printf("Total water bill: %.2f KES.\n",totalbill);
	
	return 0;
	
}

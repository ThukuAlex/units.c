#include <stdio.h>
#include <math.h>

int main(){
    float principal_amount;
    float time;
    float rate;
    float simple_interest;
    float compound_interest;
    
    printf("Enter the principal amount.\t");
    scanf("%f",&principal_amount);
    
    printf("Enter time.\t");
    scanf("%f",&time);
    
    printf("Enter rate.\t");
    scanf("%f",&rate);
    
    simple_interest = (principal_amount*time*rate)/100;
    compound_interest = principal_amount * pow((1+rate/100),time) - principal_amount;
    
    printf("Simple Interest: Kshs %.2f.\n",simple_interest);
    printf("Compounded Interest: Kshs %.2f.\n",compound_interest);
    
    return 0;
}

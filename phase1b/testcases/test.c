#include<stdio.h>
#include<time.h>
int main(){
    //Time a loop 
    clock_t start, end;
    double cpu_time_used;
    start = clock();
    for(int i = 0; i < 10000000; i++){
        if(i == 50000000){
            printf("Halfway there!\n");
        }
    }
    end = clock();
    cpu_time_used = ((double) (end - start));
    printf("Time taken: %f\n", cpu_time_used);
}
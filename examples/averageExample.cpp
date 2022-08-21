#include "protoAverage.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc,char** argv)
{
    double total = 0;
    double count = 0;
    ProtoAverage table;
    int i=0;
    double bignumber =pow(2,1023);
    for(i = 0;i<10000000;i++)
    {
        total +=bignumber; count++;
        if(!table.AddNumber(bignumber))
        {
            exit(0);
        }
    }
    double littlenumber = .01;
    for(i = 0;i<10000000;i++)
    {
        total+=littlenumber;count++;
        if(!table.AddNumber(littlenumber))
        {
            exit(0);
        }
    }
    //table.Print();
    double average;
    average = table.GetAverage();
    printf("%f with protoAverage %f with straight average\n",average,total/count);
    printf("%f is the average divided by the really big number which should be ~.5\n",average/bignumber);
    for(i = 0;i<80000000;i++)
    {
        total+=littlenumber;count++;
        if(!table.AddNumber(littlenumber))
        {
            exit(0);
        }
    }
    average = table.GetAverage();
    printf("%f is the average divided by the really big number which should be ~.1\n",average/bignumber);
}

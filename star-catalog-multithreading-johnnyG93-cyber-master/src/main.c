// MIT License
//
// Copyright (c) 2023 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <time.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <pthread.h>
#include "utility.h"
#include "star.h"
#include "float.h"

#define NUM_STARS 30000
#define MAX_LINE 1024
#define DELIMITER " \t\n"
struct Star star_array[ NUM_STARS ];
uint8_t   (*distance_calculated)[NUM_STARS];

// struct star_distance values //another way to implement multithreading
// {
//   double  min  = FLT_MAX;
//   double  max  = FLT_MIN;
//   double mean = 0;
// }

double  min  = FLT_MAX;
double  max  = FLT_MIN;
double mean = 0;
int variable; //uncreative global variable for dividing the work

void showHelp()
{
    printf("Use: findAngular [options]\n");
    printf("Where options are:\n");
    printf("-t          Number of threads to use\n");
    printf("-h          Show this help\n");
}

//
// Embarassingly inefficient, intentionally bad method
// to calculate all entries one another to determine the
// average angular separation between any two stars

/*
   float determineAverageAngularDistance( struct Star arr[] )
{
    double mean = 0;

    uint32_t i, j;
    uint64_t count = 0;


    for (i = 0; i < NUM_STARS; i++)
    {
      for (j = i; j < NUM_STARS; j++)
      {
        if( i!=j && distance_calculated[i][j] == 0 )
        {
          double distance = calculateAngularDistance( arr[i].RightAscension, arr[i].Declination,
                                                      arr[j].RightAscension, arr[j].Declination ) ;
          distance_calculated[i][j] = 1;
          distance_calculated[j][i] = 1;
          count++;

          if( min > distance )
          {
            min = distance;
          }

          if( max < distance )
          {
            max = distance;
          }
          mean = mean + (distance-mean)/count;
        }
      }
    }
    return mean;
}
*/

pthread_mutex_t mutex;

void *multi_determineAverageAngularDistance( void *vargp )
{


    int tid = *((int *)vargp); // cast void pointer to int array pointer 



    int idx1=tid*variable; //partions the first workload for the outer loop
    int idx2=tid*variable+variable; //partitions the workload for the inner loop
    //printf("%d %d %d\n",idx1,idx2,tid ); //shows the indexes being passed to partition the work
    struct Star *arr =  star_array;


    int i, j;
    int count=0;
    double local_mean = 0;
    double  local_min  = FLT_MAX;
    double  local_max  = FLT_MIN;


    for (i = idx1; i < idx2 ; i++)
    {
        for (j =  i; j < NUM_STARS; j++)
        {


            if( i!=j )
            {

                double distance = calculateAngularDistance( arr[i].RightAscension, arr[i].Declination,
                                  arr[j].RightAscension, arr[j].Declination ) ;



                if( local_min > distance )
                {
                    local_min = distance;
                }

                if( local_max < distance )
                {
                    local_max = distance;
                }
                local_mean +=  distance;

            }

        }

    }



    pthread_mutex_lock(&mutex); //locks access to globals by other threads
    if( local_min < min )
    {
        min = local_min;
    }

    if( local_max > max )
    {
        max = local_max;
    }
    mean += local_mean;
    pthread_mutex_unlock(&mutex); //unlocks access to globals by other threads

}


int main( int argc, char * argv[] )
{

    FILE *fp;
    uint32_t star_count = 0;

    uint32_t n;

    pthread_t tid;


    pthread_t store_tid[1000]= {};

    char *Token=NULL;
    int thread_count = 1; //program threads at least once
    char val[10]= {};

    //time_t start= clock();   //execution time

    for( n = 1; n < argc; n++ )
    {
        if( strcmp(argv[n], "-help" ) == 0 )
        {
            showHelp();
            exit(0);
        }
        if( strcmp(argv[n], "-t" ) == 0 )
        {
            Token=strtok(argv[n+1]," ");
            strcpy(val,Token);
            thread_count=atoi(Token);
            if(thread_count==0)
            {
                printf("No threads were created\n");
                return 0;
            }
            if(thread_count<0)
            {
                printf("Invalid number of threads\n");
                exit(0);
            }
        }

    }

    fp = fopen( "data/tycho-trimmed.csv", "r" );

    if( fp == NULL )
    {
        printf("ERROR: Unable to open the file data/tycho-trimmed.csv\n");
        exit(1);
    }

    char line[MAX_LINE];
    while (fgets(line, 1024, fp))
    {
        uint32_t column = 0;

        char* tok;
        for (tok = strtok(line, " ");
                tok && *tok;
                tok = strtok(NULL, " "))
        {
            switch( column )
            {
            case 0:
                star_array[star_count].ID = atoi(tok);
                break;

            case 1:
                star_array[star_count].RightAscension = atof(tok);
                break;

            case 2:
                star_array[star_count].Declination = atof(tok);
                break;

            default:
                printf("ERROR: line %d had more than 3 columns\n", star_count );
                exit(1);
                break;
            }
            column++;
        }
        star_count++;
    }

    printf("%d records read\n", star_count );
    printf("%d threads created\n",thread_count);


    variable=(NUM_STARS/thread_count);
    //printf("%d \n",thread_count);
    for(int i=0; i<thread_count; i++)
    {
        int *arg = malloc(sizeof(*arg)); //allows us to send the tid
        *arg = i;
        pthread_create(&store_tid[i],NULL,&multi_determineAverageAngularDistance,arg); //creates threads and passes the ID to the working function
    }


    for(int i=0; i<thread_count; i++)
        pthread_join(store_tid[i],NULL);


    mean/=(NUM_STARS*(NUM_STARS+1)/2);

    //time_t end= clock();

    //double time_spent =(double)(end - start);

    // Find the average angular distance in the most inefficient way possible
    // double distance =  determineAverageAngularDistance( star_array );
    printf("Average distance found is %lf\n", mean );
    printf("Minimum distance found is %lf\n", min );
    printf("Maximum distance found is %lf\n", max );

    pthread_exit(NULL);

    fclose(fp);

    //printf("The run time of the program is %f seconds", time_spent);

    return 0;
}


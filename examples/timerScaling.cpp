#include <protoTimer.h>
#include <stdio.h>
#include <stdlib.h>  // for rand()

// This test program benchmarks the ProtoTimer insertion cost
// as increasing numbers of timers are activated.


// These functions measure CPU time used

#ifdef WIN32

#include <Windows.h>
double get_cpu_time()
{
    FILETIME a,b,c,d;
    if (GetProcessTimes(GetCurrentProcess(),&a,&b,&c,&d) != 0){
        //  Returns total user time.
        //  Can be tweaked to include kernel times as well.
        return
            (double)(d.dwLowDateTime |
            ((unsigned long long)d.dwHighDateTime << 32)) * 0.0000001;
    }else{
        //  Handle error
        return 0;
    }
}

#else // UNIX

#include <time.h>
#include <sys/time.h>
double get_cpu_time()
{
    return (double)clock() / CLOCKS_PER_SEC;
}

#endif // if/else WIN32/UNIX

double UniformRand(double min, double max)
{
    double range = max - min;
    return (((((double)rand()) * range) / ((double)RAND_MAX)) + min); 
}


class TestTimerMgr : public ProtoTimerMgr
{
    public:
         bool UpdateSystemTimer(ProtoTimer::Command command,
                                double              delay) {return true;} 
    
        void OnTimeout(ProtoTimer& theTimer)
        {
            //fprintf(stderr, "timer timeout interval: %lf\n", theTimer.GetInterval());
            count++;
        }
        
        unsigned int count;
};

int main(int argc, char* argv[])
{
    TestTimerMgr mgr;
    
    mgr.count = 0;
    
    fprintf(stderr, "sizeof(ProtoTimer) = %lu bytes\n", (unsigned long)sizeof(ProtoTimer));
    ProtoTime t1, t2;
    
    /*for (int size = 2; size < 5000; size *= 1.5)
    {
        double total = 0.0;
        ProtoTimer* array = new ProtoTimer[size];
        int trials = 5000;
        for (int k = 0; k < trials; k++)
        {
            t1.GetCurrentTime();
            for (int i = 0; i < size; i++)
            {
                double interval = UniformRand(0.0, 8.0);
                array[i].SetInterval(interval);
                mgr.ActivateTimer(array[i]);
            }
            t2.GetCurrentTime();
            total += ProtoTime::Delta(t2, t1) / (double)size;
            for (int i = 0; i < size; i++)
                array[i].Deactivate();
        }
        delete[] array;
        double ave = total / (double)trials;
        //printf("size:%d items average time: %lf seconds\n", size, ave);
        printf("%d, %e\n", size/2, ave);
    }
    */
    
    // Run a bunch of very short timouts and benchmark
    
    int trials = 10;
    //int size = 10000;
    
    for (int size = 2; size < 100000; size *= 1.5)
    {
        double total = 0.0;
        for (int k = 0; k < trials; k++)
        {
            ProtoTimer* array = new ProtoTimer[size];
            for (int i = 0; i < size; i++)
            {
                double interval = UniformRand(10.0e-06, 100.0e-06);
                array[i].SetInterval(interval);
                array[i].SetRepeat(-1);
                array[i].SetListener(&mgr, &TestTimerMgr::OnTimeout);
                mgr.ActivateTimer(array[i]);
            }

            t1.GetCurrentTime();
            double elapsed = 0.0;
            double c1 = get_cpu_time();
            while (elapsed < 0.5)
            {
                double wait = mgr.GetTimeRemaining();
                struct timespec ts;
                if (wait < 0)
                {
                    ts.tv_sec = 1;
                    ts.tv_nsec = 0;
                }
                else
                {
                    ts.tv_sec = (time_t)wait;
                    ts.tv_nsec = (wait - (time_t)wait)*1.0e+09;
                }
                //fprintf(stderr, "waiting %lf sec ...\n", wait);
                nanosleep(&ts, NULL);
                mgr.OnSystemTimeout();
                t2.GetCurrentTime();
                elapsed = ProtoTime::Delta(t2, t1);
            }
            double c2 = get_cpu_time();
            total += c2 - c1;

            for (int i = 0; i < size; i++)
                array[i].Deactivate();
            delete[] array;
        }
        printf("%d, %lf\n", size, total / (double)trials);
        fprintf(stderr, "%d, %lf\n", size, total / (double)trials);
    }
    
}  // end main()

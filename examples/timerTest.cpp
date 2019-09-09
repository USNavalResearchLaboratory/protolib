#include "protoApp.h"
#include "protoTimer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


// Simple self-scaling linear/non-linear histogram (one-sided)
class Histogram
{
    public:
        Histogram();
        bool IsEmpty() {return (NULL == bin);}
        void Init(unsigned long numBins, double linearity)
        {
            num_bins = numBins;
            q = linearity;
            if (bin) delete[] bin;
            bin = NULL;
        }
        bool InitBins(double rangeMin, double rangeMax);
        bool Tally(double value, unsigned long count = 1);
        void Print(FILE* file, bool showAll = false);
        unsigned long Count();
        double PercentageInRange(double rangeMin, double rangeMax);
        unsigned long CountInRange(double rangeMin, double rangeMax);
        double Min() {return min_val;}
        double Max() {return ((max_val < 0.0) ? 2.0*max_val : 0.5*max_val);}
        double Percentile(double p);
               
    private:   
            
        double GetBinValue(unsigned int i)
        {
            if (bin && bin[i].count)
            {
                return (bin[i].total / ((double)bin[i].count));
            }
            else
            {
                double x = pow(((double)i) / ((double)num_bins-1), 1.0/q);
                x *= (max_val - min_val);
                x += min_val;
                return x;  
            }
        }
              
        typedef struct
        {
            double          total;
            unsigned long   count;
        } Bin;
        
        double          q;
        unsigned long   num_bins;
        double          min_val;
        double          max_val;  
        Bin*            bin;           
}; // end class Histogram


class TimerTestApp : public ProtoApp
{
    public:
      TimerTestApp();
      ~TimerTestApp();

      bool OnStartup(int argc, const char*const* argv);
      bool ProcessCommands(int argc, const char*const* argv);
      void OnShutdown();

    private:
      enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
      static const char* const CMD_LIST[];
      static CmdType GetCmdType(const char* string);
      bool OnCommand(const char* cmd, const char* val);        
      void Usage();
      
      bool OnTimeout(ProtoTimer& theTimer);
      
      ProtoTimer    the_timer;
      
      bool          first_timeout;
      unsigned long timeout_count;
      double        ave_sum;
      double        squ_sum;
      double        delta_min;
      double        delta_max;
      double        elapsed_sec;
      
      Histogram     histogram;
      
      struct timeval last_time;
      
      
      bool          use_nanosleep;
      

}; // end class TimerTestApp

void TimerTestApp::Usage()
{
    fprintf(stderr, "Usage: timerTest [help] interval <timerInterval>\n");
}

const char* const TimerTestApp::CMD_LIST[] =
{
    "-help",        // print help info an exit
    "+interval",    // <timerInterval>
    "-nanosleep",   
    "-priority", 
    "-precise",
    "+debug",       // <debugLevel>
    NULL
};

/**
 * This macro creates our ProtoApp derived application instance 
 */
PROTO_INSTANTIATE_APP(TimerTestApp) 

TimerTestApp::TimerTestApp()
    : first_timeout(true)
{
    the_timer.SetListener(this, &TimerTestApp::OnTimeout);
    the_timer.SetInterval(1.0);
    the_timer.SetRepeat(-1);
}

TimerTestApp::~TimerTestApp()
{
}

TimerTestApp::CmdType TimerTestApp::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    unsigned int len = strlen(cmd);
    bool matched = false;
    CmdType type = CMD_INVALID;
    const char* const* nextCmd = CMD_LIST;
    while (*nextCmd)
    {
        if (!strncmp(cmd, *nextCmd+1, len))
        {
            if (matched)
            {
                // ambiguous command (command should match only once)
                return CMD_INVALID;
            }
            else
            {
                matched = true;   
                if ('+' == *nextCmd[0])
                    type = CMD_ARG;
                else
                    type = CMD_NOARG;
            }
        }
        nextCmd++;
    }
    return type; 
}  // end TimerTestApp::GetCmdType()


bool TimerTestApp::OnStartup(int argc, const char*const* argv)
{
    
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "TimerTestApp::OnStartup() error processing command line options\n");
        return false;   
    }
    
    histogram.Init(1000, 1.0);

#ifdef LINUX    
    if (use_nanosleep)
    {
        struct timespec tspec;
        double interval = the_timer.GetInterval();
        tspec.tv_sec = (unsigned long)interval;
        tspec.tv_nsec = (unsigned long)(1.0e+09 * (interval - (double)tspec.tv_sec));
        while (1)
        {
            OnTimeout(the_timer);
            if (0 != clock_nanosleep(CLOCK_MONOTONIC, 0, &tspec, NULL))
            {
                if (EINTR == errno)
                {
                    TRACE("timerTest error: clock_nanosleep() EINTR error: %s\n", GetErrorString());
                    
                }
                else
                {
                    TRACE("timerTest error: clock_nanosleep() error: %s\n", GetErrorString());
                    break;
                }
            }
        }
        return true;
    }
    else
#endif // LINUX
    {
        ActivateTimer(the_timer);
    }
    
    return true;
}  // end TimerTestApp::OnStartup()

void TimerTestApp::OnShutdown()
{
   histogram.Print(stdout);
   PLOG(PL_ERROR, "timerTest: Done.\n"); 
}  // end TimerTestApp::OnShutdown()

bool TimerTestApp::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a timerTest command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                PLOG(PL_ERROR, "TimerTestApp::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                Usage();
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "TimerTestApp::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    Usage();
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "TimerTestApp::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    Usage();
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end TimerTestApp::ProcessCommands()

bool TimerTestApp::OnCommand(const char* cmd, const char* val)
{
    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    size_t len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "TimerTestApp::ProcessCommand(%s) missing argument\n", cmd);
        Usage();
        return false;
    }
    else if (!strncmp("help", cmd, len))
    {
        Usage();
        exit(0);
    }
    else if (!strncmp("nanosleep", cmd, len))
    {
        use_nanosleep = true;
    }
    else if (!strncmp("priority", cmd, len))
    {
        dispatcher.SetPriorityBoost(true);
        dispatcher.BoostPriority();
    }
    else if (!strncmp("precise", cmd, len))
    {
        dispatcher.SetPreciseTiming(true);
    }
    else if (!strncmp("interval", cmd, len))
    {
        float timerInterval;
        if (1 != sscanf(val, "%e", &timerInterval))
        {
            PLOG(PL_ERROR, "TimerTestApp::OnCommand(interval) error: invalid argument\n");
            return false;
        }
        the_timer.SetInterval((double)timerInterval);
    }
    else if (!strncmp("debug", cmd, len))
    {
        SetDebugLevel(atoi(val));
    }
    else
    {
        PLOG(PL_ERROR, "timerTest error: invalid command\n");
        Usage();
        return false;
    }
    return true;
}  // end TimerTestApp::OnCommand()


bool TimerTestApp::OnTimeout(ProtoTimer& /*theTimer*/)
{
    struct timeval currentTime;
    ProtoSystemTime(currentTime);
    if (first_timeout)
    {
        elapsed_sec = 0.0;
        first_timeout = false;
        ave_sum = 0.0;
        squ_sum = 0.0;
        timeout_count = 0;
        delta_min = 1000.0;
        delta_max = 0.0;
    }
    else
    {
        double delta = currentTime.tv_sec - last_time.tv_sec;
        delta += 1.0e-06 * (currentTime.tv_usec - last_time.tv_usec);
        
        histogram.Tally(delta);
        
        if (delta > delta_max)
            delta_max = delta;
        if (delta < delta_min)
            delta_min = delta;
        
        elapsed_sec += delta;
        timeout_count++;
        
        ave_sum += delta;
        squ_sum += (delta *delta);
        
        if (elapsed_sec > 5.0)
        {
            double ave = ave_sum / timeout_count;
            double var = (squ_sum - (ave_sum * ave)) / timeout_count;
            
            TRACE("timer interval: ave>%lf min>%lf max>%lf var>%lf\n",
                    ave, delta_min, delta_max, var);
            
            elapsed_sec = 0.0;
        }
    }
    last_time = currentTime;
    return true;   
}  // end TimerTestApp::OnTimeout()


////////////////////////////////////////////////////////////////////
//
// Histogram implementation
//

Histogram::Histogram()
 : q(1.0), num_bins(1000), min_val(0.0), max_val(0.0), bin(NULL)
{
}

/**  This method creates an empty histogram with a preset
  *  value range.  This is useful for outputting 
  *  equivalent histgrams for multiplots
  */
bool Histogram::InitBins(double rangeMin, double rangeMax)
{
    if (bin) delete[] bin;
    if (!(bin = new Bin[num_bins]))
    {
        perror("hcat: Histogram::InitBins() Error allocating bins");
        return false;   
    } 
    memset(bin, 0, num_bins*sizeof(Bin));
    min_val = rangeMin;
    max_val = rangeMax;
    return true;
}  // end Histogram::InitBins()

bool Histogram::Tally(double value, unsigned long count)
{
    if (!bin)
    {
        if (!(bin = new Bin[num_bins]))
        {
            perror("trpr: Histogram::Tally() Error allocating histogram");
            return false;   
        }
        memset(bin, 0, num_bins*sizeof(Bin));
        min_val = max_val = value;
        bin[0].count = count;
        bin[0].total = (value * (double)count);
    }
    else if ((value > max_val) || (value < min_val))
    {
        Bin* newBin = new Bin[num_bins];
        if (!newBin)
        {
            perror("trpr: Histogram::Tally() Error reallocating histogram");
            return false; 
        }
        memset(newBin, 0, num_bins*sizeof(Bin));

        double newScale, minVal;
        if (value < min_val)
        {        
            newScale = ((double)(num_bins-1)) / pow(max_val - value, q);
            minVal = value;
        }
        else
        {
            double s = (value < 0.0) ? 0.5 : 2.0;   
            newScale = ((double)(num_bins-1)) / pow(s*value - min_val, q);
            minVal = min_val;
        }
        
        // Copy old histogram bins into new bins
        for (unsigned int i = 0; i < num_bins; i++)
        {
            if (bin[i].count)
            {
                double x = bin[i].total / ((double)bin[i].count);
                unsigned long index = (unsigned long)ceil(newScale * pow(x - minVal, q));
                if (index > (num_bins-1)) index = num_bins - 1;
                newBin[index].count += bin[i].count;
                newBin[index].total += bin[i].total;
            }   
        }
        
        
        if (value < min_val)
        {
            newBin[0].count += count;
            newBin[0].total += (value * (double)count);
            min_val = value;
        }
        else
        {
            double s = (value < 0.0) ? 0.5 : 2.0;   
            max_val = s*value;
            unsigned long index = 
                (unsigned long)ceil(((double)(num_bins-1)) * pow((value-min_val)/(max_val-min_val), q));        
            if (index > (num_bins-1)) index = num_bins - 1;
            newBin[index].count += count;
            newBin[index].total += (value * (double)count);
        }
        delete[] bin;
        bin = newBin;
    }
    else
    {
        unsigned long index = 
            (unsigned long)ceil(((double)(num_bins-1)) * pow((value-min_val)/(max_val-min_val), q));        
        if (index > (num_bins-1)) index = num_bins - 1;
        bin[index].count += count;
        bin[index].total += (value * (double)count);
    }
    return true;
}  // end Histogram::Tally()

void Histogram::Print(FILE* file, bool showAll)
{
    if (bin)
    {
        for (unsigned int i = 0; i < num_bins; i++)
        {
            if ((0 != bin[i].count) || showAll)
            {
                fprintf(file, "%f, %lu\n", GetBinValue(i), bin[i].count);    
            }
        }
    }
}  // end Histogram::Print()


unsigned long Histogram::Count()
{
    if (bin)
    {
        unsigned long total = 0 ;
        for (unsigned int i = 0; i < num_bins; i++)
        {
            total += bin[i].count;
        }
        return total;
    }
    else
    {
         return 0;
    }   
}  // end Histogram::Count()

double Histogram::PercentageInRange(double rangeMin, double rangeMax)
{
    if (bin)
    {
        unsigned long countTotal = 0;
        unsigned long rangeTotal = 0;
        for (unsigned long i = 0; i < num_bins; i++)
        {
            if (bin[i].count)
            {
                double value = bin[i].total / ((double)bin[i].count);
                countTotal += bin[i].count;
                if (value < rangeMin)
                    continue;
                else if (value > rangeMax)
                    continue;
                else
                    rangeTotal += bin[i].count;
            }
        }
        return (100.0 * ((double)rangeTotal) / ((double)countTotal));
    }
    else
    {
        return 0.0;
    }         
}  // end Histogram::PercentageInRange()

unsigned long Histogram::CountInRange(double rangeMin, double rangeMax)
{
    if (bin)
    {
        unsigned long rangeTotal = 0;
        for (unsigned long i = 0; i < num_bins; i++)
        {
            if (bin[i].count)
            {
                double value = bin[i].total / ((double)bin[i].count);
                if (value < rangeMin)
                    continue;
                else if (value > rangeMax)
                    break;
                else
                    rangeTotal += bin[i].count;
            }
        }
        return rangeTotal;
    }
    else
    {
        return 0;
    }         
}  // end Histogram::CountInRange()


double Histogram::Percentile(double p)
{
    unsigned long goal = Count();
    goal = (unsigned long)(((double)goal) * p + 0.5);
    unsigned long count = 0;
    if (bin)
    {
        for (unsigned long i = 0; i < num_bins; i++)
        {
            count += bin[i].count;
            if (count >= goal)
            {
                double x = pow(((double)i) / ((double)num_bins-1), 1.0/q);
                x *= (max_val - min_val);
                x += min_val;
                return x;   
            }
        }
    }
    return max_val;
}  // end Histogram::Percentile()



/**
* @file protoTime.cpp 
* 
* @brief Provides parsing and comparison time operations.
 
*/
#include "protoTime.h"
#include "protoDebug.h"

#ifndef SIMULATE
static const ProtoTime PROTO_TIME_INIT = ProtoTime().GetCurrentTime();
#else
static const ProtoTime PROTO_TIME_INIT(0);
#endif

ProtoTime::ProtoTime()
{
    tval.tv_sec = tval.tv_usec = 0;
}

ProtoTime::ProtoTime(const struct timeval& timeVal)
 : tval(timeVal)
{
}

ProtoTime::ProtoTime(double seconds)
{
    //TRACE("enter ProtoTime::ProtoTime(%lf seconds) ctor ...\n", seconds);
    tval.tv_sec = (unsigned long)seconds;
    tval.tv_usec = (unsigned long)(1.0e+06 * (seconds - ((double)tval.tv_sec)));
    
    //TRACE("   tval.tv_sec = %lu\n", tval.tv_sec);
}

ProtoTime::ProtoTime(unsigned long sec, unsigned long usec)
{
    tval.tv_sec = sec;
    tval.tv_usec = usec;
}


void ProtoTime::operator+=(const ProtoTime& t)
{
    tval.tv_sec += t.sec();
    tval.tv_usec += t.usec();
    if (tval.tv_usec >= 1000000)
    {
        tval.tv_sec++;
        tval.tv_usec -= 1000000;
    }
}  // end ProtoTime::operator+=() 

void ProtoTime::operator-=(double sec)
{
    unsigned long secInt = (unsigned long)sec;
    if ((unsigned long)tval.tv_sec < secInt)
    {
        tval.tv_sec = tval.tv_usec = 0;
    }
    else if ((unsigned long)tval.tv_sec == secInt)
    {
        tval.tv_sec = 0;
        unsigned long usecInt = (unsigned long)(1.0e+06*(sec - (double)secInt) + 05);
        if ((unsigned long)tval.tv_usec < usecInt)
            tval.tv_usec = 0;
        else
            tval.tv_usec -= usecInt;
    }
    else
    {
        unsigned long usecInt = (unsigned long)(1.0e+06*(sec - (double)secInt) + 05);
        if ((unsigned long)tval.tv_usec < usecInt)
        {
            tval.tv_sec--;  // borrow
            tval.tv_usec += 1000000 - usecInt;
        }
        tval.tv_sec -= secInt;
    }
}  // end ProtoTime::operator-=()

double ProtoTime::GetOffsetValue() const
{
    return ProtoTime::Delta(*this, PROTO_TIME_INIT);
}

double ProtoTime::Delta(const ProtoTime& t1, const ProtoTime& t2)
{
    // (TBD) perhaps we should have "long" instead "unsigned long"
    // sec()/usec() methods to simplify this?
    double delta = (t1.sec() >= t2.sec()) ?
                        (double)(t1.sec() - t2.sec()) :
                        -(double)(t2.sec() - t1.sec());
    if (t1.usec() > t2.usec())
        delta += 1.0e-06 * (double)(t1.usec() - t2.usec());
    else
        delta -= 1.0e-06 * (double)(t2.usec() - t1.usec());
    return delta;
}  // end ProtoTime::Delta()


#ifdef WIN32
// These are state variables that are needed to manage
// use of the WIN32 High Performance Counter to get more
// precise timing (needed for WinCE)
bool proto_performance_counter_init = false;
LARGE_INTEGER proto_performance_counter_frequency = {0, 0};
#ifdef _WIN32_WCE
long proto_performance_counter_offset = 0;
long proto_system_time_last_sec = 0;
unsigned long proto_system_count_roll_sec = 0;
LARGE_INTEGER proto_system_count_last = {0, 0};
#endif // _WIN32_WCE
#endif // WIN32




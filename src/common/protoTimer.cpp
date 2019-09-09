/**
* @file protoTimer.cpp
* 
* @brief A generic timer class which will notify a ProtoTimer::Listener upon timeout.
*/


#include "protoTimer.h"
#include "protoDebug.h"

#include <stdio.h>  // for getchar() debug

/**
* @brief Default constructor
*
* @param listener(NULL)
* @param interval(1.0)
* @param repeat(0)
* @param repeat_count(0)
* @param mgr(NULL)
* @param prev(NULL)
* @param next(NULL)
*/
ProtoTimer::ProtoTimer()
 : listener(NULL), interval(1.0), repeat(0), repeat_count(0),
   mgr(NULL), prev(NULL), next(NULL)
{

}
/**
* Default Destructor
*
* Deactivates timer and deletes any associated listener.
*/

ProtoTimer::~ProtoTimer()
{
    if (IsActive()) Deactivate();  
    if (listener)
    {
        delete listener;
        listener = NULL;
    } 
}
/**
* Reschedules the timer in the timer queue according to the (modified?) timer
* interval. (also resets repeat count)
*
*/
bool ProtoTimer::Reschedule()
{
    ASSERT(IsActive());
    if (IsActive())
    {
        ProtoTimerMgr* timerMgr = mgr;
        bool updatePending = timerMgr->update_pending;
        timerMgr->update_pending = true;
        timerMgr->DeactivateTimer(*this);
        timerMgr->update_pending = updatePending;
        timerMgr->ActivateTimer(*this);
        return true;
    }
    else
    {
        PLOG(PL_ERROR, "ProtoTimer::Reschedule() error: timer not active\n");
        return false;
    }
}  // end ProtoTimer::Reschedule()

/**
 * This method stretches (factor > 1.0) or
 * compresses the timer interval, rescheduling
 * the timer if it is active
 * (note the repeat count is not impacted)
 */
void ProtoTimer::Scale(double factor)
{
    if (IsActive())
    {
        // Calculate, reschedule and then adjust interval
        double newInterval = factor*interval;
        double timeRemaining = GetTimeRemaining();
        if (timeRemaining > 0.0)
        {
            interval = factor*timeRemaining;
            int repeatCountSaved = repeat_count;
            Reschedule();
            repeat_count = repeatCountSaved;
        }
        interval = newInterval;
    }
    else
    {
        interval *= factor;   
    }
}  // end ProtoTimer::Scale()

void ProtoTimer::Deactivate()
{
    ASSERT(IsActive());
    mgr->DeactivateTimer(*this);   
}
/**
* Get the time remaining for the timer according to current "proto time".
*/
double ProtoTimer::GetTimeRemaining() const
{
    if (NULL != mgr)
    {
        ProtoTime currentTime;
        mgr->GetCurrentProtoTime(currentTime);
        double timeRemaining = ProtoTime::Delta(timeout, currentTime);
        if (timeRemaining < 0.0) timeRemaining = 0.0;
        return timeRemaining;
    }
    else
    {
        return -1.0;
    }
}  // end ProtoTimer::GetTimeRemaining()

/**
 *  This class manages ProtoTimer instances when they are
 *  "activated". The ProtoDispatcher derives from this to manage
 *  ProtoTimers for an application.  (The ProtoSimAgent base class
 *  contains a ProtoTimerMgr to similarly manage timers for a simulation
 *  instance).
 */
ProtoTimerMgr::ProtoTimerMgr()
: update_pending(false), timeout_scheduled(false),
  long_head(NULL), long_tail(NULL), short_head(NULL), short_tail(NULL)
{
    pulse_timer.SetListener(this, &ProtoTimerMgr::OnPulseTimeout);
    pulse_timer.SetInterval(1.0);
    pulse_timer.SetRepeat(-1);
}

ProtoTimerMgr::~ProtoTimerMgr()
{
    // (TBD) Uninstall or halt, deactivate all timers ...   
}
/**
* Calls inlined ProtoSystemTime function
*/
void ProtoTimerMgr::GetSystemTime(struct timeval& currentTime)
{
    ::ProtoSystemTime(currentTime);
}  // end ProtoTimerMgr::GetSystemTime()

const double ProtoTimerMgr::PRECISION_TIME_THRESHOLD = 8.0;

/**
*
*/
void ProtoTimerMgr::OnSystemTimeout()
{
    timeout_scheduled = false;
    bool updateStatus = update_pending;
    update_pending = true;
    ProtoTimer* next = short_head;
    ProtoTime now;
    GetCurrentProtoTime(now);
    while (next)
    {
        double delta = ProtoTime::Delta(next->timeout, now);
        // We limit to within a microsecond of accuracy on 
        // real-world systems to avoid overzealous attempts
        // at scheduling
        if (delta < 1.0e-06)
        {
            if(next->DoTimeout())
            {
                if (next->IsActive())
                {
                    RemoveShortTimer(*next);
                    int repeatCount = next->repeat_count;
                    if (0 != repeatCount) 
                    {
                        ReactivateTimer(*next, now);
                        if (repeatCount > 0) repeatCount--;
                        next->repeat_count = repeatCount;
                    }
                }
            }
            next = short_head;
        } 
        else
        {
            next = NULL;
        }
    }
    update_pending = updateStatus;
    if (!updateStatus) Update();
}  // ProtoTimerMgr::OnSystemTimeout()

bool ProtoTimerMgr::OnPulseTimeout(ProtoTimer& /*theTimer*/)
{
    ProtoTimer* next = long_head;
    pulse_mark += 1.0;
    while (next)
    {
        double delta = ProtoTime::Delta(next->timeout, pulse_mark);
        if (delta < PRECISION_TIME_THRESHOLD)
        {
            ProtoTimer& current = *next;
            next = next->next;
            RemoveLongTimer(current);
            GetCurrentProtoTime(current.timeout);
            current.timeout += delta;
            InsertShortTimer(current);
        }
        else
        {
            break;   
        }
    }
    if (NULL == long_head)
    {
        DeactivateTimer(pulse_timer);
        return false;
    }
    else
    {
        return true;
    }
}  // end ProtoTimerMgr::OnPulseTimeout()

void ProtoTimerMgr::ActivateTimer(ProtoTimer& theTimer)
{
    ASSERT(!theTimer.IsActive());
    double timerInterval = theTimer.GetInterval();
    if (PRECISION_TIME_THRESHOLD > timerInterval)
    {       
        GetCurrentProtoTime(theTimer.timeout);
        theTimer.timeout += timerInterval;
        InsertShortTimer(theTimer);
    }
    else
    {
        if (!pulse_timer.IsActive())
        {
            GetCurrentProtoTime(pulse_mark);
            bool updateStatus = update_pending;
            update_pending = true;
            ActivateTimer(pulse_timer);
            update_pending = updateStatus;
        }
        theTimer.timeout = pulse_mark;
        theTimer.timeout += timerInterval + 1.0 - pulse_timer.GetTimeRemaining();
        InsertLongTimer(theTimer);   
    }
    theTimer.repeat_count = theTimer.repeat;
    if (!update_pending) Update();
}  // end ProtoTimerMgr::ActivateTimer()

void ProtoTimerMgr::ReactivateTimer(ProtoTimer& theTimer, const ProtoTime& now)
{
    double timerInterval = theTimer.interval;
    if (PRECISION_TIME_THRESHOLD > timerInterval)
    {
        theTimer.timeout += timerInterval;
        double delta = ProtoTime::Delta(theTimer.timeout, now);
        if (delta < -1.0)
        {
            GetCurrentProtoTime(theTimer.timeout);
            PLOG(PL_ERROR, "ProtoTimerMgr: Warning! real time failure interval:%lf (delta:%lf)\n", 
                           timerInterval, delta);
        }   
        InsertShortTimer(theTimer);
    }
    else
    {
        if (!pulse_timer.IsActive())
        {
            GetCurrentProtoTime(pulse_mark);
            bool updateStatus = update_pending;
            update_pending = true;
            ActivateTimer(pulse_timer);
            update_pending = updateStatus;
        }
        GetPulseTime(theTimer.timeout);
        theTimer.timeout += timerInterval;
        InsertLongTimer(theTimer);   
    }        
    if (!update_pending) Update();
}  // end ProtoTimerMgr::ReactivateTimer(()

void ProtoTimerMgr::DeactivateTimer(ProtoTimer& theTimer)
{
    if (theTimer.mgr == this)
    {
        if (theTimer.is_precise)
        {
            RemoveShortTimer(theTimer);
        }
        else
        {
            RemoveLongTimer(theTimer);
            if (NULL == long_head) 
            {
                bool updateStatus = update_pending;
                update_pending = true;
                DeactivateTimer(pulse_timer); 
                update_pending = updateStatus;
            }
        }
        if (!update_pending) Update();
    }
}  // end ProtoTimerMgr::DeactivateTimer()

void ProtoTimerMgr::Update()
{
    if (NULL == short_head)
    {
        // REMOVE existing scheduled system timeout if applicable
        if (timeout_scheduled)
        {
            if (!UpdateSystemTimer(ProtoTimer::REMOVE, -1.0))
                PLOG(PL_ERROR, "ProtoTimerMgr::Update() error: scheduled system timeout REMOVE failure\n");
            timeout_scheduled = false;
        }
    }
    else if (timeout_scheduled)
    {
        // MODIFY existing scheduled system timeout if different
        if (scheduled_timeout != short_head->timeout)
        {
            if (UpdateSystemTimer(ProtoTimer::MODIFY, short_head->GetTimeRemaining()))
            {
                scheduled_timeout = short_head->timeout;
            }
            else  // (TBD) if MODIFY fails, do we still have a system timeout ???
            {
                PLOG(PL_ERROR, "ProtoTimerMgr::Update() error: scheduled system timeout MODIFY failure\n");
                timeout_scheduled = false;  // (TBD) ???
            }
        }
    }
    else 
    {
        // INSTALL new scheduled system timeout
        if (UpdateSystemTimer(ProtoTimer::INSTALL, short_head->GetTimeRemaining()))
        {
                scheduled_timeout = short_head->timeout;
                timeout_scheduled = true;
        }  
        else
        {
            PLOG(PL_ERROR, "ProtoTimerMgr::Update() error: scheduled system timeout INSTALL failure\n");
        }
    }
}  // end ProtoTimerMgr::Update()

void ProtoTimerMgr::InsertShortTimer(ProtoTimer& theTimer)
{
    unsigned breakCount = 0;
    ProtoTimer* next  = short_head;
    theTimer.mgr = this;
    theTimer.is_precise = true;
    while(next)
    {
        double delta = ProtoTime::Delta(theTimer.timeout, next->timeout);
        if (delta <= 0.0)
        {
            theTimer.next = next;
            if(NULL != (theTimer.prev = next->prev))
                theTimer.prev->next = &theTimer;
            else
                short_head = &theTimer;
            next->prev = &theTimer;
            return;
        }
        else
        {
            next = next->next;
        }
        breakCount++;
        if(breakCount == 10)//go ahead and try to add the entry to the end of the list
        //if(breakCount == 100000)//go ahead and try to add the entry to the end of the list
        {
            if(InsertShortTimerReverse(theTimer))
            {
                //The entry was succesfully added to the end so we are done
                return;
            }
        }
        ASSERT(breakCount < 5000);
    }
    if (NULL != (theTimer.prev = short_tail))
        short_tail->next = &theTimer;
    else
        short_head = &theTimer;
    short_tail = &theTimer;
    theTimer.next = NULL;
}  // end ProtoTimerMgr::InsertShortTimer()

bool ProtoTimerMgr::InsertShortTimerReverse(ProtoTimer& theTimer)
{
    unsigned breakCount = 0;
    ProtoTimer* prev  = short_tail;
    theTimer.mgr = this;
    theTimer.is_precise = true;
    while(prev)
    {
        double delta = ProtoTime::Delta(theTimer.timeout, prev->timeout);
        if (delta > 0.0)
        {
            if(NULL == (theTimer.next = prev->next))
                short_tail = &theTimer;
            else
                theTimer.next->prev = &theTimer;
            theTimer.prev = prev;
            prev->next = &theTimer;
            //DMSG(0,"bunny Reverse breakcount was %d with timeout time %f\n",breakCount,theTimer.timeout.GetValue());
            return true;
        }
        else
        {
            prev = prev->prev;
        }
        breakCount++;
        if(breakCount == 10)
        {
            return false;
        }
    }
    if (NULL == (theTimer.next = short_head))
        short_tail = &theTimer;
    else
        short_head->prev = &theTimer;
    short_head = &theTimer;
    theTimer.prev = NULL;
    return true;
}  // end ProtoTimerMgr::InsertShortTimerReverse()

void ProtoTimerMgr::RemoveShortTimer(ProtoTimer& theTimer)
{
    if (theTimer.prev)
        theTimer.prev->next = theTimer.next;
    else
        short_head = theTimer.next;
    if (theTimer.next)
        theTimer.next->prev = theTimer.prev;
    else
        short_tail = theTimer.prev;
    theTimer.mgr = NULL;
}  // end ProtoTimerMgr::RemoveShortTimer()

void ProtoTimerMgr::InsertLongTimer(ProtoTimer& theTimer)
{
    unsigned breakCount = 0;
    ProtoTimer* next  = long_head;
    theTimer.mgr = this;
    theTimer.is_precise = false;
    while(next)
    {
        double delta = ProtoTime::Delta(theTimer.timeout, next->timeout);
        if (delta <= 0.0)
        {
            theTimer.next = next;
            if((theTimer.prev = next->prev))
                theTimer.prev->next = &theTimer;
            else
                long_head = &theTimer;
            next->prev = &theTimer;
            return;
        }
        else
        {
            next = next->next;
        }
        breakCount++;
        if(breakCount == 10)//go ahead and try to add the entry to the end of the list
        {
            if(InsertLongTimerReverse(theTimer))
            {
                //The entry was succesfully added to the end so we are done
                return;
            }
        }
    }
    if ((theTimer.prev = long_tail))
        long_tail->next = &theTimer;
    else
        long_head = &theTimer;
    long_tail = &theTimer;
    theTimer.next = NULL;
}  // end ProtoTimerMgr::InsertLongTimer()

bool ProtoTimerMgr::InsertLongTimerReverse(ProtoTimer& theTimer)
{
    unsigned breakCount = 0;
    ProtoTimer* prev  = long_tail;
    theTimer.mgr = this;
    theTimer.is_precise = false;
    while(prev)
    {
        double delta = ProtoTime::Delta(theTimer.timeout, prev->timeout);
        if (delta > 0.0)
        {
            if(NULL == (theTimer.next = prev->next))
                long_tail = &theTimer;
            else
                theTimer.next->prev = &theTimer;
            theTimer.prev = prev;
            prev->next = &theTimer;
            return true;
        }
        else
        {
            prev = prev->prev;
        }
        breakCount++;
        if(breakCount == 10)
        {
            return false;
        }
    }
    if (NULL == (theTimer.next = long_head))
        long_tail = &theTimer;
    else
        long_head->prev = &theTimer;
    long_head = &theTimer;
    theTimer.prev = NULL;
    return true;
}  // end ProtoTimerMgr::InsertLongTimerReverse()

void ProtoTimerMgr::RemoveLongTimer(ProtoTimer& theTimer)
{
    if (theTimer.prev)
        theTimer.prev->next = theTimer.next;
    else
        long_head = theTimer.next;
    if (theTimer.next)
        theTimer.next->prev = theTimer.prev;
    else
        long_tail = theTimer.prev;
    theTimer.mgr = NULL;
}  // end ProtoTimerMgr::RemoveLongTimer()

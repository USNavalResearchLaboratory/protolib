#ifndef _PROTO_TIME
#define _PROTO_TIME

#include "protoDefs.h"

#ifdef WIN32
#include <winsock2.h>
typedef long suseconds_t;
#else  // UNIX
#include <arpa/inet.h>  // for htonl()
#endif  // if/else WIN32

/**
 * @class ProtoTime
 *
 * @brief System time conversion routines.
 */

class ProtoTime
{
    public:
        ProtoTime();
        ProtoTime(const struct timeval& theTime);
        ProtoTime(double seconds);
        ProtoTime(unsigned long sec, unsigned long usec);
            
        ProtoTime& GetCurrentTime() 
        {
            ProtoSystemTime(tval);
            return *this;
        }
    
        unsigned long sec() const
            {return ((unsigned long)tval.tv_sec);}
        unsigned long usec() const
            {return ((unsigned long)tval.tv_usec);}
        unsigned long nsec() const
            {return ((unsigned long)(1000* tval.tv_usec));}
        
        double GetValue() const
        {
            double val = (double)sec();
            val += 1.0e-06 * (double)usec();
            return val;
        }
        double GetOffsetValue() const;
        
	    bool IsZero() const
	        {return ((0 == tval.tv_sec) && (0 == tval.tv_usec));}
        void Zeroize()
            {tval.tv_sec = tval.tv_usec = 0;}

		// for debugging
        void Invalidate()
            {Zeroize();}
		bool IsValid() const
		{
			return ((tval.tv_sec > 0) || (tval.tv_usec > 0));
		}       
        
        const struct timeval& GetTimeVal() const
            {return tval;}
        struct timeval& AccessTimeVal()
            {return tval;}
        
        void operator+=(const ProtoTime& t);
        void operator+=(double seconds);
        void operator-=(double seconds)
            {return operator+=(-seconds);}
        bool operator==(const ProtoTime& t) const
        {
            return ((sec() == t.sec()) &&
                    (usec() == t.usec()));
        }
        bool operator!=(const ProtoTime& t) const
            {return !(*this == t);}
        bool operator>(const ProtoTime& t) const
        {
            return ((sec() > t.sec()) ||
                     ((sec() == t.sec()) &&
                      (usec() > t.usec())));
        }
        bool operator>=(const ProtoTime& t) const
        {
            return ((sec() > t.sec()) ||
                     ((sec() == t.sec()) &&
                      (usec() >= t.usec())));
        }
        bool operator<(const ProtoTime& t) const
        {
            return ((sec() < t.sec()) ||
                     ((sec() == t.sec()) &&
                      (usec() < t.usec())));
        }
        bool operator<=(const ProtoTime& t) const
        {
            return ((sec() < t.sec()) ||
                     ((sec() == t.sec()) &&
                      (usec() <= t.usec())));
        }
        
        // Computes (t1 - t2)
        static double Delta(const ProtoTime& t1, const ProtoTime& t2);
        double operator-(const ProtoTime& t) const
            {return Delta(*this, t);} 
        
        class Key
        {
            public:
                Key() {Invalidate();}
                void SetValue(const ProtoTime& theTime)
                {
                    key.tv_sec = htont(theTime.tval.tv_sec);
                    key.tv_usec = htontu(theTime.tval.tv_usec);
                }   
                void Invalidate()
                {
                    key.tv_sec = 0;
                    key.tv_usec = 1000000;
                }
                bool IsValid() const
                    {return (1000000 != key.tv_usec);}
                
                const char* GetKey() const
                    {return (const char*)&key;}
                unsigned int GetKeysize() const
                    {return ((sizeof(time_t) + sizeof(suseconds_t)) << 3);}

            private:
                // These helper methods convert the tv_sec and tv_usec
                // fields to network byte order (Big Endian) to provide
                // a key suitable for ProtoTree use. (e.g., with ProtoSortedTree,
                // the key can be used to build time-sorted lists.  ProtoTimerMgr
                // uses this for managing timeouts.
                static time_t htont(time_t t)
                {
                    if (8 == sizeof(time_t))
                    {
                        UINT32* t1 = (UINT32*)&t;
                        time_t result;
                        UINT32* t2 = (UINT32*)&result;
                        t2[0] = htonl(t1[1]);
                        t2[1] = htonl(t1[0]);
                        return result;
                    }
                    else // if (4 = sizeof(time_t))
                    {
                        return ((time_t)htonl((UINT32)t));
                    }  
                }  // end ProtoTime::Key::htont()
                static suseconds_t htontu(suseconds_t t)
                {
                    if (8 == sizeof(suseconds_t))
                    {
                        UINT32* t1 = (UINT32*)&t;
                        suseconds_t result;
                        UINT32* t2 = (UINT32*)&result;
                        t2[0] = htonl(t1[1]);
                        t2[1] = htonl(t1[0]);
                        return result;
                    }
                    else // if (4 = sizeof(suseconds_t))
                    {
                        return ((suseconds_t)htonl((UINT32)t));
                    }  
                }  // end htontu()

                struct timeval key;  // network byte ordered version 
               
        };  // end class ProtoTime::Key
    
    private:
        // (TBD) for now we use struct timeval for convenience, but in future
        //       we may want to control the layout of the struct so it can be
        //       used in a ProtoSortedTree as a sortable key.
        //       We also may want to maintain more _precise_ time as well
        struct timeval tval;        
        
};  // end class ProtoTime

#endif // _PROTO_TIME

#ifndef _PROTO_AVERAGE
#define _PROTO_AVERAGE

/**
* @class ProtoAverage
*
* @brief The ProtoAverage uses the proto list class to keep
* a sub averaged record of numbers to be averaged.  This will
* allow for averaging of VERY large sets of numbers with minimal
* rounding and overflow errors along with minimal memory storage.
*/

#include "protoDefs.h"
#include "protoList.h"

class ProtoAverage
{
    public:
        ProtoAverage();
        ~ProtoAverage();
        void Reset();
        bool AddNumber(double number);
        double GetAverage();
        unsigned long GetCount() const;
        void Print();
    private:
        class AverageItem : public ProtoList::Item
        {
            friend class ProtoAverage;
            public:
                AverageItem(double v) : value(v) , depth(0) , factor(1) {}
                double GetValue() const {return value;}
                double GetDepth() const {return depth;}
                double GetFactor() const {return factor;}
                bool Merge(const ProtoAverage::AverageItem &otherItem);
            private:
                double value;
                double depth;
                double factor;
                void SetValue(double v);
                void SetDepth(double d);
        };
        //this is the list used to store the values along with depth information on the values
        class AverageList : public ProtoListTemplate<AverageItem> {};
        AverageList list;
        unsigned long count;
};
#endif

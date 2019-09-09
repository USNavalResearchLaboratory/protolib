#include "protoAverage.h"
#include "protoDebug.h"
#include "math.h"
#include <limits>
using std::numeric_limits;

ProtoAverage::ProtoAverage() : count(0)
{
}
ProtoAverage::~ProtoAverage()
{
}
void
ProtoAverage::Reset()
{
    list.Destroy();
    count = 0;
}
bool
ProtoAverage::AddNumber(double number)
{
    count++;
    //if its the first entry just make new Item and add it to the list
    if(list.IsEmpty())
    {
        AverageItem* newItem = new AverageItem(number);
        list.Prepend(*newItem);
        return true;
    }
    //check the depth of the last value
    if(list.GetHead()->GetDepth()!=0)//first entry is not a single value so just prepend the item
    {
        AverageItem* newItem = new AverageItem(number);
        list.Prepend(*newItem);
    } else { //the head entry was a single value so we need to bubble back the averages
        AverageList::Iterator iterator(list);
        AverageItem tempItem(number);
        AverageItem* firstItemPtr = &tempItem;
        AverageItem* secondItemPtr = list.GetHead();
        while(firstItemPtr->GetDepth()==secondItemPtr->GetDepth())
        {
            if(!secondItemPtr->Merge(*firstItemPtr))
            {
                TRACE("failed merging numbers!\n");
                return false;
            }
            //if first item is the head destroy it
            if(firstItemPtr==list.GetHead())
            {
                firstItemPtr = list.RemoveHead();
                delete firstItemPtr;
            }
            //fix the pointers
            iterator.Reset();
            firstItemPtr = iterator.GetNextItem();
            secondItemPtr = iterator.GetNextItem();
            if(secondItemPtr==NULL)
            {
                //we bubbled all the way to one value
                return true;
            }
        }
    }
    return true;
}
double
ProtoAverage::GetAverage()
{
    //iterate over list in reverse and get the weighted average
    AverageList::Iterator iterator(list);
    AverageItem* item = iterator.GetNextItem();
    if(item!=NULL)
    {
        ProtoAverage::AverageItem tempItem(item->GetValue());//used for storing the moving average as we go
        tempItem.SetDepth(item->GetDepth());
        double x , y, z;
        double dpx=0;
        double dpy=0;
        double dpz=0;
        double numeral;
        double denom;
        while((item = iterator.GetNextItem()))
        {
            x = tempItem.GetValue();
            dpx = tempItem.GetDepth();
            y = item->GetValue();
            dpy = item->GetDepth();

            numeral = (x + y * pow(2,dpy-dpx));
            if(numeral==numeric_limits<double>::infinity()){
                //we need to go under instead of over
                numeral = (x * pow(2,dpx-dpy) + y);
                denom = (pow(2,dpx-dpy)+1);
            } 
            else 
            {
                denom = (1+pow(2,dpy-dpx));
            }
            z = numeral/denom; //z is the new average
            dpz  = log(denom)/log(2)+dpx; //dpz is the weighted depth of the new average
            dpz  = log(1+pow(2,dpy-dpx))/log(2)+dpx; //dpz is the weighted depth of the new average

            tempItem.SetValue(z);
            tempItem.SetDepth(dpz);
        }
        return tempItem.GetValue();
    }
    return 0;
}
unsigned long
ProtoAverage::GetCount() const
{
    return count;
}
void
ProtoAverage::Print()
{
    AverageList::Iterator iterator(list);
    AverageItem* item;
    while((item = iterator.GetNextItem()))
    {
        TRACE("(%f,%f) ",item->GetValue(),item->GetDepth());
    }
    TRACE("\n");
}
bool
ProtoAverage::AverageItem::Merge(const ProtoAverage::AverageItem &otherItem)
{
    //verify that they are at the same depth
    double tempvalue = 0;
    if(otherItem.GetDepth()!=GetDepth())
    {
        return false;
    }
    tempvalue = (value + otherItem.GetValue())/2;
    if(tempvalue==numeric_limits<double>::infinity())
    {
        //lets try dividing first
        value = value/2 + otherItem.GetValue()/2;
    } else {
        value = tempvalue;
    }
    
    if(value==numeric_limits<double>::infinity())
    {
        TRACE("still getting inf\n");
        TRACE("%f\n\n",value/2);
        TRACE("%f\n\n",otherItem.GetValue()/2);
        TRACE("%f\n\n",value/2+otherItem.GetValue()/2);
        return false;
    }
    depth = depth+1;
    return true;
}
void
ProtoAverage::AverageItem::SetValue(double v)
{
    value = v;
    return;
}
void
ProtoAverage::AverageItem::SetDepth(double d)
{
    depth = d;
    return;
}

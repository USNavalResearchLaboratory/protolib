#ifndef _PROTOLOCATION
#define _PROTOLOCATION

//#include <stdlib.h>  // for random(), etc
#include <math.h>
//#include <stdio.h>
#include "protoDefs.h"

class ProtoLocation
{
    public:
        enum Type
        {
            INVALID,
            CART,
            GPS,
            UTM
        };  // "Cartesian", and "Geodetic', respectively

        enum Reference
        {
            AGL,    // above ground level (relative)
            MSL     // mean sea level (absolute)
        };

        enum Hemisphere
        {
            NORTH,
            SOUTH
        };

        ProtoLocation(Type theType = INVALID);
        ProtoLocation(double xLat, double yLon, Type theType = CART);
        ProtoLocation(double xLat, double yLon, double zAlt, Type theType = CART);

        // UTM-specific constructors
        ProtoLocation(unsigned int utmZone, Hemisphere hemisphere, double utmX, double utmY);
        ProtoLocation(unsigned int utmZone, Hemisphere hemisphere, double utmX, double utmY, double utmZ);
        // For these, a postive "utmZoneNum" implies Northern hemisphere while negative implies Southern
        ProtoLocation(int utmZoneNumber, double utmX, double utmY);
        ProtoLocation(int utmZoneNumber, Hemisphere hemisphere, double utmX, double utmY, double utmZ);

        virtual ~ProtoLocation();

        // Note UTM args only used for UTM type
        void Set(Type           theType,    // CART, GPS, or UTM
                 double         xLat,       // x-axis, "latitude", or "easting"
                 double         yLon,       // y-axis, "longitude", or "northing"
                 unsigned int   utmZone = 0,
                 Hemisphere     utmHemi = NORTH);


        void Set(Type           theType,    // CART, GPS, or UTM
                 double         xLat,       // "x-axis", "latitude", or "easting"
                 double         yLon,       // "y-axis", "longitude", or "northing"
                 double         zAlt,       // "z-axis" or "altitude"
                 unsigned int   utmZone = 0,
                 Hemisphere     utmHemi = NORTH);

        bool InitFromString(Type          theType,
                            const char*   text,
                            unsigned int  utmZone = 0,
                            Hemisphere    utmHemi = NORTH);

        bool IsValid() const
            {return (INVALID != type);}

        Type GetType() const;

        void Invalidate()
        {
            type = INVALID;
            latx = lony = altz = 0.0;
            altz_valid = false;
        }

        // Use these carefully, making sure you are supply proper
        // x,y,z values for the given ProtoLocation::Type.  These
        // methods do not change the "type" for you!
        void SetCoordinates(double x, double y)
        {
            latx = x;
            lony = y;
        }
        void SetCoordinates(double x, double y, double z)
        {
            latx = x;
            lony = y;
            altz = z;
            altz_valid = true;
        }
        void SetValueX(double x)
            {latx = x;}
        void SetValueY(double y)
            {lony = y;}
        void SetValueZ(double z)
        {
            altz = z;
            altz_valid = true;
        }
        void ClearValueZ()
            {altz_valid = false;}
        bool HasZ() const
            {return altz_valid;}

        // These methods set the type to "GPS" as well
        // as setting the value
        void SetLatLon(double latitude, double longitude)
        {
            type = GPS;
            latx = latitude;
            lony = longitude;
        }
        void SetLatLon(double latitude, double longitude, double altitude)
        {
            type = GPS;
            latx = latitude;
            lony = longitude;
            altz = altitude;
            altz_valid = true;
        }
        void SetLat(double latitude)
        {
            type = GPS;
            latx = latitude;
        }
        void SetLon(double longitude)
        {
            type = GPS;
            lony = longitude;
        }
        void SetAlt(double altitude)
        {
            //type = GPS;
            altz = altitude;
            altz_valid = true;
        }
        void ClearAlt()
            {altz_valid = false;}

        bool HasAlt() const
            {return altz_valid;}
        double GetAlt() const
            {return GetValueZ();}

        void SetAltitudeReference(Reference ref)
            {altz_ref = ref;}
        Reference GetAltitudeReference() const
            {return altz_ref;}

        double GetLat() const
            {return latx;}
        double GetLon() const
            {return lony;}

        double GetValueX() const
            {return latx;}
        double GetValueY() const
            {return lony;}
        double GetValueZ() const
            {return altz_valid ? altz : 0.0;}


        // Returns negative value to indicate southern hemisphere,
        // positive value to indicate northern hemisphere (0 is invalid)
        int GetUtmZoneNumber() const;
        unsigned int GetUtmZone() const
            {return abs(GetUtmZoneNumber());}
        Hemisphere GetUtmHemisphere() const
            { return ((GetUtmZoneNumber() < 0) ? SOUTH : NORTH);}
        double GetUTMX() const
            {return latx;}
        double GetUTMY() const
            {return lony;}
        double GetUTMZ() const
            {return ((altz_valid) ? altz : 0.0);}


        // ProtoLocation::Type MUST match to return true
        bool operator==(const ProtoLocation& loc) const;
        bool operator!=(const ProtoLocation& loc) const
            {return !(*this == loc);}

        // Some helper methods that may be useful
        // returns distance in units (meters)
        // Note that currently the ProtoLocation::Type MUST match!
        // Also - UTM not yet supported for many of these
        double ComputeDistanceTo(const ProtoLocation& dest,
                                 bool            ignoreAltitude = false,
                                 bool            greatCircle = true) const;

        // returns azimuth (bearing) angle  in radians
        double ComputeAzimuthTo(const ProtoLocation& dest) const;
        // returns elevation angle  in radians
        double ComputeElevationTo(const ProtoLocation& dest, bool greatCircle=true) const;

        void MoveTowards(const ProtoLocation& dest, double distance, bool greatCircle = false);

        // angles must be in radians
        void Move(double distance, double azimuthAngle, double elevationAngle = 0.0);

        bool ConvertTo(Type theType, bool eccentric = true, ProtoLocation* origin = NULL);

        // Some constants and useful helper functions
        static const double GLOBE_EQU_RADIUS_EARTH; // equatorial radius in meters
        static const double GLOBE_POL_RADIUS_EARTH; // polar radius in meters
        static const double GLOBE_ES2; // eccentricity squared, semi-major axis

        static void ConvertGeodeticToCartesian(double lat, double lon, double alt,
                                               double& x, double& y, double& z,
                                               bool eccentric=true);

        static void ConvertCartesianToGeodetic(double x, double y, double z,
                                               double& lat, double& lon, double& alt,
                                               bool eccentric = true);

        static bool ConvertGeodeticToUtm(double  lat,   // input, in degrees
                                         double  lon,   // input, in degrees
                                         int&    zone,  // output, negative value indicates southern hemisphere
                                         double& easting,
                                         double& northing);

        static void ConvertUtmToGeodetic(int     zone,  // input, negative value indicates southern hemisphere
                                         double  easting,
                                         double  northing,
                                         double& lat,   // output, in degrees
                                         double& lon);  // output, in degrees
        
        // additional methods
        static double GetArchimedeanSpiralLength(double a, double angle1, double angle2);
        static void GetArchimedeanSpiralParams(double R, double dist, double elev, 
                                               double& alpha, double& deltaPhi,
                                               unsigned int iterMax = 4);

        static const double PI;
        static const double UTM_LAT_MIN;
        static const double UTM_LAT_MAX;
        static const double UTM_LON_MIN;
        static const double UTM_LON_MAX;
        static const double UTM_SCALE_FACTOR;

    protected:
        // Positive value indicates Northern, negative Southern, and 0 is invalid zone
        void SetUtmZoneNumber(int utmZoneNumber);
        static double ComputeMeridianArcLength(double phi); //  "phi" is latitude in radians
        static double ComputeFootpointLatitude(double northing);

        unsigned int type;  // the "type" is a ProtoLocation::Type or UTM zone (+/-60) offset by ProtoLocation::UTM
        double       latx;  // x-axis ordinate (CART) or latitude in degrees (GPS)
        double       lony;  // y-axis ordinate (CART) or longitude in degrees (GPS)
        double       altz;  // z-axis ordinate (CART) or altitude in meters
        bool         altz_valid; // "true" when the z-axis/altitude has been explicitly set
        Reference    altz_ref;
};  // end class ProtoLocation


#endif // !_PROTOLOCATION

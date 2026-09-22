#include "protoLocation.h"

#include "protoDebug.h"

// Some constants
const double ProtoLocation::PI = 2.0*acos(0.0);
const double ProtoLocation::UTM_LAT_MIN = -80.5; // -80.5 degrees in radians
const double ProtoLocation::UTM_LAT_MAX = 84.5;  // +84.5 degrees in radians
const double ProtoLocation::UTM_LON_MIN = -180.0;
const double ProtoLocation::UTM_LON_MAX = 180.0;
const double ProtoLocation::UTM_SCALE_FACTOR = 0.9996;

// Earth ellipsoid parameters
const double ProtoLocation::GLOBE_EQU_RADIUS_EARTH = 6378137.0;  // equatorial radius in meters
const double ProtoLocation::GLOBE_POL_RADIUS_EARTH = 6356752.314;  // polar radius in meters
const double ProtoLocation::GLOBE_ES2 = 0.00669437999013; // eccentricity squared, semi-major axis

// Constructors
ProtoLocation::ProtoLocation(Type theType)
 : type((unsigned int)theType), latx(0.0), lony(0.0), altz(0.0), altz_valid(false), altz_ref(AGL)
{
    if (UTM == (Type)theType) SetUtmZoneNumber(30);
}

ProtoLocation::ProtoLocation(double xLat, double yLon, Type theType)
 : type((unsigned int)theType), latx(xLat), lony(yLon), altz(0.0), altz_valid(false), altz_ref(AGL)
{
    if (UTM == (Type)theType) SetUtmZoneNumber(30);
}

ProtoLocation::ProtoLocation(double xLat, double yLon, double zAlt, Type theType)
 : type((unsigned int)theType), latx(xLat), lony(yLon), altz(zAlt), altz_valid(true), altz_ref(AGL)
{
    if (UTM == (Type)theType) SetUtmZoneNumber(30);
}

// UTM-specific constructors
ProtoLocation::ProtoLocation(unsigned int utmZone, Hemisphere hemisphere, double utmX, double utmY)
 : latx(utmX), lony(utmY), altz(0.0), altz_valid(false), altz_ref(AGL)
{
    ASSERT(0);
    SetUtmZoneNumber(NORTH == hemisphere ? (int)utmZone: -((int)utmZone));
}

ProtoLocation::ProtoLocation(unsigned int utmZone, Hemisphere hemisphere, double utmX, double utmY, double utmZ)
 : latx(utmX), lony(utmY), altz(utmZ), altz_valid(true), altz_ref(AGL)
{
    ASSERT(0);
    SetUtmZoneNumber(NORTH == hemisphere ? (int)utmZone: -((int)utmZone));
}

// For these, a postive "utmZoneNum" implies Northern hemisphere while negative implies Southern
ProtoLocation::ProtoLocation(int utmZoneNumber, double utmX, double utmY)
 : latx(utmX), lony(utmY), altz(0.0), altz_valid(false), altz_ref(AGL)
{
    ASSERT(0);
    SetUtmZoneNumber(utmZoneNumber);
}

ProtoLocation::ProtoLocation(int utmZoneNumber, Hemisphere hemisphere, double utmX, double utmY, double utmZ)
 : latx(utmX), lony(utmY), altz(utmZ), altz_valid(true), altz_ref(AGL)
{
    ASSERT(0);
    SetUtmZoneNumber(utmZoneNumber);
}

ProtoLocation::~ProtoLocation()
{
}

// Note UTM args only used for UTM type
void ProtoLocation::Set(Type           theType,    // CART, GPS, or UTM
                   double         xLat,       // x-axis, "latitude", or "easting"
                   double         yLon,       // y-axis, "longitude", or "northing"
                   unsigned int   utmZone,
                   Hemisphere     utmHemi)
{
    latx = xLat;
    lony = yLon;
    altz = 0.0;
    altz_valid = false;
    if (theType < UTM)
        type = theType;
    else
        SetUtmZoneNumber((NORTH == utmHemi) ? (int)utmZone : -((int)utmZone));
}  // end ProtoLocation::Set()


void ProtoLocation::Set(Type           theType,    // CART, GPS, or UTM
                   double         xLat,       // "x-axis", "latitude", or "easting"
                   double         yLon,       // "y-axis", "longitude", or "northing"
                   double         zAlt,       // "z-axis" or "altitude"
                   unsigned int   utmZone,
                   Hemisphere     utmHemi)
{
    latx = xLat;
    lony = yLon;
    altz = zAlt;
    altz_valid = true;
    if (theType < UTM)
        type = theType;
    else
        SetUtmZoneNumber((NORTH == utmHemi) ? (int)utmZone : -((int)utmZone));
}  // end ProtoLocation::Set()


bool ProtoLocation::InitFromString(Type          theType,
                              const char*   text,
                              unsigned int  utmZone,
                              Hemisphere    utmHemi)
{
    double a, b, c;
    if (3 == sscanf(text, "%lf,%lf,%lf", &a, &b, &c))
    {
        // TBD - validate values (i.e., make sure in range if GPS)
        Set(theType, a, b, c, utmZone, utmHemi);
    }
    else if (2 == sscanf(text, "%lf,%lf", &a, &b))
    {
        // TBD - validate values (i.e., make sure in range if GPS)
        Set(theType, a, b, utmZone, utmHemi);
    }
    else
    {
        PLOG(PL_ERROR, " ProtoLocation::InitFromString() error: invalid coordinates\n");
        return false;
    }
    return true;
}  // end ProtoLocation::InitFromString()

ProtoLocation::Type ProtoLocation::GetType() const
{
    if (type < UTM)
        return (Type)type;
    else
        return UTM;
}  // end ProtoLocation::GetType()

void ProtoLocation::SetUtmZoneNumber(int utmZoneNumber)
{
    if ((utmZoneNumber < -60) || (utmZoneNumber > 60))
        SetUtmZoneNumber(0);  // invalid zone
    else
        type = (unsigned int)UTM + 60 + utmZoneNumber;
}  // end ProtoLocation::SetUtmZoneNumber()

int ProtoLocation::GetUtmZoneNumber() const
{
    if (type < UTM)
        return 0;
    else
        return ((int)type - 60 - UTM);
}  // end ProtoLocation::GetUtmZoneNumber()

bool ProtoLocation::operator==(const ProtoLocation& loc) const
{
    // TBD - if types are different, convert and check???
    if ((type == loc.type) &&
        (latx == loc.latx) &&
        (lony == loc.lony))
    {
        if (altz_valid)
            return (loc.altz_valid && (altz == loc.altz) && (altz_ref == loc.altz_ref));
        else
            return (!loc.altz_valid);
    }
    else
    {
        return false;
    }
}  // end ProtoLocation::operator==()

void ProtoLocation::ConvertGeodeticToCartesian(double    lat,
                                          double    lon,
                                          double    alt,
                                          double&   x,
                                          double&   y,
                                          double&   z,
                                          bool      eccentric)
{
    // from Vermeille, H.
    // "Direct transformation from geocentric coordinates to geodetic coordinates"
    // Journal of Geodesy (2002) 76: pp 451-454
    lat *= PI/180.0;  // convert to radians
    lon *= PI/180.0;  // convert to radians
    double cosLat = cos(lat);
    double sinLat = sin(lat);
    const double a = GLOBE_EQU_RADIUS_EARTH;
    const double e2 = eccentric ? GLOBE_ES2 : 0.0;
    double n = a / sqrt(1.0 - (e2 * sinLat * sinLat));
    x = (alt + n) * cosLat*cos(lon);
    y = (alt + n) * cosLat*sin(lon);
    z = (alt + n*(1.0 - e2)) * sinLat;
}  // end ProtoLocation::ConvertGeodeticToCartesian()

void ProtoLocation::ConvertCartesianToGeodetic(double    x,
                                          double    y,
                                          double    z,
                                          double&   lat,
                                          double&   lon,
                                          double&   alt,
                                          bool      eccentric)
{
    // from Vermeille, H.
    // "Computing geodetic coordinates from geocentric coordinates"
    // Journal of Geodesy (2004) 78: pp 94-95
    const double a2 = GLOBE_EQU_RADIUS_EARTH * GLOBE_EQU_RADIUS_EARTH;
    const double e2 = eccentric ? GLOBE_ES2 : 0.0;
    const double e4 = e2*e2;
    double X2 = x*x;
    double Y2 = y*y;
    double Z2 = z*z;
    double p = (X2 + Y2 )/ a2;
    double q = Z2*((1.0 - e2) / a2);
    double r = (p + q - e4) / 6.0;
    double s = e4 * ((p * q) / (4.0*r*r*r));
    // We use cubedRoot(x) = e ^(ln(x) / 3.0) for cubed root of 't' here
    double t = 1.0 + s + sqrt(s*(2.0 + s));
    t = exp(log(t) / 3.0);
    double u = r * (1.0 + t + (1.0/t));
    double v = sqrt(u*u + e4*q);
    double w = e2 * ((u + v - q) / (2.0 * v));
    double k = sqrt(u + v + w*w) - w;

    double D = k * (sqrt(X2 + Y2) / (k + e2));
    double D2 = D*D;
    double sqrtD2pZ2 = sqrt(D2 + Z2);
    alt = (0.0 != k) ? ((k + e2 - 1.0) / k) * sqrtD2pZ2 : -GLOBE_POL_RADIUS_EARTH;
    if (alt <= -GLOBE_POL_RADIUS_EARTH)
    {
        lat = lon = 0.0;
        alt = -GLOBE_POL_RADIUS_EARTH;
    }
    else
    {
        lat = 2.0 * atan2(z, D + sqrtD2pZ2);
        lon = 2.0 * atan2(y, x + sqrt(X2 + Y2));
        lat *= 180.0/PI;  // convert to degrees
        lon *= 180.0/PI;  // convert to degrees
    }
}  // end ProtoLocation::ConvertCartesianToGeodetic()

bool ProtoLocation::ConvertTo(ProtoLocation::Type theType,
                              bool                eccentric,
                              ProtoLocation*      origin)
{
    if (GetType() == theType) return true;
    switch (GetType())
    {
        case GPS:
            switch(theType)
            {
                case CART:  // Geodetic to Geocentric
                {
                    if (NULL == origin)
                    {
                        // Geodetic / ECEF Cartesian
                        ConvertGeodeticToCartesian(latx, lony, HasZ() ? altz : 0.0, latx, lony, altz, eccentric);
                        altz_valid = true; // Geocentric MUST have altz set.
                        type = CART;
                    }
                    else
                    {
                        // "Flat Earth" Cartesian referenced from origin
                        double dist = origin->ComputeDistanceTo(*this);
                        double azim = origin->ComputeAzimuthTo(*this);
                        double elev = origin->ComputeElevationTo(*this);
                        latx = lony = altz = 0.0; // set to CART origin (0,0,0)
                        altz_valid = true;
                        type = CART;
                        Move(dist, azim, elev);
                        break;
                    }
                    break;
                }
                case UTM:  // Geodetic to UTM (note not all GPS locs are allowed)
                {
                    int utmZoneNumber;
                    double easting, northing;
                    if (!ConvertGeodeticToUtm(latx, lony, utmZoneNumber, easting, northing))
                    {
                        PLOG(PL_ERROR, "ProtoLocation::ConvertTo(UTM) error: invalid UTM location\n");
                        return false;
                    }
                    latx = easting;
                    lony = northing;
                    SetUtmZoneNumber(utmZoneNumber);
                    break;
                }
                default:
                {
                    PLOG(PL_ERROR, "ProtoLocation::ConvertTo() error: invalid location type!\n");
                    return false;
                }
            }
            break;
        case CART:
            if (!HasZ())
            {
                PLOG(PL_ERROR, "ProtoLocation::ConvertTo(GPS/UTM) warning: geocentric cartesian with no Z-value!\n");
                return false;
            }
            switch(theType)
            {
                case GPS:
                {
                    if (NULL == origin)
                    {
                        ConvertCartesianToGeodetic(latx, lony, altz, latx, lony, altz, eccentric);
                        type = GPS;
                    }
                    else
                    {
                        ProtoLocation cartOrigin(0.0, 0.0, 0.0, CART);
                        double dist = cartOrigin.ComputeDistanceTo(*this);
                        double azim = cartOrigin.ComputeAzimuthTo(*this);
                        double elev = cartOrigin.ComputeElevationTo(*this);
                        *this = *origin;
                        Move(dist, azim, elev);
                    }
                    break;
                }
                case UTM:
                {
                    double lat, lon, alt;
                    ConvertCartesianToGeodetic(latx, lony, HasZ() ? altz : 0.0, lat, lon, alt, eccentric);
                    int utmZoneNumber;
                    double easting, northing;
                    if (!ConvertGeodeticToUtm(lat, lon, utmZoneNumber, easting, northing))
                    {
                        PLOG(PL_ERROR, "ProtoLocation::ConvertTo(UTM) error: invalid UTM location\n");
                        return false;
                    }
                    latx = easting;
                    lony = northing;
                    altz = alt; // note if there was no geocentric Z-value, then all is invalid
                    SetUtmZoneNumber(utmZoneNumber);
                    break;
                }
                default:
                {
                    PLOG(PL_ERROR, "ProtoLocation::ConvertTo() error: invalid location type!\n");
                    return false;
                }
            }
            break;
        case UTM:
            switch (theType)
            {
                case CART:
                {
                    ConvertUtmToGeodetic(GetUtmZoneNumber(), latx, lony, latx, lony);
                    ConvertGeodeticToCartesian(latx, lony, HasZ() ? altz : 0.0, latx, lony, altz, eccentric);
                    altz_valid = true; // Geocentric MUST have altz set.
                    type = CART;
                    break;
                }
                case GPS:
                {
                    ConvertUtmToGeodetic(GetUtmZoneNumber(), latx, lony, latx, lony);
                    type = GPS;
                    break;
                }
                default:
                {
                    PLOG(PL_ERROR, "ProtoLocation::ConvertTo() error: invalid location type!\n");
                    return false;
                }
            }
            break;
        default:
            PLOG(PL_ERROR, "ProtoLocation::ConvertTo() error: invalid location!\n");
            return false;
    }
    return true;
}  // end ProtoLocation::ConvertTo()

double ProtoLocation::ComputeDistanceTo(const ProtoLocation& p, bool ignoreAltitude, bool greatCircle) const
{
    // TBD - copy 'p' and convert this->type.  Meanwhile, just return negative value to indicate
    //       non-matching location type.
    if (p.type != this->type)
    {
        PLOG(PL_ERROR, "ProtoLocation::ComputeDistanceTo() error: non-matching location types\n");
        ASSERT(0);
        return -1.0;
    }
    switch (GetType())
    {
        case CART:
        {
            // Compute dist = sqrt(dx*dx + dy*dy [+ dz*dz])
            double d = p.latx - latx;
            double sum = d*d;
            d = p.lony - lony;
            sum += d*d;
            if (!ignoreAltitude)
            {
                d = p.HasZ() ? p.altz : 0.0;
                d -= HasZ() ? altz : 0.0;
                sum += d*d;
            }
            return sqrt(sum);
        }
        case GPS:
        {
            if (greatCircle)
            {
                // First, compute great circle ground distance
                // a) convert to radians
                double lat1 = this->latx * PI/180.0;
                double lon1 = this->lony * PI/180.0;
                double lat2 = p.latx * PI/180.0;
                double lon2 = p.lony * PI/180.0;
                double dLon = lon2 - lon1;
                double dLat = lat2 - lat1;
                double sinHalfDeltaLat = sin(dLat/2.0);
                double sinHalfDeltaLon = sin(dLon/2.0);
                double a = (sinHalfDeltaLat*sinHalfDeltaLat) +
                           (cos(lat1) * cos(lat2) *
                            sinHalfDeltaLon*sinHalfDeltaLon);
                double angle = 2 * atan2(sqrt(a), sqrt(1.0 - a));
                // Second, supplement great circle distance based on average altitude
                if (ignoreAltitude)
                {
                    // Just use sea level great circle distance
                    return (GLOBE_EQU_RADIUS_EARTH  * angle);
                }
                else
                {
                    double aveAlt = this->altz_valid ? this->altz : 0.0;
                    double dz = (p.altz_valid ? p.altz : 0.0) - aveAlt;
                    aveAlt += dz / 2.0;
                    // Base of our great circle movement "triangle"
                    double dist = (GLOBE_EQU_RADIUS_EARTH + aveAlt) * angle;
                    return sqrt(dist*dist + dz*dz);
                }
            }
            else
            {
                ProtoLocation a = *this;
                ProtoLocation b = p;
                a.ConvertTo(CART);
                b.ConvertTo(CART);
                return a.ComputeDistanceTo(b, ignoreAltitude);
            }
        }
        default:
        {
            // TBD - add UTM support for this method
            PLOG(PL_ERROR, "ProtoLocation::ComputeDistanceTo() error: invalid location type!\n");
            return -1.0;
        }
    }
}  // end ProtoLocation::ComputeDistanceTo()

// return azimuth (bearing) angle in radians
double ProtoLocation::ComputeAzimuthTo(const ProtoLocation& dest) const
{
    // TBD - copy/convert dest coords to common type???
    if (GetType() != dest.GetType())
    {
        PLOG(PL_ERROR, "ProtoLocation::ComputeAzimuthTo() error: non-matching location types\n");
        return 0.0;
    }
    switch (GetType())
    {
        case GPS:
        {
            // a) convert to radians
            double lat1 = latx * PI/180.0;
            double lon1 = lony * PI/180.0;
            double lat2 = dest.latx * PI/180.0;
            double lon2 = dest.lony * PI/180.0;
            // 1) Calculate bearing to use
            double deltaLon = lon2 - lon1;
            double cosDestLat = cos(lat2);
            double sinLat = sin(lat1);
            double cosLat = cos(lat1);
            double y = sin(deltaLon) * cosDestLat;
            double x = cosLat * sin(lat2) - sinLat*cosDestLat*cos(deltaLon);
            return (atan2(y, x));  // bearing in +/- PI
        }
        case CART :
        {
            double dx = dest.latx - latx;
            double dy = dest.lony - lony;
            return (atan2(dx, dy));
        }
        default:
        {
            // TBD - add UTM support for this method
            PLOG(PL_ERROR, "ProtoLocation::ComputeAzimuthTo() error: invalid location type\n");
            ASSERT(0);
            return 0.0;
        }
    }
}  // end ProtoLocation::ComputeAzimuthTo()

// returns elevation angle in radians
double ProtoLocation::ComputeElevationTo(const ProtoLocation& dest, bool greatCircle) const
{
    // TBD - copy/convert dest coords to common type
    if (GetType() != dest.GetType())
    {
        PLOG(PL_ERROR, "ProtoLocation::ComputeAzimuthTo() error: non-matching location types\n");
        return 0.0;
    }
    switch (GetType())
    {
        case GPS:
            if (greatCircle)
            {
                double dist = ComputeDistanceTo(dest, false, true);
                if (dist < 0.0)
                {
                    // TBD - add UTM support for this method
                    PLOG(PL_ERROR, "ProtoLocation::ComputeElevationTo() error: invalid location type\n");
                    ASSERT(0);
                    return 0.0;
                }
                double h1 = HasZ() ? altz : 0.0;
                double h2 = dest.HasZ() ? dest.altz : 0.0;
                double dz = h2 - h1;
                // This needs to be clamped because of possible roundoff errors
                double term = dz/dist;
                if (term > 1.0)
                    term = 1.0;
                else if (term < -1.0)
                    term = -1.0;
                return asin(term);
            }
            else
            {
                // Compute look angle elevation referenced to astronomical horizon
                // This computes the elevation angle as
                // PI/2 - arccos(dot product of V(origin, observer) * V(observer, target))
                // as projected onto the plane defined by those two vectors
                ProtoLocation a = *this;
                ProtoLocation b = dest;
                // Convert to ECEF coords for dot product
                a.ConvertTo(CART, true);
                b.ConvertTo(CART, true);
                double x = a.latx;
                double y = a.lony;
                double z = a.altz;
                double mag = sqrt(x*x + y*y +z*z);
                double dx = b.latx - x;
                double dy = b.lony - y;
                double dz = b.altz - z;
                double dmag = sqrt(dx*dx + dy*dy + dz*dz);
                // This needs to be clamped because of possible roundoff errors
                double term = (x*dx + y*dy + z*dz) / (mag*dmag);
                if (term > 1.0)
                    term = 1.0;
                else if (term < -1.0)
                    term = -1.0;
                return (PI/2.0 - acos(term));
            }
            break;
        case CART:
        {
            double dx = dest.latx - latx;
            double dy = dest.lony - lony;
            double z1 = HasZ() ? altz : 0.0;
            double z2 = dest.HasZ() ? dest.altz : 0.0;
            double dz = z2 - z1;
            double dxy = sqrt(dx*dx + dy*dy);
            return atan2(dz, dxy);
        }
        default:
            PLOG(PL_ERROR, "ProtoLocation::ComputeElevationTo() error: invalid location type\n");
            return 0.0;
    }
}  // end ProtoLocation::ComputeElevationTo()

// Note "azimuthAngle" and "elevationAngle" are in radians
void ProtoLocation::Move(double distance, double azimuthAngle, double elevationAngle)
{
    // First, adjust altitude/height as needed
    double origAltz = altz;
    if (fabs(elevationAngle) == PI/2.0)
    {
        if (elevationAngle < 0.0)
            altz = HasZ() ? altz - distance : -distance;
        else
            altz = HasZ() ? altz + distance : distance;
        return;
    }
    else if (0.0 != elevationAngle)
    {
        double dz = distance*sin(elevationAngle);
        altz = HasZ() ? altz + dz : dz;
    }
    // else 0.0 == elevation so no altz change
    // Move remaining distance via "azimuthAngle"
    switch (GetType())
    {
        case GPS:
        {
            // Compute new location using "azimuthAngle" bearing from location
            // This based on a great circle route from the "center" to the
            // edge of our circle.  The distance is scaled for average altitude
            double R = ProtoLocation::GLOBE_EQU_RADIUS_EARTH + origAltz;
            double deltaAngle = 0.0;
            if (0.0 == elevationAngle)
            {
                deltaAngle = distance / R;
            }
            else
            {
                double alpha = 0.0;
                GetArchimedeanSpiralParams(R, distance, elevationAngle, alpha, deltaAngle);
            }
            double cosDeltaAngle = cos(deltaAngle);
            double sinDeltaAngle = sin(deltaAngle);
            double cosLat = cos(GetLat()*PI/180.0);
            double sinLat = sin(GetLat()*PI/180.0);
            double lat2 = asin(sinLat*cosDeltaAngle +
                               cosLat*sinDeltaAngle*cos(azimuthAngle));
            double lon2 = GetLon()*(PI/180.0) +
                          atan2(sin(azimuthAngle)*sinDeltaAngle*cosLat,
                                cos(deltaAngle)-sinLat*sin(lat2));
            if (lat2 > PI/2)
                lat2 = fmod(lat2, PI/2);
            else if (lat2 < -PI/2)
                lat2 = -fmod(fabs(lat2), PI/2);

            if (lon2 > PI)
            {
                if (lon2 > 2*PI) lon2 = fmod(lon2, 2*PI);
                if (lon2 > PI) lon2 -= 2*PI;
            }
            else if (lon2 < - PI)
            {
                if (lon2 < -2*PI) lon2 = -fmod(fabs(lon2), 2*PI);
                if (lon2 < -PI) lon2 += 2*PI;
            }
            if (lon2 > PI)
                lon2 = fmod(lon2, PI);
            else if (lon2 < -PI)
                lon2 = -fmod(fabs(lon2), PI);
            lat2 *= 180.0 / PI;
            lon2 *= 180.0 / PI;
            SetLatLon(lat2, lon2);
            break;
        }
        case CART:
        {
            double dxy = distance*cos(elevationAngle);
            double x = GetValueX() + (dxy * sin(azimuthAngle));
            double y = GetValueY() + (dxy * cos(azimuthAngle));
            SetCoordinates(x, y);
            break;
        }
        default:
        {
            // TBD - add UTM support for this method
            PLOG(PL_ERROR, "ProtoLocation::Move() error: invalid location type\n");
            ASSERT(0);
            break;
        }
    }
}  // end ProtoLocation::Move()

void ProtoLocation::MoveTowards(const ProtoLocation& dest, double dist, bool greatCircle)
{
    Type origType = GetType();
    if (greatCircle)
    {
        if (GPS != origType)
            ConvertTo(GPS);
    }
    else
    {
        if (CART != origType)
            ConvertTo(CART);
    }
    // In case the dest is not the proper geometry
    const ProtoLocation* d = &dest;
    ProtoLocation destAlt;
    if (dest.GetType() != GetType())
    {
        destAlt = dest;
        destAlt.ConvertTo(GetType());
        d = &destAlt;
    }
    if (greatCircle)
    {
        // We need to scale "dist" taking deltaHeight into consideration!
        double distRemain = this->ComputeDistanceTo(*d, false);
        if (dist < distRemain)
        {
            double azimuth = ComputeAzimuthTo(*d);
            double elevation = ComputeElevationTo(*d, true);
            Move(dist, azimuth, elevation);
        }
        else
        {
            // Go directly there
            *this = *d;
        }
    }
    else
    {
        // Cartesian coords, move in a "bee-line"
        // 1) Compute delta vector
        double dx = d->latx - latx;
        double dy = d->lony - lony;
        double dz = d->HasZ() ? d->altz : 0.0;
        dz -= HasZ() ? altz : 0.0;
        double distRemain = sqrt(dx*dx + dy*dy + dz*dz);
        if (dist < distRemain)
        {   // 2) Add scaled delta vector to self
            double scale = dist / distRemain;
            latx += dx * scale;
            lony += dy * scale;
            altz += dz * scale;
            if (!altz_valid && (0.0 != dz)) altz_valid = true;
        }
        else
        {
            *this = *d;
        }
    }
    if (GetType() != origType)
        ConvertTo(origType);
}  // end ProtoLocation::MoveTowards()

double ProtoLocation::ComputeMeridianArcLength(double phi)
{
    // Precalculate n
    double n = (GLOBE_EQU_RADIUS_EARTH - GLOBE_POL_RADIUS_EARTH) / (GLOBE_EQU_RADIUS_EARTH + GLOBE_POL_RADIUS_EARTH);
    // Precalculate alpha
    double alpha = ((GLOBE_EQU_RADIUS_EARTH + GLOBE_POL_RADIUS_EARTH) / 2.0) *
                   (1.0 + (n*n/4.0) + pow(n,4.0)/64.0);
    // Precalculate beta
    double beta = (-3.0*n/2.0) + (9.0*pow(n,3.0)/16.0) + (-3.0* pow(n,5.0)/ 32.0);
    // Precalculate gamma
    double gamma = (15.0*n*n/ 16.0) - (15.0*pow(n,4.0)/32.0);
    // Precalculate delta
    double delta = (-35.0* pow(n,3.0)/48.0) + (105.0*pow(n,5.0)/256.0);
    // Precalculate epsilon
    double epsilon = 315.0*pow(n,4.0)/ 512.0;
    // Now calculate the sum of the series and return result
    double result = alpha * (phi + (beta * sin (2.0 * phi)) +
                             (gamma * sin (4.0 * phi)) +
                             (delta * sin (6.0 * phi)) +
                             (epsilon * sin (8.0 * phi)));
    return result;
}  // end ProtoLocation::ComputeMeridianArcLength()

bool ProtoLocation::ConvertGeodeticToUtm(double  lat,
                                    double  lon,
                                    int&    zone,
                                    double& easting,
                                    double& northing)
{
    // a) validate input lat/lon range
    if ((lat < UTM_LAT_MIN) || (lat > UTM_LAT_MAX))
    {
        PLOG(PL_ERROR, "ProtoLocation::ConvertGeodeticToUtm() error: latitude:%lf out-of-range!\n", lat);
        return false;
    }
    if ((lon < UTM_LON_MIN) || (lon > UTM_LON_MAX))
    {
        PLOG(PL_ERROR, "ProtoLocation::ConvertGeodeticToUtm() error: longitude:%lf out-of-range!\n", lon);
        return false;
    }
    // b) compute "zone"
    zone = (int)floor((lon + 180.0)/6.0) + 1;
    // c) compute central meridian of "zone"
    double centralMeridian = (zone*6.0) - 183.0;
    // d) compute Transverse Mercator projection
    // ( we need lat/lon/centralMeridian in radians for this
    lat *= PI / 180.0;
    lon *= PI / 180.0;
    centralMeridian *= PI / 180.0;
    double rad2 = GLOBE_EQU_RADIUS_EARTH * GLOBE_EQU_RADIUS_EARTH;
    double ep2 = GLOBE_POL_RADIUS_EARTH * GLOBE_POL_RADIUS_EARTH;
    ep2 = (rad2/ep2) - 1.0;
    double nu2 = cos(lat);
    nu2 = ep2 * nu2 * nu2;
    double N = rad2 / (GLOBE_POL_RADIUS_EARTH * sqrt(1.0 + nu2));
    double t = tan(lat);
    double t2 = t * t;
    double l = lon - centralMeridian;
    // calculate coeffs (note l1ceof and l2coef = 1.0)
    double l3coef = 1.0 - t2 + nu2;
    double l4coef = 5.0 - t2 + (9.0*nu2) + (4.0*nu2*nu2);
    double l5coef = 5.0 - (18.0*t2) + (t2*t2) + (14.0*nu2) - (58.0*t2*nu2);
    double l6coef = 61.0 - (58.0*t2) + (t2*t2) + (270.0*nu2) - (330.0*t2*nu2);
    double l7coef = 61.0 - (479.0*t2) + (179.0*t2*t2) - (t2*t2*t2);
    double l8coef = 1385.0 - (3111.0*t2) + (543.0*t2*t2) - (t2*t2*t2);

    // compute easting and adjust
    easting = (N * cos(lat) * l) +
              (N / 6.0 * pow(cos(lat), 3.0) * l3coef * pow(l, 3.0)) +
              (N / 120.0 * pow(cos(lat), 5.0) * l5coef * pow(l, 5.0)) +
              (N / 5040.0 * pow(cos(lat), 7.0) * l7coef * pow(l, 7.0));
    easting = (UTM_SCALE_FACTOR * easting) + 500000.0;

    // Compute northing and adjust
    northing = ComputeMeridianArcLength(lat);
    northing += (t / 2.0 * N * pow(cos(lat), 2.0) * pow(l, 2.0)) +
                (t / 24.0 * N * pow(cos(lat), 4.0) * l4coef * pow(l, 4.0)) +
                (t / 720.0 * N * pow(cos(lat), 6.0) * l6coef * pow(l, 6.0)) +
                (t / 40320.0 * N * pow(cos(lat), 8.0) * l8coef * pow(l, 8.0));
    northing *= UTM_SCALE_FACTOR;
    if (northing < 0.0) northing += 10000000.0;
    if (lat < 0.0) zone = -zone;  // negative "zone" value indicates Southern hemisphere
    return true;
}  // end ProtoLocation::ConvertGeodeticToUtm()


double ProtoLocation::ComputeFootpointLatitude(double northing)
{
    // Reference: Hoffmann-Wellenhof, B., Lichtenegger, H., and Collins, J.,
    // GPS: Theory and Practice, 3rd ed. New York: Springer-Verlag Wien, 1994.

    // Precalculate n (Eq. 10.18)
    double n = (GLOBE_EQU_RADIUS_EARTH - GLOBE_POL_RADIUS_EARTH) /
               (GLOBE_EQU_RADIUS_EARTH + GLOBE_POL_RADIUS_EARTH);

    // Precalculate alpha_ (Eq. 10.22)
    // (Same as alpha in Eq. 10.17)
    double alpha = ((GLOBE_EQU_RADIUS_EARTH + GLOBE_POL_RADIUS_EARTH) / 2.0) *
                    (1.0 + (n*n/ 4) + (pow(n,4.0)/ 64));

    // Precalculate y
    double y = northing / alpha;
    // Precalculate beta_ (Eq. 10.22)
    double beta = (3.0*n/2.0) + (-27.0*pow(n,3.0)/32.0) + (269.0*pow(n,5.0)/512.0);
    // Precalculate gamma_ (Eq. 10.22)
    double gamma = (21.0*n*n/16.0) + (-55.0*pow(n,4.0)/32.0);
    // Precalculate delta_ (Eq. 10.22)
    double delta = (151.0*pow(n,3.0)/96.0) + (-417.0*pow(n,5.0)/128.0);
    // Precalculate epsilon_ (Eq. 10.22) */
    double epsilon = (1097.0 * pow(n,4.0) / 512.0);

    // Now calculate the sum of the series (Eq. 10.21)
    double result = y + (beta*sin(2.0*y)) + (gamma*sin(4.0*y)) +
                   (delta*sin(6.0*y)) + (epsilon*sin(8.0*y));
    return result;
}  // end ProtoLocation::ComputeFootpointLatitude()

void ProtoLocation::ConvertUtmToGeodetic(int     zone,   // input, negative value indicates southern hemisphere
                                    double  easting,
                                    double  northing,
                                    double& lat,   // output, in degrees
                                    double& lon)   // output, in degrees
{
    easting -= 500000.0;
    easting /= UTM_SCALE_FACTOR;
    // If in southern hemisphere, adjust northing
    if (zone < 0) northing -= 10000000.0;
    northing /= UTM_SCALE_FACTOR;

    // Compute "lambda0" (zone central meridian in radians)
    double lambda0 = -183.0 + abs(zone)*6.0;  // in degrees
    lambda0 *= PI / 180.0;  // convert to radians

    // Precalculate ep2
    double rad2 = GLOBE_EQU_RADIUS_EARTH * GLOBE_EQU_RADIUS_EARTH;
    double ep2 = GLOBE_POL_RADIUS_EARTH * GLOBE_POL_RADIUS_EARTH;
    ep2 = (rad2/ep2) - 1.0;

    // Get the value of phif, the footpoint latitude
    double phif = ComputeFootpointLatitude(northing);
    // Precalculate cos(phif)
    double cf = cos (phif);

    // Precalculate nuf2
    double nuf2 = ep2 * pow(cf, 2.0);

    // Precalculate Nf and initialize Nfpow
    double Nf = (GLOBE_EQU_RADIUS_EARTH * GLOBE_EQU_RADIUS_EARTH)/
                (GLOBE_POL_RADIUS_EARTH * sqrt(1 + nuf2));
    double Nfpow = Nf;

    // Precalculate tf
    double tf = tan (phif);
    double tf2 = tf * tf;
    double tf4 = tf2 * tf2;

    // Precalculate fractional coefficients for x**n in the equations
    // below to simplify the expressions for latitude and longitude.
    double x1frac = 1.0 / (Nfpow * cf);
    Nfpow *= Nf;   // now equals Nf**2)
    double x2frac = tf / (2.0 * Nfpow);
    Nfpow *= Nf;   // now equals Nf**3)
    double x3frac = 1.0 / (6.0 * Nfpow * cf);
    Nfpow *= Nf;   // now equals Nf**4)
    double x4frac = tf / (24.0 * Nfpow);
    Nfpow *= Nf;   // now equals Nf**5)
    double x5frac = 1.0 / (120.0 * Nfpow * cf);
    Nfpow *= Nf;   // now equals Nf**6)
    double x6frac = tf / (720.0 * Nfpow);
    Nfpow *= Nf;   // now equals Nf**7)
    double x7frac = 1.0 / (5040.0 * Nfpow * cf);
    Nfpow *= Nf;   // now equals Nf**8)
    double x8frac = tf / (40320.0 * Nfpow);

    // Precalculate polynomial coefficients for x**n.
    // Note x**1 does not have a polynomial coefficient.
    double x2poly = -1.0 - nuf2;

    double x3poly = -1.0 - 2 * tf2 - nuf2;

    double x4poly = 5.0 + (3.0*tf2) + (6.0*nuf2) - 6.0 * tf2*nuf2 -
                    (3.0*nuf2*nuf2) - (9.0*tf2*nuf2*nuf2);

    double x5poly = 5.0 + (28.0*tf2) + (24.0*tf4) + (6.0*nuf2) + (8.0*tf2*nuf2);

    double x6poly = -61.0 - (90.0*tf2) - (45.0*tf4) - (107.0*nuf2) + (162.0*tf2*nuf2);

    double x7poly = -61.0 - (662.0*tf2) - (1320.0*tf4) - (720.0*tf4*tf2);

    double x8poly = 1385.0 + (3633.0*tf2) + (4095.0*tf4) + (1575*tf4*tf2);

    // Calculate latitude (in radians)
    lat = phif + (x2frac * x2poly * easting * easting) + (x4frac * x4poly * pow(easting, 4.0)) +
          (x6frac * x6poly * pow(easting, 6.0)) + (x8frac * x8poly * pow(easting, 8.0));
    lat *= 180.0/PI; // convert to degrees
    if (lat > 90.0)
        lat -= 180.0;
    else if (lat < -90.0)
        lat += 180.0;

    // Calculate longitude
    lon = lambda0 + (x1frac * easting) + (x3frac * x3poly * pow(easting, 3.0)) +
          (x5frac * x5poly * pow(easting, 5.0)) + (x7frac * x7poly * pow(easting, 7.0));
    lon *= 180.0/PI; // convert to degrees
    if (lon > 180.0)
        lon -= 360.0;
    else if (lon < -180.0)
        lon += 360.0;
}  // end ProtoLocation::ConvertUtmToGeodetic()

double ProtoLocation::GetArchimedeanSpiralLength(double a, double angle1, double angle2)
{
    // for spiral defined by r = (a * angle)
    double b1 = sqrt(1.0 + angle1*angle1);  // re-used inner term
    double len1 =  (a/2.0)*(angle1*b1 + log(angle1 + b1));
    double b2 = sqrt(1.0 + angle2*angle2);  // re-used inner term
    double len2 =  (a/2.0)*(angle2*b2 + log(angle2 + b2));
    return fabs(len2 - len1);
}  // end ProtoLocation::GetArchimedeanSpiralLength()

void ProtoLocation::GetArchimedeanSpiralParams(double R, double dist, double elev,
                                              double& alpha, double& deltaPhi,
                                              unsigned int iterMax)
{
//     Iterative algorithm to calculate the Archimedean Spiral
//     parameters for a "great circle route" with a given
//     distance _and_ elevation where the elevation is non-zero
//     and non-vertical (i.e. -90.0 < elev < 90.0 degrees, elev != 0.0)
//     (I.e., a constant rate of altitude change over the distance)
//     Note that if the elev is less than zero, the returned
//     spiral parameter reflects a starting point of the lower
//     destination radius (i.e. 'R - dist*sin(elev)', not "R").
//     The 'iterMax' parameter can be increased for greater
//     precision for larger number of elevation angles, but
//     4 provides sub-milimeter accuracy and beyond 64 or so
//     doesn't add much value.
//     Inputs:
//         R    : start radius (meters from Earth center)
//         dist : goal distance to move (meters)
//         elev : elevation angle (radians)
//     Returns:
//         alpha: Archimedean spiral constant (meters per radian)
//         phi  : Delta angle (in radians) for spiral segment that
//                starts at angle 'R1/a' and ends at 'Ra/a + phi'
//                where a*phi == dist*sin(elev) and R1 is the
//               'loc' altitude plus Earth radius

    // 'dh' is our goal altitude change over the
    // path "dist" given the "elev"
    double dh = fabs(dist*sin(elev));
    if (elev < 0.0) R = R - dh;
    assert(R > 0.0);   // burrowed past center of the Earth
    // Compute initial estimate of angular distance
    // using average great circle radius
    double phi = fabs(dist*cos(elev)) / (R + dh/2.0);
    double phiPrev = phi;
    double spiralLength = 0.0;
    unsigned int niter = 0;
    double alphaBest = 0.0;
    double phiBest = 0.0;
    double deltaMin = 0.0;
    while (niter < iterMax)
    {
        niter++;
        double a = dh / phi;
        double angle1 = R / a;
        double angle2 = angle1 + phi;
        double lengthPrev = spiralLength;
        spiralLength = GetArchimedeanSpiralLength(a, angle1, angle2);
        // Cache the best param values
        double delta = fabs(dist - spiralLength);
        if ((0.0 == lengthPrev) || (delta < deltaMin))
        {
            deltaMin = delta;
            alphaBest = a;
            phiBest = phi;
        }
        // Iteratively update "phi" to converge spiralLength == dist
        if (0.0 == lengthPrev)
        {
            // Scale phi according to dist/spiralLength
            // so we can learn current "slope"
            phi *= dist/spiralLength;
        }
        else
        {
            // Now use the slope knowledge gained for fast gradient descent
            if (phi == phiPrev) break;
            if (spiralLength == lengthPrev) break;
            double slope = (spiralLength - lengthPrev) / (phi - phiPrev);
            phiPrev = phi;
            phi += (dist - spiralLength) / slope;
        }
    }
    alpha = alphaBest;
    deltaPhi = phiBest;
}  // end ProtoLocation::GetArchimedeanSpiralParams()

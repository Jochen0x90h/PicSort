#pragma once



/// @brief Daylight saving time type
///
enum class DstType {
    NONE,
    EU,
    US
};

/// @brief Calculate if daylight saving time is in effect.
/// @param dstType Daylight saving time type
/// @param year Year (e.g., 2025)
/// @param month Month (1..12)
/// @param day Day of month (1..31)
/// @param dow Day of week (1 = monday .. 7 = sunday)
/// @return true if daylight saving time is in effect
inline bool isDst(DstType dstType, int year, int month, int day, int dow) {
    switch (dstType) {
    case DstType::EU:
        // https://stackoverflow.com/questions/5590429/calculating-daylight-saving-time-from-only-date
        {
            if (month < 3 || month > 10)  return false;
            if (month > 3 && month < 10)  return true;

            int previousSunday = day - dow;

            if (month == 3) return previousSunday >= 25;
            if (month == 10) return previousSunday < 25;
        }

        // https://forum.arduino.cc/t/rtc-mit-sommerzeit/168068
        // European Daylight Savings Time calculation by "jurs" for German Arduino Forum
        // input parameters: "normal time" for year, month, day, hour and tzHours (0=UTC, 1=MEZ)
        // return value: returns true during Daylight Saving Time, false otherwise
        /*{
            int tzHours = 0; // UTC
            if (month<3 || month>10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
            if (month>3 && month<10) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
            if (month==3 && (hour + 24 * day)>=(1 + tzHours + 24*(31 - (5 * year /4 + 4) % 7)) || month==10 && (hour + 24 * day)<(1 + tzHours + 24*(31 - (5 * year /4 + 1) % 7)))
                return true;
            else
                return false;
        }*/
    case DstType::US:
        // https://stackoverflow.com/questions/5590429/calculating-daylight-saving-time-from-only-date
        {
            //January, february, and december are out.
            if (month < 3 || month > 11) { return false; }
            //April to October are in
            if (month > 3 && month < 11) { return true; }
            int previousSunday = day - dow;
            //In march, we are DST if our previous sunday was on or after the 8th.
            if (month == 3) { return previousSunday >= 8; }
            //In november we must be before the first sunday to be dst.
            //That means the previous sunday must be before the 1st.
            return previousSunday <= 0;
        }
    default:
        return false;
    }
}

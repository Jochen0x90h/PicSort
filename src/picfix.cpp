#include "TinyEXIF.h" // https://github.com/cdcseacave/TinyEXIF
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <filesystem>
#include <ranges>
#include <errno.h>


namespace fs = std::filesystem;

// https://forum.arduino.cc/t/rtc-mit-sommerzeit/168068
bool summertime_EU(int year, int month, int day, int hour, int tzHours)
// European Daylight Savings Time calculation by "jurs" for German Arduino Forum
// input parameters: "normal time" for year, month, day, hour and tzHours (0=UTC, 1=MEZ)
// return value: returns true during Daylight Saving Time, false otherwise
{
    if (month<3 || month>10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
    if (month>3 && month<10) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
    if (month==3 && (hour + 24 * day)>=(1 + tzHours + 24*(31 - (5 * year /4 + 4) % 7)) || month==10 && (hour + 24 * day)<(1 + tzHours + 24*(31 - (5 * year /4 + 1) % 7)))
        return true;
    else
        return false;
}


void fix(const fs::path &path) {
    // determine jpeg size
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    int jpegSize = int(file.tellg());
    file.seekg(0);

    // allocate jpeg buffer
    uint8_t *jpegBuf = new uint8_t[jpegSize];

    // read jpeg into buffer
    file.read(reinterpret_cast<char *>(jpegBuf), jpegSize);
    file.close();

    // read exif
    TinyEXIF::EXIFInfo exif(jpegBuf, jpegSize);
    if (exif.Fields) {
        // get date
        if (!exif.DateTime.empty()) {
            auto in = std::istringstream(exif.DateTime);
            std::chrono::time_point<std::chrono::file_clock> time;
            in >> std::chrono::parse("%Y:%m:%d %H:%M:%S", time);
            if (time.time_since_epoch().count() != 0) {
                using namespace std::chrono_literals;

                // calc UTC time from exif time for Berlin
                time -= 1h; // convert from MEZ to UTC assuming winter time
                auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(time);
                auto tt = std::chrono::system_clock::to_time_t(systemTime);
                tm t = *gmtime(&tt); // UTC
                //tm t = *localtime(&tt);
                if (summertime_EU(1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, 0)) {
                    // summer time
                    time -= 1h;
                }

                // set file date
                fs::last_write_time(path, time);
            }
        }
    }
}

void doDirectory(const fs::path &path) {
	if (fs::is_directory(path)) {
		fs::directory_iterator end; // default construction yields past-the-end
		for (fs::directory_iterator it(path); it != end; ++it) {
			std::cout << fs::path(*it).string() << std::endl;
			doDirectory(*it);
		}
	} else {
        std::string ext = path.extension().string();
        if (ext == ".jpg" || ext == ".JPG") {
		    fix(path);
	    }
    }
}


int main(int argc, const char **argv) {
    doDirectory(".");

    return 0;
}
